#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QScrollBar;
class QSpinBox;
class QTimer;
class QToolButton;

namespace tuxscr {

class ConverterWindow;
class Document;

// The frames of an animation (ConvImgCpc's "Animation" window): five of
// them from the current one on, each with how long it stays and a button
// that takes it away; the frames played in turn; the converted frames saved
// as an animated GIF. A tab under the picture, which has something to show
// when a file gave several pictures.
class AnimationPanel : public QWidget {
    Q_OBJECT

public:
    explicit AnimationPanel(ConverterWindow* window);

    void refresh();
    void applyTexts();
    // Asks how the animation is to be saved as a source, and saves it.
    bool saveAnimation(const QString& path);

private:
    ConverterWindow* window_;
    Document* document_;
    QLabel* lblMaxImage_;
    QLabel* lblNumImage_;
    QSpinBox* numImage_;
    QScrollBar* scroll_;
    QPushButton* bpDel1_2_;
    QRadioButton* rbSource_;
    QRadioButton* rvCalculee_;
    QPushButton* bpSaveGif_;
    QPushButton* bpPlay_;
    QPushButton* bpStop_;
    QToolButton* pictures_[5];
    QLineEdit* times_[5];
    QPushButton* removes_[5];
    QTimer* timer_;
    qint64 shownSince_ = 0;
    bool filling_ = false;
};

}  // namespace tuxscr
