#pragma once

#include <QObject>
#include <QString>

#include "rasters.h"

class ColourSwatch;
class QLineEdit;
class QSlider;

// What is typed in a box as a whole number, as C# takes it: spaces around
// it are passed over. False, `value` being 0, if it is not one.
bool typedInt(const QString& text, int& value);
// The same for a number that may have a fraction, written with a point or
// with a comma.
bool typedNumber(const QString& text, double& value);

// The red, green and blue of a colour of the Plus as a window sets them:
// for each a slider from 0 to 15 and a box its number is typed in, and a
// square that shows the colour. A number typed moves its slider if it is
// one from 0 to 15; what the window uses is what the boxes say.
class ShadeControls : public QObject {
    Q_OBJECT

public:
    enum Component { Red, Green, Blue };

    // The controls are made here, without a place: the window puts them
    // where it wants them. They take ConvImgCpc's names: "trk", "txb" and
    // "lbl" with `stem` ("Start", "End"), then "R", "V", "B" or "Color".
    ShadeControls(const QString& stem, QObject* parent);

    QSlider* slider(Component component) const { return sliders_[component]; }
    QLineEdit* box(Component component) const { return boxes_[component]; }
    ColourSwatch* swatch() const { return swatch_; }

    // Writes the numbers in the boxes: the sliders and the square follow.
    void set(int red, int green, int blue);
    // What the boxes say: false if one of them is no number.
    bool read(tuxscr::Shade& shade) const;

signals:
    // A slider was moved, by the mouse or the keyboard and not by its box.
    void moved();

private:
    QSlider* sliders_[3] = {};
    QLineEdit* boxes_[3] = {};
    ColourSwatch* swatch_ = nullptr;
    bool fromBox_ = false;

    void showColour();
};
