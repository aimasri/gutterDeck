#include "XcbConnection.h"

#include <stdexcept>
#include <xcb/xcb.h>

void XcbConnection::XcbDisconnector::operator()(xcb_connection_t* c) const noexcept {
    if (c) {
        xcb_disconnect(c);
    }
}

XcbConnection::XcbConnection() {
    int defaultScreenNum = 0;
    xcb_connection_t* rawConn = xcb_connect(nullptr, &defaultScreenNum);
    if (!rawConn || xcb_connection_has_error(rawConn)) {
        if (rawConn) {
            xcb_disconnect(rawConn);
        }
        throw std::runtime_error("Failed to connect to X server via XCB");
    }

    m_connection.reset(rawConn);

    const xcb_setup_t* setup = xcb_get_setup(m_connection.get());
    xcb_screen_iterator_t screenIter = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < defaultScreenNum && screenIter.rem > 0; ++i) {
        xcb_screen_next(&screenIter);
    }

    if (screenIter.rem == 0) {
        throw std::runtime_error("Failed to retrieve default XCB screen");
    }
    m_screen = screenIter.data;

    // Initialize EWMH atoms
    xcb_intern_atom_cookie_t* ewmhCookie = xcb_ewmh_init_atoms(m_connection.get(), &m_ewmh);
    if (ewmhCookie) {
        if (xcb_ewmh_init_atoms_replies(&m_ewmh, ewmhCookie, nullptr)) {
            m_ewmhInitialized = true;
        }
    }
}

XcbConnection::~XcbConnection() {
    if (m_ewmhInitialized) {
        xcb_ewmh_connection_wipe(&m_ewmh);
        m_ewmhInitialized = false;
    }
    // m_connection unique_ptr will safely call xcb_disconnect
}

xcb_connection_t* XcbConnection::get() const noexcept {
    return m_connection.get();
}

xcb_screen_t* XcbConnection::defaultScreen() const noexcept {
    return m_screen;
}

xcb_ewmh_connection_t* XcbConnection::ewmh() noexcept {
    return m_ewmhInitialized ? &m_ewmh : nullptr;
}

const xcb_ewmh_connection_t* XcbConnection::ewmh() const noexcept {
    return m_ewmhInitialized ? &m_ewmh : nullptr;
}

xcb_window_t XcbConnection::rootWindow() const noexcept {
    return m_screen ? m_screen->root : static_cast<xcb_window_t>(0);
}

int XcbConnection::fileDescriptor() const noexcept {
    return m_connection ? xcb_get_file_descriptor(m_connection.get()) : -1;
}

void XcbConnection::flush() const noexcept {
    if (m_connection) {
        xcb_flush(m_connection.get());
    }
}
