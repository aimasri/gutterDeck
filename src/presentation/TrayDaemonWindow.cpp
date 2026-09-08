#include "TrayDaemonWindow.h"
#include "AppIcon.h"
#include "../infrastructure/ConfigManager.h"
#include <QGuiApplication>
#include <QScreen>
#include <QProcess>
#include <QDebug>

TrayDaemonWindow::TrayDaemonWindow(QObject* parent)
    : QObject(parent) {
    m_server = new QLocalServer(this);
    QLocalServer::removeServer("gutterdeck-tray-socket");
    if (!m_server->listen("gutterdeck-tray-socket")) {
        qWarning() << "Failed to start tray server:" << m_server->errorString();
        return;
    }
    
    connect(m_server, &QLocalServer::newConnection, this, &TrayDaemonWindow::onNewConnection);
    
    m_trayIcon = new QSystemTrayIcon(AppIcon::createAppIcon(), this);
    m_trayIcon->setToolTip("GutterDeck Tray Daemon");
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &TrayDaemonWindow::onTrayActivated);
    
    rebuildMenu();
    m_trayIcon->show();
}

TrayDaemonWindow::~TrayDaemonWindow() {
    m_server->close();
}

void TrayDaemonWindow::onNewConnection() {
    while (QLocalSocket* client = m_server->nextPendingConnection()) {
        connect(client, &QLocalSocket::readyRead, this, &TrayDaemonWindow::onClientReadyRead);
        connect(client, &QLocalSocket::disconnected, this, &TrayDaemonWindow::onClientDisconnected);
    }
}

void TrayDaemonWindow::onClientReadyRead() {
    auto* client = qobject_cast<QLocalSocket*>(sender());
    if (!client) return;
    
    while (client->canReadLine()) {
        QString line = QString::fromUtf8(client->readLine()).trimmed();
        if (line.startsWith("REGISTER:")) {
            QString profileId = line.mid(9);
            m_activeClients[profileId] = client;
            m_lastActiveProfileId = profileId;
            qDebug() << "TrayDaemon registered profile:" << profileId;
            rebuildMenu();
        }
    }
}

void TrayDaemonWindow::onClientDisconnected() {
    auto* client = qobject_cast<QLocalSocket*>(sender());
    if (!client) return;
    
    QString toRemove;
    for (auto it = m_activeClients.begin(); it != m_activeClients.end(); ++it) {
        if (it.value() == client) {
            toRemove = it.key();
            break;
        }
    }
    
    if (!toRemove.isEmpty()) {
        m_activeClients.remove(toRemove);
        if (m_lastActiveProfileId == toRemove) {
            m_lastActiveProfileId = m_activeClients.isEmpty() ? QString() : m_activeClients.keys().first();
        }
        rebuildMenu();
    }
    client->deleteLater();
}

void TrayDaemonWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        if (!m_lastActiveProfileId.isEmpty()) {
            sendCommandToProfile(m_lastActiveProfileId, "CMD:NEXT_DECK");
        }
    }
}

void TrayDaemonWindow::sendCommandToProfile(const QString& profileId, const QString& cmd) {
    if (m_activeClients.contains(profileId)) {
        QLocalSocket* sock = m_activeClients[profileId];
        QString msg = cmd + "\n";
        sock->write(msg.toUtf8());
        sock->flush();
    }
}

void TrayDaemonWindow::rebuildMenu() {
    auto* menu = new QMenu();
    
    auto profiles = ConfigManager::listProfiles();
    for (const auto& p : profiles) {
        bool isActive = m_activeClients.contains(p.id);
        QMenu* profileMenu = nullptr;
        if (isActive) {
            profileMenu = createActiveProfileMenu(p.id, p.displayName);
        } else {
            profileMenu = createInactiveProfileMenu(p.id, p.displayName);
        }
        profileMenu->setIcon(QIcon(AppIcon::createAppIcon().pixmap(16, 16)));
        profileMenu->setTitle(p.displayName + (isActive ? " (Active)" : ""));
        menu->addMenu(profileMenu);
    }
    
    menu->addSeparator();
    auto* quitAction = menu->addAction("✕ Quit Tray Daemon");
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
    
    m_trayIcon->setContextMenu(menu);
}

QMenu* TrayDaemonWindow::createActiveProfileMenu(const QString& profileId, const QString& profileName) {
    auto* m = new QMenu(profileName);
    
    ConfigManager cm(profileId);
    cm.loadConfig();
    const auto& decks = cm.getDecks();
    
    for (int i = 0; i < decks.size(); ++i) {
        auto* dm = new QMenu(decks[i].name, m);
        auto* editName = dm->addAction("✎ Edit Deck Name...");
        connect(editName, &QAction::triggered, [this, profileId, i]() { sendCommandToProfile(profileId, QString("CMD:EDIT_NAME:%1").arg(i)); });
        
        auto* editCmd = dm->addAction("⚙ Edit Launch Command...");
        connect(editCmd, &QAction::triggered, [this, profileId, i]() { sendCommandToProfile(profileId, QString("CMD:EDIT_CMD:%1").arg(i)); });
        
        auto* changeCol = dm->addAction("🎨 Change Color...");
        connect(changeCol, &QAction::triggered, [this, profileId, i]() { sendCommandToProfile(profileId, QString("CMD:CHANGE_COLOR:%1").arg(i)); });
        
        dm->addSeparator();
        auto* addDeck = dm->addAction("➕ Add New Deck...");
        connect(addDeck, &QAction::triggered, [this, profileId, i]() { sendCommandToProfile(profileId, QString("CMD:ADD_DECK:%1").arg(i)); });
        
        dm->addSeparator();
        auto* delDeck = dm->addAction("🗑 Delete Deck");
        connect(delDeck, &QAction::triggered, [this, profileId, i]() { sendCommandToProfile(profileId, QString("CMD:DELETE:%1").arg(i)); });
        
        m->addMenu(dm);
    }
    
    m->addSeparator();
    auto* closeAction = m->addAction("✕ Close Profile");
    connect(closeAction, &QAction::triggered, [this, profileId]() { sendCommandToProfile(profileId, "CMD:CLOSE_APP"); });
    
    return m;
}

QMenu* TrayDaemonWindow::createInactiveProfileMenu(const QString& profileId, const QString& profileName) {
    auto* m = new QMenu(profileName);
    
    auto screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        auto* s = screens[i];
        auto* action = m->addAction(QString("Launch on Display %1 (%2)").arg(i + 1).arg(s->name()));
        connect(action, &QAction::triggered, [profileId, s]() {
            QProcess::startDetached("gutterdeck", QStringList() << "-p" << profileId << "--screen" << s->name());
        });
    }
    
    return m;
}
