#pragma once

#include <QQuickPaintedItem>
#include <QImage>
#include <QColor>

namespace msi::ui {

/**
 * Fond verre dépoli : capture l'écran derrière la fenêtre, le floute,
 * puis le teinte. La capture doit se faire tant que la fenêtre est invisible.
 */
class BackdropBlur : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QColor tint READ tint WRITE setTint NOTIFY tintChanged)
    Q_PROPERTY(int cornerRadius READ cornerRadius WRITE setCornerRadius NOTIFY cornerRadiusChanged)

public:
    explicit BackdropBlur(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    QColor tint() const { return m_tint; }
    void setTint(const QColor &tint);

    int cornerRadius() const { return m_cornerRadius; }
    void setCornerRadius(int radius);

    Q_INVOKABLE void capture(qreal x, qreal y, qreal width, qreal height);

signals:
    void tintChanged();
    void cornerRadiusChanged();

private:
    QImage m_image;
    qreal m_dpr = 1;
    QColor m_tint = QColor(255, 255, 255, 130);
    int m_cornerRadius = 32;
};

} // namespace msi::ui
