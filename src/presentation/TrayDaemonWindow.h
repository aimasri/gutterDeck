#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMap>
#include <QMenu>

class TrayDaemonWindow : public QObject {
    Q_OBJECT
public:
    explicit TrayDaemonWindow(QObject* parent = nullptr);
    ~TrayDaemonWindow() override;

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void rebuildMenu();
    void sendCommandToProfile(const QString& profileId, const QString& cmd);

private:
    QSystemTrayIcon* m_trayIcon = nullptr;
    QLocalServer* m_server = nullptr;
    QMap<QString, QLocalSocket*> m_activeClients;
    QString m_lastActiveProfileId;
    
    QMenu* createActiveProfileMenu(const QString& profileId, const QString& profileName);
    QMenu* createInactiveProfileMenu(const QString& profileId, const QString& profileName);
};
