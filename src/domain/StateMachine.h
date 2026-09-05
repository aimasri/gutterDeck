#pragma once

#include <QObject>
#include <QTimer>

/**
 * @brief Discrete operational states for the deck-switching lifecycle.
 */
enum class AppState {
    IDLE,           ///< Normal state: Gutters are interactive and click-through mask is active.
    SWITCHING_OUT,  ///< Phase 1: Curtain is animating inward over the screen, input is locked.
    SWITCHING_IN    ///< Phase 2: Windows are swapped and curtain is animating outward, input is locked.
};

/**
 * @brief Thread-safe and deadlock-proof state machine governing deck switching.
 * @details Enforces strict state transitions:
 *          IDLE -> SWITCHING_OUT -> SWITCHING_IN -> IDLE.
 *          Integrates an automatic watchdog timer that forces a recovery reset
 *          if an animation or transition is dropped or aborts prematurely.
 * @note Permanently prevents the "frozen brick" bug documented in the project history.
 */
class StateMachine : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Constructs the state machine with a watchdog safety timeout.
     * @param watchdogTimeoutMs Maximum milliseconds allowed in a non-IDLE state before auto-reset.
     * @param parent Optional Qt parent.
     */
    explicit StateMachine(int watchdogTimeoutMs = 5000, QObject* parent = nullptr);
    ~StateMachine() override = default;

    StateMachine(const StateMachine&) = delete;
    StateMachine& operator=(const StateMachine&) = delete;
    StateMachine(StateMachine&&) = delete;
    StateMachine& operator=(StateMachine&&) = delete;

    /**
     * @brief Attempts to transition from an expected current state to a target state.
     * @param from The expected current state.
     * @param to The requested target state.
     * @return True if transition was valid and successfully applied; false otherwise.
     */
    [[nodiscard]] bool tryTransition(AppState from, AppState to);

    /**
     * @brief Queries the active state.
     */
    [[nodiscard]] AppState currentState() const noexcept;

    /**
     * @brief Convenience check if the state is IDLE.
     */
    [[nodiscard]] bool isIdle() const noexcept;

    /**
     * @brief Emergency reset to IDLE state, disabling watchdog timer.
     */
    void forceReset();

signals:
    /**
     * @brief Emitted whenever the active state changes.
     */
    void stateChanged(AppState oldState, AppState newState);

    /**
     * @brief Emitted when the watchdog triggers an automatic deadlock recovery.
     */
    void watchdogTriggered();

private slots:
    void onWatchdogTimeout();

private:
    AppState m_state = AppState::IDLE;
    QTimer m_watchdogTimer;
    int m_timeoutMs;

    [[nodiscard]] bool isValidTransition(AppState from, AppState to) const noexcept;
};
