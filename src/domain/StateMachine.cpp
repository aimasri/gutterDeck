#include "StateMachine.h"

#include <QDebug>

StateMachine::StateMachine(int watchdogTimeoutMs, QObject* parent)
    : QObject(parent),
      m_timeoutMs(watchdogTimeoutMs) {
    m_watchdogTimer.setSingleShot(true);
    connect(&m_watchdogTimer, &QTimer::timeout, this, &StateMachine::onWatchdogTimeout);
}

bool StateMachine::isValidTransition(AppState from, AppState to) const noexcept {
    if (from == AppState::IDLE && to == AppState::SWITCHING_OUT) {
        return true;
    }
    if (from == AppState::SWITCHING_OUT && to == AppState::SWITCHING_IN) {
        return true;
    }
    if (from == AppState::SWITCHING_IN && to == AppState::IDLE) {
        return true;
    }
    return false;
}

bool StateMachine::tryTransition(AppState from, AppState to) {
    if (m_state != from || !isValidTransition(from, to)) {
        qWarning() << "Invalid state transition attempt from"
                   << static_cast<int>(m_state) << "to" << static_cast<int>(to);
        return false;
    }

    AppState oldState = m_state;
    m_state = to;

    if (m_state == AppState::IDLE) {
        m_watchdogTimer.stop();
    } else {
        m_watchdogTimer.start(m_timeoutMs);
    }

    emit stateChanged(oldState, m_state);
    return true;
}

AppState StateMachine::currentState() const noexcept {
    return m_state;
}

bool StateMachine::isIdle() const noexcept {
    return m_state == AppState::IDLE;
}

void StateMachine::forceReset() {
    if (m_state != AppState::IDLE) {
        AppState oldState = m_state;
        m_state = AppState::IDLE;
        m_watchdogTimer.stop();
        qWarning() << "Emergency state machine reset triggered from state"
                   << static_cast<int>(oldState);
        emit stateChanged(oldState, AppState::IDLE);
        emit watchdogTriggered();
    }
}

void StateMachine::onWatchdogTimeout() {
    qCritical() << "StateMachine deadlock watchdog expired! Forcing reset to IDLE.";
    forceReset();
}
