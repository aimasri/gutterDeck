#include <memory>
#include <QApplication>
#include <QCursor>
#include <QDebug>
#include <QScreen>
#include <QSystemTrayIcon>
#include <iostream>

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

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("GutterDeck");
    app.setApplicationVersion("3.0");
    app.setWindowIcon(AppIcon::createAppIcon());

    try {
        // 1. Infrastructure Layer Setup
        ConfigManager config;
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
        QRect screenGeometry = QGuiApplication::primaryScreen() 
            ? QGuiApplication::primaryScreen()->geometry() 
            : xcbEngine.getScreenGeometry();
        const auto& settings = config.getSettings();
        if (settings.targetScreen == "auto" || settings.targetScreen.isEmpty()) {
            QPoint cursorPos = QCursor::pos();
            QScreen* cursorScreen = QGuiApplication::screenAt(cursorPos);
            if (cursorScreen) {
                screenGeometry = cursorScreen->geometry();
                qDebug() << "Startup screen auto-detected at cursor position" << cursorPos
                         << "on display" << cursorScreen->name() << ":" << screenGeometry;
            } else if (QScreen* primary = QGuiApplication::primaryScreen()) {
                screenGeometry = primary->geometry();
                qDebug() << "Fallback to primary screen" << primary->name() << ":" << screenGeometry;
            }
        } else {
            bool found = false;
            for (auto* s : QGuiApplication::screens()) {
                if (s->name() == settings.targetScreen) {
                    screenGeometry = s->geometry();
                    found = true;
                    qDebug() << "Using target screen by name:" << s->name() << ":" << screenGeometry;
                    break;
                }
            }
            if (!found && settings.screenWidth > 0 && settings.screenHeight > 0) {
                screenGeometry = QRect(0, 0, settings.screenWidth, settings.screenHeight);
            }
        }
        OverlayWindow overlay(config, screenGeometry);
        overlay.setWindowIcon(app.windowIcon());
        controller.setOverlay(&overlay);

        // 4. Wire Presentation User Interactions to Domain Controller
        QObject::connect(&overlay, &OverlayWindow::gutterClicked,
                         &controller, &DeckController::onGutterClicked);
        QObject::connect(&overlay, &OverlayWindow::previousRequested,
                         &controller, [&controller]() { controller.switchToPreviousDeck(false); });
        QObject::connect(&overlay, &OverlayWindow::nextRequested,
                         &controller, [&controller]() { controller.switchToNextDeck(false); });
        QObject::connect(&overlay, &OverlayWindow::contextMenuRequested,
                         &controller, &DeckController::onGutterContextMenuRequested);

        // 5. Initialize Controller & Show Overlay
        controller.initialize();

        // 6. Setup Taskbar Desktop System Tray Icon with colored barcode graphic
        std::unique_ptr<QSystemTrayIcon> trayIcon;
        if (QSystemTrayIcon::isSystemTrayAvailable()) {
            trayIcon = std::make_unique<QSystemTrayIcon>(app.windowIcon(), &app);
            trayIcon->setToolTip(QStringLiteral("GutterDeck"));
            QObject::connect(trayIcon.get(), &QSystemTrayIcon::activated, &controller,
                             [&controller](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger) {
                    controller.switchToNextDeck(false);
                } else if (reason == QSystemTrayIcon::Context) {
                    controller.onGutterContextMenuRequested(controller.activeDeckIndex(), QCursor::pos());
                }
            });
            trayIcon->show();
        }

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
