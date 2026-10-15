#pragma once

#include <QWidget>

class Document;
class PenButton;
class QCheckBox;
class QLabel;

// The picture's palette (the top of ConvImgCpc's "Image CPC" window): for
// each pen the mode has, its colour, a box above that takes it out of the
// conversion and a box below that keeps its colour from being searched
// again. A pen the picture uses has a tick.
class PaletteBar : public QWidget {
    Q_OBJECT

public:
    explicit PaletteBar(Document* document, QWidget* parent = nullptr);

    // Shows what the document has.
    void refresh();
    void retranslate();

    PenButton* penButton(int pen) const { return pens_[pen]; }
    QCheckBox* lockBox(int pen) const { return locks_[pen]; }
    QCheckBox* disableBox(int pen) const { return disables_[pen]; }

signals:
    // A pen was clicked, with the left button or the right one.
    void penClicked(int pen, bool right);

private:
    Document* document_;
    PenButton* pens_[16] = {};
    QCheckBox* locks_[16] = {};
    QCheckBox* disables_[16] = {};
    QLabel* disableLabel_ = nullptr;
    QLabel* lockLabel_ = nullptr;
};
