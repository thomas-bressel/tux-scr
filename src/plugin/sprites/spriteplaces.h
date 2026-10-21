#pragma once

#include <QPointer>
#include <QWidget>

#include "pictureview.h"

class QCheckBox;
class QLabel;
class QPainter;
class QPushButton;
class QSpinBox;

namespace tuxscr {

class ConverterWindow;
class Document;

// The hardware sprites shown on the picture (ConvImgCpc's box "Affichage
// sprites hard sur image"), there on a Plus in one of the CPC's own modes.
// Each of the first bank's sixteen sprites has a place on the picture and a
// zoom across and down, as the ASIC's registers have them: 0 hides it, 1, 2
// and 3 show it once, twice and four times its size. "Enable" draws them
// over the picture; a right click on the picture then puts the chosen sprite
// there. The places are copied as a source to assemble, and kept in a file.
class SpritePlacesPanel : public QWidget {
    Q_OBJECT

public:
    explicit SpritePlacesPanel(ConverterWindow* window);
    ~SpritePlacesPanel() override;

    // Whether sprites can be shown: a Plus, and mode 0, 1 or 2.
    bool applies() const;
    // Shows what the document has: called when it changed.
    void refresh();
    void applyTexts();
    // The sprites are to be drawn again: they or their palette changed.
    void redraw();

protected:
    // A right click on the picture is the sprites' while they are shown.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    ConverterWindow* window_;
    Document* document_;
    QPointer<PictureView> view_;
    QCheckBox* chkAfficheSH_;
    QLabel* labels_[5];  // sprite, zoom X, zoom Y, pos X, pos Y
    QSpinBox* numSprite_;
    QSpinBox* numZoomX_;
    QSpinBox* numZoomY_;
    QSpinBox* numPosX_;
    QSpinBox* numPosY_;
    QCheckBox* chkCopyZoom_;
    QPushButton* bpSaveCoord_;
    QPushButton* bpReadCoord_;
    QPushButton* bpCopiePosSprites_;
    QLabel* hint_;
    int overlay_ = 0;

    void showChosen();
    void paint(QPainter& painter);
    void copyPlaces();
    void readPlaces();
    void savePlaces();
};

}  // namespace tuxscr
