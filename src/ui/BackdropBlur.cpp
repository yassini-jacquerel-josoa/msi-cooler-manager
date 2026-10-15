#include "BackdropBlur.h"

#include <QGuiApplication>
#include <QPainter>
#include <QQuickWindow>
#include <QPainterPath>
#include <QScreen>
#include <algorithm>

namespace msi::ui {

namespace {

QImage boxBlur(const QImage &source, int radius)
{
    if (radius < 1 || source.isNull())
        return source;

    const QImage src = source.convertToFormat(QImage::Format_ARGB32);
    const int width = src.width();
    const int height = src.height();
    const int span = radius * 2 + 1;
    QImage horizontal(width, height, QImage::Format_ARGB32);
    QImage result(width, height, QImage::Format_ARGB32);

    for (int y = 0; y < height; ++y) {
        const auto *row = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        auto *out = reinterpret_cast<QRgb *>(horizontal.scanLine(y));
        int red = 0;
        int green = 0;
        int blue = 0;
        int alpha = 0;
        for (int i = -radius; i <= radius; ++i) {
            const QRgb pixel = row[std::clamp(i, 0, width - 1)];
            red += qRed(pixel);
            green += qGreen(pixel);
            blue += qBlue(pixel);
            alpha += qAlpha(pixel);
        }
        for (int x = 0; x < width; ++x) {
            out[x] = qRgba(red / span, green / span, blue / span, alpha / span);
            const QRgb add = row[std::clamp(x + radius + 1, 0, width - 1)];
            const QRgb sub = row[std::clamp(x - radius, 0, width - 1)];
            red += qRed(add) - qRed(sub);
            green += qGreen(add) - qGreen(sub);
            blue += qBlue(add) - qBlue(sub);
            alpha += qAlpha(add) - qAlpha(sub);
        }
    }

    for (int x = 0; x < width; ++x) {
        int red = 0;
        int green = 0;
        int blue = 0;
        int alpha = 0;
        auto sample = [&](int y) {
            const auto *row = reinterpret_cast<const QRgb *>(horizontal.constScanLine(std::clamp(y, 0, height - 1)));
            return row[x];
        };
        for (int i = -radius; i <= radius; ++i) {
            const QRgb pixel = sample(i);
            red += qRed(pixel);
            green += qGreen(pixel);
            blue += qBlue(pixel);
            alpha += qAlpha(pixel);
        }
        for (int y = 0; y < height; ++y) {
            auto *out = reinterpret_cast<QRgb *>(result.scanLine(y));
            out[x] = qRgba(red / span, green / span, blue / span, alpha / span);
            const QRgb add = sample(y + radius + 1);
            const QRgb sub = sample(y - radius);
            red += qRed(add) - qRed(sub);
            green += qGreen(add) - qGreen(sub);
            blue += qBlue(add) - qBlue(sub);
            alpha += qAlpha(add) - qAlpha(sub);
        }
    }

    return result;
}

QImage soften(const QImage &source)
{
    const int smallWidth = std::max(1, source.width() / 5);
    const int smallHeight = std::max(1, source.height() / 5);
    QImage small = source.scaled(smallWidth, smallHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    small = boxBlur(small, 4);
    small = boxBlur(small, 2);

    for (int y = 0; y < small.height(); ++y) {
        auto *row = reinterpret_cast<QRgb *>(small.scanLine(y));
        for (int x = 0; x < small.width(); ++x) {
            const int red = qRed(row[x]);
            const int green = qGreen(row[x]);
            const int blue = qBlue(row[x]);
            const int gray = (red * 77 + green * 150 + blue * 29) >> 8;
            const auto boost = [gray](int channel) {
                return std::clamp(gray + ((channel - gray) * 5) / 4, 0, 255);
            };
            row[x] = qRgba(boost(red), boost(green), boost(blue), 255);
        }
    }

    return small.scaled(source.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

} // namespace

BackdropBlur::BackdropBlur(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setOpaquePainting(false);
}

void BackdropBlur::setTint(const QColor &tint)
{
    if (m_tint == tint)
        return;
    m_tint = tint;
    emit tintChanged();
    update();
}

void BackdropBlur::setCornerRadius(int radius)
{
    if (m_cornerRadius == radius)
        return;
    m_cornerRadius = radius;
    emit cornerRadiusChanged();
    update();
}

void BackdropBlur::capture(qreal x, qreal y, qreal width, qreal height)
{
    const int grabWidth = std::max(1, qRound(width));
    const int grabHeight = std::max(1, qRound(height));
    const QPoint topLeft(qRound(x), qRound(y));
    QScreen *screen = window() ? window()->screen() : nullptr;
    if (!screen)
        screen = QGuiApplication::screenAt(topLeft);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const QRect screenRect = screen->geometry();
    const QRect requested(topLeft, QSize(grabWidth, grabHeight));
    const QRect clipped = requested.intersected(screenRect);
    if (clipped.isEmpty())
        return;

    const QPoint local = clipped.topLeft() - screenRect.topLeft();
    const QPixmap shot = screen->grabWindow(0, local.x(), local.y(), clipped.width(), clipped.height());
    if (shot.isNull())
        return;

    m_dpr = shot.devicePixelRatio() > 0 ? shot.devicePixelRatio() : 1;
    m_image = soften(shot.toImage());
    m_image.setDevicePixelRatio(1);
    update();
}

void BackdropBlur::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QRectF bounds(0, 0, width(), height());
    QPainterPath path;
    path.addRoundedRect(bounds, m_cornerRadius, m_cornerRadius);
    painter->setClipPath(path);

    if (!m_image.isNull()) {
        const QPointF scene = mapToScene(QPointF(0, 0));
        const QRectF source(scene.x() * m_dpr,
                            scene.y() * m_dpr,
                            width() * m_dpr,
                            height() * m_dpr);
        painter->drawImage(bounds, m_image, source);
    }
    painter->fillRect(bounds, m_tint);
}

} // namespace msi::ui
