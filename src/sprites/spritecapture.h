#pragma once

#include <QDialog>
#include <QPoint>
#include <QPointer>

#include "pictureview.h"

class ConverterWindow;
class Document;
class QCheckBox;
class QComboBox;
class QLabel;
class QPainter;
class QPushButton;
class QRadioButton;
class QSpinBox;
class SpriteCaptureView;

// Hardware sprites taken from the picture (ConvImgCpc's "Capture Sprites"
// mode: the picture is converted as wide as mode 1 with sixteen colours, a
// sprite being 16 of its pixels by 16 of its lines).

// The capture window (ConvImgCpc's Capture): how many sprites the rectangle
// on the picture takes, what it last took, and "Capture", which makes
// sprites of it from a sprite of a bank on, with the picture's palette.
class SpriteCaptureWindow : public QDialog {
    Q_OBJECT

public:
    explicit SpriteCaptureWindow(Document* document, QWidget* parent = nullptr);

    // The sprites the rectangle takes, across and down.
    int across() const { return across_; }
    int down() const { return down_; }
    // Shows what the picture has from this place of it on. `withClick`
    // says a click asked for it: sprites are then made of it at once if
    // "Auto capture" is ticked.
    void setCapture(int x, int y, bool withClick = false);
    // Makes sprites of what is shown.
    void capture();
    void retranslate();
    void reject() override;

signals:
    // The size of the rectangle changed.
    void sizeChanged();
    // The window was closed.
    void closed();

private:
    Document* document_;
    int across_ = 1;
    int down_ = 1;
    SpriteCaptureView* view_;
    QRadioButton* rbCapt1_;
    QRadioButton* rbCapt2_;
    QRadioButton* rbCapt4_;
    QRadioButton* rbCaptUser_;
    QLabel* lblNbX_;
    QLabel* lblNbY_;
    QSpinBox* txbNbX_;
    QSpinBox* txbNbY_;
    QLabel* label1_;
    QSpinBox* numSprite_;
    QComboBox* comboBanque_;
    QPushButton* bpCapture_;
    QCheckBox* chkAutoCapture_;

    void setSize(int across, int down, int lastStart);
    void userSize();
};

// The capture's side of the converter's window: the boxes "Sprites capture"
// and "Sprites grid", there when the mode is "Capture Sprites", and what
// they put over the picture. With the first ticked the capture window is
// shown and the picture's mouse is the capture's: it moves a rectangle (as
// do the arrow keys), and a click (or Space) shows what is in it in the
// capture window. The second draws the lines that part the sprites, every
// 32 pixels of the PC.
class SpriteCapturePanel : public QWidget {
    Q_OBJECT

public:
    explicit SpriteCapturePanel(ConverterWindow* window);
    ~SpriteCapturePanel() override;

    // Whether the mode is the one sprites are captured in.
    bool applies() const;
    // The document changed: what is shown follows, and the capture ends if
    // the mode is no longer its own.
    void refresh();
    void retranslate();
    // Ends the capture, if it is on: its window goes and the mouse is given
    // back.
    void stop();
    SpriteCaptureWindow* captureWindow() const { return capture_; }
    // The corner of the rectangle on the picture; (-1, -1) if it is not
    // shown.
    QPoint corner() const { return corner_; }

private:
    ConverterWindow* window_;
    Document* document_;
    QPointer<PictureView> view_;
    QCheckBox* modeCapture_;
    QCheckBox* grid_;
    QLabel* hint_;
    QPointer<SpriteCaptureWindow> capture_;
    QPoint corner_{-1, -1};
    // The click that is held was already taken.
    bool clicked_ = false;
    // The settings were put out of reach from here.
    bool holdsSettings_ = false;
    int overlay_ = 0;

    void redraw();
    bool hasMouse();
    void captureToggled(bool on);
    void settingsReach();
    void mouse(QPoint at, Qt::MouseButtons buttons);
    void arrow(int dx, int dy, bool choose);
    void paint(QPainter& painter);
};
