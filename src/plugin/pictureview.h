#pragma once

#include <functional>

#include <QImage>
#include <QList>
#include <QWidget>

// The CPC's picture on the PC: a pixel of mode 2 is one pixel across and a
// line two rows, or twice that. What is over the picture (the mark of a
// palette interrupt's line, grids, the sprites, a rectangle being drawn) is
// drawn on top by those who know of it.
class PictureView : public QWidget {
    Q_OBJECT

public:
    explicit PictureView(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    const QImage& image() const { return image_; }
    // Twice the size.
    void setDoubled(bool doubled);
    int scale() const { return doubled_ ? 2 : 1; }

    // The lines where the palette changes, and the one being set: a mark
    // across the picture at each, the current one heavier. A line is the
    // CPC's (two rows of the picture).
    void setInterruptLines(const QList<int>& lines, int current);
    // The marks are not drawn over a picture that is shown enlarged: they
    // would not be at their lines.
    void setMarksVisible(bool visible);
    QList<int> interruptLines() const { return lines_; }
    int currentInterruptLine() const { return currentLine_; }

    // Something more to draw over the picture, in the picture's own pixels:
    // each of those who ask is called in turn. The number returned takes it
    // away again.
    int addOverlay(std::function<void(QPainter&)> overlay);
    void removeOverlay(int overlay);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

signals:
    // The mouse moved or a button changed, at this place of the picture
    // (in its own pixels, whatever its size on the screen).
    void mouse(QPoint at, Qt::MouseButtons buttons);
    void mouseLeft();
    // A key that moves or chooses, when the view has the keyboard.
    void arrow(int dx, int dy, bool choose);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    QImage image_;
    bool doubled_ = false;
    QList<int> lines_;
    int currentLine_ = -1;
    bool marksVisible_ = true;
    QList<std::pair<int, std::function<void(QPainter&)>>> overlays_;
    int nextOverlay_ = 1;

    QPoint pictureAt(const QMouseEvent* event) const;
};
