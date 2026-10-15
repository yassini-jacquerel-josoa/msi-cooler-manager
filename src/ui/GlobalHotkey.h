#pragma once

#include <QAbstractNativeEventFilter>
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include <xcb/xcb.h>

#include <vector>

namespace msi::ui {

class GlobalHotkey : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey() override;

    bool install();
    bool isActive() const { return m_active; }

signals:
    void holdStarted();
    void toggleRequested();
    void expandRequested();

protected:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    void handlePress();
    void handleRelease();
    void finishRelease();
    void ungrab();
    bool keyPhysicallyDown() const;

    xcb_connection_t *m_conn = nullptr;
    xcb_window_t m_root = 0;
    std::vector<xcb_keycode_t> m_keycodes;
    bool m_active = false;
    bool m_down = false;
    bool m_longFired = false;
    QElapsedTimer m_holdClock;
    QTimer m_holdTimer;
    QTimer m_releaseTimer;
};

} // namespace msi::ui
