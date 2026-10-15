#pragma once

#include <QAbstractButton>
#include <QColor>

// A pen of a palette: a square of its colour with its number on it. The
// number is on a plate of the window's own colours, so that it reads
// whatever the pen's colour is. A mark in a corner says something more of
// the pen: that the picture uses it, or that a line gives it a colour of
// its own; it is a shape, not a hue.
class PenButton : public QAbstractButton {
    Q_OBJECT

public:
    enum class Mark { None, Tick, Dot };

    explicit PenButton(int pen, QWidget* parent = nullptr);

    int pen() const { return pen_; }
    void setColour(const QColor& colour);
    QColor colour() const { return colour_; }
    void setMark(Mark mark);
    Mark mark() const { return mark_; }

    QSize sizeHint() const override { return {40, 30}; }
    QSize minimumSizeHint() const override { return {30, 26}; }

signals:
    // Clicked with the right button, or Delete pressed on it: what the
    // right button does is within reach of the keyboard too.
    void rightClicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    int pen_;
    QColor colour_ = Qt::black;
    Mark mark_ = Mark::None;
};
