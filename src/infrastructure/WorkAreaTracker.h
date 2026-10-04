#pragma once

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QTimer>
#include <cstdint>

class QScreen;
class XcbEngine;
class WindowWatcher;

/**
 * @brief Tracks the usable (panel-free) work area of GutterDeck's target monitor.
 * @details Bridges three asynchronous change sources into one debounced, deduplicated signal:
 *          - X11: root _NET_WORKAREA rewrites (WindowWatcher::workAreaInvalidated) and managed
 *            client churn (windowMapped / windowDestroyed — a panel appearing or dying).
 *          - Qt/RandR: target QScreen geometry changes, screens being added or removed.
 *          On each (debounced) trigger it gathers the raw struts via XcbEngine and delegates the
 *          geometry math to the pure WorkAreaCalculator, applying the approved policy:
 *          _NET_WORKAREA is intersected ONLY when exactly one monitor is attached.
 *
 *          Why a separate class: DeckController already orchestrates deck switching; adding
 *          monitor/strut bookkeeping there would violate SRP. The tracker owns "where may we
 *          draw", the controller owns "what do we draw there".
 * @note Not thread-safe; lives on the GUI thread (QScreen and the XCB engine are GUI-thread
 *       objects). The target QScreen is held through QPointer, so a hot-unplugged monitor can
 *       never leave a dangling pointer — the tracker re-binds to the primary screen instead.
 *       Geometry is computed in X11 root pixels; a device pixel ratio other than 1.0 is
 *       logged as a warning because the rest of the codebase does not yet map logical pixels.
 */
class WorkAreaTracker : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Constructs the tracker with constructor-injected infrastructure.
     * @param engine XCB engine used to query struts, root size and _NET_WORKAREA.
     * @param watcher Event watcher providing X11 invalidation signals.
     * @param targetScreen Monitor GutterDeck is bound to; may be nullptr (falls back to primary).
     * @param desktop Virtual desktop the deck lives on; struts pinned to other desktops are ignored.
     * @param parent Optional Qt parent.
     */
    WorkAreaTracker(XcbEngine& engine,
                    WindowWatcher& watcher,
                    QScreen* targetScreen,
                    uint32_t desktop,
                    QObject* parent = nullptr);
    ~WorkAreaTracker() override = default;

    WorkAreaTracker(const WorkAreaTracker&) = delete;
    WorkAreaTracker& operator=(const WorkAreaTracker&) = delete;
    WorkAreaTracker(WorkAreaTracker&&) = delete;
    WorkAreaTracker& operator=(WorkAreaTracker&&) = delete;

    /**
     * @brief Wires all change sources and performs the first computation SYNCHRONOUSLY.
     * @details The synchronous first pass guarantees the overlay receives the correct geometry
     *          before it is first shown, avoiding a one-frame flash over the panel.
     *          Calling start() more than once is a no-op.
     */
    void start();

    /**
     * @brief Returns the most recently computed work area.
     * @return Work area in root coordinates, or an invalid QRect before start().
     */
    [[nodiscard]] QRect currentWorkArea() const noexcept;

signals:
    /**
     * @brief Emitted when the computed work area differs from the previous value.
     * @param workArea New usable area of the target monitor in root coordinates.
     */
    void workAreaChanged(const QRect& workArea);

private slots:
    /**
     * @brief Restarts the debounce timer; coalesces bursts (tint2 startup, RandR storms).
     */
    void scheduleRecompute();

    /**
     * @brief Gathers inputs, runs WorkAreaCalculator and emits on change.
     */
    void recompute();

    /**
     * @brief Re-binds to the primary screen if the target monitor was removed.
     * @param screen The screen being removed.
     */
    void onScreenRemoved(QScreen* screen);

private:
    /**
     * @brief Binds geometry-change tracking to @p screen, dropping any previous binding.
     * @param screen New target screen (may be nullptr).
     */
    void bindScreen(QScreen* screen);

    XcbEngine& m_engine;
    WindowWatcher& m_watcher;
    QPointer<QScreen> m_screen;
    QMetaObject::Connection m_screenGeometryConnection;
    uint32_t m_desktop = 0;
    QTimer m_debounce;
    QRect m_workArea;
    bool m_started = false;
    bool m_dprWarningIssued = false;

    static constexpr int kDebounceMs = 150;
};
