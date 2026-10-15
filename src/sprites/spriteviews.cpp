#include "spriteviews.h"

#include <algorithm>

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>

#include "contrast.h"
#include "pictures.h"
#include "spritefiles.h"

using namespace tuxscr;

namespace {

// A pixel of a sprite, in its colour of the sprites' palette.
QColor pixelColour(const Screen& screen, int bank, int sprite, int x, int y)
{
    return spriteColour(screen.spritePalette[screen.sprites[bank][sprite][x][y] & 0x0F]);
}

// What parts the pixels of a sprite from each other: a grey of the look in
// use, which shows on the window and on black and white pixels alike.
QColor gapColour(const QPalette& look)
{
    return readable(look.color(QPalette::Mid), look.color(QPalette::Window), kGraphicContrast);
}

// A whole number of `size` in `value`, going down below zero too.
int cellOf(int value, int size)
{
    return value >= 0 ? value / size : -1;
}

}  // namespace

void spriteMarkLine(QPainter& painter, const QPointF& from, qreal length, bool across, qreal thick)
{
    // The two colours that stand out most from each other: one of them
    // stands out from whatever is under the mark.
    const QColor paper = inkOn(Qt::black);
    const QColor ink = inkOn(paper);
    painter.fillRect(across ? QRectF(from.x(), from.y(), length, thick) : QRectF(from.x(), from.y(), thick, length), paper);
    QPen dashes(ink, thick);
    dashes.setDashPattern({4, 4});
    dashes.setCapStyle(Qt::FlatCap);
    painter.setPen(dashes);
    const qreal half = thick / 2;
    if (across)
        painter.drawLine(QPointF(from.x(), from.y() + half), QPointF(from.x() + length, from.y() + half));
    else
        painter.drawLine(QPointF(from.x() + half, from.y()), QPointF(from.x() + half, from.y() + length));
}

void spriteMarkFrame(QPainter& painter, const QRectF& frame, qreal thick)
{
    spriteMarkLine(painter, frame.topLeft(), frame.width(), true, thick);
    spriteMarkLine(painter, QPointF(frame.left(), frame.bottom() - thick), frame.width(), true, thick);
    spriteMarkLine(painter, frame.topLeft(), frame.height(), false, thick);
    spriteMarkLine(painter, QPointF(frame.right() - thick, frame.top()), frame.height(), false, thick);
}

// ---- The sprites of a bank ----

SpriteStrip::SpriteStrip(const Screen* screen, QWidget* parent)
    : QWidget(parent)
    , screen_(screen)
{
    // The sprites, and under them the bar of the one being drawn.
    setFixedSize(16 * kSpriteWidth, kSpriteWidth + 6);
}

void SpriteStrip::setBank(int bank)
{
    bank_ = bank;
    update();
}

void SpriteStrip::setCurrent(int sprite)
{
    current_ = sprite;
    update();
}

void SpriteStrip::setMarked(int sprite)
{
    marked_ = sprite;
    update();
}

void SpriteStrip::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Window));
    painter.fillRect(0, 0, width(), kSpriteWidth, gapColour(palette()));
    for (int sprite = 0; sprite < kSpritesPerBank; ++sprite)
        for (int y = 0; y < kSpriteSize; ++y)
            for (int x = 0; x < kSpriteSize; ++x)
                // The last column is a pixel short: that parts the sprites.
                painter.fillRect((x + sprite * kSpriteSize) * kZoom, y * kZoom, x == kSpriteSize - 1 ? kZoom - 1 : kZoom, kZoom,
                                 pixelColour(*screen_, bank_, sprite, x, y));
    if (marked_ >= 0)
        spriteMarkFrame(painter, QRectF(marked_ * kSpriteWidth, 0, kSpriteWidth - 1, kSpriteWidth), 2);
    painter.fillRect(current_ * kSpriteWidth, kSpriteWidth + 1, kSpriteWidth - 1, 4, palette().color(QPalette::WindowText));
}

void SpriteStrip::mousePressEvent(QMouseEvent* event)
{
    const int sprite = cellOf(event->position().toPoint().x(), kSpriteWidth);
    if (sprite < 0 || sprite >= kSpritesPerBank)
        return;
    if (event->button() == Qt::LeftButton)
        emit chosen(sprite);
    else if (event->button() == Qt::RightButton)
        emit pointed(sprite);
}

// ---- The sprite being drawn ----

SpriteGrid::SpriteGrid(const Screen* screen, QWidget* parent)
    : QWidget(parent)
    , screen_(screen)
{
    setFixedSize(kSpriteSize * kCell, kSpriteSize * kCell);
    setMouseTracking(true);
}

void SpriteGrid::setSprite(int bank, int sprite)
{
    bank_ = bank;
    sprite_ = sprite;
    update();
}

void SpriteGrid::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), gapColour(palette()));
    for (int y = 0; y < kSpriteSize; ++y)
        for (int x = 0; x < kSpriteSize; ++x)
            painter.fillRect(x * kCell, y * kCell, kCell - 2, kCell - 2, pixelColour(*screen_, bank_, sprite_, x, y));
}

void SpriteGrid::mousePressEvent(QMouseEvent* event)
{
    const QPoint at = event->position().toPoint();
    emit pointed(QPoint(cellOf(at.x(), kCell), cellOf(at.y(), kCell)), event->buttons());
}

void SpriteGrid::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint at = event->position().toPoint();
    emit pointed(QPoint(cellOf(at.x(), kCell), cellOf(at.y(), kCell)), event->buttons());
}

void SpriteGrid::mouseReleaseEvent(QMouseEvent* event)
{
    const QPoint at = event->position().toPoint();
    emit pointed(QPoint(cellOf(at.x(), kCell), cellOf(at.y(), kCell)), event->buttons());
}

void SpriteGrid::leaveEvent(QEvent*)
{
    emit left();
}

void SpriteGrid::contextMenuEvent(QContextMenuEvent* event)
{
    // The right button rubs out.
    event->accept();
}

// ---- Where sprites are tried ----

SpriteTestView::SpriteTestView(QWidget* parent)
    : QWidget(parent)
    , image_(kSize, kSize, QImage::Format_RGB32)
{
    image_.fill(Qt::black);
    setFixedSize(kSize, kSize);
    setMouseTracking(true);
}

void SpriteTestView::setFrame(const QRect& frame)
{
    if (frame_ == frame)
        return;
    frame_ = frame;
    update();
}

void SpriteTestView::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.drawImage(0, 0, image_);
    if (!frame_.isNull())
        spriteMarkFrame(painter, QRectF(frame_), 1);
}

void SpriteTestView::mousePressEvent(QMouseEvent* event)
{
    emit pressed(event->position().toPoint());
}

void SpriteTestView::mouseMoveEvent(QMouseEvent* event)
{
    emit hovered(event->position().toPoint());
}

void SpriteTestView::leaveEvent(QEvent*)
{
    emit left();
}

// ---- What was captured ----

SpriteCaptureView::SpriteCaptureView(const Screen* screen, QWidget* parent)
    : QWidget(parent)
    , screen_(screen)
{
    setFixedSize(kSize, kSize);
    clear(1, 1);
}

void SpriteCaptureView::clear(int across, int down)
{
    across_ = std::max(1, across);
    down_ = std::max(1, down);
    pens_.assign(static_cast<size_t>(across_ * kSpriteSize) * static_cast<size_t>(down_ * kSpriteSize), kNothing);
    update();
}

void SpriteCaptureView::setPen(int x, int y, int pen)
{
    pens_[static_cast<size_t>(y * across_ * kSpriteSize + x)] = pen;
    update();
}

int SpriteCaptureView::cell() const
{
    return std::clamp(kSize / (kSpriteSize * std::max(across_, down_)), 1, 8);
}

void SpriteCaptureView::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    // What a CPC shows where there is nothing.
    painter.fillRect(rect(), Qt::black);
    const int side = cell();
    for (int y = 0; y < down_ * kSpriteSize; ++y)
        for (int x = 0; x < across_ * kSpriteSize; ++x) {
            const int value = pen(x, y);
            if (value != kNothing)
                painter.fillRect(x * side, y * side, side, side, toQColor(screen_->rgb(screen_->palette[static_cast<size_t>(value & 0x0F)])));
        }
}

// ---- A pen in its place ----

PenHolder::PenHolder(QWidget* pen, QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(pen, 0, Qt::AlignCenter);
    setFixedSize(48, 40);
}

void PenHolder::setFramed(bool framed)
{
    if (framed_ == framed)
        return;
    framed_ = framed;
    update();
}

void PenHolder::paintEvent(QPaintEvent*)
{
    if (!framed_)
        return;
    QPainter painter(this);
    QPen frame(palette().color(QPalette::WindowText), 3);
    frame.setJoinStyle(Qt::MiterJoin);
    painter.setPen(frame);
    painter.drawRect(QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5));
}
