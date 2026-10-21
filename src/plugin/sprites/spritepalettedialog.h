#pragma once

#include <array>

#include <QDialog>

class QLabel;
class QSlider;
class QSpinBox;

namespace tuxscr {

// The maker of palettes as the sprite editor uses it (ConvImgCpc's
// GenPalette): shades from one colour to another, put in the sprites'
// palette from a pen to another. Moving a slider shows the shades at once;
// "Generate" makes them from what the boxes say. The window stays until it
// is closed. Pen 0 is the sprites' transparent one, and is left alone.
class SpritePaletteDialog : public QDialog {
    Q_OBJECT

public:
    // `firstPen` is the lowest pen the shades may start at.
    SpritePaletteDialog(std::array<int, 16>* palette, int firstPen, QWidget* parent = nullptr);

signals:
    // The palette was made again.
    void paletteChanged();

private:
    std::array<int, 16>* palette_;
    int firstPen_;
    bool setting_ = false;  // a slider is being set from its box
    QSpinBox* from_;
    QSpinBox* to_;
    // Red, green, blue of the first colour, then of the last.
    QSlider* sliders_[6];
    QSpinBox* boxes_[6];
    QLabel* samples_[2];
    QLabel* error_;

    void showSample(int end);
    void makePalette();
    void takeColour();
};

}  // namespace tuxscr
