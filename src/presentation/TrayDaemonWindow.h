#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMap>
#include <QMenu>

/**
 * @brief Headless system tray daemon managing global tray menus and profile lifecycle.
 * @details Runs as an independent headless support daemon (`gutterdeck --tray`) hosting
 *          a local Unix domain socket server (QLocalServer at "gutterdeck-tray-socket").
 *          Aggregates all running gutterDeck profile instances into a single unified
 *          system tray icon. Allows users to switch decks, open deck configuration dialogs,
 *          and spawn inactive profiles on connected monitors.
 * @note Automatically shuts down cleanly when the last connected profile client disconnects.
 */
class TrayDaemonWindow : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Constructs the tray daemon, listens on local socket, and displays tray icon.
     * @param parent Optional Qt parent object.
     */
    explicit TrayDaemonWindow(QObject* parent = nullptr);
    ~TrayDaemonWindow() override;

private slots:
    /**
     * @brief Accepts pending incoming client profile connections.
     */
    void onNewConnection();

    /**
     * @brief Reads registration messages from connected profile clients.
     */
    void onClientReadyRead();

    /**
     * @brief Unregisters disconnected profile clients and exits daemon if none remain.
     */
    void onClientDisconnected();

    /**
     * @brief Handles system tray icon clicks (e.g. left click cycles decks).
     * @param reason The trigger activation reason.
     */
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);

    /**
     * @brief Dynamically reconstructs the hierarchical context menu listing active and inactive profiles.
     */
    void rebuildMenu();

    /**
     * @brief Formats and writes an IPC text command to a specific registered profile client socket.
     * @param profileId Target profile slug.
     * @param cmd Command string (e.g. "CMD:NEXT_DECK").
     */
    void sendCommandToProfile(const QString& profileId, const QString& cmd);

private:
    QSystemTrayIcon* m_trayIcon = nullptr;
    QLocalServer* m_server = nullptr;
    QMap<QString, QLocalSocket*> m_activeClients;
    QString m_lastActiveProfileId;
    
    /**
     * @brief Builds context submenu for a currently running profile instance.
     * @param profileId Profile slug identifier.
     * @param profileName Human-readable display name.
     * @return Fully populated QMenu pointer.
     */
    QMenu* createActiveProfileMenu(const QString& profileId, const QString& profileName);

    /**
     * @brief Builds context submenu for an inactive profile allowing launch on specific displays.
     * @param profileId Profile slug identifier.
     * @param profileName Human-readable display name.
     * @return Fully populated QMenu pointer with monitor options.
     */
    QMenu* createInactiveProfileMenu(const QString& profileId, const QString& profileName);
};
