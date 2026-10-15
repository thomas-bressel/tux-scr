#pragma once

#include <QDialog>
#include <QWidget>

#include "base/bitmap.h"
#include "rasters.h"

class Document;
class PictureView;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class ShadeControls;

// The window that asks which of the picture's colours the rasters take the
// place of (ConvImgCpc's SelectColor): the pens the mode has, of which one
// is clicked. Closed without a choice, every colour is taken.
class SelectColourDialog : public QDialog {
    Q_OBJECT

public:
    explicit SelectColourDialog(const tuxscr::Screen& screen, QWidget* parent = nullptr);

    // The pen chosen, or -1.
    int chosen() const { return chosen_; }

private:
    int chosen_ = -1;
};

// The Plus's raster table (ConvImgCpc's RasterTablePlus): a colour for each
// of the 272 lines of an overscan picture, shown on the picture where it
// has the colour chosen. Lines are given one colour for so many lines, or
// colours going from one to another; the table is moved up and down, read
// and saved, and written as the words of an assembler source, as they are
// or with the tables that fade them in.
class RasterTable : public QWidget {
    Q_OBJECT

public:
    // `rastered` is the colour of the picture the lines' colours are shown
    // in place of, as a PC has it (&RRGGBB), or -1 for all of them.
    RasterTable(Document* document, int rastered, QWidget* parent = nullptr);

    // Shows the lines on the picture as it is now: called when the picture
    // changed.
    void drawLines();
    void retranslate();

    const tuxscr::Raster& raster() const { return raster_; }
    int rastered() const { return rastered_; }
    PictureView* pictureView() const { return view_; }

private:
    Document* document_;
    int rastered_;
    tuxscr::Raster raster_;
    // Where the next lines go, the colour of the last given, and what
    // "Undo" takes back.
    int newStart_ = 0, newEnd_ = 15;
    int lastAddConstant_ = 0, lastDirection_ = 1;
    tuxscr::Rgb lastColour_;
    QString folder_;

    PictureView* view_ = nullptr;
    QLabel* lblLine_ = nullptr;
    QCheckBox* chkSprites_ = nullptr;
    QLineEdit* txbConstant_ = nullptr;
    QLineEdit* txbStart_ = nullptr;
    QLineEdit* txbScroll_ = nullptr;
    QLineEdit* txbLineStart_ = nullptr;
    QLineEdit* txbLineEnd_ = nullptr;
    QRadioButton* rbPlus_ = nullptr;
    QPushButton* bpUndo_ = nullptr;
    QPushButton* bpGenerate_ = nullptr;
    ShadeControls* shade_ = nullptr;
    int shownLine_ = -1;

    QWidget* buildControls();
    void addConstant();
    void undo();
    void addLines();
    void clearAll();
    void scroll(bool up);
    void importKit();
    void load();
    void save();
    void copyToClipboard();
    void generate(bool fade);
    // The lines to write out, as typed: false, having said why, if they
    // cannot be.
    bool outputRange(int& start, int& end);
    void drawSprites(QImage& image) const;
    void showLine();
};
