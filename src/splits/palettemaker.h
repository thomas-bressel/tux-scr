#pragma once

#include <functional>

#include <QDialog>

#include "base/bitmap.h"

class QLabel;
class QLineEdit;
class QPushButton;
class ShadeControls;

// The maker of palettes (ConvImgCpc's GenPalette): from a colour to
// another, the colours in between, each of red, green and blue going evenly
// from the first to the last. It is used for the pens of a Plus's palette,
// from the main window, and for the lines of the raster table.
class PaletteMaker : public QDialog {
    Q_OBJECT

public:
    // For the pens of a palette, from `minStart` on: the colours are
    // written as soon as a slider moves, and `changed` is called each time.
    // The window stays until it is closed.
    PaletteMaker(int* palette, int minStart, std::function<void()> changed, QWidget* parent = nullptr);
    // For the lines of a raster table, `from` and `to` being offered, the
    // first colour being the last the table was given. "Generate" closes
    // the window.
    PaletteMaker(int* lines, int from, int to, tuxscr::Rgb last, QWidget* parent = nullptr);

    // Whether lines were given colours, and which: the raster table goes on
    // from there.
    bool done() const { return done_; }
    int start() const { return start_; }
    int end() const { return end_; }

    void retranslate();

private:
    int* palette_;
    int minStart_ = 0;
    std::function<void()> changed_;
    bool rasters_ = false;
    bool done_ = false;
    int start_ = 0, end_ = 0;

    QLineEdit* txbFrom_ = nullptr;
    QLineEdit* txbTo_ = nullptr;
    QPushButton* bpGetCol_ = nullptr;
    QPushButton* bpGenerate_ = nullptr;
    QLabel* lblError_ = nullptr;
    ShadeControls* first_ = nullptr;
    ShadeControls* last_ = nullptr;

    void build();
    // Works the colours out and writes them: false if what is typed cannot
    // be used.
    bool calcPalette();
    bool calcPens();
    bool calcLines();
    void takeColour();
};
