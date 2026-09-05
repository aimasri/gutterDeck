#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QVector>

class ConfigManager;

/**
 * @brief Application launcher for user-configured shell commands.
 * @details Executes user-defined commands via /bin/sh -c asynchronously,
 *          capturing operating system Process IDs (PIDs). Facilitates
 *          PID-to-X11 window binding in the DeckController.
 * @note Operates on a staggered timer during bulk startup to avoid overloading
 *       the X server and window manager.
 */
class AppLauncher : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Constructs launcher with injected configuration.
     * @param config Reference to active ConfigManager.
     * @param parent Optional Qt parent.
     */
    explicit AppLauncher(const ConfigManager& config, QObject* parent = nullptr);
    ~AppLauncher() override = default;

    AppLauncher(const AppLauncher&) = delete;
    AppLauncher& operator=(const AppLauncher&) = delete;
    AppLauncher(AppLauncher&&) = delete;
    AppLauncher& operator=(AppLauncher&&) = delete;

    /**
     * @brief Launches all configured decks sequentially using a staggered timer.
     */
    void launchAll();

    /**
     * @brief Launches an individual deck command immediately.
     * @param index Zero-based index of the deck in ConfigManager.
     * @return Process ID of launched application, or -1 on failure.
     */
    qint64 launchDeck(int index);

    /**
     * @brief Retrieves captured PID for a given deck index.
     * @param index Zero-based index of the deck.
     * @return PID, or 0 if not running or uncaptured.
     */
    [[nodiscard]] qint64 getDeckPid(int index) const;

    /**
     * @brief Finds which deck index corresponds to a given process ID.
     * @param pid Process ID to search for.
     * @return Deck index, or -1 if no match found.
     */
    [[nodiscard]] int findDeckIndexByPid(qint64 pid) const;

    /**
     * @brief Inserts an empty PID entry for a newly added deck at a specific index.
     */
    void insertDeck(int index);

    /**
     * @brief Removes tracked PID entry for a deleted deck index.
     */
    void removeDeck(int index);

    /**
     * @brief Reorders tracked PIDs matching a new deck index permutation.
     */
    void reorderDecks(const QVector<int>& newOrder);

signals:
    /**
     * @brief Emitted when a deck application process is started.
     * @param index Deck index.
     * @param pid Operating system Process ID.
     * @param command The executed command string.
     */
    void deckLaunched(int index, qint64 pid, const QString& command);

    /**
     * @brief Emitted once all configured decks have been dispatched.
     */
    void allDecksLaunched();

private slots:
    void onLaunchNextStaggered();

private:
    const ConfigManager& m_config;
    QTimer m_staggerTimer;
    int m_currentIndex = 0;
    QVector<qint64> m_pids;
};
