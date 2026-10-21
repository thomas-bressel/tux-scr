#pragma once

#include <QDialog>
#include <QWidget>

#include "base/bitmap.h"
#include "splits/splitscreen.h"

class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;
class QScrollBar;
class QSpinBox;

namespace tuxscr {

class ColourSwatch;
class Document;
class PenButton;
class PictureView;

// The window that makes the splits of a picture that shows them (ConvImgCpc's
// ImportSplit): a picture of 384 by 272, a pixel for each pixel of mode 1,
// in which the colour of each line changes where the pen's is to change.
// Reading one gives the lines their splits at once; what the lines cannot
// hold is listed.
class ImportSplitDialog : public QDialog {
    Q_OBJECT

public:
    explicit ImportSplitDialog(tuxscr::SplitLines* lines, const QString& folder, QWidget* parent = nullptr);

private:
    tuxscr::SplitLines* lines_;
    QString folder_;
    PictureView* view_ = nullptr;
    QSpinBox* numPen_ = nullptr;
    QListWidget* listErr_ = nullptr;

    void readPicture();
};

// The split editor (ConvImgCpc's EditSplit): the picture on an overscan
// screen of a CPC, and for each of its lines the pen whose colour changes
// along it, where the first change falls and up to seven colours with their
// lengths. The screen is shown as the splits make it, with the line being
// set enlarged under it; the lines are read and saved, taken from a picture
// of them, and written as the program that shows the screen.
class SplitEditor : public QWidget {
    Q_OBJECT

public:
    explicit SplitEditor(Document* document, QWidget* parent = nullptr);

    // Takes the picture and its palette again: called when they changed.
    void updateScreen();
    void applyTexts();

    tuxscr::SplitScreen& screen() { return screen_; }
    PictureView* pictureView() const { return view_; }
    PictureView* zoomView() const { return zoom_; }

private:
    Document* document_;
    tuxscr::SplitScreen screen_;
    tuxscr::Bitmap shown_;
    // The controls are being set from the line: what they say is not to be
    // taken as a change.
    bool loading_ = false;
    QString folder_;

    QSpinBox* numLigne_ = nullptr;
    QSpinBox* numPen_ = nullptr;
    QSpinBox* retard_ = nullptr;
    QCheckBox* chkChgt_ = nullptr;
    QCheckBox* chkSplit_[tuxscr::kSplitsPerLine] = {};
    QSpinBox* largSplit_[tuxscr::kSplitsPerLine] = {};
    ColourSwatch* lblColor_[tuxscr::kSplitsPerLine] = {};
    QCheckBox* chkChangeMode_ = nullptr;
    QSpinBox* modeCpc_ = nullptr;
    PenButton* colors_[16] = {};
    PictureView* view_ = nullptr;
    PictureView* zoom_ = nullptr;
    QScrollBar* hScrollZoom_ = nullptr;
    QLabel* lblInfo_ = nullptr;

    QWidget* buildControls();
    QWidget* buildPicture();
    tuxscr::SplitLine& currentLine();
    void displayLine();
    void updatePalette();
    void render();
    void drawZoom();
    void drawChanges(QPainter& painter);
    void checkLastSplit();
    void changeWidth(int index);
    void enableSplit(int index);
    void changeColour(int index);
    void changeStartColour(int pen);
    void copyLineAbove();
    void importSplit();
    void load();
    void save();
    void generate();
    void pictureClicked(QPoint at);
};

}  // namespace tuxscr
