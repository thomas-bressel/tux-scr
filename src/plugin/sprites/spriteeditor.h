#pragma once

#include <cstdint>

#include <QDialog>
#include <QPoint>

class QAbstractButton;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QToolButton;

namespace tuxscr {

class Document;
class PenButton;
class PenHolder;
class SpriteGrid;
class SpriteStrip;
class SpriteTestView;

// The editor of the Plus's hardware sprites (ConvImgCpc's EditSprites).
//
// At the top, the sixteen sprites of a bank, of which there are eight: a
// click on one, the two buttons under them or the left and right arrows of
// the keyboard choose the one to draw; a right click on one then on another
// has them change places. Under them the sprite as a grid of large pixels: the
// left button draws with the colour chosen in the palette beside it (a
// point, a line or a filled area), the right one rubs out. The palette is
// the sprites' own, fifteen colours and the transparent one: a right click
// on one changes it. On the left the sprite is copied, pasted, flipped,
// turned, moved a pixel and cleared; on the right the sprites and their
// palette are read and saved, and the sprites tried at the sizes the Plus
// gives them.
class SpriteEditor : public QDialog {
    Q_OBJECT

public:
    explicit SpriteEditor(Document* document, QWidget* parent = nullptr);

    // Shows the window. Sprites that have no palette yet take the
    // picture's.
    void present();
    // Draws again what the document has: called when it changed.
    void refresh();
    void applyTexts();

    int bank() const { return bank_; }
    int sprite() const { return sprite_; }
    int drawingPen() const { return pen_; }

protected:
    // The left and right arrows go from a sprite to the next, unless the
    // control that has the keyboard uses them itself.
    bool eventFilter(QObject* watched, QEvent* event) override;
    // The pictograms are drawn again for another look.
    void changeEvent(QEvent* event) override;

private:
    Document* document_;
    int bank_ = 0;
    int sprite_ = 0;
    int pen_ = 1;
    // What "copy" took: a sprite is the first of them, a bank all sixteen.
    uint8_t copied_[16][256] = {};
    // The line being drawn: where it starts, and the sprite as it was.
    QPoint lineStart_{-1, -1};
    uint8_t beforeLine_[256] = {};
    // Where "SetPos" put the sprites in the test, counted in sprites.
    QPoint testPlaces_[16];
    bool testPlaced_[16] = {};

    SpriteStrip* strip_;
    SpriteGrid* grid_;
    SpriteTestView* test_;
    PenButton* pens_[16];
    PenHolder* holders_[16];
    QComboBox* comboBanque_;
    QPushButton* bpPrev_;
    QPushButton* bpSuiv_;
    QLabel* lblSelSprite_;
    QLabel* label1_;
    QLabel* label2_;
    QLabel* lblPenLeft_;
    QLabel* lblPenRight_;
    QLabel* lblColSel_[3];  // green, red, blue: the order they are shown in
    QGroupBox* grpEdition_;
    QPushButton* bpCopySprite_;
    QPushButton* bpPasteSprite_;
    QPushButton* bpHorizontalFlip_;
    QPushButton* bpVerticalFlip_;
    QPushButton* bpRotateSprite_;
    QPushButton* bpClearSprite_;
    QToolButton* bpScr_[4];  // up, down, left, right
    QRadioButton* rbPt_;
    QRadioButton* rbLine_;
    QRadioButton* rbFill_;
    QPushButton* bpCopyBank_;
    QPushButton* bpPasteBank_;
    QPushButton* bpClearBank_;
    QPushButton* bpRead_;
    QPushButton* bpSave_;
    QPushButton* bpSaveAll_;
    QCheckBox* chkWithPal_;
    QPushButton* bpReadPal_;
    QPushButton* bpSavePal_;
    QPushButton* bpGenPal_;
    QPushButton* bpInversePalette_;
    QRadioButton* rb1Sprite_;
    QRadioButton* rb2Sprite_;
    QRadioButton* rb4Sprite_;
    QRadioButton* rb42Sprite_;
    QRadioButton* rb28sprite_;
    QRadioButton* rbPosSpr_;
    QLabel* label3_;
    QLabel* label4_;
    QSpinBox* zoomX_;
    QSpinBox* zoomY_;
    QPushButton* bpTest_;

    QWidget* buildTools();
    QWidget* buildFiles();
    void setIcons();
    uint8_t& pixel(int x, int y);
    // The sprites or their palette were changed here: the document's
    // windows are told, this one among them.
    void changed();
    void select(int bank, int sprite);
    void previous();
    void next();

    // ---- Drawing ----
    void gridPointed(QPoint at, Qt::MouseButtons buttons);
    void drawLine(int x1, int y1, int x2, int y2);
    void fill(int x, int y);
    void keepSprite(uint8_t* store);
    void putSprite(const uint8_t* store);
    void stripPointed(int sprite);
    void penClicked(int pen, bool right);
    // The sprite's pixels each sent to another place of it.
    void move(int (*fromX)(int x, int y), int (*fromY)(int x, int y));

    // ---- The test ----
    int testWidth() const;
    int testHeight() const;
    void drawTest(int sprite, int x, int y);
    void runTest();
    void testHovered(QPoint at);
    void testPressed(QPoint at);

    // ---- Files ----
    void readSprites();
    void saveSprites(bool allBanks);
    void readPalette();
    void savePalette();
    bool readKit(const QString& path);
};

}  // namespace tuxscr
