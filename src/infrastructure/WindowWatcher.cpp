#include "WindowWatcher.h"
#include "XcbConnection.h"
#include "XcbEngine.h"

#include <QDebug>
#include <xcb/xproto.h>

WindowWatcher::WindowWatcher(XcbConnection& conn, XcbEngine& engine, QObject* parent)
    : QObject(parent),
      m_conn(conn),
      m_engine(engine),
      m_netClientListAtom(engine.getAtom("_NET_CLIENT_LIST")),
      m_netCurrentDesktopAtom(engine.getAtom("_NET_CURRENT_DESKTOP")) {}

WindowWatcher::~WindowWatcher() {
    stopWatching();
}

void WindowWatcher::startWatching() {
    if (m_isWatching) {
        return;
    }

    xcb_connection_t* c = m_conn.get();
    if (!c) {
        return;
    }

    // Subscribe to root window property modifications and child lifecycle notifications
    uint32_t values[1] = {
        XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
    };
    xcb_change_window_attributes(c, m_conn.rootWindow(), XCB_CW_EVENT_MASK, values);
    m_conn.flush();

    // Populate initial top-level windows
    QVector<xcb_window_t> initialWindows = m_engine.getTopLevelWindows();
    m_knownWindows.clear();
    for (xcb_window_t wid : initialWindows) {
        m_knownWindows.insert(wid);
    }

    int fd = m_conn.fileDescriptor();
    if (fd >= 0) {
        m_notifier = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Read, this);
        connect(m_notifier.get(), &QSocketNotifier::activated,
                this, &WindowWatcher::processXcbEvents);
        m_notifier->setEnabled(true);
    }

    m_isWatching = true;
}

void WindowWatcher::stopWatching() {
    if (!m_isWatching) {
        return;
    }

    if (m_notifier) {
        m_notifier->setEnabled(false);
        m_notifier.reset();
    }

    m_isWatching = false;
}

bool WindowWatcher::isWatching() const noexcept {
    return m_isWatching;
}

void WindowWatcher::processXcbEvents() {
    xcb_connection_t* c = m_conn.get();
    if (!c) {
        return;
    }

    // Drain all pending events in the XCB queue
    while (xcb_generic_event_t* rawEvent = xcb_poll_for_event(c)) {
        xcb_ptr<xcb_generic_event_t> event(rawEvent);
        uint8_t responseType = event->response_type & ~0x80;

        switch (responseType) {
            case XCB_PROPERTY_NOTIFY: {
                auto* propEvent = reinterpret_cast<xcb_property_notify_event_t*>(event.get());
                if (propEvent->window == m_conn.rootWindow()) {
                    if (propEvent->atom == m_netClientListAtom) {
                        refreshClientList();
                    } else if (propEvent->atom == m_netCurrentDesktopAtom) {
                        uint32_t currentDesktop = m_engine.getCurrentDesktop();
                        emit currentDesktopChanged(currentDesktop);
                    }
                }
                break;
            }
            case XCB_DESTROY_NOTIFY: {
                auto* destroyEvent = reinterpret_cast<xcb_destroy_notify_event_t*>(event.get());
                auto it = m_knownWindows.find(destroyEvent->window);
                if (it != m_knownWindows.end()) {
                    m_knownWindows.erase(it);
                    emit windowDestroyed(destroyEvent->window);
                }
                break;
            }
            case XCB_UNMAP_NOTIFY: {
                // An unmapped window is often minimized/iconified into background.
                // It is NOT destroyed; actual destruction arrives via XCB_DESTROY_NOTIFY or _NET_CLIENT_LIST removal.
                break;
            }
            case XCB_KEY_PRESS: {
                auto* keyEvent = reinterpret_cast<xcb_key_press_event_t*>(event.get());
                if (m_engine.isPreviousKey(keyEvent->detail)) {
                    emit previousDeckRequested();
                } else if (m_engine.isNextKey(keyEvent->detail)) {
                    emit nextDeckRequested();
                }
                break;
            }
            default:
                break;
        }
    }
}

void WindowWatcher::refreshClientList() {
    QVector<xcb_window_t> currentList = m_engine.getTopLevelWindows();
    std::unordered_set<uint32_t> currentSet(currentList.begin(), currentList.end());

    // Identify newly mapped windows
    for (uint32_t wid : currentList) {
        if (m_knownWindows.find(wid) == m_knownWindows.end()) {
            m_knownWindows.insert(wid);
            uint32_t pid = m_engine.getWindowPid(wid);
            QString title = m_engine.getWindowTitle(wid);
            emit windowMapped(wid, pid, title);
        }
    }

    // Identify removed/unmapped windows
    for (auto it = m_knownWindows.begin(); it != m_knownWindows.end();) {
        if (currentSet.find(*it) == currentSet.end()) {
            uint32_t deadWid = *it;
            it = m_knownWindows.erase(it);
            emit windowDestroyed(deadWid);
        } else {
            ++it;
        }
    }
}
