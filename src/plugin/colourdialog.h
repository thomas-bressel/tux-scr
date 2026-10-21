#pragma once

#include <QDialog>

class QLabel;
class QSlider;
class QSpinBox;

namespace tuxscr {

// The window a pen's colour is chosen in (ConvImgCpc's EditColor): one of
// the CPC's 27, or on a Plus its green, red and blue, each 0 to 15.
class ColourDialog : public QDialog {
    Q_OBJECT

public:
    // `colour` is a firmware number (0 to 26), or a Plus's twelve bits.
    ColourDialog(int pen, int colour, bool plus, QWidget* parent = nullptr);

    int colour() const { return colour_; }

    // Asks for a colour: true if one was chosen, `colour` then being it.
    static bool choose(QWidget* parent, int pen, int& colour, bool plus);

private:
    int colour_;
    bool plus_;
    QLabel* sample_ = nullptr;
    QLabel* value_ = nullptr;
    QSlider* sliders_[3] = {};
    QSpinBox* boxes_[3] = {};

    void showColour();
};

}  // namespace tuxscr
