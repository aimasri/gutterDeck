#pragma once

#include <QRect>
#include <QString>
#include <QVector>
#include <memory>
#include <unordered_map>
#include <xcb/xcb.h>

class XcbConnection;

/**
 * @brief Free functor for XCB allocated reply buffers.
 */
struct XcbReplyDeleter {
    void operator()(void* ptr) const noexcept;
};

template <typename T>
using xcb_ptr = std::unique_ptr<T, XcbReplyDeleter>;

/**
 * @brief High-performance, bare-metal X11 window management engine using XCB.
 * @details Implements EWMH and ICCCM compliant window manipulation:
 *          activation, minimization, restoration, positioning, and property queries
 *          (PID, Title, Client List).
 * @note All operations return boolean success indicators or nodiscard values
 *       to prevent silent failure in window management workflows.
 */
class XcbEngine {
public:
    /**
     * @brief Constructs the engine with an injected XcbConnection.
     * @param conn Reference to the active XcbConnection.
     */
    explicit XcbEngine(XcbConnection& conn);
    ~XcbEngine() = default;

    XcbEngine(const XcbEngine&) = delete;
    XcbEngine& operator=(const XcbEngine&) = delete;
    XcbEngine(XcbEngine&&) = delete;
    XcbEngine& operator=(XcbEngine&&) = delete;

    // Window manipulation methods
    [[nodiscard]] bool activateWindow(xcb_window_t windowId);
    [[nodiscard]] bool minimizeWindow(xcb_window_t windowId);
    [[nodiscard]] bool restoreWindow(xcb_window_t windowId);
    [[nodiscard]] bool moveResizeWindow(xcb_window_t windowId, int x, int y, int width, int height);
    [[nodiscard]] bool closeWindow(xcb_window_t windowId);
    [[nodiscard]] bool purgeMaximizedState(xcb_window_t windowId);
    [[nodiscard]] bool setSkipTaskbar(xcb_window_t windowId, bool skip);
    [[nodiscard]] bool setWindowTypeDock(xcb_window_t windowId);
    [[nodiscard]] bool setWindowAbove(xcb_window_t windowId, bool above);
    [[nodiscard]] bool raiseWindow(xcb_window_t windowId);
    [[nodiscard]] bool setWindowDesktop(xcb_window_t windowId, uint32_t desktop);
    [[nodiscard]] bool killClient(xcb_window_t windowId);

    // Window inspection methods
    [[nodiscard]] QRect getScreenGeometry() const;
    [[nodiscard]] uint32_t getWindowPid(xcb_window_t windowId) const;
    [[nodiscard]] QString getWindowTitle(xcb_window_t windowId) const;
    [[nodiscard]] QString getWindowClass(xcb_window_t windowId) const;
    [[nodiscard]] QVector<xcb_window_t> getTopLevelWindows() const;
    [[nodiscard]] bool isTransient(xcb_window_t windowId) const;
    [[nodiscard]] uint32_t getCurrentDesktop() const;
    [[nodiscard]] uint32_t getWindowDesktop(xcb_window_t windowId) const;

    // Global key grabbing methods
    [[nodiscard]] bool grabAltLeftRightKeys();
    [[nodiscard]] bool ungrabAltLeftRightKeys();
    [[nodiscard]] bool grabCtrlShiftScroll();
    [[nodiscard]] bool ungrabCtrlShiftScroll();
    [[nodiscard]] xcb_keycode_t leftKeycode() const noexcept;
    [[nodiscard]] xcb_keycode_t rightKeycode() const noexcept;
    [[nodiscard]] bool isPreviousKey(xcb_keycode_t detail) const noexcept;
    [[nodiscard]] bool isNextKey(xcb_keycode_t detail) const noexcept;

    // Atom resolution
    [[nodiscard]] xcb_atom_t getAtom(const char* name) const;

private:
    XcbConnection& m_conn;
    xcb_connection_t* m_xcbConn = nullptr;
    xcb_window_t m_root = XCB_WINDOW_NONE;

    // Cached common atoms
    xcb_atom_t m_net_wm_state = XCB_NONE;
    xcb_atom_t m_net_wm_state_skip_taskbar = XCB_NONE;
    xcb_atom_t m_net_wm_state_maximized_vert = XCB_NONE;
    xcb_atom_t m_net_wm_state_maximized_horz = XCB_NONE;
    xcb_atom_t m_net_wm_state_above = XCB_NONE;
    xcb_atom_t m_net_active_window = XCB_NONE;
    xcb_atom_t m_net_client_list = XCB_NONE;
    xcb_atom_t m_net_wm_pid = XCB_NONE;
    xcb_atom_t m_net_wm_name = XCB_NONE;
    xcb_atom_t m_wm_name = XCB_NONE;
    xcb_atom_t m_wm_change_state = XCB_NONE;
    xcb_atom_t m_wm_protocols = XCB_NONE;
    xcb_atom_t m_wm_delete_window = XCB_NONE;
    xcb_atom_t m_wm_transient_for = XCB_NONE;
    xcb_atom_t m_net_wm_window_type = XCB_NONE;
    xcb_atom_t m_net_wm_window_type_dock = XCB_NONE;
    xcb_atom_t m_net_current_desktop = XCB_NONE;
    xcb_atom_t m_net_wm_desktop = XCB_NONE;
    xcb_atom_t m_net_moveresize_window = XCB_NONE;
    xcb_atom_t m_net_close_window = XCB_NONE;

    // Cached keycodes for global navigation hotkeys (Left/Right & A/S)
    xcb_keycode_t m_leftKeycode = 0;
    xcb_keycode_t m_rightKeycode = 0;
    xcb_keycode_t m_aKeycode = 0;
    xcb_keycode_t m_sKeycode = 0;

    void initAtoms();
    [[nodiscard]] bool sendClientMessage(xcb_window_t targetWindow, xcb_atom_t messageType,
                                        uint32_t data0, uint32_t data1 = 0,
                                        uint32_t data2 = 0, uint32_t data3 = 0,
                                        uint32_t data4 = 0);
};
