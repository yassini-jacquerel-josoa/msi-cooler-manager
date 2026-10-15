#include "InputMask.h"

#include <QQuickItem>
#include <QQuickWindow>
#include <QRegion>

namespace msi::ui {

InputMask::InputMask(QObject *parent)
    : QObject(parent)
{}

void InputMask::update(QQuickItem *item)
{
    if (!item)
        return;

    QQuickWindow *window = item->window();
    if (!window)
        return;

    const QPointF origin = item->mapToScene(QPointF(0, 0));
    const int x = qRound(origin.x());
    const int y = qRound(origin.y());
    const int width = qMax(1, qRound(item->width()));
    const int height = qMax(1, qRound(item->height()));
    window->setMask(QRegion(x, y, width, height));
}

void InputMask::clear(QQuickWindow *window)
{
    if (!window)
        return;

    window->setMask(QRegion(0, 0, qMax(1, window->width()), qMax(1, window->height())));
}

} // namespace msi::ui
