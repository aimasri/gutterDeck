#include "XcbEngine.h"
#include "XcbConnection.h"

#include <cstdlib>
#include <cstring>
#include <xcb/xproto.h>
#include <X11/keysym.h>
#include <xcb/xcb_keysyms.h>

void XcbReplyDeleter::operator()(void* ptr) const noexcept {
    if (ptr) {
        std::free(ptr);
    }
}

XcbEngine::XcbEngine(XcbConnection& conn)
    : m_conn(conn),
      m_xcbConn(conn.get()),
      m_root(conn.rootWindow()) {
    initAtoms();

    // Resolve keycodes for global navigation hotkeys
    if (m_xcbConn) {
        xcb_key_symbols_t* syms = xcb_key_symbols_alloc(m_xcbConn);
        if (syms) {
            xcb_keycode_t* leftCodes = xcb_key_symbols_get_keycode(syms, XK_Left);
            if (leftCodes) {
                m_leftKeycode = leftCodes[0];
                std::free(leftCodes);
            }
            xcb_keycode_t* rightCodes = xcb_key_symbols_get_keycode(syms, XK_Right);
            if (rightCodes) {
                m_rightKeycode = rightCodes[0];
                std::free(rightCodes);
            }
            xcb_keycode_t* aCodes = xcb_key_symbols_get_keycode(syms, XK_a);
            if (aCodes) {
                m_aKeycode = aCodes[0];
                std::free(aCodes);
            }
            xcb_keycode_t* sCodes = xcb_key_symbols_get_keycode(syms, XK_s);
            if (sCodes) {
                m_sKeycode = sCodes[0];
                std::free(sCodes);
            }
            xcb_key_symbols_free(syms);
        }
    }
}

xcb_atom_t XcbEngine::getAtom(const char* name) const {
    if (!m_xcbConn || !name) {
        return XCB_NONE;
    }
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(m_xcbConn, 0, static_cast<uint16_t>(std::strlen(name)), name);
    xcb_ptr<xcb_intern_atom_reply_t> reply(xcb_intern_atom_reply(m_xcbConn, cookie, nullptr));
    return reply ? reply->atom : XCB_NONE;
}

void XcbEngine::initAtoms() {
    m_net_wm_state = getAtom("_NET_WM_STATE");
    m_net_wm_state_skip_taskbar = getAtom("_NET_WM_STATE_SKIP_TASKBAR");
    m_net_wm_state_maximized_vert = getAtom("_NET_WM_STATE_MAXIMIZED_VERT");
    m_net_wm_state_maximized_horz = getAtom("_NET_WM_STATE_MAXIMIZED_HORZ");
    m_net_wm_state_above = getAtom("_NET_WM_STATE_ABOVE");
    m_net_active_window = getAtom("_NET_ACTIVE_WINDOW");
    m_net_client_list = getAtom("_NET_CLIENT_LIST");
    m_net_wm_pid = getAtom("_NET_WM_PID");
    m_net_wm_name = getAtom("_NET_WM_NAME");
    m_wm_name = getAtom("WM_NAME");
    m_wm_change_state = getAtom("WM_CHANGE_STATE");
    m_wm_protocols = getAtom("WM_PROTOCOLS");
    m_wm_delete_window = getAtom("WM_DELETE_WINDOW");
    m_wm_transient_for = getAtom("WM_TRANSIENT_FOR");
    m_net_wm_window_type = getAtom("_NET_WM_WINDOW_TYPE");
    m_net_wm_window_type_dock = getAtom("_NET_WM_WINDOW_TYPE_DOCK");
    m_net_current_desktop = getAtom("_NET_CURRENT_DESKTOP");
    m_net_wm_desktop = getAtom("_NET_WM_DESKTOP");
    m_net_moveresize_window = getAtom("_NET_MOVERESIZE_WINDOW");
    m_net_close_window = getAtom("_NET_CLOSE_WINDOW");
}

bool XcbEngine::sendClientMessage(xcb_window_t targetWindow, xcb_atom_t messageType,
                                  uint32_t data0, uint32_t data1,
                                  uint32_t data2, uint32_t data3,
                                  uint32_t data4) {
    if (!m_xcbConn || targetWindow == XCB_WINDOW_NONE || messageType == XCB_NONE) {
        return false;
    }

    xcb_client_message_event_t event{};
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = targetWindow;
    event.type = messageType;
    event.data.data32[0] = data0;
    event.data.data32[1] = data1;
    event.data.data32[2] = data2;
    event.data.data32[3] = data3;
    event.data.data32[4] = data4;

    uint32_t eventMask = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
    xcb_void_cookie_t cookie = xcb_send_event_checked(
        m_xcbConn, 0, m_root, eventMask, reinterpret_cast<const char*>(&event));
    xcb_ptr<xcb_generic_error_t> err(xcb_request_check(m_xcbConn, cookie));
    m_conn.flush();
    return err == nullptr;
}

bool XcbEngine::activateWindow(xcb_window_t windowId) {
    if (windowId == XCB_WINDOW_NONE) {
        return false;
    }
    // EWMH _NET_ACTIVE_WINDOW: source = 2 (pager/dock), timestamp = 0 (current), active_window = 0
    return sendClientMessage(windowId, m_net_active_window, 2, XCB_CURRENT_TIME, 0);
}

bool XcbEngine::minimizeWindow(xcb_window_t windowId) {
    if (windowId == XCB_WINDOW_NONE) {
        return false;
    }
    static_cast<void>(setSkipTaskbar(windowId, true));
    // ICCCM WM_CHANGE_STATE: IconicState = 3
    return sendClientMessage(windowId, m_wm_change_state, 3);
}

bool XcbEngine::restoreWindow(xcb_window_t windowId) {
    if (!m_xcbConn || windowId == XCB_WINDOW_NONE) {
        return false;
    }
    xcb_map_window(m_xcbConn, windowId);
    static_cast<void>(setSkipTaskbar(windowId, false));
    m_conn.flush();
    return activateWindow(windowId);
}

bool XcbEngine::moveResizeWindow(xcb_window_t windowId, int x, int y, int width, int height) {
    if (!m_xcbConn || windowId == XCB_WINDOW_NONE) {
        return false;
    }

    // 1. Direct X11 configure request
    uint32_t values[4] = {
        static_cast<uint32_t>(x),
        static_cast<uint32_t>(y),
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };
    uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;

    xcb_void_cookie_t cookie = xcb_configure_window_checked(m_xcbConn, windowId, mask, values);
    xcb_ptr<xcb_generic_error_t> err(xcb_request_check(m_xcbConn, cookie));

    // 2. EWMH _NET_MOVERESIZE_WINDOW client message:
    // flags: gravity 0 (Static), bits 8-11 set (0x0F00 for x, y, width, height)
    if (m_net_moveresize_window != XCB_NONE) {
        uint32_t flags = 0x0F00;
        static_cast<void>(sendClientMessage(windowId, m_net_moveresize_window, flags,
                                            static_cast<uint32_t>(x),
                                            static_cast<uint32_t>(y),
                                            static_cast<uint32_t>(width),
                                            static_cast<uint32_t>(height)));
    }

    m_conn.flush();
    return err == nullptr;
}

bool XcbEngine::closeWindow(xcb_window_t windowId) {
    if (!m_xcbConn || windowId == XCB_WINDOW_NONE) {
        return false;
    }

    // 1. EWMH Standard: Send _NET_CLOSE_WINDOW to root window (WM handles client frame gracefully)
    if (m_net_close_window != XCB_NONE) {
        static_cast<void>(sendClientMessage(windowId, m_net_close_window, XCB_CURRENT_TIME, 2));
    }

    // 2. ICCCM Standard: Send WM_PROTOCOLS / WM_DELETE_WINDOW directly to the client window
    if (m_wm_protocols != XCB_NONE && m_wm_delete_window != XCB_NONE) {
        xcb_client_message_event_t event{};
        event.response_type = XCB_CLIENT_MESSAGE;
        event.format = 32;
        event.window = windowId;
        event.type = m_wm_protocols;
        event.data.data32[0] = m_wm_delete_window;
        event.data.data32[1] = XCB_CURRENT_TIME;

        xcb_send_event(m_xcbConn, 0, windowId, XCB_EVENT_MASK_NO_EVENT, reinterpret_cast<const char*>(&event));
    }

    m_conn.flush();
    return true;
}

bool XcbEngine::killClient(xcb_window_t windowId) {
    if (!m_xcbConn || windowId == XCB_WINDOW_NONE) {
        return false;
    }
    xcb_kill_client(m_xcbConn, windowId);
    m_conn.flush();
    return true;
}

bool XcbEngine::purgeMaximizedState(xcb_window_t windowId) {
    // 0 = _NET_WM_STATE_REMOVE
    return sendClientMessage(windowId, m_net_wm_state, 0,
                             m_net_wm_state_maximized_vert,
                             m_net_wm_state_maximized_horz);
}

bool XcbEngine::setSkipTaskbar(xcb_window_t windowId, bool skip) {
    // 1 = ADD, 0 = REMOVE
    return sendClientMessage(windowId, m_net_wm_state, skip ? 1 : 0, m_net_wm_state_skip_taskbar);
}

bool XcbEngine::setWindowTypeDock(xcb_window_t windowId) {
    if (!m_xcbConn || windowId == XCB_WINDOW_NONE ||
        m_net_wm_window_type == XCB_NONE || m_net_wm_window_type_dock == XCB_NONE) {
        return false;
    }
    xcb_change_property(m_xcbConn, XCB_PROP_MODE_REPLACE, windowId,
                        m_net_wm_window_type, XCB_ATOM_ATOM, 32, 1,
                        &m_net_wm_window_type_dock);
    m_conn.flush();
    return true;
}

bool XcbEngine::setWindowAbove(xcb_window_t windowId, bool above) {
    // 1 = ADD, 0 = REMOVE
    return sendClientMessage(windowId, m_net_wm_state, above ? 1 : 0, m_net_wm_state_above);
}

bool XcbEngine::setWindowDesktop(xcb_window_t windowId, uint32_t desktop) {
    return sendClientMessage(windowId, m_net_wm_desktop, desktop, 1);
}

bool XcbEngine::raiseWindow(xcb_window_t windowId) {
    if (!m_xcbConn || windowId == XCB_WINDOW_NONE) {
        return false;
    }
    uint32_t values[1] = { XCB_STACK_MODE_ABOVE };
    xcb_void_cookie_t cookie = xcb_configure_window_checked(m_xcbConn, windowId, XCB_CONFIG_WINDOW_STACK_MODE, values);
    xcb_ptr<xcb_generic_error_t> err(xcb_request_check(m_xcbConn, cookie));
    m_conn.flush();
    return err == nullptr;
}

QRect XcbEngine::getScreenGeometry() const {
    xcb_screen_t* screen = m_conn.defaultScreen();
    if (!screen) {
        return QRect(0, 0, 1920, 1080);
    }
    return QRect(0, 0, screen->width_in_pixels, screen->height_in_pixels);
}

uint32_t XcbEngine::getWindowPid(xcb_window_t windowId) const {
    if (!m_xcbConn || windowId == XCB_WINDOW_NONE || m_net_wm_pid == XCB_NONE) {
        return 0;
    }

    xcb_get_property_cookie_t cookie = xcb_get_property(
        m_xcbConn, 0, windowId, m_net_wm_pid, XCB_ATOM_CARDINAL, 0, 1);
    xcb_ptr<xcb_get_property_reply_t> reply(xcb_get_property_reply(m_xcbConn, cookie, nullptr));

    if (reply && reply->value_len > 0 && reply->format == 32) {
        auto* pids = static_cast<uint32_t*>(xcb_get_property_value(reply.get()));
        return pids ? pids[0] : 0;
    }
    return 0;
}

QString XcbEngine::getWindowTitle(xcb_window_t windowId) const {
    if (!m_xcbConn || windowId == XCB_WINDOW_NONE) {
        return {};
    }

    // Try _NET_WM_NAME (UTF8) first
    if (m_net_wm_name != XCB_NONE) {
        xcb_get_property_cookie_t cookie = xcb_get_property(
            m_xcbConn, 0, windowId, m_net_wm_name, XCB_GET_PROPERTY_TYPE_ANY, 0, 512);
        xcb_ptr<xcb_get_property_reply_t> reply(xcb_get_property_reply(m_xcbConn, cookie, nullptr));
        if (reply && reply->value_len > 0) {
            auto* str = static_cast<char*>(xcb_get_property_value(reply.get()));
            int len = xcb_get_property_value_length(reply.get());
            return QString::fromUtf8(str, len);
        }
    }

    // Fallback to WM_NAME (Latin-1/Ascii)
    if (m_wm_name != XCB_NONE) {
        xcb_get_property_cookie_t cookie = xcb_get_property(
            m_xcbConn, 0, windowId, m_wm_name, XCB_ATOM_STRING, 0, 512);
        xcb_ptr<xcb_get_property_reply_t> reply(xcb_get_property_reply(m_xcbConn, cookie, nullptr));
        if (reply && reply->value_len > 0) {
            auto* str = static_cast<char*>(xcb_get_property_value(reply.get()));
            int len = xcb_get_property_value_length(reply.get());
            return QString::fromLatin1(str, len);
        }
    }

    return {};
}

QString XcbEngine::getWindowClass(xcb_window_t windowId) const {
    if (!m_xcbConn || windowId == XCB_WINDOW_NONE) {
        return {};
    }

    xcb_get_property_cookie_t cookie = xcb_get_property(
        m_xcbConn, 0, windowId, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 0, 512);
    xcb_ptr<xcb_get_property_reply_t> reply(xcb_get_property_reply(m_xcbConn, cookie, nullptr));
    if (reply && reply->value_len > 0) {
        auto* str = static_cast<char*>(xcb_get_property_value(reply.get()));
        int len = xcb_get_property_value_length(reply.get());
        return QString::fromUtf8(str, len).replace('\0', ' ').trimmed();
    }

    return {};
}

bool XcbEngine::isTransient(xcb_window_t windowId) const {
    if (!m_xcbConn || windowId == XCB_WINDOW_NONE || m_wm_transient_for == XCB_NONE) {
        return false;
    }

    xcb_get_property_cookie_t cookie = xcb_get_property(
        m_xcbConn, 0, windowId, m_wm_transient_for, XCB_ATOM_WINDOW, 0, 1);
    xcb_ptr<xcb_get_property_reply_t> reply(xcb_get_property_reply(m_xcbConn, cookie, nullptr));
    return reply && reply->value_len > 0;
}

uint32_t XcbEngine::getCurrentDesktop() const {
    if (!m_xcbConn || m_net_current_desktop == XCB_NONE) {
        return 0;
    }

    xcb_get_property_cookie_t cookie = xcb_get_property(
        m_xcbConn, 0, m_root, m_net_current_desktop, XCB_ATOM_CARDINAL, 0, 1);
    xcb_ptr<xcb_get_property_reply_t> reply(xcb_get_property_reply(m_xcbConn, cookie, nullptr));

    if (reply && reply->value_len > 0 && reply->format == 32) {
        auto* desktops = static_cast<uint32_t*>(xcb_get_property_value(reply.get()));
        return desktops ? desktops[0] : 0;
    }
    return 0;
}

uint32_t XcbEngine::getWindowDesktop(xcb_window_t windowId) const {
    if (!m_xcbConn || windowId == XCB_WINDOW_NONE || m_net_wm_desktop == XCB_NONE) {
        return 0xFFFFFFFF;
    }

    xcb_get_property_cookie_t cookie = xcb_get_property(
        m_xcbConn, 0, windowId, m_net_wm_desktop, XCB_ATOM_CARDINAL, 0, 1);
    xcb_ptr<xcb_get_property_reply_t> reply(xcb_get_property_reply(m_xcbConn, cookie, nullptr));

    if (reply && reply->value_len > 0 && reply->format == 32) {
        auto* desktops = static_cast<uint32_t*>(xcb_get_property_value(reply.get()));
        return desktops ? desktops[0] : 0xFFFFFFFF;
    }
    return 0xFFFFFFFF;
}

QVector<xcb_window_t> XcbEngine::getTopLevelWindows() const {
    QVector<xcb_window_t> windows;
    if (!m_xcbConn || m_net_client_list == XCB_NONE) {
        return windows;
    }

    xcb_get_property_cookie_t cookie = xcb_get_property(
        m_xcbConn, 0, m_root, m_net_client_list, XCB_ATOM_WINDOW, 0, 1024);
    xcb_ptr<xcb_get_property_reply_t> reply(xcb_get_property_reply(m_xcbConn, cookie, nullptr));

    if (reply && reply->type == XCB_ATOM_WINDOW) {
        int count = xcb_get_property_value_length(reply.get()) / static_cast<int>(sizeof(xcb_window_t));
        auto* rawWindows = static_cast<xcb_window_t*>(xcb_get_property_value(reply.get()));

        for (int i = 0; i < count; ++i) {
            xcb_window_t wid = rawWindows[i];
            xcb_get_window_attributes_cookie_t attrCookie = xcb_get_window_attributes(m_xcbConn, wid);
            xcb_ptr<xcb_get_window_attributes_reply_t> attrReply(xcb_get_window_attributes_reply(m_xcbConn, attrCookie, nullptr));

            if (attrReply && !isTransient(wid)) {
                windows.append(wid);
            }
        }
    }

    return windows;
}

namespace {
constexpr uint16_t ALT_MOD_MASKS[] = {
    XCB_MOD_MASK_1,                                               // Alt
    XCB_MOD_MASK_1 | XCB_MOD_MASK_2,                             // Alt + NumLock
    XCB_MOD_MASK_1 | XCB_MOD_MASK_LOCK,                          // Alt + CapsLock
    XCB_MOD_MASK_1 | XCB_MOD_MASK_2 | XCB_MOD_MASK_LOCK         // Alt + NumLock + CapsLock
};
} // namespace

bool XcbEngine::grabAltLeftRightKeys() {
    if (!m_xcbConn) {
        return false;
    }
    const xcb_keycode_t keycodes[] = { m_leftKeycode, m_rightKeycode, m_aKeycode, m_sKeycode };
    for (xcb_keycode_t kc : keycodes) {
        if (kc == 0) {
            continue;
        }
        for (uint16_t mask : ALT_MOD_MASKS) {
            xcb_grab_key(m_xcbConn, 0, m_root, mask, kc,
                         XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
        }
    }
    m_conn.flush();
    return true;
}

bool XcbEngine::ungrabAltLeftRightKeys() {
    if (!m_xcbConn) {
        return false;
    }
    const xcb_keycode_t keycodes[] = { m_leftKeycode, m_rightKeycode, m_aKeycode, m_sKeycode };
    for (xcb_keycode_t kc : keycodes) {
        if (kc == 0) {
            continue;
        }
        for (uint16_t mask : ALT_MOD_MASKS) {
            xcb_ungrab_key(m_xcbConn, kc, m_root, mask);
        }
    }
    m_conn.flush();
    return true;
}

xcb_keycode_t XcbEngine::leftKeycode() const noexcept {
    return m_leftKeycode;
}

xcb_keycode_t XcbEngine::rightKeycode() const noexcept {
    return m_rightKeycode;
}

bool XcbEngine::isPreviousKey(xcb_keycode_t detail) const noexcept {
    return (detail != 0 && (detail == m_leftKeycode || detail == m_aKeycode));
}

bool XcbEngine::isNextKey(xcb_keycode_t detail) const noexcept {
    return (detail != 0 && (detail == m_rightKeycode || detail == m_sKeycode));
}
