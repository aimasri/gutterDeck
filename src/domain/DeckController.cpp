#include "DeckController.h"
#include "StateMachine.h"
#include "AppLauncher.h"
#include "../infrastructure/XcbEngine.h"
#include "../infrastructure/WindowWatcher.h"
#include "../infrastructure/ConfigManager.h"
#include "../presentation/OverlayWindow.h"
#include "../presentation/CurtainWidget.h"
#include "../presentation/GutterWidget.h"
#include "../presentation/AppIcon.h"
#include "../presentation/SleekDialogs.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QRegularExpression>
#include <QThread>
#include <QUuid>
#include <csignal>
#include <sys/types.h>

namespace {
bool matchesProcessCommand(uint32_t pid, const QString& expectedCommand) {
    if (pid == 0 || expectedCommand.trimmed().isEmpty()) {
        return false;
    }
    QFile cmdlineFile(QString("/proc/%1/cmdline").arg(pid));
    if (cmdlineFile.open(QIODevice::ReadOnly)) {
        QByteArray data = cmdlineFile.readAll();
        cmdlineFile.close();
        if (!data.isEmpty()) {
            QString procCmd = QString::fromUtf8(data).replace('\0', ' ').trimmed().toLower();
            QString expectedBin = expectedCommand.trimmed().split(' ').first();
            int slashIdx = expectedBin.lastIndexOf('/');
            if (slashIdx >= 0) {
                expectedBin = expectedBin.mid(slashIdx + 1);
            }
            expectedBin = expectedBin.toLower();

            if (!expectedBin.isEmpty() && procCmd.contains(expectedBin)) {
                return true;
            }

            // Sub-token match (e.g. "google-chrome" vs "/opt/google/chrome/chrome")
            QStringList tokens = expectedBin.split(QRegularExpression("[\\-_]"), Qt::SkipEmptyParts);
            for (const QString& token : tokens) {
                if (token.length() >= 4 && procCmd.contains(token)) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool isProcessDescendant(uint32_t childPid, uint32_t targetPid) {
    if (childPid == 0 || targetPid == 0) {
        return false;
    }
    if (childPid == targetPid) {
        return true;
    }

    uint32_t curr = childPid;
    for (int depth = 0; depth < 16; ++depth) {
        QFile statFile(QString("/proc/%1/stat").arg(curr));
        if (!statFile.open(QIODevice::ReadOnly)) {
            break;
        }
        QByteArray content = statFile.readAll();
        statFile.close();

        int closingParen = content.lastIndexOf(')');
        if (closingParen < 0) {
            break;
        }
        QByteArray afterParen = content.mid(closingParen + 2).trimmed();
        QList<QByteArray> parts = afterParen.split(' ');
        if (parts.size() < 2) {
            break;
        }
        bool ok = false;
        uint32_t ppid = parts[1].toUInt(&ok);
        if (!ok || ppid <= 1) {
            break;
        }
        if (ppid == targetPid) {
            return true;
        }
        curr = ppid;
    }
    return false;
}
} // namespace

DeckController::DeckController(
    StateMachine& stateMachine,
    XcbEngine& xcbEngine,
    WindowWatcher& windowWatcher,
    AppLauncher& appLauncher,
    ConfigManager& config,
    QObject* parent
) : QObject(parent),
    m_stateMachine(stateMachine),
    m_xcbEngine(xcbEngine),
    m_windowWatcher(windowWatcher),
    m_appLauncher(appLauncher),
    m_config(config) {}

DeckController::~DeckController() {
    static_cast<void>(m_xcbEngine.ungrabAltLeftRightKeys());
        static_cast<void>(m_xcbEngine.ungrabCtrlShiftScroll());
}

void DeckController::setOverlay(OverlayWindow* overlay) noexcept {
    m_overlay = overlay;
}

void DeckController::initialize() {
    m_decks.clear();
    const auto& configDecks = m_config.getDecks();

    for (const auto& cfg : configDecks) {
        DeckSlot slot;
        slot.id = cfg.id;
        slot.name = cfg.name;
        slot.command = cfg.command;
        slot.color = cfg.color;
        slot.pid = 0;
        slot.windowId = XCB_WINDOW_NONE;
        slot.isMapped = false;
        m_decks.append(slot);
    }

    if (m_config.getSettings().targetWorkspace >= 0) {
        m_assignedDesktop = static_cast<uint32_t>(m_config.getSettings().targetWorkspace);
    } else {
        m_assignedDesktop = m_xcbEngine.getCurrentDesktop();
    }

    // Connect WindowWatcher signals
    connect(&m_windowWatcher, &WindowWatcher::windowMapped,
            this, &DeckController::onWindowMapped);
    connect(&m_windowWatcher, &WindowWatcher::windowDestroyed,
            this, &DeckController::onWindowDestroyed);
    connect(&m_windowWatcher, &WindowWatcher::currentDesktopChanged,
            this, &DeckController::onCurrentDesktopChanged);
    connect(&m_windowWatcher, &WindowWatcher::previousDeckRequested,
            this, [this]() { switchToPreviousDeck(false); });
    connect(&m_windowWatcher, &WindowWatcher::nextDeckRequested,
            this, [this]() { switchToNextDeck(false); });
    QObject::connect(&m_windowWatcher, &WindowWatcher::globalScrollUp,
                     this, [this]() { switchToPreviousDeck(false); });
    QObject::connect(&m_windowWatcher, &WindowWatcher::globalScrollDown,
                     this, [this]() { switchToNextDeck(false); });

    // Connect AppLauncher signals
    connect(&m_appLauncher, &AppLauncher::deckLaunched,
            this, &DeckController::onDeckLaunched);

    // Connect StateMachine watchdog for emergency recovery
    connect(&m_stateMachine, &StateMachine::watchdogTriggered, this, [this]() {
        if (m_overlay) {
            m_overlay->updateMask(AppState::IDLE);
        }
    });

    // Start watching X11 window lifecycle
    m_windowWatcher.startWatching();

    // Grab Alt+Left / Alt+Right navigation hotkeys if currently on assigned desktop
    if (m_xcbEngine.getCurrentDesktop() == m_assignedDesktop) {
        static_cast<void>(m_xcbEngine.grabAltLeftRightKeys());
        static_cast<void>(m_xcbEngine.grabCtrlShiftScroll());
    }

    // Staggered launch of user commands
    m_appLauncher.launchAll();
}

uint32_t DeckController::assignedDesktop() const noexcept {
    return m_assignedDesktop;
}

int DeckController::activeDeckIndex() const noexcept {
    return m_activeDeckIndex;
}

const QVector<DeckSlot>& DeckController::decks() const noexcept {
    return m_decks;
}

bool DeckController::isSplitMode() const noexcept {
    return m_isSplitMode;
}

SplitOrientation DeckController::splitOrientation() const noexcept {
    return m_splitOrientation;
}

int DeckController::splitPrimaryIndex() const noexcept {
    return m_splitPrimaryIndex;
}

int DeckController::splitSecondaryIndex() const noexcept {
    return m_splitSecondaryIndex;
}

QRect DeckController::splitPrimaryGeometry() const {
    QRect scr = targetGeometry();
    if (m_splitOrientation == SplitOrientation::Vertical) {
        return QRect(scr.x(), scr.y(), scr.width() / 2, scr.height());
    } else {
        return QRect(scr.x(), scr.y(), scr.width(), scr.height() / 2);
    }
}

QRect DeckController::splitSecondaryGeometry() const {
    QRect scr = targetGeometry();
    if (m_splitOrientation == SplitOrientation::Vertical) {
        int halfW = scr.width() / 2;
        return QRect(scr.x() + halfW, scr.y(), scr.width() - halfW, scr.height());
    } else {
        int halfH = scr.height() / 2;
        return QRect(scr.x(), scr.y() + halfH, scr.width(), scr.height() - halfH);
    }
}

void DeckController::onSplitRequested(int index, SplitOrientation orientation) {
    if (index < 0 || index >= m_decks.size()) {
        return;
    }
    if (m_activeDeckIndex < 0 || m_activeDeckIndex >= m_decks.size()) {
        return;
    }
    if (index == m_activeDeckIndex) {
        return;
    }
    // Adjacency requirement: must be immediately adjacent to active deck
    if (std::abs(index - m_activeDeckIndex) != 1) {
        qDebug() << "Split view rejected: target deck" << index
                 << "is not adjacent to active deck" << m_activeDeckIndex;
        return;
    }
    if (!m_stateMachine.isIdle()) {
        qDebug() << "Split view rejected: StateMachine is not IDLE.";
        return;
    }

    xcb_window_t activeWin = m_decks[m_activeDeckIndex].windowId;
    xcb_window_t targetWin = m_decks[index].windowId;
    if (activeWin == XCB_WINDOW_NONE || targetWin == XCB_WINDOW_NONE) {
        qDebug() << "Split view rejected: one or both decks do not have an attached window.";
        return;
    }

    // Lower index is primary (left or top), higher index is secondary (right or bottom)
    int firstDeck = std::min(m_activeDeckIndex, index);
    int secondDeck = std::max(m_activeDeckIndex, index);

    enterSplitMode(firstDeck, secondDeck, orientation);
}

void DeckController::enterSplitMode(int firstDeck, int secondDeck, SplitOrientation orientation) {
    m_isSplitMode = true;
    m_splitPrimaryIndex = firstDeck;
    m_splitSecondaryIndex = secondDeck;
    m_splitOrientation = orientation;

    xcb_window_t win1 = m_decks[firstDeck].windowId;
    xcb_window_t win2 = m_decks[secondDeck].windowId;

    QRect geom1 = splitPrimaryGeometry();
    QRect geom2 = splitSecondaryGeometry();

    // 1. Position and restore both windows
    if (win1 != XCB_WINDOW_NONE) {
        static_cast<void>(m_xcbEngine.purgeMaximizedState(win1));
        static_cast<void>(m_xcbEngine.restoreWindow(win1));
        static_cast<void>(m_xcbEngine.moveResizeWindow(win1, geom1.x(), geom1.y(), geom1.width(), geom1.height()));
    }

    if (win2 != XCB_WINDOW_NONE) {
        static_cast<void>(m_xcbEngine.purgeMaximizedState(win2));
        static_cast<void>(m_xcbEngine.restoreWindow(win2));
        static_cast<void>(m_xcbEngine.moveResizeWindow(win2, geom2.x(), geom2.y(), geom2.width(), geom2.height()));
    }

    // Activate both windows to bring to front
    if (win1 != XCB_WINDOW_NONE) {
        static_cast<void>(m_xcbEngine.activateWindow(win1));
    }
    if (win2 != XCB_WINDOW_NONE) {
        static_cast<void>(m_xcbEngine.activateWindow(win2));
    }

    // 2. Update presentation overlay layout
    if (m_overlay) {
        m_overlay->enterSplitLayout(firstDeck, secondDeck, orientation);
    }

    emit splitModeChanged(true);
}

void DeckController::exitSplitMode(int focusDeckIndex) {
    if (!m_isSplitMode) {
        return;
    }

    int oldPrimary = m_splitPrimaryIndex;
    int oldSecondary = m_splitSecondaryIndex;

    m_isSplitMode = false;
    m_splitPrimaryIndex = -1;
    m_splitSecondaryIndex = -1;

    int keepIndex = focusDeckIndex;
    if (keepIndex < 0 || keepIndex >= m_decks.size()) {
        keepIndex = (m_activeDeckIndex >= 0) ? m_activeDeckIndex : oldPrimary;
    }

    int minimizeIndex = (keepIndex == oldPrimary) ? oldSecondary : oldPrimary;

    // Minimize unselected split window
    if (minimizeIndex >= 0 && minimizeIndex < m_decks.size()) {
        xcb_window_t minWin = m_decks[minimizeIndex].windowId;
        if (minWin != XCB_WINDOW_NONE && minWin != m_decks[keepIndex].windowId) {
            static_cast<void>(m_xcbEngine.minimizeWindow(minWin));
        }
    }

    // Restore focused window to fullscreen
    m_activeDeckIndex = keepIndex;
    if (m_activeDeckIndex >= 0 && m_activeDeckIndex < m_decks.size()) {
        xcb_window_t keepWin = m_decks[m_activeDeckIndex].windowId;
        if (keepWin != XCB_WINDOW_NONE) {
            QRect scr = targetGeometry();
            static_cast<void>(m_xcbEngine.purgeMaximizedState(keepWin));
            static_cast<void>(m_xcbEngine.restoreWindow(keepWin));
            static_cast<void>(m_xcbEngine.moveResizeWindow(keepWin, scr.x(), scr.y(), scr.width(), scr.height()));
            static_cast<void>(m_xcbEngine.activateWindow(keepWin));
        }
    }

    if (m_overlay) {
        m_overlay->exitSplitLayout(m_activeDeckIndex);
    }

    emit splitModeChanged(false);
    emit deckSwitched(m_activeDeckIndex);
}

void DeckController::onGutterClicked(int index) {
    if (index < 0 || index >= m_decks.size()) {
        return;
    }

    if (m_isSplitMode) {
        if (index == m_splitPrimaryIndex || index == m_splitSecondaryIndex) {
            exitSplitMode(index);
            return;
        }
        exitSplitMode(m_splitPrimaryIndex);
    }

    if (index == m_activeDeckIndex) {
        qDebug() << "Gutter" << index << "is already active. No-op.";
        return;
    }

    if (!m_stateMachine.tryTransition(AppState::IDLE, AppState::SWITCHING_OUT)) {
        qDebug() << "Cannot switch deck: StateMachine rejected transition from"
                 << static_cast<int>(m_stateMachine.currentState());
        return;
    }

    // Immediately make entire canvas solid to block accidental mouse clicks
    if (m_overlay) {
        m_overlay->updateMask(AppState::SWITCHING_OUT);
    }

    performSwitch(index);
}

QRect DeckController::targetGeometry() const {
    if (m_overlay) {
        return m_overlay->geometry();
    }
    const auto& s = m_config.getSettings();
    if (s.screenWidth > 0 && s.screenHeight > 0) {
        return QRect(0, 0, s.screenWidth, s.screenHeight);
    }
    return m_xcbEngine.getScreenGeometry();
}

void DeckController::performSwitch(int targetDeck) {
    if (m_overlay && m_overlay->curtain()) {
        m_slideFromRight = (m_activeDeckIndex != -1 && targetDeck > m_activeDeckIndex);

        QRect scr = targetGeometry();
        int w = scr.width();
        int h = scr.height();
        QRect offscreenLeft(-w, 0, w, h);
        QRect offscreenRight(w, 0, w, h);
        QRect onscreen(0, 0, w, h);

        QRect from = m_slideFromRight ? offscreenRight : offscreenLeft;

        int duration = m_config.getSettings().animationDurationMs;

        if (targetDeck >= 0 && targetDeck < m_decks.size()) {
            m_overlay->curtain()->setColor(m_decks[targetDeck].color);
        }

        // Use Qt::SingleShotConnection to completely eliminate connection accumulation leaks
        connect(m_overlay->curtain(), &CurtainWidget::slideComplete, this,
                [this, targetDeck]() {
                    onCurtainPhase1Complete(targetDeck);
                }, Qt::SingleShotConnection);

        m_overlay->curtain()->slideIn(from, onscreen, duration);
    } else {
        // Fallback if overlay or curtain is not present
        onCurtainPhase1Complete(targetDeck);
        onSwitchComplete();
    }
}

void DeckController::onCurtainPhase1Complete(int targetDeck) {
    // 1. Hide/minimize the outgoing active deck
    if (m_activeDeckIndex >= 0 && m_activeDeckIndex < m_decks.size()) {
        xcb_window_t oldWin = m_decks[m_activeDeckIndex].windowId;
        if (oldWin != XCB_WINDOW_NONE) {
            static_cast<void>(m_xcbEngine.minimizeWindow(oldWin));
        }
    }

    // 2. Restore, maximize and activate the incoming target deck
    m_activeDeckIndex = targetDeck;
    if (m_activeDeckIndex >= 0 && m_activeDeckIndex < m_decks.size()) {
        xcb_window_t newWin = m_decks[m_activeDeckIndex].windowId;
        if (newWin != XCB_WINDOW_NONE) {
            static_cast<void>(m_xcbEngine.purgeMaximizedState(newWin));
            QRect scr = targetGeometry();
            static_cast<void>(m_xcbEngine.restoreWindow(newWin));
            static_cast<void>(m_xcbEngine.moveResizeWindow(newWin, scr.x(), scr.y(), scr.width(), scr.height()));
            static_cast<void>(m_xcbEngine.activateWindow(newWin));
        }
    }

    // 3. Update presentation active indicators and accordion layout
    if (m_overlay) {
        m_overlay->setActiveGutter(m_activeDeckIndex);
    }

    // 4. Transition to Phase 2 (SWITCHING_IN)
    static_cast<void>(m_stateMachine.tryTransition(AppState::SWITCHING_OUT, AppState::SWITCHING_IN));

    // 5. Sweep curtain out to reveal new deck
    if (m_overlay && m_overlay->curtain()) {
        QRect scr = targetGeometry();
        int w = scr.width();
        int h = scr.height();
        QRect onscreen(0, 0, w, h);
        QRect offscreenLeft(-w, 0, w, h);
        QRect offscreenRight(w, 0, w, h);

        QRect to = m_slideFromRight ? offscreenLeft : offscreenRight;

        int duration = m_config.getSettings().animationDurationMs;

        connect(m_overlay->curtain(), &CurtainWidget::slideComplete, this,
                [this]() {
                    onSwitchComplete();
                }, Qt::SingleShotConnection);

        m_overlay->curtain()->slideOut(onscreen, to, duration);
    } else {
        onSwitchComplete();
    }
}

void DeckController::onSwitchComplete() {
    static_cast<void>(m_stateMachine.tryTransition(AppState::SWITCHING_IN, AppState::IDLE));

    // Re-assert target geometry on completion to lock placement
    if (m_isSplitMode) {
        if (m_splitPrimaryIndex >= 0 && m_splitPrimaryIndex < m_decks.size()) {
            xcb_window_t w1 = m_decks[m_splitPrimaryIndex].windowId;
            if (w1 != XCB_WINDOW_NONE) {
                QRect g1 = splitPrimaryGeometry();
                static_cast<void>(m_xcbEngine.moveResizeWindow(w1, g1.x(), g1.y(), g1.width(), g1.height()));
            }
        }
        if (m_splitSecondaryIndex >= 0 && m_splitSecondaryIndex < m_decks.size()) {
            xcb_window_t w2 = m_decks[m_splitSecondaryIndex].windowId;
            if (w2 != XCB_WINDOW_NONE) {
                QRect g2 = splitSecondaryGeometry();
                static_cast<void>(m_xcbEngine.moveResizeWindow(w2, g2.x(), g2.y(), g2.width(), g2.height()));
            }
        }
    } else if (m_activeDeckIndex >= 0 && m_activeDeckIndex < m_decks.size()) {
        xcb_window_t activeWin = m_decks[m_activeDeckIndex].windowId;
        if (activeWin != XCB_WINDOW_NONE) {
            QRect scr = targetGeometry();
            static_cast<void>(m_xcbEngine.moveResizeWindow(activeWin, scr.x(), scr.y(), scr.width(), scr.height()));
        }
    }

    if (m_overlay) {
        m_overlay->updateMask(AppState::IDLE);
    }

    emit deckSwitched(m_activeDeckIndex);
}

void DeckController::onDeckLaunched(int index, qint64 pid, const QString& command) {
    Q_UNUSED(command);
    if (index >= 0 && index < m_decks.size()) {
        m_decks[index].pid = pid;
        m_lastLaunchedDeckIndex = index;
        m_lastLaunchedTimestampMs = QDateTime::currentMSecsSinceEpoch();
    }
}

void DeckController::onWindowMapped(uint32_t wid, uint32_t pid, const QString& title) {
    // 0. Safety filters: ignore GutterDeck's own windows, already-tracked windows, and foreign workspaces
    if (wid == XCB_WINDOW_NONE) {
        return;
    }
    if (m_overlay && wid == static_cast<xcb_window_t>(m_overlay->winId())) {
        return;
    }
    if (pid > 0 && pid == static_cast<uint32_t>(QCoreApplication::applicationPid())) {
        return;
    }
    for (const auto& deck : m_decks) {
        if (deck.windowId == wid) {
            return;
        }
    }

    // Ignore windows mapped on a different specific workspace (0xFFFFFFFF = all workspaces)
    uint32_t winDesktop = m_xcbEngine.getWindowDesktop(wid);
    if (winDesktop != 0xFFFFFFFF && winDesktop != m_assignedDesktop) {
        qDebug() << "Ignoring window" << wid << "on foreign desktop" << winDesktop
                 << "(Assigned deck desktop:" << m_assignedDesktop << ")";
        return;
    }

    QString wmClass = m_xcbEngine.getWindowClass(wid);
    qDebug() << "Window mapped: WID =" << wid << "PID =" << pid << "Title =" << title << "WM_CLASS =" << wmClass;

    int matchedIndex = -1;

    // 1. Primary matching algorithm: Match by direct OS Process ID or process tree ancestry
    if (pid > 0) {
        for (int i = 0; i < m_decks.size(); ++i) {
            if (m_decks[i].windowId == XCB_WINDOW_NONE && m_decks[i].pid > 0) {
                if (m_decks[i].pid == pid || isProcessDescendant(pid, static_cast<uint32_t>(m_decks[i].pid))) {
                    matchedIndex = i;
                    break;
                }
            }
        }
    }

    // 2. Secondary matching: WM_CLASS against configured deck command
    if (matchedIndex == -1 && !wmClass.isEmpty()) {
        QString wmLower = wmClass.toLower();
        for (int i = 0; i < m_decks.size(); ++i) {
            if (m_decks[i].windowId != XCB_WINDOW_NONE) {
                continue;
            }
            QString cmd = m_decks[i].command.trimmed().split(' ').first();
            int slashIdx = cmd.lastIndexOf('/');
            if (slashIdx >= 0) {
                cmd = cmd.mid(slashIdx + 1);
            }
            cmd = cmd.toLower();

            if (!cmd.isEmpty() && (wmLower.contains(cmd) || cmd.contains(wmLower))) {
                matchedIndex = i;
                break;
            }

            QStringList tokens = cmd.split(QRegularExpression("[\\-_]"), Qt::SkipEmptyParts);
            for (const QString& t : tokens) {
                if (t.length() >= 4 && wmLower.contains(t)) {
                    matchedIndex = i;
                    break;
                }
            }
            if (matchedIndex != -1) {
                break;
            }
        }
    }

    // 3. Process cmdline matching: Verify if /proc/<pid>/cmdline matches configured deck command
    if (matchedIndex == -1 && pid > 0) {
        for (int i = 0; i < m_decks.size(); ++i) {
            if (m_decks[i].windowId == XCB_WINDOW_NONE && matchesProcessCommand(pid, m_decks[i].command)) {
                matchedIndex = i;
                break;
            }
        }
    }

    // 4. Pending launch correlation: If a window mapped within 3s of launching a slot awaiting window
    // and there is plausible relation (class, title, or process matches command binary)
    if (matchedIndex == -1 && m_lastLaunchedDeckIndex >= 0 && m_lastLaunchedDeckIndex < m_decks.size()) {
        qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_lastLaunchedTimestampMs;
        if (elapsed < 3000 && m_decks[m_lastLaunchedDeckIndex].windowId == XCB_WINDOW_NONE) {
            QString cmd = m_decks[m_lastLaunchedDeckIndex].command.trimmed().split(' ').first();
            int slashIdx = cmd.lastIndexOf('/');
            if (slashIdx >= 0) {
                cmd = cmd.mid(slashIdx + 1);
            }
            cmd = cmd.toLower();

            bool correlated = wmClass.isEmpty() ||
                              (!cmd.isEmpty() && (wmClass.toLower().contains(cmd) || title.toLower().contains(cmd))) ||
                              (pid > 0 && matchesProcessCommand(pid, m_decks[m_lastLaunchedDeckIndex].command));
            if (correlated) {
                matchedIndex = m_lastLaunchedDeckIndex;
            }
        }
    }

    // Note: Fallback 5 removed completely. Blindly capturing arbitrary windows causes
    // unrelated user applications to be hijacked into GutterDeck.

    if (matchedIndex != -1) {
        qDebug() << "Attaching Window ID" << wid << "to Deck ["
                 << m_decks[matchedIndex].name << "] (Index:" << matchedIndex << ")";

        m_decks[matchedIndex].windowId = wid;
        m_decks[matchedIndex].isMapped = true;
        if (pid > 0) {
            m_decks[matchedIndex].pid = pid;
        }

        QImage appIcon = AppIcon::createAppIcon().pixmap(64, 64).toImage();
        static_cast<void>(m_xcbEngine.overrideWindowIconAndClass(wid, QString("gutterdeck"), appIcon));

        static_cast<void>(m_xcbEngine.purgeMaximizedState(wid));
        static_cast<void>(m_xcbEngine.setSkipTaskbar(wid, true));
        static_cast<void>(m_xcbEngine.setWindowDesktop(wid, m_assignedDesktop));

        QRect scr = targetGeometry();

        // If no active deck exists yet or this window belongs to the active deck, activate and position it
        if (m_activeDeckIndex == -1 || matchedIndex == m_activeDeckIndex) {
            m_activeDeckIndex = matchedIndex;
            static_cast<void>(m_xcbEngine.restoreWindow(wid));
            static_cast<void>(m_xcbEngine.moveResizeWindow(wid, scr.x(), scr.y(), scr.width(), scr.height()));
            static_cast<void>(m_xcbEngine.activateWindow(wid));

            if (m_overlay) {
                m_overlay->setActiveGutter(m_activeDeckIndex);
            }
        } else if (matchedIndex != m_activeDeckIndex) {
            // Background decks should stay minimized/hidden
            static_cast<void>(m_xcbEngine.minimizeWindow(wid));
        }
    }
}

void DeckController::onWindowDestroyed(uint32_t wid) {
    for (int i = 0; i < m_decks.size(); ++i) {
        if (m_decks[i].windowId == wid) {
            qDebug() << "Attached window for Deck [" << m_decks[i].name << "] was destroyed.";
            m_decks[i].windowId = XCB_WINDOW_NONE;
            m_decks[i].isMapped = false;
            if (m_isSplitMode && (i == m_splitPrimaryIndex || i == m_splitSecondaryIndex)) {
                int survivingIndex = (i == m_splitPrimaryIndex) ? m_splitSecondaryIndex : m_splitPrimaryIndex;
                exitSplitMode(survivingIndex);
            }
            emit deckClosed(i);
            break;
        }
    }
}

void DeckController::onCurrentDesktopChanged(uint32_t currentDesktop) {
    qDebug() << "Desktop switched to:" << currentDesktop
             << "Assigned deck desktop:" << m_assignedDesktop;

    if (!m_overlay) {
        return;
    }

    if (currentDesktop == m_assignedDesktop) {
        static_cast<void>(m_xcbEngine.grabAltLeftRightKeys());
        static_cast<void>(m_xcbEngine.grabCtrlShiftScroll());
        m_overlay->show();
        m_overlay->updateMask(m_stateMachine.currentState());
    } else {
        static_cast<void>(m_xcbEngine.ungrabAltLeftRightKeys());
        static_cast<void>(m_xcbEngine.ungrabCtrlShiftScroll());
        m_overlay->hide();
    }
}

void DeckController::switchToPreviousDeck(bool wrap) {
    if (m_decks.isEmpty()) {
        return;
    }
    int total = m_decks.size();
    int current = m_isSplitMode ? m_splitPrimaryIndex : ((m_activeDeckIndex >= 0) ? m_activeDeckIndex : 0);
    int target = current - 1;
    if (target < 0) {
        if (!wrap) {
            if (m_isSplitMode) {
                exitSplitMode(m_splitPrimaryIndex);
            }
            return;
        }
        target = total - 1;
    }
    onGutterClicked(target);
}

void DeckController::switchToNextDeck(bool wrap) {
    if (m_decks.isEmpty()) {
        return;
    }
    int total = m_decks.size();
    int current = m_isSplitMode ? m_splitSecondaryIndex : ((m_activeDeckIndex >= 0) ? m_activeDeckIndex : 0);
    int target = current + 1;
    if (target >= total) {
        if (!wrap) {
            if (m_isSplitMode) {
                exitSplitMode(m_splitSecondaryIndex);
            }
            return;
        }
        target = 0;
    }
    onGutterClicked(target);
}

void DeckController::onGutterContextMenuRequested(int index, const QPoint& globalPos) {
    if (index < 0 || index >= m_decks.size()) {
        return;
    }

    SleekContextMenu menu;
    auto* editNameAct = menu.addAction(getSleekMenuIcon(SleekMenuIcon::EditName), QStringLiteral("Edit Name..."));
    auto* editCmdAct = menu.addAction(getSleekMenuIcon(SleekMenuIcon::EditCommand), QStringLiteral("Edit Command..."));
    auto* changeColorAct = menu.addAction(getSleekMenuIcon(SleekMenuIcon::ChangeColor), QStringLiteral("Change Color && Opacity..."));
    auto* addDeckAct = menu.addAction(getSleekMenuIcon(SleekMenuIcon::AddDeck), QStringLiteral("Add New Deck..."));
    auto* reorderDecksAct = menu.addAction(getSleekMenuIcon(SleekMenuIcon::ReorderDecks), QStringLiteral("Reorder Decks..."));

    QAction* splitVertAct = nullptr;
    QAction* splitHorzAct = nullptr;
    QAction* exitSplitAct = nullptr;

    if (m_isSplitMode) {
        menu.addSeparator();
        exitSplitAct = menu.addAction(getSleekMenuIcon(SleekMenuIcon::ExitSplit), QStringLiteral("Exit Split View"));
    } else if (m_activeDeckIndex >= 0 && index != m_activeDeckIndex && std::abs(index - m_activeDeckIndex) == 1) {
        if (m_decks[index].windowId != XCB_WINDOW_NONE && m_decks[m_activeDeckIndex].windowId != XCB_WINDOW_NONE) {
            menu.addSeparator();
            splitVertAct = menu.addAction(getSleekMenuIcon(SleekMenuIcon::SplitVertical),
                                         QStringLiteral("Split Vertically (50/50 Side-by-Side)"));
            splitHorzAct = menu.addAction(getSleekMenuIcon(SleekMenuIcon::SplitHorizontal),
                                         QStringLiteral("Split Horizontally (50/50 Top/Bottom)"));
        }
    }

    menu.addSeparator();
    auto* deleteDeckAct = menu.addAction(getSleekMenuIcon(SleekMenuIcon::DeleteDeck), QStringLiteral("Delete Deck"));

    if (m_decks.size() <= 1) {
        deleteDeckAct->setEnabled(false);
        deleteDeckAct->setText(QStringLiteral("Delete Deck (Last deck)"));
    }

    menu.addSeparator();
    auto* quitAct = menu.addAction(getSleekMenuIcon(SleekMenuIcon::CloseApp), QStringLiteral("Close Gutter Deck"));

    if (m_overlay) m_overlay->setHoverLock(true);
    QAction* selected = menu.exec(globalPos);
    if (m_overlay) m_overlay->setHoverLock(false);
    if (!selected) {
        return;
    }

    if (selected == editNameAct) {
        onEditDeckName(index);
    } else if (selected == editCmdAct) {
        onEditDeckCommand(index);
    } else if (selected == changeColorAct) {
        onChangeDeckColor(index);
    } else if (selected == addDeckAct) {
        onAddNewDeck(index);
    } else if (selected == reorderDecksAct) {
        onReorderDecks();
    } else if (selected == splitVertAct) {
        onSplitRequested(index, SplitOrientation::Vertical);
    } else if (selected == splitHorzAct) {
        onSplitRequested(index, SplitOrientation::Horizontal);
    } else if (selected == exitSplitAct) {
        exitSplitMode(index);
    } else if (selected == deleteDeckAct) {
        onCloseDeck(index);
    } else if (selected == quitAct) {
        closeGutterDeck();
    }
}

void DeckController::onEditDeckName(int index, const QPoint& customCenter) {
    if (index < 0 || index >= m_decks.size()) {
        return;
    }

    SleekInputDialog dlg(
        QStringLiteral("Edit Deck Name"),
        QStringLiteral("Enter a label for this deck tab:"),
        m_decks[index].name,
        m_overlay
    );
    dlg.adjustSize();
    if (m_overlay) {
        QPoint center = customCenter.isNull() ? m_overlay->geometry().center() : customCenter; dlg.move(center - dlg.rect().center());
    }

    if (dlg.exec() == QDialog::Accepted) {
        QString newName = dlg.value().trimmed();
        if (!newName.isEmpty() && newName != m_decks[index].name) {
            m_decks[index].name = newName;
            m_config.updateDeck(index, newName, m_decks[index].command, m_decks[index].color);
            if (m_overlay) {
                m_overlay->updateGutterVisuals(index, newName, m_decks[index].color);
            }
        }
    }
}

void DeckController::onEditDeckCommand(int index, const QPoint& customCenter) {
    if (index < 0 || index >= m_decks.size()) {
        return;
    }

    SleekInputDialog dlg(
        QStringLiteral("Edit Launch Command"),
        QStringLiteral("Enter command or application path to execute:"),
        m_decks[index].command,
        m_overlay
    );
    dlg.adjustSize();
    if (m_overlay) {
        QPoint center = customCenter.isNull() ? m_overlay->geometry().center() : customCenter; dlg.move(center - dlg.rect().center());
    }

    if (dlg.exec() == QDialog::Accepted) {
        QString newCmd = dlg.value().trimmed();
        if (!newCmd.isEmpty() && newCmd != m_decks[index].command) {
            m_decks[index].command = newCmd;
            m_config.updateDeck(index, m_decks[index].name, newCmd, m_decks[index].color);
            if (!m_decks[index].isMapped && m_decks[index].pid <= 0) {
                m_appLauncher.launchDeck(index);
            }
        }
    }
}

void DeckController::onChangeDeckColor(int index, const QPoint& customCenter) {
    if (index < 0 || index >= m_decks.size()) {
        return;
    }

    SleekColorDialog dlg(m_decks[index].color, m_overlay);
    dlg.adjustSize();
    if (m_overlay) {
        QPoint center = customCenter.isNull() ? m_overlay->geometry().center() : customCenter; dlg.move(center - dlg.rect().center());
    }

    if (dlg.exec() == QDialog::Accepted) {
        QColor newColor = dlg.selectedColor();
        if (newColor.isValid() && newColor != m_decks[index].color) {
            m_decks[index].color = newColor;
            m_config.updateDeck(index, m_decks[index].name, m_decks[index].command, newColor);
            if (m_overlay) {
                m_overlay->updateGutterVisuals(index, m_decks[index].name, newColor);
            }
        }
    }
}

void DeckController::onAddNewDeck(int relativeToIndex, const QPoint& customCenter) {
    QString relName;
    if (relativeToIndex >= 0 && relativeToIndex < m_decks.size()) {
        relName = m_decks[relativeToIndex].name;
    }

    SleekAddDeckDialog dlg(relName, m_overlay);
    dlg.adjustSize();
    if (m_overlay) {
        QPoint center = customCenter.isNull() ? m_overlay->geometry().center() : customCenter; dlg.move(center - dlg.rect().center());
    }

    if (dlg.exec() == QDialog::Accepted) {
        QString name = dlg.deckName().trimmed();
        QString cmd = dlg.deckCommand().trimmed();
        QColor color = dlg.deckColor();

        if (name.isEmpty()) {
            name = QStringLiteral("Deck %1").arg(m_decks.size() + 1);
        }
        if (cmd.isEmpty()) {
            cmd = QStringLiteral("x-terminal-emulator");
        }

        int insertIndex = m_decks.size();
        if (relativeToIndex >= 0 && relativeToIndex < m_decks.size()) {
            insertIndex = dlg.insertLeft() ? relativeToIndex : (relativeToIndex + 1);
        }

        QString id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);

        DeckConfig newCfg;
        newCfg.id = id;
        newCfg.name = name;
        newCfg.command = cmd;
        newCfg.color = color;

        if (m_config.insertDeck(insertIndex, newCfg)) {
            DeckSlot slot;
            slot.id = id;
            slot.name = name;
            slot.command = cmd;
            slot.color = color;
            slot.pid = 0;
            slot.windowId = XCB_WINDOW_NONE;
            slot.isMapped = false;
            m_decks.insert(insertIndex, slot);
            m_appLauncher.insertDeck(insertIndex);

            if (m_activeDeckIndex >= 0 && m_activeDeckIndex >= insertIndex) {
                m_activeDeckIndex++;
            }

            if (m_overlay) {
                m_overlay->rebuildGutters();
                m_overlay->setActiveGutter(m_activeDeckIndex);
            }

            m_appLauncher.launchDeck(insertIndex);
        }
    }
}

void DeckController::onReorderDecks() {
    if (m_decks.size() <= 1) {
        return;
    }

    QVector<DeckItemInfo> items;
    items.reserve(m_decks.size());
    for (int i = 0; i < m_decks.size(); ++i) {
        items.append(DeckItemInfo{i, m_decks[i].name, m_decks[i].command, m_decks[i].color});
    }

    SleekReorderDialog dlg(items, m_activeDeckIndex, m_overlay);
    dlg.adjustSize();
    if (m_overlay) {
        dlg.move(m_overlay->geometry().center() - dlg.rect().center());
    }

    if (dlg.exec() == QDialog::Accepted) {
        QVector<int> newOrder = dlg.newOrder();
        if (newOrder.size() != m_decks.size()) {
            return;
        }

        m_config.reorderDecks(newOrder);
        m_appLauncher.reorderDecks(newOrder);

        QVector<DeckSlot> reorderedSlots;
        reorderedSlots.reserve(newOrder.size());
        for (int oldIdx : newOrder) {
            reorderedSlots.append(m_decks[oldIdx]);
        }
        m_decks = std::move(reorderedSlots);

        if (m_activeDeckIndex >= 0) {
            int newActive = newOrder.indexOf(m_activeDeckIndex);
            m_activeDeckIndex = (newActive >= 0) ? newActive : 0;
        }

        if (m_overlay) {
            m_overlay->rebuildGutters();
            m_overlay->setActiveGutter(m_activeDeckIndex);
        }
    }
}

void DeckController::onCloseDeck(int index) {
    if (index < 0 || index >= m_decks.size() || m_decks.size() <= 1) {
        return;
    }

    if (m_isSplitMode && (index == m_splitPrimaryIndex || index == m_splitSecondaryIndex)) {
        int survivingIndex = (index == m_splitPrimaryIndex) ? m_splitSecondaryIndex : m_splitPrimaryIndex;
        exitSplitMode(survivingIndex);
    }

    xcb_window_t winId = m_decks[index].windowId;

    if (winId != XCB_WINDOW_NONE) {
        static_cast<void>(m_xcbEngine.restoreWindow(winId));
        static_cast<void>(m_xcbEngine.closeWindow(winId));
    }
    // Note: Do NOT kill(pid, SIGTERM) here. Process IDs may be shared across
    // multiple windows (e.g. Google Chrome, GNOME Terminal server, VS Code).
    // Standard graceful close requests via closeWindow() cleanly notify the specific
    // window to close without terminating other instances outside of GutterDeck.

    int oldActive = m_activeDeckIndex;
    int nextActive = oldActive;

    if (oldActive == index) {
        if (index > 0) {
            nextActive = index - 1;
        } else {
            nextActive = 0;
        }
    } else if (oldActive > index) {
        nextActive = oldActive - 1;
    }

    m_decks.removeAt(index);
    m_appLauncher.removeDeck(index);
    m_config.removeDeck(index);

    emit deckClosed(index);

    if (m_overlay) {
        m_overlay->rebuildGutters();
    }

    if (oldActive == index) {
        m_activeDeckIndex = -1;
        if (nextActive >= 0 && nextActive < m_decks.size()) {
            onGutterClicked(nextActive);
        }
    } else {
        m_activeDeckIndex = nextActive;
        if (m_overlay) {
            m_overlay->setActiveGutter(m_activeDeckIndex);
        }
    }
}

void DeckController::closeGutterDeck() {
    qDebug() << "Closing Gutter Deck: gracefully closing managed deck windows...";

    static_cast<void>(m_xcbEngine.ungrabAltLeftRightKeys());
        static_cast<void>(m_xcbEngine.ungrabCtrlShiftScroll());
    static_cast<void>(m_xcbEngine.ungrabCtrlShiftScroll());

    // 1. Hide overlay window immediately so the desktop is instantly responsive
    if (m_overlay) {
        m_overlay->hide();
    }

    // 2. Restore minimized windows and send graceful close requests to managed windows
    QVector<xcb_window_t> pendingWindows;

    for (const auto& deck : m_decks) {
        if (deck.windowId != XCB_WINDOW_NONE) {
            static_cast<void>(m_xcbEngine.restoreWindow(deck.windowId));
            static_cast<void>(m_xcbEngine.closeWindow(deck.windowId));
            pendingWindows.append(deck.windowId);
        }
    }

    // 3. Graceful wait loop: allow applications up to 1500ms to save sessions and exit cleanly
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < 1500 && !pendingWindows.isEmpty()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

        auto liveWindows = m_xcbEngine.getTopLevelWindows();
        for (auto it = pendingWindows.begin(); it != pendingWindows.end();) {
            if (!liveWindows.contains(*it)) {
                it = pendingWindows.erase(it);
            } else {
                ++it;
            }
        }

        if (pendingWindows.isEmpty()) {
            qDebug() << "All deck windows closed cleanly in" << timer.elapsed() << "ms.";
            break;
        }

        QThread::msleep(50);
    }

    // 4. Safe recovery: If any window remained open after the grace period (e.g. user cancelled
    //    an unsaved document prompt), do NOT call killClient or SIGTERM. That would destroy
    //    unsaved user data or terminate shared master processes (such as browser windows
    //    running outside GutterDeck). Instead, restore taskbar visibility so the user can interact.
    auto remainingWindows = m_xcbEngine.getTopLevelWindows();
    for (xcb_window_t wid : pendingWindows) {
        if (remainingWindows.contains(wid)) {
            qWarning() << "Window" << wid << "remained open; restoring taskbar visibility.";
            static_cast<void>(m_xcbEngine.setSkipTaskbar(wid, false));
            static_cast<void>(m_xcbEngine.restoreWindow(wid));
        }
    }

    QApplication::quit();
}
