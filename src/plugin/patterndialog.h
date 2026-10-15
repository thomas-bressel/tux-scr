#pragma once

#include <QDialog>

class ConverterWindow;
class Document;
class PenButton;
class QLabel;
class QPushButton;
class QSpinBox;

// The sixteen patterns of mode ASC-UT (ConvImgCpc's "Edition trames"):
// each is 4 by 4 pixels of mode 1. They are shown in a row; the one chosen
// is drawn large and changed with the mouse, a pen for each button. They
// can be read and saved, taken from one of eight sets, or made from the
// pictures themselves: the sixteen blocks found most.
class PatternDialog : public QDialog {
    Q_OBJECT

public:
    explicit PatternDialog(ConverterWindow* window);

    int current() const { return current_; }
    // The place of a pixel of the pattern being edited, and of a pattern
    // in the row: for those who click there.
    QPoint cellCentre(int x, int y) const;
    QPoint patternCentre(int pattern) const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    ConverterWindow* window_;
    Document* document_;
    QWidget* all_;
    QWidget* edit_;
    QPushButton* bpPrev_;
    QPushButton* bpSuiv_;
    PenButton* pens_[4];
    QLabel* lblPenLeft_;
    QLabel* lblPenRight_;
    QSpinBox* numTabTrame_;
    int current_ = 0;
    int penLeft_ = 1, penRight_ = 0;

    void showAll();
    void paintAll();
    void paintEdit();
    void readPatterns();
    void savePatterns();
    void makePatterns();
};
