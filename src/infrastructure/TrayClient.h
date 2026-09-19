#pragma once

#include <QObject>
#include <QLocalSocket>
#include <QString>

/**
 * @brief IPC client connecting an active gutterDeck profile instance to the headless Tray Daemon.
 * @details Establishes a local Unix domain socket connection (QLocalSocket) to
 *          "gutterdeck-tray-socket". Registers the running profile identity upon connection
 *          and parses incoming remote procedure text commands (e.g. CMD:NEXT_DECK, CMD:EDIT_NAME).
 *          Decouples system tray event handling from individual window management instances.
 * @note Operates asynchronously without blocking the GUI event loop. Emits strongly-typed
 *       Qt signals directly consumed by DeckController.
 */
class TrayClient : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Constructs a tray client tied to a specific profile identity.
     * @param profileId Unique slug identifier of the local profile.
     * @param parent Optional Qt parent object.
     */
    explicit TrayClient(const QString& profileId, QObject* parent = nullptr);
    ~TrayClient() override;

    /**
     * @brief Attempts to connect to the running tray daemon local server.
     * @return True if connection succeeded or was already open; false otherwise.
     */
    bool connectToDaemon();

signals:
    /**
     * @brief Emitted when the tray daemon triggers a transition to the next deck.
     */
    void nextDeckRequested();

    /**
     * @brief Emitted when the tray daemon triggers a transition to the previous deck.
     */
    void previousDeckRequested();

    /**
     * @brief Emitted when a deck name editing dialog is requested from the tray.
     * @param index Target deck index.
     */
    void editDeckNameRequested(int index);

    /**
     * @brief Emitted when a deck launch command edit is requested from the tray.
     * @param index Target deck index.
     */
    void editDeckCommandRequested(int index);

    /**
     * @brief Emitted when a deck color change dialog is requested from the tray.
     * @param index Target deck index.
     */
    void changeDeckColorRequested(int index);

    /**
     * @brief Emitted when adding a new deck slot is requested from the tray.
     * @param index Insertion target index.
     */
    void addNewDeckRequested(int index);

    /**
     * @brief Emitted when deleting a deck slot is requested from the tray.
     * @param index Target deck index to delete.
     */
    void deleteDeckRequested(int index);

    /**
     * @brief Emitted when application termination is requested from the tray.
     */
    void closeAppRequested();

private slots:
    /**
     * @brief Reads incoming newline-delimited command messages from the daemon socket.
     */
    void onReadyRead();

    /**
     * @brief Registers the local profile ID with the daemon once connected.
     */
    void onConnected();

    /**
     * @brief Handles unexpected server disconnection.
     */
    void onDisconnected();

    /**
     * @brief Logs socket connection or transport errors.
     * @param socketError The local socket error code.
     */
    void onError(QLocalSocket::LocalSocketError socketError);

private:
    QString m_profileId;
    QLocalSocket m_socket;
};
