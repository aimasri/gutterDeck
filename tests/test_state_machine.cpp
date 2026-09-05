#include <QtTest/QtTest>
#include <QSignalSpy>
#include "domain/StateMachine.h"

/**
 * @brief Unit test suite for StateMachine and deadlock watchdog timer.
 */
class TestStateMachine : public QObject {
    Q_OBJECT

private slots:
    void testInitialState();
    void testValidTransitions();
    void testInvalidTransitions();
    void testForceReset();
    void testWatchdogAutoRecovery();
};

void TestStateMachine::testInitialState() {
    StateMachine sm;
    QCOMPARE(sm.currentState(), AppState::IDLE);
    QVERIFY(sm.isIdle());
}

void TestStateMachine::testValidTransitions() {
    StateMachine sm;
    QSignalSpy stateSpy(&sm, &StateMachine::stateChanged);

    // IDLE -> SWITCHING_OUT
    QVERIFY(sm.tryTransition(AppState::IDLE, AppState::SWITCHING_OUT));
    QCOMPARE(sm.currentState(), AppState::SWITCHING_OUT);
    QCOMPARE(stateSpy.count(), 1);

    // SWITCHING_OUT -> SWITCHING_IN
    QVERIFY(sm.tryTransition(AppState::SWITCHING_OUT, AppState::SWITCHING_IN));
    QCOMPARE(sm.currentState(), AppState::SWITCHING_IN);
    QCOMPARE(stateSpy.count(), 2);

    // SWITCHING_IN -> IDLE
    QVERIFY(sm.tryTransition(AppState::SWITCHING_IN, AppState::IDLE));
    QCOMPARE(sm.currentState(), AppState::IDLE);
    QCOMPARE(stateSpy.count(), 3);
}

void TestStateMachine::testInvalidTransitions() {
    StateMachine sm;

    // Direct jump from IDLE to SWITCHING_IN must fail
    QVERIFY(!sm.tryTransition(AppState::IDLE, AppState::SWITCHING_IN));
    QCOMPARE(sm.currentState(), AppState::IDLE);

    // Non-matching source state must fail
    QVERIFY(!sm.tryTransition(AppState::SWITCHING_OUT, AppState::SWITCHING_IN));
    QCOMPARE(sm.currentState(), AppState::IDLE);
}

void TestStateMachine::testForceReset() {
    StateMachine sm;
    QSignalSpy resetSpy(&sm, &StateMachine::watchdogTriggered);

    QVERIFY(sm.tryTransition(AppState::IDLE, AppState::SWITCHING_OUT));
    QCOMPARE(sm.currentState(), AppState::SWITCHING_OUT);

    sm.forceReset();
    QCOMPARE(sm.currentState(), AppState::IDLE);
    QCOMPARE(resetSpy.count(), 1);
}

void TestStateMachine::testWatchdogAutoRecovery() {
    // Set 100ms watchdog for fast test execution
    StateMachine sm(100);
    QSignalSpy watchdogSpy(&sm, &StateMachine::watchdogTriggered);

    QVERIFY(sm.tryTransition(AppState::IDLE, AppState::SWITCHING_OUT));
    QCOMPARE(sm.currentState(), AppState::SWITCHING_OUT);

    // Wait for watchdog to trigger (100ms + margin)
    QTest::qWait(250);

    QCOMPARE(watchdogSpy.count(), 1);
    QCOMPARE(sm.currentState(), AppState::IDLE);
}

QTEST_MAIN(TestStateMachine)
#include "test_state_machine.moc"
