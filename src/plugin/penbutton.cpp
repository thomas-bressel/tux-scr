#include "penbutton.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOptionFocusRect>

#include "contrast.h"

PenButton::PenButton(int pen, QWidget* parent)
    : QAbstractButton(parent)
    , pen_(pen)
{
    setFocusPolicy(Qt::TabFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void PenButton::setColour(const QColor& colour)
{
    if (colour_ == colour)
        return;
    colour_ = colour;
    update();
}

void PenButton::setMark(Mark mark)
{
    if (mark_ == mark)
        return;
    mark_ = mark;
    update();
}

void PenButton::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        emit rightClicked();
        return;
    }
    QAbstractButton::mousePressEvent(event);
}

void PenButton::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        emit rightClicked();
        return;
    }
    QAbstractButton::keyPressEvent(event);
}

void PenButton::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    const QPalette look = palette();
    const QRect square = rect().adjusted(0, 0, -1, -1);
    painter.fillRect(square, isEnabled() ? colour_ : look.color(QPalette::Window));
    // The frame stands out from the window whatever the pen's colour.
    painter.setPen(QPen(readable(look.color(QPalette::Mid), look.color(QPalette::Window), kGraphicContrast), 1));
    painter.drawRect(square);
    if (!isEnabled())
        return;

    // The pen's number, on a plate.
    QFont small = font();
    small.setPointSizeF(std::max(7.0, small.pointSizeF() - 1));
    small.setBold(true);
    painter.setFont(small);
    const QString number = QString::number(pen_);
    const QSize textSize = painter.fontMetrics().size(Qt::TextSingleLine, number);
    QRect plate(0, 0, textSize.width() + 6, textSize.height());
    plate.moveCenter(square.center());
    painter.setPen(Qt::NoPen);
    painter.setBrush(look.color(QPalette::Base));
    painter.drawRoundedRect(plate, 3, 3);
    painter.setPen(look.color(QPalette::Text));
    painter.drawText(plate, Qt::AlignCenter, number);

    if (mark_ != Mark::None) {
        const QRect corner(square.right() - 10, square.top() + 2, 9, 9);
        painter.setPen(Qt::NoPen);
        painter.setBrush(look.color(QPalette::Base));
        painter.drawRect(corner);
        painter.setRenderHint(QPainter::Antialiasing);
        const QColor ink = look.color(QPalette::Text);
        if (mark_ == Mark::Tick) {
            painter.setPen(QPen(ink, 1.6));
            painter.drawPolyline(QPolygonF({QPointF(corner.left() + 1.5, corner.center().y() + 0.5),
                                            QPointF(corner.left() + 3.5, corner.bottom() - 1.5),
                                            QPointF(corner.right() - 1.0, corner.top() + 1.5)}));
        } else {
            painter.setBrush(ink);
            painter.drawEllipse(QRectF(corner).adjusted(2, 2, -2, -2));
        }
        painter.setRenderHint(QPainter::Antialiasing, false);
    }

    if (hasFocus()) {
        QStyleOptionFocusRect option;
        option.initFrom(this);
        option.rect = rect().adjusted(1, 1, -1, -1);
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, &painter, this);
    }
}
