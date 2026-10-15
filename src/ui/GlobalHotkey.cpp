#include "GlobalHotkey.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QtGui/qguiapplication_platform.h>

#include <X11/XKBlib.h>

#include <cstdlib>

#undef None
#undef KeyPress
#undef KeyRelease
#undef FocusIn
#undef FocusOut

namespace msi::ui {

namespace {

constexpr xcb_keysym_t kF7 = 0xffc4;

const uint16_t kModifiers[] = {
    0,
    XCB_MOD_MASK_LOCK,
    XCB_MOD_MASK_2,
    static_cast<uint16_t>(XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2),
    XCB_MOD_MASK_1,
    static_cast<uint16_t>(XCB_MOD_MASK_1 | XCB_MOD_MASK_LOCK),
    static_cast<uint16_t>(XCB_MOD_MASK_1 | XCB_MOD_MASK_2),
    static_cast<uint16_t>(XCB_MOD_MASK_1 | XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2),
};

} // namespace

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
{
    m_holdTimer.setSingleShot(true);
    m_releaseTimer.setSingleShot(true);
    connect(&m_holdTimer, &QTimer::timeout, this, [this]() {
        m_longFired = true;
        emit expandRequested();
    });
    connect(&m_releaseTimer, &QTimer::timeout, this, [this]() { finishRelease(); });
}

GlobalHotkey::~GlobalHotkey()
{
    if (m_active && qApp)
        qApp->removeNativeEventFilter(this);
    ungrab();
}

bool GlobalHotkey::install()
{
    auto *x11 = qApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11)
        return false;

    m_conn = x11->connection();
    if (!m_conn)
        return false;

    const xcb_setup_t *setup = xcb_get_setup(m_conn);
    if (!setup)
        return false;

    xcb_screen_iterator_t screens = xcb_setup_roots_iterator(setup);
    if (!screens.data)
        return false;
    m_root = screens.data->root;

    const int keycodeCount = setup->max_keycode - setup->min_keycode + 1;
    if (keycodeCount <= 0 || keycodeCount > 255)
        return false;

    xcb_get_keyboard_mapping_reply_t *mapping = xcb_get_keyboard_mapping_reply(
        m_conn,
        xcb_get_keyboard_mapping(m_conn, setup->min_keycode, static_cast<uint8_t>(keycodeCount)),
        nullptr);
    if (!mapping)
        return false;

    const xcb_keysym_t *keysyms = xcb_get_keyboard_mapping_keysyms(mapping);
    const int perCode = mapping->keysyms_per_keycode;
    for (int index = 0; index < keycodeCount; ++index) {
        for (int slot = 0; slot < perCode; ++slot) {
            if (keysyms[index * perCode + slot] == kF7) {
                m_keycodes.push_back(static_cast<xcb_keycode_t>(setup->min_keycode + index));
                break;
            }
        }
    }
    std::free(mapping);

    if (m_keycodes.empty())
        return false;

    bool grabbed = false;
    for (xcb_keycode_t code : m_keycodes) {
        for (uint16_t modifier : kModifiers) {
            xcb_void_cookie_t cookie = xcb_grab_key_checked(
                m_conn, 0, m_root, modifier, code, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
            xcb_generic_error_t *error = xcb_request_check(m_conn, cookie);
            if (error) {
                std::free(error);
                continue;
            }
            grabbed = true;
        }
    }
    xcb_flush(m_conn);

    if (Display *display = x11->display()) {
        Bool supported = False;
        XkbSetDetectableAutoRepeat(display, True, &supported);
    }

    if (!grabbed) {
        qWarning() << "Impossible de capturer F7";
        return false;
    }

    qApp->installNativeEventFilter(this);
    m_active = true;
    return true;
}

bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *)
{
    if (!m_active || eventType != QByteArrayLiteral("xcb_generic_event_t") || !message)
        return false;

    auto *event = static_cast<xcb_generic_event_t *>(message);
    const uint8_t type = event->response_type & 0x7f;
    if (type != XCB_KEY_PRESS && type != XCB_KEY_RELEASE)
        return false;

    auto *keyEvent = reinterpret_cast<xcb_key_press_event_t *>(event);
    bool ours = false;
    for (xcb_keycode_t code : m_keycodes) {
        if (keyEvent->detail == code) {
            ours = true;
            break;
        }
    }
    if (!ours)
        return false;

    if (type == XCB_KEY_PRESS)
        handlePress();
    else
        handleRelease();
    return true;
}

bool GlobalHotkey::keyPhysicallyDown() const
{
    if (!m_conn)
        return false;

    xcb_query_keymap_reply_t *map = xcb_query_keymap_reply(m_conn, xcb_query_keymap(m_conn), nullptr);
    if (!map)
        return false;

    bool down = false;
    for (xcb_keycode_t code : m_keycodes) {
        if (map->keys[code / 8] & (1u << (code % 8))) {
            down = true;
            break;
        }
    }
    std::free(map);
    return down;
}

void GlobalHotkey::handlePress()
{
    if (m_releaseTimer.isActive()) {
        m_releaseTimer.stop();
        return;
    }
    if (m_down)
        return;

    m_down = true;
    m_longFired = false;
    m_holdClock.start();
    m_holdTimer.start(2000);
    emit holdStarted();
}

void GlobalHotkey::handleRelease()
{
    if (!m_down)
        return;
    // La répétition X11 envoie Relâche puis Presse alors que la touche est encore enfoncée.
    if (keyPhysicallyDown())
        return;
    m_releaseTimer.start(80);
}

void GlobalHotkey::finishRelease()
{
    if (keyPhysicallyDown()) {
        m_down = true;
        return;
    }

    const bool heldLong = m_longFired || (m_holdClock.isValid() && m_holdClock.elapsed() >= 2000);
    const bool alreadyExpanded = m_longFired;
    m_holdTimer.stop();
    m_holdClock.invalidate();
    m_down = false;
    m_longFired = false;
    if (heldLong) {
        if (!alreadyExpanded)
            emit expandRequested();
    } else {
        emit toggleRequested();
    }
}

void GlobalHotkey::ungrab()
{
    if (!m_conn || m_root == 0)
        return;

    for (xcb_keycode_t code : m_keycodes)
        xcb_ungrab_key(m_conn, code, m_root, XCB_MOD_MASK_ANY);
    xcb_flush(m_conn);
    m_keycodes.clear();
    m_active = false;
}

} // namespace msi::ui
