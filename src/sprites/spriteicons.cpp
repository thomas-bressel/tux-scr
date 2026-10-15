#include "spriteicons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

QIcon spriteIcon(SpriteIcon icon, const QColor& ink)
{
    // On a grid of 24, at twice the size a button shows it.
    QPixmap pixmap(48, 48);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(2, 2);
    QPen pen(ink, 1.5);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    // What is behind a sheet does not show through it.
    const auto wipe = [&painter](const QRectF& area) {
        painter.save();
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillRect(area, Qt::black);
        painter.restore();
    };

    switch (icon) {
    case SpriteIcon::Copy: {
        // Two sheets, one over the other.
        const QRectF front(9.5, 7.5, 10.0, 13.0);
        painter.drawRect(QRectF(4.5, 3.5, 10.0, 13.0));
        wipe(front);
        painter.drawRect(front);
        for (const qreal y : {11.5, 14.0, 16.5})
            painter.drawLine(QPointF(12.0, y), QPointF(17.0, y));
        break;
    }
    case SpriteIcon::Paste: {
        // A board with its clip, and the sheet on it.
        painter.drawRoundedRect(QRectF(4.5, 4.5, 15.0, 16.5), 1.5, 1.5);
        painter.setBrush(ink);
        painter.drawRect(QRectF(9.0, 2.5, 6.0, 4.0));
        painter.setBrush(Qt::NoBrush);
        for (const qreal y : {11.0, 14.0, 17.0})
            painter.drawLine(QPointF(8.0, y), QPointF(16.0, y));
        break;
    }
    case SpriteIcon::FlipDown:
    case SpriteIcon::FlipAcross: {
        // A shape and what it becomes on the other side of the line.
        // Across is the same drawing, what goes down going across.
        if (icon == SpriteIcon::FlipAcross)
            painter.setTransform(QTransform(0, 1, 1, 0, 0, 0), true);
        QPen axis(ink, 1.2);
        axis.setDashPattern({2, 2});
        axis.setCapStyle(Qt::FlatCap);
        painter.setPen(axis);
        painter.drawLine(QPointF(2.0, 12.0), QPointF(22.0, 12.0));
        painter.setPen(pen);
        painter.setBrush(ink);
        painter.drawPolygon(QPolygonF({QPointF(5.0, 9.5), QPointF(19.0, 9.5), QPointF(19.0, 3.0)}));
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(QPolygonF({QPointF(5.0, 14.5), QPointF(19.0, 14.5), QPointF(19.0, 21.0)}));
        break;
    }
    case SpriteIcon::Rotate: {
        // Most of a circle, the way a clock's hands go.
        painter.drawArc(QRectF(5.0, 5.5, 14.0, 14.0), 120 * 16, -300 * 16);
        painter.setBrush(ink);
        painter.drawPolygon(QPolygonF({QPointF(5.0, 8.5), QPointF(2.0, 13.5), QPointF(8.0, 13.5)}));
        break;
    }
    case SpriteIcon::Clear: {
        // A rubber, and the line it leaves clean.
        painter.drawLine(QPointF(5.0, 20.5), QPointF(20.0, 20.5));
        painter.translate(12, 11);
        painter.rotate(-40);
        painter.drawRoundedRect(QRectF(-8.0, -3.5, 16.0, 7.0), 1.5, 1.5);
        painter.setBrush(ink);
        painter.drawRoundedRect(QRectF(-8.0, -3.5, 6.0, 7.0), 1.5, 1.5);
        break;
    }
    case SpriteIcon::Load: {
        // A folder, open.
        QPainterPath back;
        back.moveTo(3.5, 19.5);
        back.lineTo(3.5, 5.5);
        back.lineTo(9.0, 5.5);
        back.lineTo(11.0, 7.5);
        back.lineTo(18.5, 7.5);
        back.lineTo(18.5, 11.0);
        painter.drawPath(back);
        painter.drawPolygon(QPolygonF({QPointF(3.5, 19.5), QPointF(7.0, 11.0), QPointF(21.5, 11.0), QPointF(18.0, 19.5)}));
        break;
    }
    case SpriteIcon::Save: {
        // A disc: its shutter and its label.
        painter.drawPolygon(QPolygonF({QPointF(4.5, 4.5), QPointF(16.5, 4.5), QPointF(19.5, 7.5), QPointF(19.5, 19.5), QPointF(4.5, 19.5)}));
        painter.drawRect(QRectF(7.5, 13.0, 9.0, 6.5));
        painter.setBrush(ink);
        painter.drawRect(QRectF(8.0, 4.5, 6.5, 4.5));
        break;
    }
    case SpriteIcon::Previous:
    case SpriteIcon::Next: {
        QPen thick(ink, 2.5);
        thick.setJoinStyle(Qt::MiterJoin);
        thick.setCapStyle(Qt::RoundCap);
        painter.setPen(thick);
        if (icon == SpriteIcon::Previous)
            painter.drawPolyline(QPolygonF({QPointF(15.0, 5.0), QPointF(8.0, 12.0), QPointF(15.0, 19.0)}));
        else
            painter.drawPolyline(QPolygonF({QPointF(9.0, 5.0), QPointF(16.0, 12.0), QPointF(9.0, 19.0)}));
        break;
    }
    }
    return QIcon(pixmap);
}
