#include "icons.h"

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPixmap>

QIcon screenIcon()
{
    const QColor ink = QApplication::palette().color(QPalette::WindowText);
    // On a grid of 20, at twice the size a button shows it.
    QPixmap pixmap(40, 40);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(2, 2);
    QPen pen(ink, 1.5);
    pen.setJoinStyle(Qt::MiterJoin);
    painter.setPen(pen);
    // The screen, and its foot.
    painter.drawRect(QRectF(2.5, 3.5, 15.0, 11.0));
    painter.drawLine(QPointF(7.0, 17.5), QPointF(13.0, 17.5));
    painter.drawLine(QPointF(10.0, 14.5), QPointF(10.0, 17.5));
    // The line where the colours change: what is under it is filled.
    painter.setPen(Qt::NoPen);
    painter.setBrush(ink);
    painter.drawRect(QRectF(4.5, 9.5, 11.0, 3.0));
    painter.drawRect(QRectF(4.5, 5.5, 3.0, 2.5));
    painter.drawRect(QRectF(9.5, 5.5, 3.0, 2.5));
    return QIcon(pixmap);
}
