#pragma once

#include <memory>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/**
 * @brief RAII wrapper for an XCB connection to the X Display Server.
 * @details Connects on construction, initializes EWMH atoms, and guarantees
 *          clean disconnection and resource cleanup upon destruction.
 *          Provides access to the default screen, EWMH client handle,
 *          and underlying socket file descriptor.
 * @note This class is non-copyable and non-movable to guarantee single ownership
 *       of the underlying display server connection.
 */
class XcbConnection {
public:
    /**
     * @brief Establishes connection to the X server and initializes EWMH support.
     * @throws std::runtime_error If connection to the X server fails.
     */
    XcbConnection();

    /**
     * @brief Cleans up EWMH atoms and disconnects from the X server.
     */
    ~XcbConnection();

    // Disable copy and move semantics
    XcbConnection(const XcbConnection&) = delete;
    XcbConnection& operator=(const XcbConnection&) = delete;
    XcbConnection(XcbConnection&&) = delete;
    XcbConnection& operator=(XcbConnection&&) = delete;

    /**
     * @brief Gets the underlying non-owning raw XCB connection pointer.
     * @return Non-owning pointer to xcb_connection_t.
     */
    [[nodiscard]] xcb_connection_t* get() const noexcept;

    /**
     * @brief Gets the cached default screen.
     * @return Non-owning pointer to xcb_screen_t.
     */
    [[nodiscard]] xcb_screen_t* defaultScreen() const noexcept;

    /**
     * @brief Gets the non-owning pointer to EWMH connection handle.
     * @return Pointer to xcb_ewmh_connection_t.
     */
    [[nodiscard]] xcb_ewmh_connection_t* ewmh() noexcept;
    [[nodiscard]] const xcb_ewmh_connection_t* ewmh() const noexcept;

    /**
     * @brief Gets the root window of the default screen.
     * @return The xcb_window_t root window ID.
     */
    [[nodiscard]] xcb_window_t rootWindow() const noexcept;

    /**
     * @brief Gets the file descriptor associated with the XCB connection.
     * @details Used by QSocketNotifier for event-driven integration with Qt's event loop.
     * @return File descriptor integer.
     */
    [[nodiscard]] int fileDescriptor() const noexcept;

    /**
     * @brief Flushes any buffered outgoing requests to the X server.
     */
    void flush() const noexcept;

private:
    struct XcbDisconnector {
        void operator()(xcb_connection_t* c) const noexcept;
    };

    std::unique_ptr<xcb_connection_t, XcbDisconnector> m_connection;
    xcb_ewmh_connection_t m_ewmh{};
    xcb_screen_t* m_screen = nullptr;
    bool m_ewmhInitialized = false;
};
