#include "pictureview.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

PictureView::PictureView(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void PictureView::setImage(const QImage& image)
{
    const bool resized = image.size() != image_.size();
    image_ = image;
    if (resized)
        updateGeometry();
    update();
}

void PictureView::setDoubled(bool doubled)
{
    if (doubled_ == doubled)
        return;
    doubled_ = doubled;
    updateGeometry();
    update();
}

void PictureView::setInterruptLines(const QList<int>& lines, int current)
{
    lines_ = lines;
    currentLine_ = current;
    update();
}

void PictureView::setMarksVisible(bool visible)
{
    marksVisible_ = visible;
    update();
}

int PictureView::addOverlay(std::function<void(QPainter&)> overlay)
{
    overlays_.append({nextOverlay_, std::move(overlay)});
    update();
    return nextOverlay_++;
}

void PictureView::removeOverlay(int overlay)
{
    overlays_.removeIf([overlay](const auto& one) { return one.first == overlay; });
    update();
}

QSize PictureView::sizeHint() const
{
    return image_.isNull() ? QSize(640, 400) * scale() : image_.size() * scale();
}

void PictureView::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    if (image_.isNull())
        return;
    painter.scale(scale(), scale());
    painter.drawImage(0, 0, image_);
    for (const auto& overlay : overlays_) {
        painter.save();
        overlay.second(painter);
        painter.restore();
    }

    // A mark is black and white in turn: it shows on any colour.
    const auto mark = [&](int line, bool current) {
        const qreal y = line * 2;
        const qreal thick = current ? 2 : 1;
        painter.fillRect(QRectF(0, y - thick, image_.width(), thick), Qt::white);
        QPen dashes(Qt::black, thick);
        dashes.setDashPattern({4, 4});
        dashes.setCapStyle(Qt::FlatCap);
        painter.setPen(dashes);
        painter.drawLine(QPointF(0, y - thick / 2), QPointF(image_.width(), y - thick / 2));
        if (current) {
            // The line's number, on a plate, at the left.
            const QString text = QString::number(line);
            QFont small = font();
            small.setBold(true);
            painter.setFont(small);
            const QSize size = painter.fontMetrics().size(Qt::TextSingleLine, text) + QSize(8, 2);
            QRectF plate(2, y - thick - size.height() - 1, size.width(), size.height());
            if (plate.top() < 0)
                plate.moveTop(y + 1);
            painter.setPen(Qt::black);
            painter.setBrush(Qt::white);
            painter.drawRect(plate);
            painter.drawText(plate, Qt::AlignCenter, text);
        }
    };
    if (!marksVisible_)
        return;
    for (const int line : lines_)
        if (line != currentLine_)
            mark(line, false);
    if (currentLine_ >= 0)
        mark(currentLine_, true);
}

// The pixel of the picture the mouse is in: at twice the size, two pixels
// of the screen are one of the picture (a division of a QPoint would round
// to the nearest).
QPoint PictureView::pictureAt(const QMouseEvent* event) const
{
    const QPoint at = event->position().toPoint();
    return QPoint(at.x() / scale(), at.y() / scale());
}

void PictureView::mousePressEvent(QMouseEvent* event)
{
    emit mouse(pictureAt(event), event->buttons());
}

void PictureView::mouseMoveEvent(QMouseEvent* event)
{
    emit mouse(pictureAt(event), event->buttons());
}

void PictureView::mouseReleaseEvent(QMouseEvent* event)
{
    emit mouse(pictureAt(event), event->buttons());
}

void PictureView::leaveEvent(QEvent*)
{
    emit mouseLeft();
}

void PictureView::contextMenuEvent(QContextMenuEvent* event)
{
    // The right button is one of the picture's tools.
    event->accept();
}

void PictureView::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Left:
        emit arrow(-1, 0, false);
        return;
    case Qt::Key_Right:
        emit arrow(1, 0, false);
        return;
    case Qt::Key_Up:
        emit arrow(0, -1, false);
        return;
    case Qt::Key_Down:
        emit arrow(0, 1, false);
        return;
    case Qt::Key_Space:
        emit arrow(0, 0, true);
        return;
    default:
        QWidget::keyPressEvent(event);
    }
}
