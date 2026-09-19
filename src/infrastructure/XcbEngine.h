#pragma once

#include <QRect>
#include <QString>
#include <QImage>
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

    /**
     * @brief Sends an EWMH _NET_ACTIVE_WINDOW ClientMessage to give input focus to a window.
     * @param windowId Target X11 window.
     * @return True if message was successfully dispatched to X server.
     */
    [[nodiscard]] bool activateWindow(xcb_window_t windowId);

    /**
     * @brief Minimizes/iconifies an X11 window via WM_CHANGE_STATE message.
     * @param windowId Target X11 window.
     * @return True if minimization request was sent successfully.
     */
    [[nodiscard]] bool minimizeWindow(xcb_window_t windowId);

    /**
     * @brief Unmaps or maps an iconified window back to normal viewable state.
     * @param windowId Target X11 window.
     * @return True if restore request was dispatched successfully.
     */
    [[nodiscard]] bool restoreWindow(xcb_window_t windowId);

    /**
     * @brief Positions and dimensions a top-level X11 window.
     * @param windowId Target X11 window.
     * @param x Horizontal screen offset in pixels.
     * @param y Vertical screen offset in pixels.
     * @param width Target window width in pixels.
     * @param height Target window height in pixels.
     * @return True if configuration request was dispatched successfully.
     */
    [[nodiscard]] bool moveResizeWindow(xcb_window_t windowId, int x, int y, int width, int height);

    /**
     * @brief Gracefully closes a window via WM_DELETE_WINDOW client message protocol.
     * @param windowId Target X11 window.
     * @return True if delete message was sent; false if window or connection is invalid.
     */
    [[nodiscard]] bool closeWindow(xcb_window_t windowId);

    /**
     * @brief Strips maximized horizontal and vertical EWMH states to allow arbitrary resizing.
     * @param windowId Target X11 window.
     * @return True if states were successfully purged.
     */
    [[nodiscard]] bool purgeMaximizedState(xcb_window_t windowId);

    /**
     * @brief Sets or unsets _NET_WM_STATE_SKIP_TASKBAR to control taskbar visibility.
     * @param windowId Target X11 window.
     * @param skip True to hide from window manager taskbars; false to reveal.
     * @return True if state was applied.
     */
    [[nodiscard]] bool setSkipTaskbar(xcb_window_t windowId, bool skip);

    /**
     * @brief Sets _NET_WM_WINDOW_TYPE to _NET_WM_WINDOW_TYPE_DOCK to prevent window manager decorations.
     * @param windowId Target X11 window.
     * @return True if dock window type was successfully set.
     */
    [[nodiscard]] bool setWindowTypeDock(xcb_window_t windowId);

    /**
     * @brief Sets or removes _NET_WM_STATE_ABOVE to keep a window on top.
     * @param windowId Target X11 window.
     * @param above True to pin window on top; false to release.
     * @return True if message was dispatched.
     */
    [[nodiscard]] bool setWindowAbove(xcb_window_t windowId, bool above);

    /**
     * @brief Raises a window to the top of the X11 stacking order.
     * @param windowId Target X11 window.
     * @return True if configure request was sent.
     */
    [[nodiscard]] bool raiseWindow(xcb_window_t windowId);

    /**
     * @brief Binds a window to a specific virtual desktop via _NET_WM_DESKTOP.
     * @param windowId Target X11 window.
     * @param desktop Zero-based desktop index, or 0xFFFFFFFF for all desktops.
     * @return True if desktop assignment message was dispatched.
     */
    [[nodiscard]] bool setWindowDesktop(xcb_window_t windowId, uint32_t desktop);

    /**
     * @brief Overrides window WM_CLASS and _NET_WM_ICON for taskbar visual alignment.
     * @param windowId Target X11 window.
     * @param className New WM_CLASS string name.
     * @param icon ARGB32 icon image to serialize.
     * @return True if properties were written to X server.
     */
    [[nodiscard]] bool overrideWindowIconAndClass(xcb_window_t windowId, const QString& className, const QImage& icon);

    /**
     * @brief Forcibly terminates an X11 client connection using xcb_kill_client.
     * @param windowId Target X11 window.
     * @return True if kill request was sent.
     */
    [[nodiscard]] bool killClient(xcb_window_t windowId);

    // Window inspection methods

    /**
     * @brief Queries full desktop screen geometry via XCB default screen.
     * @return QRect representing total display geometry.
     */
    [[nodiscard]] QRect getScreenGeometry() const;

    /**
     * @brief Retrieves operating system PID associated with a window via _NET_WM_PID.
     * @param windowId Target X11 window.
     * @return Process ID, or 0 if property is missing or query fails.
     */
    [[nodiscard]] uint32_t getWindowPid(xcb_window_t windowId) const;

    /**
     * @brief Retrieves the window title via _NET_WM_NAME or WM_NAME.
     * @param windowId Target X11 window.
     * @return UTF-8 title string, or empty string on failure.
     */
    [[nodiscard]] QString getWindowTitle(xcb_window_t windowId) const;

    /**
     * @brief Retrieves the window WM_CLASS instance and class strings.
     * @param windowId Target X11 window.
     * @return Class identifier string, or empty string on failure.
     */
    [[nodiscard]] QString getWindowClass(xcb_window_t windowId) const;

    /**
     * @brief Retrieves all active top-level client windows from _NET_CLIENT_LIST.
     * @return List of client X11 window IDs.
     */
    [[nodiscard]] QVector<xcb_window_t> getTopLevelWindows() const;

    /**
     * @brief Checks if a window is transient (dialog or popup) via WM_TRANSIENT_FOR.
     * @param windowId Target X11 window.
     * @return True if window is a transient child.
     */
    [[nodiscard]] bool isTransient(xcb_window_t windowId) const;

    /**
     * @brief Gets current active virtual desktop index from root window.
     * @return Zero-based desktop index.
     */
    [[nodiscard]] uint32_t getCurrentDesktop() const;

    /**
     * @brief Gets the virtual desktop index to which a specific window is assigned.
     * @param windowId Target X11 window.
     * @return Zero-based desktop index, or 0 on error.
     */
    [[nodiscard]] uint32_t getWindowDesktop(xcb_window_t windowId) const;

    // Global key grabbing methods

    /**
     * @brief Grabs global Alt+Left and Alt+Right (and Alt+A/S) hotkeys on the root window.
     * @return True if grabs succeeded.
     */
    [[nodiscard]] bool grabAltLeftRightKeys();

    /**
     * @brief Releases global Alt+Left and Alt+Right hotkey grabs.
     * @return True if ungrabs succeeded.
     */
    [[nodiscard]] bool ungrabAltLeftRightKeys();

    /**
     * @brief Grabs global Ctrl+Shift+Scroll (mouse buttons 4 & 5) on the root window.
     * @return True if button grabs succeeded.
     */
    [[nodiscard]] bool grabCtrlShiftScroll();

    /**
     * @brief Releases global Ctrl+Shift+Scroll grabs.
     * @return True if button ungrabs succeeded.
     */
    [[nodiscard]] bool ungrabCtrlShiftScroll();

    /**
     * @brief Returns cached keycode for the Left/Previous navigation key.
     */
    [[nodiscard]] xcb_keycode_t leftKeycode() const noexcept;

    /**
     * @brief Returns cached keycode for the Right/Next navigation key.
     */
    [[nodiscard]] xcb_keycode_t rightKeycode() const noexcept;

    /**
     * @brief Checks whether a raw keycode detail matches the Previous deck hotkey.
     * @param detail Keycode detail received from XCB_KEY_PRESS event.
     * @return True if matching Previous hotkey.
     */
    [[nodiscard]] bool isPreviousKey(xcb_keycode_t detail) const noexcept;

    /**
     * @brief Checks whether a raw keycode detail matches the Next deck hotkey.
     * @param detail Keycode detail received from XCB_KEY_PRESS event.
     * @return True if matching Next hotkey.
     */
    [[nodiscard]] bool isNextKey(xcb_keycode_t detail) const noexcept;

    // Atom resolution

    /**
     * @brief Interns or resolves an X11 atom by string name with caching.
     * @param name C-string atom name (e.g. "_NET_WM_STATE").
     * @return xcb_atom_t identifier, or XCB_NONE on failure.
     */
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
    xcb_atom_t m_net_wm_icon = XCB_NONE;

    // Cached keycodes for global navigation hotkeys (Left/Right & A/S)
    xcb_keycode_t m_leftKeycode = 0;
    xcb_keycode_t m_rightKeycode = 0;
    xcb_keycode_t m_aKeycode = 0;
    xcb_keycode_t m_sKeycode = 0;

    /**
     * @brief Resolves and caches frequently referenced EWMH and ICCCM X11 atoms.
     */
    void initAtoms();

    /**
     * @brief Low-level helper to construct and dispatch an XCB_CLIENT_MESSAGE event.
     * @param targetWindow Window receiving or subjected to the message.
     * @param messageType The primary Atom designating the message intent.
     * @param data0 32-bit payload argument 0.
     * @param data1 32-bit payload argument 1.
     * @param data2 32-bit payload argument 2.
     * @param data3 32-bit payload argument 3.
     * @param data4 32-bit payload argument 4.
     * @return True if client message request was successfully dispatched.
     */
    [[nodiscard]] bool sendClientMessage(xcb_window_t targetWindow, xcb_atom_t messageType,
                                        uint32_t data0, uint32_t data1 = 0,
                                        uint32_t data2 = 0, uint32_t data3 = 0,
                                        uint32_t data4 = 0);
};
