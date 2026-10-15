#pragma once

#include <QObject>

class QQuickItem;
class QQuickWindow;

namespace msi::ui {

class InputMask : public QObject
{
    Q_OBJECT

public:
    explicit InputMask(QObject *parent = nullptr);

    Q_INVOKABLE void update(QQuickItem *item);
    Q_INVOKABLE void clear(QQuickWindow *window);
};

} // namespace msi::ui
