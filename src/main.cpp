#include <memory>
#include <QApplication>
#include <QCursor>
#include <QDebug>
#include <QScreen>
#include <QSystemTrayIcon>
#include <iostream>
#include <QCommandLineParser>
#include <QProcess>
#include <QThread>

#include "infrastructure/XcbConnection.h"
#include "infrastructure/XcbEngine.h"
#include "infrastructure/WindowWatcher.h"
#include "infrastructure/ConfigManager.h"

#include "domain/StateMachine.h"
#include "domain/AppLauncher.h"
#include "domain/DeckController.h"

#include "presentation/OverlayWindow.h"
#include "presentation/GutterWidget.h"
#include "presentation/AppIcon.h"
#include "presentation/ProfilePickerWindow.h"
#include "presentation/TrayDaemonWindow.h"
#include "infrastructure/TrayClient.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("GutterDeck");
    app.setApplicationVersion("3.0");
    app.setWindowIcon(AppIcon::createAppIcon());

    QCommandLineParser parser;
    parser.setApplicationDescription("GutterDeck - High-Performance Linux Desktop Compositor Dock");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption profileOpt(QStringList() << "p" << "profile", "Load a specific profile by ID.", "profile_id");
    parser.addOption(profileOpt);
    
    QCommandLineOption listProfilesOpt(QStringList() << "l" << "list-profiles", "List available profiles and exit.");
    parser.addOption(listProfilesOpt);
    
    QCommandLineOption resetAutoLaunchOpt(QStringList() << "r" << "reset-auto-launch", "Reset auto-launch preference and show picker.");
    parser.addOption(resetAutoLaunchOpt);
    
    QCommandLineOption trayOpt(QStringList() << "tray", "Run as headless IPC Tray Daemon.");
    parser.addOption(trayOpt);
    

    
    parser.process(app);

    try {
        ConfigManager::migrateIfNeeded();
        
        if (parser.isSet(trayOpt)) {
            TrayDaemonWindow daemon;
            qDebug() << "Running IPC Tray Daemon...";
            return app.exec();
        }
        
        if (parser.isSet(listProfilesOpt)) {
            auto profiles = ConfigManager::listProfiles();
            std::cout << "Available Profiles:\n";
            for (const auto& p : profiles) {
                std::cout << " - " << p.id.toStdString() << " (" << p.displayName.toStdString() << ")\n";
            }
            return 0;
        }
        
        if (parser.isSet(resetAutoLaunchOpt)) {
            ConfigManager::setAutoLaunchProfile(QString());
        }

        QString targetProfileId;

        if (parser.isSet(profileOpt)) {
            targetProfileId = parser.value(profileOpt);
            if (!ConfigManager::profileExists(targetProfileId)) {
                std::cerr << "Error: Profile '" << targetProfileId.toStdString() << "' does not exist.\n";
                return 1;
            }
        } else {
            targetProfileId = ConfigManager::getAutoLaunchProfile();
            if (targetProfileId.isEmpty() || !ConfigManager::profileExists(targetProfileId)) {
                ProfilePickerWindow picker;
                QObject::connect(&picker, &ProfilePickerWindow::profileSelected, [&](const QString& id) {
                    targetProfileId = id;
                    picker.close();
                });
                QObject::connect(&picker, &ProfilePickerWindow::closedWithoutSelection, [&]() {
                    QApplication::quit();
                });
                picker.show();
                app.exec(); // Phase 1 Event Loop
                
                if (targetProfileId.isEmpty()) {
                    return 0; // User closed picker
                }
            }
        }

        // IPC Tray System
        TrayClient trayClient(targetProfileId);
        if (!trayClient.connectToDaemon()) {
            qDebug() << "Tray Daemon not found. Spawning it...";
            QProcess::startDetached(argv[0], QStringList() << "--tray");
            QThread::msleep(200);
            trayClient.connectToDaemon();
        }

        // 1. Infrastructure Layer Setup
        ConfigManager config(targetProfileId);
        if (!config.loadConfig()) {
            qWarning() << "Notice: Initialized with default deck configuration.";
        }

        XcbConnection xcbConn;
        XcbEngine xcbEngine(xcbConn);
        WindowWatcher windowWatcher(xcbConn, xcbEngine);

        // 2. Domain Layer Setup
        int watchdogTimeoutMs = std::max(3000, config.getSettings().animationDurationMs * 4);
        StateMachine stateMachine(watchdogTimeoutMs);
        AppLauncher appLauncher(config);

        DeckController controller(
            stateMachine,
            xcbEngine,
            windowWatcher,
            appLauncher,
            config
        );

        // 3. Presentation Layer Setup - Dynamic Startup Screen Adaptation
        QRect screenGeometry;
        QPoint cursorPos = QCursor::pos();
        QScreen* targetScreen = QGuiApplication::screenAt(cursorPos);
        
        // Fallback: If point is outside logical bounds (e.g. DPI scaling gaps), find the closest screen
        if (!targetScreen) {
            long long minDistance = -1;
            for (QScreen* s : QGuiApplication::screens()) {
                QPoint center = s->geometry().center();
                long long dx = center.x() - cursorPos.x();
                long long dy = center.y() - cursorPos.y();
                long long dist = (dx * dx) + (dy * dy);
                if (minDistance == -1 || dist < minDistance) {
                    minDistance = dist;
                    targetScreen = s;
                }
            }
        }

        if (targetScreen) {
            screenGeometry = targetScreen->geometry();
            qDebug() << "Startup screen auto-detected based on cursor position" << cursorPos
                     << "on display" << targetScreen->name() << ":" << screenGeometry;
        } else if (QScreen* primary = QGuiApplication::primaryScreen()) {
            screenGeometry = primary->geometry();
            qDebug() << "Fallback to primary screen" << primary->name() << ":" << screenGeometry;
        }
        
        // Final fallback if the user provided specific explicit bounds
        const auto& settings = config.getSettings();
        if (settings.screenWidth > 0 && settings.screenHeight > 0 && !targetScreen) {
             screenGeometry = QRect(0, 0, settings.screenWidth, settings.screenHeight);
        }

        OverlayWindow overlay(config, screenGeometry);
        overlay.setWindowIcon(app.windowIcon());
        controller.setOverlay(&overlay);

        // 4. Wire Presentation User Interactions to Domain Controller
        QObject::connect(&overlay, &OverlayWindow::gutterClicked,
                         &controller, &DeckController::onGutterClicked);
        QObject::connect(&overlay, &OverlayWindow::splitRequested,
                         &controller, &DeckController::onSplitRequested);
        QObject::connect(&overlay, &OverlayWindow::previousRequested,
                         &controller, [&controller]() { controller.switchToPreviousDeck(false); });
        QObject::connect(&overlay, &OverlayWindow::nextRequested,
                         &controller, [&controller]() { controller.switchToNextDeck(false); });
        QObject::connect(&overlay, &OverlayWindow::contextMenuRequested,
                         &controller, &DeckController::onGutterContextMenuRequested);

        // 5. Initialize Controller & Show Overlay
        controller.initialize();

        // 6. Wire IPC Tray Client to Domain Controller
        QObject::connect(&trayClient, &TrayClient::nextDeckRequested,
                         &controller, [&]() { controller.switchToNextDeck(false); });
        QObject::connect(&trayClient, &TrayClient::previousDeckRequested,
                         &controller, [&]() { controller.switchToPreviousDeck(false); });
        QObject::connect(&trayClient, &TrayClient::closeAppRequested,
                         &app, &QCoreApplication::quit);
        QObject::connect(&trayClient, &TrayClient::editDeckNameRequested,
                         &controller, [&](int idx) { controller.onEditDeckName(idx, QCursor::pos()); });
        QObject::connect(&trayClient, &TrayClient::editDeckCommandRequested,
                         &controller, [&](int idx) { controller.onEditDeckCommand(idx, QCursor::pos()); });
        QObject::connect(&trayClient, &TrayClient::changeDeckColorRequested,
                         &controller, [&](int idx) { controller.onChangeDeckColor(idx, QCursor::pos()); });
        QObject::connect(&trayClient, &TrayClient::addNewDeckRequested,
                         &controller, [&](int idx) { controller.onAddNewDeck(idx, QCursor::pos()); });
        QObject::connect(&trayClient, &TrayClient::deleteDeckRequested,
                         &controller, [&](int idx) { controller.onCloseDeck(idx); });
                         
        // Ensure X11 EWMH window type is DOCK so Compton/window managers exempt it from drop shadows
        static_cast<void>(xcbEngine.setWindowTypeDock(static_cast<xcb_window_t>(overlay.winId())));

        if (xcbEngine.getCurrentDesktop() == controller.assignedDesktop()) {
            overlay.show();
        } else {
            overlay.hide();
        }

        qDebug() << "Gutter Deck v3.0 successfully initialized. Running event loop.";
        return app.exec();

    } catch (const std::exception& ex) {
        std::cerr << "Fatal initialization error in Gutter Deck: " << ex.what() << std::endl;
        return 1;
    }
}
