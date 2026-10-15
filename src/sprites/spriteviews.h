#pragma once

#include <vector>

#include <QImage>
#include <QWidget>

#include "engine/screen.h"

class QPainter;

// What the sprites' windows draw themselves: the sixteen sprites of a bank
// side by side, the sprite being drawn as a grid of large pixels, the place
// where sprites are tried at the sizes the Plus gives them, and what was
// captured from the picture.

// A rectangle that shows on any colour: white, with black dashes on it. It
// is drawn inside `frame`, `thick` wide.
void spriteMarkFrame(QPainter& painter, const QRectF& frame, qreal thick = 1);
// A line of the same kind, from a point to the right or down.
void spriteMarkLine(QPainter& painter, const QPointF& from, qreal length, bool across, qreal thick = 1);

// The sprites of a bank (ConvImgCpc's pictAllSprites): a pixel is four
// across and four down. A bar under one says it is the one being drawn; a
// frame around one says it is waiting to change places with another.
class SpriteStrip : public QWidget {
    Q_OBJECT

public:
    static constexpr int kZoom = 4;
    static constexpr int kSpriteWidth = 16 * kZoom;

    explicit SpriteStrip(const tuxscr::Screen* screen, QWidget* parent = nullptr);

    void setBank(int bank);
    void setCurrent(int sprite);
    void setMarked(int sprite);
    int marked() const { return marked_; }

signals:
    // A sprite was clicked: with the left button, or with the right one.
    void chosen(int sprite);
    void pointed(int sprite);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    const tuxscr::Screen* screen_;
    int bank_ = 0;
    int current_ = 0;
    int marked_ = -1;
};

// The sprite being drawn (pictEditSprite): each of its pixels is a square of
// 40, the last two of them left to what is behind, which makes the grid.
class SpriteGrid : public QWidget {
    Q_OBJECT

public:
    static constexpr int kCell = 40;

    explicit SpriteGrid(const tuxscr::Screen* screen, QWidget* parent = nullptr);

    void setSprite(int bank, int sprite);

signals:
    // The mouse is at this pixel of the sprite (which may be outside it),
    // with these buttons down.
    void pointed(QPoint pixel, Qt::MouseButtons buttons);
    void left();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    const tuxscr::Screen* screen_;
    int bank_ = 0;
    int sprite_ = 0;
};

// Where sprites are tried (pictTest): a picture of 512 by 512 the editor
// draws on, and over it the frame of the place a sprite is about to be put
// at.
class SpriteTestView : public QWidget {
    Q_OBJECT

public:
    static constexpr int kSize = 512;

    explicit SpriteTestView(QWidget* parent = nullptr);

    QImage& image() { return image_; }
    const QImage& image() const { return image_; }
    // The frame that follows the mouse; a null one takes it away.
    void setFrame(const QRect& frame);
    QRect frame() const { return frame_; }

signals:
    void hovered(QPoint at);
    void pressed(QPoint at);
    void left();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QImage image_;
    QRect frame_;
};

// What was captured from the picture (pictCapture): the pens of so many
// sprites across and down, each pixel a square of eight, or less when that
// would not fit. A pixel nothing was captured for yet is black.
class SpriteCaptureView : public QWidget {
    Q_OBJECT

public:
    static constexpr int kSize = 512;
    static constexpr int kNothing = -1;

    explicit SpriteCaptureView(const tuxscr::Screen* screen, QWidget* parent = nullptr);

    // Forgets what was captured, for so many sprites across and down.
    void clear(int across, int down);
    int across() const { return across_; }
    int down() const { return down_; }
    // The pen of a pixel, counted from the first sprite's corner; kNothing
    // if none was captured there.
    int pen(int x, int y) const { return pens_[static_cast<size_t>(y * across_ * 16 + x)]; }
    void setPen(int x, int y, int pen);
    // The side of a pixel as it is drawn.
    int cell() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    const tuxscr::Screen* screen_;
    int across_ = 1;
    int down_ = 1;
    std::vector<int> pens_;
};

// A pen of the sprites' palette in its place, with a frame around it when
// the pixel under the mouse is of that pen.
class PenHolder : public QWidget {
    Q_OBJECT

public:
    explicit PenHolder(QWidget* pen, QWidget* parent = nullptr);

    void setFramed(bool framed);
    bool framed() const { return framed_; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    bool framed_ = false;
};
