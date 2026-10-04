#include "WorkAreaTracker.h"
#include "WindowWatcher.h"
#include "WorkAreaCalculator.h"
#include "XcbEngine.h"

#include <QDebug>
#include <QGuiApplication>
#include <QScreen>

WorkAreaTracker::WorkAreaTracker(XcbEngine& engine,
                                 WindowWatcher& watcher,
                                 QScreen* targetScreen,
                                 uint32_t desktop,
                                 QObject* parent)
    : QObject(parent),
      m_engine(engine),
      m_watcher(watcher),
      m_desktop(desktop) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(kDebounceMs);
    connect(&m_debounce, &QTimer::timeout, this, &WorkAreaTracker::recompute);

    bindScreen(targetScreen ? targetScreen : QGuiApplication::primaryScreen());
}

void WorkAreaTracker::start() {
    if (m_started) {
        return;
    }
    m_started = true;

    // X11 invalidation sources. Client churn is included because a panel appearing/dying
    // changes the strut set even on WMs that do not maintain _NET_WORKAREA.
    connect(&m_watcher, &WindowWatcher::workAreaInvalidated,
            this, &WorkAreaTracker::scheduleRecompute);
    connect(&m_watcher, &WindowWatcher::windowMapped,
            this, [this](uint32_t, uint32_t, const QString&) { scheduleRecompute(); });
    connect(&m_watcher, &WindowWatcher::windowDestroyed,
            this, [this](uint32_t) { scheduleRecompute(); });

    // RandR sources. Monitor count changes also flip the _NET_WORKAREA intersection policy.
    if (auto* app = qGuiApp) {
        connect(app, &QGuiApplication::screenAdded,
                this, [this](QScreen*) { scheduleRecompute(); });
        connect(app, &QGuiApplication::screenRemoved,
                this, &WorkAreaTracker::onScreenRemoved);
    }

    // Synchronous first pass so the overlay is correct before its first show().
    recompute();
}

QRect WorkAreaTracker::currentWorkArea() const noexcept {
    return m_workArea;
}

void WorkAreaTracker::scheduleRecompute() {
    m_debounce.start();
}

void WorkAreaTracker::bindScreen(QScreen* screen) {
    if (m_screenGeometryConnection) {
        disconnect(m_screenGeometryConnection);
        m_screenGeometryConnection = {};
    }
    m_screen = screen;
    if (m_screen) {
        m_screenGeometryConnection = connect(m_screen.data(), &QScreen::geometryChanged,
                                             this, [this](const QRect&) { scheduleRecompute(); });
    }
}

void WorkAreaTracker::onScreenRemoved(QScreen* screen) {
    if (screen && screen == m_screen.data()) {
        QScreen* fallback = QGuiApplication::primaryScreen();
        if (fallback == screen) {
            fallback = nullptr;
            for (QScreen* s : QGuiApplication::screens()) {
                if (s != screen) {
                    fallback = s;
                    break;
                }
            }
        }
        qWarning() << "WorkAreaTracker: target screen" << screen->name()
                   << "removed; re-binding to" << (fallback ? fallback->name() : QStringLiteral("<none>"));
        bindScreen(fallback);
    }
    scheduleRecompute();
}

void WorkAreaTracker::recompute() {
    if (!m_screen) {
        bindScreen(QGuiApplication::primaryScreen());
        if (!m_screen) {
            qWarning() << "WorkAreaTracker: no screen available; work area unchanged.";
            return;
        }
    }

    if (!m_dprWarningIssued && !qFuzzyCompare(m_screen->devicePixelRatio(), 1.0)) {
        m_dprWarningIssued = true;
        qWarning() << "WorkAreaTracker: device pixel ratio" << m_screen->devicePixelRatio()
                   << "on" << m_screen->name()
                   << "- X11 strut pixels and Qt logical pixels will diverge (HiDPI unsupported).";
    }

    const QRect monitor = m_screen->geometry();
    const QSize root = m_engine.getRootSize();
    const QVector<StrutReservation> struts = m_engine.getStrutReservations(m_desktop);

    // Approved policy: _NET_WORKAREA is authoritative only on single-head setups.
    const bool singleHead = QGuiApplication::screens().size() == 1;
    const std::optional<QRect> netWorkArea =
        singleHead ? m_engine.getNetWorkArea(m_desktop) : std::nullopt;

    const QRect workArea = WorkAreaCalculator::compute(monitor, root, struts, netWorkArea);

    qDebug() << "WorkAreaTracker: monitor" << m_screen->name() << monitor
             << "root" << root
             << "struts" << struts.size()
             << "singleHead" << singleHead
             << "_NET_WORKAREA" << (netWorkArea ? *netWorkArea : QRect())
             << "=> work area" << workArea;

    if (workArea != m_workArea) {
        m_workArea = workArea;
        emit workAreaChanged(m_workArea);
    }
}
