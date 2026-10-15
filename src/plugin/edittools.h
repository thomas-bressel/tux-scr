#pragma once

#include <vector>

#include <QImage>
#include <QObject>
#include <QPoint>
#include <QPointer>

#include "engine/picture.h"

class ConverterWindow;
class Document;
class PictureView;
class QCheckBox;
class QComboBox;
class QDialog;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QScrollBar;
class QSpinBox;

// The picture's editor (ConvImgCpc's "Editer image", its Edition.cs): a
// pencil, a magnifying glass, the copy of a block, a pen picker and a
// filler, with what is done undone and done again, the picture turned over
// either way, a block saved as a window of OCP Art Studio (.WIN), and the
// grids ConvImgCpc draws over the picture. It is two tabs under the
// picture: "Edit" and "Tiles".
class EditTools : public QObject {
    Q_OBJECT

public:
    explicit EditTools(ConverterWindow* window);

    bool editing() const;
    int zoom() const { return zoom_; }
    // The pens the left and the right button draw with.
    int drawPen() const { return drawPen_; }
    int undrawPen() const { return undrawPen_; }
    bool canUndo() const { return done_ > 0; }
    bool canRedo() const { return done_ < static_cast<int>(steps_.size()); }
    // The block that was copied: its width in pixels of mode 2 and its
    // height in rows; nothing copied if either is 0.
    QSize block() const { return {blockWidth_, blockHeight_}; }

private:
    enum class Tool { Draw, Zoom, Copy, Pick, Fill };

    // A pixel that was changed: where, how wide, and its pen before and
    // after.
    struct Change {
        int x = 0, y = 0, width = 0, before = 0, after = 0;
    };

    ConverterWindow* window_;
    Document* document_;
    PictureView* view_;

    QWidget* editTab_ = nullptr;
    QWidget* tilesTab_ = nullptr;
    QCheckBox* modeEdition_ = nullptr;
    QWidget* grpEdition_ = nullptr;
    QRadioButton* rbDraw_ = nullptr;
    QRadioButton* rbZoom_ = nullptr;
    QRadioButton* rbCopy_ = nullptr;
    QRadioButton* rbPick_ = nullptr;
    QRadioButton* rbFill_ = nullptr;
    QComboBox* tailleCrayon_ = nullptr;
    QLabel* drawColor_ = nullptr;
    QLabel* undrawColor_ = nullptr;
    QLabel* lblInfoPos_ = nullptr;
    QLabel* lblZoom_ = nullptr;
    QPushButton* bpUndo_ = nullptr;
    QPushButton* bpRedo_ = nullptr;
    QPushButton* bpLoadWin_ = nullptr;
    QPushButton* bpSaveWin_ = nullptr;
    QCheckBox* chkRendu_ = nullptr;
    QCheckBox* chkDoRedo_ = nullptr;
    QScrollBar* hScroll_ = nullptr;
    QScrollBar* vScroll_ = nullptr;
    QCheckBox* chkGrille_ = nullptr;
    QLineEdit* txbStartNop_ = nullptr;
    QLineEdit* txbTailleNop_ = nullptr;
    QCheckBox* chkAutoCopy_ = nullptr;
    QCheckBox* chkTilesGrid_ = nullptr;
    QSpinBox* tilesSizeX_ = nullptr;
    QSpinBox* tilesSizeY_ = nullptr;
    QCheckBox* chkPack_ = nullptr;
    QRadioButton* rbRows_ = nullptr;
    QPointer<QDialog> renderWindow_;
    QLabel* renderPicture_ = nullptr;
    QCheckBox* renderDoubled_ = nullptr;

    Tool tool_ = Tool::Draw;
    int zoom_ = 1;
    int offsetX_ = 0, offsetY_ = 0;
    int drawPen_ = 1, undrawPen_ = 0;
    int penWidth_ = 1;
    bool drawing_ = false;
    // What was done, a step for each stroke; `done_` of them are in the
    // picture.
    std::vector<std::vector<Change>> steps_;
    std::vector<Change> stroke_;
    int done_ = 0;
    // The rectangle being drawn with the mouse, for the zoom or the copy.
    bool dragging_ = false;
    bool unzooming_ = false;
    QPoint dragFrom_, dragTo_;
    // The block copied, and where the mouse has it.
    int blockWidth_ = 0, blockHeight_ = 0;
    std::vector<uint8_t> blockPens_;
    QPoint blockAt_{-1, -1};
    int gridStart_ = 0, gridSize_ = 1;
    bool replaying_ = false;
    int frames_ = 1;

    void buildEditTab();
    void buildTilesTab();
    void retranslate();
    void setEditing(bool on);
    void setTool(Tool tool);
    void mouse(QPoint at, Qt::MouseButtons buttons);
    void toolDraw(QPoint at, Qt::MouseButtons buttons);
    void toolZoom(QPoint at, Qt::MouseButtons buttons);
    void toolCopy(QPoint at, Qt::MouseButtons buttons);
    void toolPick(QPoint at, Qt::MouseButtons buttons);
    void toolFill(QPoint at, Qt::MouseButtons buttons);
    void applyZoom();
    void endStroke();
    void setPixel(int x, int y, int pen, int width);
    void undo();
    void redo();
    void flip(bool rows);
    void releaseBlock();
    void showPens();
    QImage zoomed() const;
    void paintOver(QPainter& painter) const;
    void pictureChanged();
    void penChosen(int pen, bool right);
    void saveBlock();
    void loadBlock();
    void saveTileset();
    void copyToClipboard();
    std::vector<uint8_t> blockBytes(bool columns) const;
    int penCount(int y) const;
};
