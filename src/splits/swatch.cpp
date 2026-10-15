#include "swatch.h"

#include <QPainter>

#include "contrast.h"

ColourSwatch::ColourSwatch(QSize size, QWidget* parent)
    : QAbstractButton(parent)
    , size_(size)
{
    setFocusPolicy(Qt::TabFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void ColourSwatch::setColour(const QColor& colour)
{
    if (colour_ == colour)
        return;
    colour_ = colour;
    update();
}

void ColourSwatch::setCaption(const QString& caption)
{
    if (caption_ == caption)
        return;
    caption_ = caption;
    update();
}

void ColourSwatch::setShowOnly(bool showOnly)
{
    showOnly_ = showOnly;
    setFocusPolicy(showOnly ? Qt::NoFocus : Qt::TabFocus);
    setAttribute(Qt::WA_TransparentForMouseEvents, showOnly);
}

void ColourSwatch::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    const QPalette look = palette();
    const QRect square = rect().adjusted(0, 0, -1, -1);
    painter.fillRect(square, colour_);
    // The frame stands out from the window whatever the colour is.
    painter.setPen(QPen(readable(look.color(QPalette::Mid), look.color(QPalette::Window), kGraphicContrast), 1));
    painter.drawRect(square);

    if (!caption_.isEmpty()) {
        QFont small = font();
        small.setPointSizeF(std::max(7.0, small.pointSizeF() - 1));
        small.setBold(true);
        painter.setFont(small);
        const QSize textSize = painter.fontMetrics().size(Qt::TextSingleLine, caption_);
        QRect plate(0, 0, textSize.width() + 6, textSize.height());
        plate.moveCenter(square.center());
        painter.setPen(Qt::NoPen);
        painter.setBrush(look.color(QPalette::Base));
        painter.drawRoundedRect(plate, 3, 3);
        painter.setPen(look.color(QPalette::Text));
        painter.drawText(plate, Qt::AlignCenter, caption_);
    }

    // The keyboard is on it: a ring of the window's two colours, of which one
    // at least stands out from any colour.
    if (hasFocus() && !showOnly_) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(look.color(QPalette::Base), 1));
        painter.drawRect(square.adjusted(1, 1, -1, -1));
        painter.setPen(QPen(look.color(QPalette::Text), 1));
        painter.drawRect(square.adjusted(2, 2, -2, -2));
    }
}
