#pragma once

#include <QObject>
#include <QSocketNotifier>
#include <QString>
#include <memory>
#include <unordered_set>
#include <xcb/xcb.h>

class XcbConnection;
class XcbEngine;

/**
 * @brief Event-driven watcher for native X11 window mapping and destruction.
 * @details Subscribes to XCB root window property changes and substructure events.
 *          Integrates non-blockingly with the Qt event loop via QSocketNotifier.
 *          Emits signals when top-level client windows appear or close, extracting
 *          PID and Title information for PID-based application docking.
 * @note Replaces timer-based window polling, eliminating CPU churn and latency.
 */
class WindowWatcher : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Constructs the watcher with required XCB infrastructure.
     * @param conn Reference to active XcbConnection.
     * @param engine Reference to active XcbEngine.
     * @param parent Optional parent QObject for Qt hierarchy.
     */
    explicit WindowWatcher(XcbConnection& conn, XcbEngine& engine, QObject* parent = nullptr);
    ~WindowWatcher() override;

    WindowWatcher(const WindowWatcher&) = delete;
    WindowWatcher& operator=(const WindowWatcher&) = delete;
    WindowWatcher(WindowWatcher&&) = delete;
    WindowWatcher& operator=(WindowWatcher&&) = delete;

    /**
     * @brief Starts listening for X11 root window events.
     */
    void startWatching();

    /**
     * @brief Stops listening and disables the socket notifier.
     */
    void stopWatching();

    /**
     * @brief Checks if event watching is currently active.
     * @return True if actively monitoring events.
     */
    [[nodiscard]] bool isWatching() const noexcept;

signals:
    /**
     * @brief Emitted when a new top-level client window is mapped.
     * @param wid The native X11 window ID.
     * @param pid Process ID associated with the window (_NET_WM_PID).
     * @param title Window title text.
     */
    void windowMapped(uint32_t wid, uint32_t pid, const QString& title);

    /**
     * @brief Emitted when a monitored window is unmapped or destroyed.
     * @param wid The native X11 window ID.
     */
    void windowDestroyed(uint32_t wid);

    /**
     * @brief Emitted when the active X11 virtual desktop changes (_NET_CURRENT_DESKTOP).
     * @param currentDesktop Zero-based index of the newly active desktop workspace.
     */
    void currentDesktopChanged(uint32_t currentDesktop);

    /**
     * @brief Emitted when global Alt+Left is triggered.
     */
    void previousDeckRequested();

    /**
     * @brief Emitted when global Alt+Right is triggered.
     */
    void nextDeckRequested();

private slots:
    void processXcbEvents();

private:
    XcbConnection& m_conn;
    XcbEngine& m_engine;
    std::unique_ptr<QSocketNotifier> m_notifier;
    std::unordered_set<uint32_t> m_knownWindows;
    xcb_atom_t m_netClientListAtom = XCB_NONE;
    xcb_atom_t m_netCurrentDesktopAtom = XCB_NONE;
    bool m_isWatching = false;

    void refreshClientList();
};
