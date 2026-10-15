#pragma once

#include <QAbstractButton>
#include <QColor>

// A square of a colour: the colour of a split, clicked to choose another, or
// the one a window is making, only shown. What is written on it (the
// colour's number) is on a plate of the window's own colours, so that it
// reads whatever the colour is; the frame is the window's too.
class ColourSwatch : public QAbstractButton {
    Q_OBJECT

public:
    explicit ColourSwatch(QSize size, QWidget* parent = nullptr);

    void setColour(const QColor& colour);
    QColor colour() const { return colour_; }
    // What is written on it; nothing if empty.
    void setCaption(const QString& caption);
    QString caption() const { return caption_; }
    // A swatch that only shows a colour is not clicked nor reached with the
    // keyboard.
    void setShowOnly(bool showOnly);

    QSize sizeHint() const override { return size_; }
    QSize minimumSizeHint() const override { return size_; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QSize size_;
    QColor colour_ = Qt::black;
    QString caption_;
    bool showOnly_ = false;
};
