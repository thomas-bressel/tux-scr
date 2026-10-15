#include "edittools.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include "contrast.h"
#include "controls.h"
#include "converterwindow.h"
#include "formats/amsdos.h"
#include "formats/asmgen.h"
#include "pictures.h"
#include "pictureview.h"
#include "prompts.h"
#include "texts.h"

using namespace tuxscr;

namespace {

bool english()
{
    return texts::language() == QLatin1String("EN");
}

QString both(const char* french, const char* englishText)
{
    return QString::fromUtf8(english() ? englishText : french);
}

// A line that shows on any colour: white, with black dashes over it.
void markLine(QPainter& painter, QPointF from, QPointF to)
{
    painter.setPen(QPen(Qt::white, 1));
    painter.drawLine(from, to);
    QPen dashes(Qt::black, 1);
    dashes.setDashPattern({3, 3});
    painter.setPen(dashes);
    painter.drawLine(from, to);
}

void markRect(QPainter& painter, const QRect& rect)
{
    const QRectF r = QRectF(rect.normalized()).adjusted(0.5, 0.5, -0.5, -0.5);
    markLine(painter, r.topLeft(), r.topRight());
    markLine(painter, r.bottomLeft(), r.bottomRight());
    markLine(painter, r.topLeft(), r.bottomLeft());
    markLine(painter, r.topRight(), r.bottomRight());
}

}  // namespace

EditTools::EditTools(ConverterWindow* window)
    : QObject(window)
    , window_(window)
    , document_(window->document())
    , view_(window->pictureView())
{
    buildEditTab();
    buildTilesTab();
    window_->tools()->addTab(editTab_, QString());
    window_->tools()->addTab(tilesTab_, QString());
    view_->addOverlay([this](QPainter& painter) { paintOver(painter); });
    connect(view_, &PictureView::mouse, this, &EditTools::mouse);
    connect(document_, &Document::changed, this, &EditTools::pictureChanged);
    // Other pictures: what was done to the ones before is not theirs to
    // undo.
    const auto forget = [this] {
        frames_ = document_->frameCount();
        steps_.clear();
        stroke_.clear();
        done_ = 0;
        drawing_ = false;
    };
    connect(document_, &Document::titleChanged, this, forget);
    connect(document_, &Document::framesChanged, this, [this, forget] {
        if (document_->frameCount() != frames_)
            forget();
    });
    connect(window_, &ConverterWindow::languageChanged, this, &EditTools::retranslate);
    retranslate();
    setEditing(false);
}

bool EditTools::editing() const
{
    return modeEdition_->isChecked();
}

int EditTools::penCount(int y) const
{
    const Screen& screen = document_->screen;
    if (screen.mode == kModeSplit || screen.mode == kModeCaptureSprites)
        return 16;
    return std::min(16, 1 << std::min(4, screen.pixelWidth(y)));
}

void EditTools::buildEditTab()
{
    editTab_ = new QWidget;
    editTab_->setObjectName(QStringLiteral("editTab"));
    auto* column = new QVBoxLayout(editTab_);
    column->setContentsMargins(6, 6, 6, 6);
    auto* top = new QHBoxLayout;
    modeEdition_ = namedCheck("modeEdition");
    chkRendu_ = namedCheck("chkRendu");
    chkDoRedo_ = namedCheck("chkDoRedo");
    top->addWidget(modeEdition_);
    top->addWidget(chkRendu_);
    top->addWidget(chkDoRedo_);
    top->addStretch();
    column->addLayout(top);

    grpEdition_ = new QWidget;
    grpEdition_->setObjectName(QStringLiteral("grpEdition"));
    auto* grid = new QGridLayout(grpEdition_);
    grid->setContentsMargins(0, 0, 0, 0);
    rbDraw_ = namedRadio("rbDraw");
    rbZoom_ = namedRadio("rbZoom");
    rbCopy_ = namedRadio("rbCopy");
    rbPick_ = namedRadio("rbPickColor");
    rbFill_ = namedRadio("rbFill");
    rbDraw_->setChecked(true);
    auto* tools = new QButtonGroup(this);
    int column0 = 0;
    for (QRadioButton* radio : {rbDraw_, rbZoom_, rbCopy_, rbPick_, rbFill_}) {
        tools->addButton(radio);
        grid->addWidget(radio, 0, column0++);
    }
    lblZoom_ = namedLabel("lblZoom", QStringLiteral("1:1"));
    grid->addWidget(lblZoom_, 0, column0++);
    lblInfoPos_ = namedLabel("lblInfoPos");
    lblInfoPos_->setMinimumWidth(190);
    grid->addWidget(lblInfoPos_, 0, column0, 1, 3);

    auto* lblPenSize = namedLabel("lblPenSize");
    tailleCrayon_ = new QComboBox;
    tailleCrayon_->setObjectName(QStringLiteral("tailleCrayon"));
    tailleCrayon_->addItems({QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"), QStringLiteral("4"), QStringLiteral("8")});
    lblPenSize->setBuddy(tailleCrayon_);
    auto* lblPenColor = namedLabel("lblPenColor");
    drawColor_ = namedLabel("drawColor");
    undrawColor_ = namedLabel("undrawColor");
    for (QLabel* label : {drawColor_, undrawColor_}) {
        label->setFixedSize(46, 24);
        label->setAlignment(Qt::AlignCenter);
        label->setAutoFillBackground(true);
        label->setFrameShape(QFrame::Box);
    }
    grid->addWidget(lblPenSize, 1, 0);
    grid->addWidget(tailleCrayon_, 1, 1);
    grid->addWidget(lblPenColor, 1, 2);
    grid->addWidget(drawColor_, 1, 3);
    grid->addWidget(undrawColor_, 1, 4);
    bpUndo_ = namedButton("bpUndo", QStringLiteral("Undo"));
    bpRedo_ = namedButton("bpRedo", QStringLiteral("Redo"));
    auto* bpHorFlip = namedButton("bpHorFlip", QStringLiteral("Hor. Flip"));
    auto* bpVerFlip = namedButton("bpVerFlip", QStringLiteral("Ver. Flip"));
    bpLoadWin_ = namedButton("bpLoadWin", QStringLiteral("Load Win"));
    bpSaveWin_ = namedButton("bpSaveWin", QStringLiteral("Save Win"));
    auto* buttons = new QHBoxLayout;
    for (QPushButton* button : {bpUndo_, bpRedo_, bpHorFlip, bpVerFlip, bpLoadWin_, bpSaveWin_})
        buttons->addWidget(button);
    buttons->addStretch();
    grid->addLayout(buttons, 2, 0, 1, 9);
    // Where the enlarged picture starts.
    hScroll_ = new QScrollBar(Qt::Horizontal);
    hScroll_->setObjectName(QStringLiteral("hScrollBar"));
    vScroll_ = new QScrollBar(Qt::Horizontal);
    vScroll_->setObjectName(QStringLiteral("vScrollBar"));
    auto* lblX = namedLabel("lblScrollX", QStringLiteral("X"));
    auto* lblY = namedLabel("lblScrollY", QStringLiteral("Y"));
    lblX->setBuddy(hScroll_);
    lblY->setBuddy(vScroll_);
    auto* scrolls = new QHBoxLayout;
    scrolls->addWidget(lblX);
    scrolls->addWidget(hScroll_, 1);
    scrolls->addWidget(lblY);
    scrolls->addWidget(vScroll_, 1);
    grid->addLayout(scrolls, 3, 0, 1, 9);
    grid->setColumnStretch(8, 1);
    column->addWidget(grpEdition_);
    column->addStretch();

    connect(modeEdition_, &QCheckBox::toggled, this, &EditTools::setEditing);
    connect(rbDraw_, &QRadioButton::toggled, this, [this](bool on) { if (on) setTool(Tool::Draw); });
    connect(rbZoom_, &QRadioButton::toggled, this, [this](bool on) { if (on) setTool(Tool::Zoom); });
    connect(rbCopy_, &QRadioButton::toggled, this, [this](bool on) { if (on) setTool(Tool::Copy); });
    connect(rbPick_, &QRadioButton::toggled, this, [this](bool on) { if (on) setTool(Tool::Pick); });
    connect(rbFill_, &QRadioButton::toggled, this, [this](bool on) { if (on) setTool(Tool::Fill); });
    connect(tailleCrayon_, &QComboBox::currentTextChanged, this, [this](const QString& text) { penWidth_ = std::max(1, text.toInt()); });
    connect(bpUndo_, &QPushButton::clicked, this, &EditTools::undo);
    connect(bpRedo_, &QPushButton::clicked, this, &EditTools::redo);
    connect(bpHorFlip, &QPushButton::clicked, this, [this] { flip(true); });
    connect(bpVerFlip, &QPushButton::clicked, this, [this] { flip(false); });
    connect(bpLoadWin_, &QPushButton::clicked, this, &EditTools::loadBlock);
    connect(bpSaveWin_, &QPushButton::clicked, this, &EditTools::saveBlock);
    connect(hScroll_, &QScrollBar::valueChanged, this, [this](int value) {
        offsetX_ = (value >> 3) << 3;
        document_->touch();
    });
    connect(vScroll_, &QScrollBar::valueChanged, this, [this](int value) {
        offsetY_ = (value >> 1) << 1;
        document_->touch();
    });
    connect(chkRendu_, &QCheckBox::toggled, this, [this](bool on) {
        if (!on) {
            if (renderWindow_)
                renderWindow_->close();
            return;
        }
        // The picture as it is, beside the one that is being worked on
        // enlarged.
        renderWindow_ = new QDialog(window_);
        renderWindow_->setObjectName(QStringLiteral("Rendu"));
        renderWindow_->setAttribute(Qt::WA_DeleteOnClose);
        renderWindow_->setWindowTitle(both("Rendu", "Rendering"));
        auto* box = new QVBoxLayout(renderWindow_);
        renderDoubled_ = namedCheck("chkX2", QStringLiteral("X2"));
        renderPicture_ = namedLabel("Picture");
        box->addWidget(renderDoubled_);
        box->addWidget(renderPicture_);
        connect(renderDoubled_, &QCheckBox::toggled, this, &EditTools::pictureChanged);
        connect(renderWindow_, &QDialog::finished, this, [this] { chkRendu_->setChecked(false); });
        renderWindow_->show();
        pictureChanged();
    });
}

void EditTools::buildTilesTab()
{
    tilesTab_ = new QWidget;
    tilesTab_->setObjectName(QStringLiteral("tilesTab"));
    auto* grid = new QGridLayout(tilesTab_);
    grid->setContentsMargins(6, 6, 6, 6);
    chkTilesGrid_ = namedCheck("chkTilesGrid", QStringLiteral("Grid"));
    tilesSizeX_ = namedSpin("tilesSizeX", 1, 64, 16);
    tilesSizeY_ = namedSpin("tilesSizeY", 1, 64, 16);
    auto* label9 = namedLabel("label9", QStringLiteral("TilesSizeX"));
    auto* label10 = namedLabel("label10", QStringLiteral("TilesSizeY"));
    label9->setBuddy(tilesSizeX_);
    label10->setBuddy(tilesSizeY_);
    auto* bpSaveTiles = namedButton("bpSaveTiles", QStringLiteral("Save as Tileset"));
    chkPack_ = namedCheck("chkPack", QStringLiteral("Pack"));
    rbRows_ = namedRadio("rbRows", QStringLiteral("Rows"));
    auto* rbCols = namedRadio("rbCols", QStringLiteral("Cols"));
    rbRows_->setChecked(true);
    grid->addWidget(chkTilesGrid_, 0, 0);
    grid->addWidget(label9, 0, 1);
    grid->addWidget(tilesSizeX_, 0, 2);
    grid->addWidget(label10, 0, 3);
    grid->addWidget(tilesSizeY_, 0, 4);
    grid->addWidget(rbRows_, 0, 5);
    grid->addWidget(rbCols, 0, 6);
    grid->addWidget(chkPack_, 0, 7);
    grid->addWidget(bpSaveTiles, 0, 8);

    chkGrille_ = namedCheck("chkGrille");
    txbStartNop_ = new QLineEdit(QStringLiteral("0"));
    txbStartNop_->setObjectName(QStringLiteral("txbStartNop"));
    txbTailleNop_ = new QLineEdit(QStringLiteral("1"));
    txbTailleNop_->setObjectName(QStringLiteral("txbTailleNop"));
    txbStartNop_->setMaximumWidth(50);
    txbTailleNop_->setMaximumWidth(50);
    auto* label1 = namedLabel("label1", QStringLiteral("start NOP"));
    auto* label4 = namedLabel("label4", QStringLiteral("nb NOPs"));
    label1->setBuddy(txbStartNop_);
    label4->setBuddy(txbTailleNop_);
    grid->addWidget(chkGrille_, 1, 0);
    grid->addWidget(label1, 1, 1);
    grid->addWidget(txbStartNop_, 1, 2);
    grid->addWidget(label4, 1, 3);
    grid->addWidget(txbTailleNop_, 1, 4);

    auto* bpCopyImage = namedButton("bpCopyImage");
    chkAutoCopy_ = namedCheck("chkAutoCopy", QStringLiteral("AllCopy"));
    grid->addWidget(bpCopyImage, 2, 0, 1, 2);
    grid->addWidget(chkAutoCopy_, 2, 2, 1, 2);
    grid->setColumnStretch(9, 1);
    grid->setRowStretch(3, 1);

    connect(chkTilesGrid_, &QCheckBox::toggled, this, [this](bool on) {
        // The size of the tiles is not changed while their grid is shown.
        tilesSizeX_->setEnabled(!on);
        tilesSizeY_->setEnabled(!on);
        view_->update();
    });
    connect(bpSaveTiles, &QPushButton::clicked, this, &EditTools::saveTileset);
    connect(chkGrille_, &QCheckBox::toggled, this, [this](bool on) {
        if (on) {
            bool okStart = false, okSize = false;
            const int start = txbStartNop_->text().toInt(&okStart);
            const int size = txbTailleNop_->text().toInt(&okSize);
            if (!okStart || !okSize || size <= 0 || size > 16 || start + size >= 64) {
                chkGrille_->setChecked(false);
                return;
            }
            gridStart_ = start;
            gridSize_ = size;
        }
        txbStartNop_->setEnabled(!on);
        txbTailleNop_->setEnabled(!on);
        view_->update();
    });
    connect(bpCopyImage, &QPushButton::clicked, this, &EditTools::copyToClipboard);
}

void EditTools::retranslate()
{
    texts::apply(editTab_, "ImageCpc");
    texts::apply(tilesTab_, "ImageCpc");
    QTabWidget* tools = window_->tools();
    tools->setTabText(tools->indexOf(editTab_), both("Édition", "Edit"));
    tools->setTabText(tools->indexOf(tilesTab_), both("Tuiles et grilles", "Tiles and Grids"));
    rbDraw_->setText(both("Crayon", "Pencil"));
    rbZoom_->setText(both("Loupe", "Magnifier"));
    rbCopy_->setText(both("Copie de bloc", "Block Copy"));
    rbPick_->setText(both("Pipette", "Pick Colour"));
    rbFill_->setText(both("Remplir", "Fill"));
    chkGrille_->setText(both("Afficher grille verticale", "Show vertical grid"));
    tilesTab_->findChild<QLabel*>(QStringLiteral("label1"))->setText(both("NOP de début", "Start NOP"));
    tilesTab_->findChild<QLabel*>(QStringLiteral("label4"))->setText(both("Nb de NOPs", "Number of NOPs"));
    tilesTab_->findChild<QLabel*>(QStringLiteral("label9"))->setText(both("Largeur des tuiles", "Tiles' width"));
    tilesTab_->findChild<QLabel*>(QStringLiteral("label10"))->setText(both("Hauteur des tuiles", "Tiles' height"));
    hScroll_->setAccessibleName(both("Début de l'image agrandie, en X", "Start of the enlarged picture, X"));
    vScroll_->setAccessibleName(both("Début de l'image agrandie, en Y", "Start of the enlarged picture, Y"));
    tilesSizeX_->setAccessibleName(QStringLiteral("TilesSizeX"));
    tilesSizeY_->setAccessibleName(QStringLiteral("TilesSizeY"));
    showPens();
}

// ---- Editing on and off ----

void EditTools::setEditing(bool on)
{
    // Another tool has the picture's mouse (sprites are being captured):
    // it keeps it.
    if (on && window_->mouseOwner() && window_->mouseOwner() != this) {
        modeEdition_->setChecked(false);
        return;
    }
    releaseBlock();
    zoom_ = 1;
    offsetX_ = offsetY_ = 0;
    dragging_ = false;
    chkRendu_->setChecked(false);
    grpEdition_->setEnabled(on);
    chkRendu_->setEnabled(on);
    if (on) {
        steps_.clear();
        stroke_.clear();
        done_ = 0;
        // A picture that is drawn on is not converted again at every change
        // of a setting.
        if (auto* automatic = window_->findChild<QCheckBox*>(QStringLiteral("autoRecalc")))
            automatic->setChecked(false);
        document_->param.autoRecalc = false;
        window_->setMouseOwner(this);
        window_->setPenHandler([this](int pen, bool right) { penChosen(pen, right); });
        window_->setImageProvider([this] { return zoomed(); });
    } else if (window_->mouseOwner() == this) {
        window_->setMouseOwner(nullptr);
        window_->setPenHandler({});
        window_->setImageProvider({});
    }
    applyZoom();
    bpUndo_->setEnabled(on && canUndo());
    bpRedo_->setEnabled(on && canRedo());
    setTool(tool_);
}

void EditTools::setTool(Tool tool)
{
    if (tool != Tool::Copy && tool != Tool::Pick && tool != Tool::Fill)
        releaseBlock();
    tool_ = tool;
    dragging_ = false;
    bpLoadWin_->setEnabled(editing() && tool == Tool::Copy);
    bpSaveWin_->setEnabled(editing() && tool == Tool::Copy && blockWidth_ > 0);
    view_->update();
}

void EditTools::penChosen(int pen, bool right)
{
    if (tool_ != Tool::Draw && tool_ != Tool::Fill)
        rbDraw_->setChecked(true);
    (right ? undrawPen_ : drawPen_) = pen;
    showPens();
}

void EditTools::showPens()
{
    const Screen& screen = document_->screen;
    const int count = penCount(0);
    const struct {
        QLabel* label;
        int pen;
        const char* french;
        const char* englishText;
    } shown[2] = {{drawColor_, drawPen_ % count, "Bouton gauche : encre ", "Left button: pen "},
                  {undrawColor_, undrawPen_ % count, "Bouton droit : encre ", "Right button: pen "}};
    for (const auto& one : shown) {
        const QColor colour = toQColor(screen.rgb(screen.palette[static_cast<size_t>(one.pen)]));
        QPalette look = one.label->palette();
        look.setColor(QPalette::Window, colour);
        // The number reads on the pen's colour, whatever it is.
        look.setColor(QPalette::WindowText, readable(inkOn(colour), colour));
        one.label->setPalette(look);
        one.label->setText(QString::number(one.pen));
        one.label->setToolTip(both(one.french, one.englishText) + QString::number(one.pen));
        one.label->setAccessibleName(one.label->toolTip());
    }
}

// ---- What is shown ----

QImage EditTools::zoomed() const
{
    const QImage shown = document_->shown();
    if (zoom_ == 1)
        return shown;
    QImage enlarged(shown.size(), QImage::Format_RGB32);
    enlarged.fill(QApplication::palette().color(QPalette::Window));
    QPainter painter(&enlarged);
    const QRect part(offsetX_, offsetY_, shown.width() / zoom_ + 1, shown.height() / zoom_ + 1);
    painter.drawImage(QRect(0, 0, part.width() * zoom_, part.height() * zoom_), shown, part);
    return enlarged;
}

void EditTools::pictureChanged()
{
    // Converted again, the picture is given what was drawn on it once more.
    if (chkDoRedo_->isChecked() && editing() && !replaying_ && !drawing_) {
        replaying_ = true;
        Picture& picture = document_->picture();
        bool any = false;
        for (int step = 0; step < done_; ++step)
            for (const Change& change : steps_[static_cast<size_t>(step)])
                if (picture.pen(change.x, change.y) != change.after) {
                    picture.setPixel(change.x, change.y, change.after, change.width);
                    any = true;
                }
        if (any)
            document_->touch();
        replaying_ = false;
    }
    view_->setMarksVisible(zoom_ == 1);
    if (renderWindow_ && renderPicture_) {
        const QImage shown = document_->shown();
        renderPicture_->setPixmap(QPixmap::fromImage(renderDoubled_ && renderDoubled_->isChecked() ? shown.scaled(shown.size() * 2) : shown));
    }
    if (chkAutoCopy_->isChecked())
        copyToClipboard();
    showPens();
    bpUndo_->setEnabled(editing() && canUndo());
    bpRedo_->setEnabled(editing() && canRedo());
}

void EditTools::paintOver(QPainter& painter) const
{
    const Screen& screen = document_->screen;
    if (chkGrille_->isChecked())
        for (int x = gridStart_ * 16; x < screen.width(); x += gridSize_ * 16)
            markLine(painter, QPointF(x + 0.5, 0), QPointF(x + 0.5, screen.height()));
    if (chkTilesGrid_->isChecked()) {
        const int sizeX = tilesSizeX_->value() * screen.pixelWidth(0);
        const int sizeY = tilesSizeY_->value() * 2;
        for (int x = 0; x < screen.width(); x += sizeX)
            markLine(painter, QPointF(x + 0.5, 0), QPointF(x + 0.5, screen.height()));
        for (int y = 0; y < screen.height(); y += sizeY)
            markLine(painter, QPointF(0, y + 0.5), QPointF(screen.width(), y + 0.5));
    }
    if (!editing())
        return;
    if (dragging_)
        markRect(painter, QRect(dragFrom_, dragTo_));
    if (tool_ == Tool::Copy && blockWidth_ > 0 && blockAt_.x() >= 0) {
        // The block, where a click would put it.
        for (int y = 0; y < blockHeight_; y += 2) {
            const int row = blockAt_.y() + y;
            if (row >= screen.height())
                break;
            for (int x = 0; x < blockWidth_ && blockAt_.x() + x < screen.width(); ++x) {
                const int pen = blockPens_[static_cast<size_t>((y >> 1) * blockWidth_ + x)];
                painter.fillRect(blockAt_.x() + x, row, 1, 2, toQColor(screen.rgb(screen.colourAt(pen, row))));
            }
        }
        markRect(painter, QRect(blockAt_, QSize(blockWidth_, blockHeight_)));
    }
}

// ---- The mouse ----

void EditTools::mouse(QPoint at, Qt::MouseButtons buttons)
{
    if (!editing() || window_->mouseOwner() != this)
        return;
    const Screen& screen = document_->screen;
    const int incY = screen.mode >= kModeAsc0 && screen.mode < kModeCaptureSprites ? 8 : 2;
    const int row = (offsetY_ + at.y() / zoom_) & -incY;
    const int tx = screen.pixelWidth(row);
    const int x = (offsetX_ + at.x() / zoom_) & -tx;
    const int line = (row >> 1) - (document_->param.modeImpDraw && screen.height() == 544 && screen.width() == 768 ? 1 : 0);
    lblInfoPos_->setText(QStringLiteral("x:%1 y:%2 - @:%3")
                             .arg(x, 3, 10, QLatin1Char('0'))
                             .arg(line, 3, 10, QLatin1Char('0'))
                             .arg((x >> 3) + screen.address(line * 2), 4, 16, QLatin1Char('0'))
                             .toUpper()
                             .replace(QLatin1String("X:"), QLatin1String("x:"))
                             .replace(QLatin1String(" Y:"), QLatin1String(" y:")));
    if (x < 0 || row < 0)
        return;
    switch (tool_) {
    case Tool::Draw:
        toolDraw(at, buttons);
        break;
    case Tool::Zoom:
        toolZoom(at, buttons);
        break;
    case Tool::Copy:
        toolCopy(at, buttons);
        break;
    case Tool::Pick:
        toolPick(at, buttons);
        break;
    case Tool::Fill:
        toolFill(at, buttons);
        break;
    }
    if (buttons == Qt::NoButton) {
        bpUndo_->setEnabled(canUndo());
        bpRedo_->setEnabled(canRedo());
    }
}

void EditTools::setPixel(int x, int y, int pen, int width)
{
    Picture& picture = document_->picture();
    const int before = picture.pen(x, y);
    if (!drawing_) {
        // A new stroke: what was undone is no longer there to do again.
        steps_.resize(static_cast<size_t>(done_));
        stroke_.clear();
        drawing_ = true;
    }
    if (stroke_.empty() || stroke_.back().x != x || stroke_.back().y != y || stroke_.back().after != pen)
        stroke_.push_back({x, y, width, before, pen});
    picture.setPixel(x, y, pen, width);
    picture.computed = true;
}

void EditTools::endStroke()
{
    if (!drawing_)
        return;
    drawing_ = false;
    if (stroke_.empty())
        return;
    steps_.push_back(stroke_);
    stroke_.clear();
    done_ = static_cast<int>(steps_.size());
}

void EditTools::toolDraw(QPoint at, Qt::MouseButtons buttons)
{
    const Screen& screen = document_->screen;
    if (!(buttons & (Qt::LeftButton | Qt::RightButton))) {
        endStroke();
        return;
    }
    const int incY = screen.mode >= kModeAsc0 && screen.mode < kModeCaptureSprites ? 8 : 2;
    int row = (offsetY_ + at.y() / zoom_) & -incY;
    const int pen = buttons & Qt::LeftButton ? drawPen_ : undrawPen_;
    for (int y = 0; y < penWidth_ * incY; y += 2) {
        const int tx = screen.pixelWidth(row);
        const int realPen = pen % penCount(row);
        for (int dx = 0; dx < penWidth_ * tx; dx += tx) {
            const int x = (dx + offsetX_ + at.x() / zoom_) & -tx;
            if (x >= 0 && row >= 0 && x < screen.width() && row < screen.height())
                setPixel(x, row, realPen, tx);
        }
        row += 2;
    }
    document_->touch();
}

void EditTools::applyZoom()
{
    const Screen& screen = document_->screen;
    const int maxX = std::max(0, screen.width() - screen.width() / zoom_);
    const int maxY = std::max(0, screen.height() - screen.height() / zoom_);
    for (QScrollBar* bar : {hScroll_, vScroll_})
        bar->blockSignals(true);
    hScroll_->setRange(0, maxX);
    vScroll_->setRange(0, maxY);
    hScroll_->setPageStep(8);
    vScroll_->setPageStep(8);
    hScroll_->setValue(std::clamp(offsetX_, 0, maxX));
    vScroll_->setValue(std::clamp(offsetY_, 0, maxY));
    for (QScrollBar* bar : {hScroll_, vScroll_})
        bar->blockSignals(false);
    offsetX_ = (hScroll_->value() >> 3) << 3;
    offsetY_ = (vScroll_->value() >> 1) << 1;
    hScroll_->setEnabled(editing() && zoom_ > 1);
    vScroll_->setEnabled(editing() && zoom_ > 1);
    lblZoom_->setText(QString::number(zoom_) + QStringLiteral(":1"));
    document_->touch();
}

void EditTools::toolZoom(QPoint at, Qt::MouseButtons buttons)
{
    if (buttons & Qt::LeftButton) {
        if (zoom_ != 1)
            return;
        if (!dragging_) {
            dragging_ = true;
            dragFrom_ = at;
        }
        dragTo_ = at;
        view_->update();
    } else if (buttons & Qt::RightButton) {
        if (!unzooming_) {
            unzooming_ = true;
            if (zoom_ >= 2)
                zoom_ >>= 1;
            applyZoom();
        }
    } else {
        unzooming_ = false;
        if (!dragging_)
            return;
        dragging_ = false;
        const int w = dragTo_.x() - dragFrom_.x();
        const int h = dragTo_.y() - dragFrom_.y();
        if (w != 0 && h != 0) {
            zoom_ = std::max(1, std::min(std::abs(768 / w), std::abs(544 / h)) & 0x7E);
            offsetX_ = std::max(0, std::min(dragFrom_.x(), dragTo_.x()));
            offsetY_ = std::max(0, std::min(dragFrom_.y(), dragTo_.y()));
            applyZoom();
        }
        view_->update();
    }
}

void EditTools::releaseBlock()
{
    blockWidth_ = blockHeight_ = 0;
    blockPens_.clear();
    blockAt_ = QPoint(-1, -1);
    if (bpSaveWin_)
        bpSaveWin_->setEnabled(false);
    if (view_)
        view_->update();
}

void EditTools::toolCopy(QPoint at, Qt::MouseButtons buttons)
{
    const Screen& screen = document_->screen;
    const int row = ((at.y() / zoom_) & 0xFFE) * zoom_;
    const int tx = screen.pixelWidth(row);
    const int x = ((at.x() / zoom_) & -tx) * zoom_;
    if ((buttons & Qt::RightButton) && blockWidth_ > 0)
        releaseBlock();
    if ((buttons & Qt::LeftButton) && x >= 0 && row >= 0) {
        if (blockWidth_ > 0) {
            // The block is put where the mouse is.
            for (int y = 0; y < blockHeight_; y += 2) {
                const int width = screen.pixelWidth(y + row);
                for (int bx = 0; bx < blockWidth_; bx += width)
                    if (bx + x < screen.columns << 3 && y + row < screen.lines << 1)
                        setPixel(bx + x, y + row, blockPens_[static_cast<size_t>((y >> 1) * blockWidth_ + bx)], width);
            }
            document_->touch();
        } else {
            if (!dragging_) {
                dragging_ = true;
                dragFrom_ = QPoint(x, row);
            }
            dragTo_ = QPoint(x, row);
            view_->update();
        }
        return;
    }
    if (dragging_ && buttons == Qt::NoButton) {
        dragging_ = false;
        const QRect rect = QRect(dragFrom_, dragTo_).normalized();
        const int w = std::abs(dragTo_.x() - dragFrom_.x()) / zoom_;
        const int h = std::abs(dragTo_.y() - dragFrom_.y()) / zoom_;
        if (w != 0 && h != 0) {
            blockWidth_ = w;
            blockHeight_ = h & ~1;
            blockPens_.assign(static_cast<size_t>(blockWidth_ * std::max(1, blockHeight_ >> 1)), 0);
            const Picture& picture = document_->picture();
            for (int y = 0; y < blockHeight_; y += 2)
                for (int bx = 0; bx < blockWidth_; ++bx)
                    blockPens_[static_cast<size_t>((y >> 1) * blockWidth_ + bx)]
                        = static_cast<uint8_t>(picture.pen(offsetX_ + bx + rect.left() / zoom_, offsetY_ + y + rect.top() / zoom_));
            bpSaveWin_->setEnabled(true);
            // A block is put on the picture at its real size.
            if (zoom_ != 1) {
                zoom_ = 1;
                offsetX_ = offsetY_ = 0;
                applyZoom();
            }
        }
    }
    endStroke();
    blockAt_ = QPoint(x, row);
    view_->update();
}

void EditTools::toolPick(QPoint at, Qt::MouseButtons buttons)
{
    const Screen& screen = document_->screen;
    const int incY = screen.mode >= kModeAsc0 ? 8 : 2;
    const int row = (offsetY_ + at.y() / zoom_) & -incY;
    const int tx = screen.pixelWidth(row);
    const int x = (offsetX_ + at.x() / zoom_) & -tx;
    const int pen = document_->picture().pen(x, row);
    if (buttons & Qt::LeftButton)
        drawPen_ = pen;
    if (buttons & Qt::RightButton)
        undrawPen_ = pen;
    showPens();
}

void EditTools::toolFill(QPoint at, Qt::MouseButtons buttons)
{
    const Screen& screen = document_->screen;
    if (!(buttons & (Qt::LeftButton | Qt::RightButton))) {
        endStroke();
        return;
    }
    const int incY = screen.mode >= kModeAsc0 ? 8 : 2;
    const int row = (offsetY_ + at.y() / zoom_) & -incY;
    const int tx = screen.pixelWidth(row);
    const int x = (offsetX_ + at.x() / zoom_) & -tx;
    if (x < 0 || row < 0 || x >= screen.width() || row >= screen.height())
        return;
    const Picture& picture = document_->picture();
    const int fill = buttons & Qt::LeftButton ? drawPen_ : undrawPen_ % penCount(row);
    // What is filled is what has the colour of where it starts, whatever
    // pens give it.
    const auto colourAt = [&](int px, int py) { return screen.rgb(screen.colourAt(picture.pen(px, py), py)); };
    const int old = colourAt(x, row);
    if (old == screen.rgb(screen.colourAt(fill, row)))
        return;
    std::vector<QPoint> stack{QPoint(x, row)};
    while (!stack.empty()) {
        const QPoint a = stack.back();
        stack.pop_back();
        if (a.x() >= screen.width() || a.x() < 0 || a.y() >= screen.height() || a.y() < 0)
            continue;
        if (colourAt(a.x(), a.y()) != old || picture.pen(a.x(), a.y()) == fill)
            continue;
        setPixel(a.x(), a.y(), fill, tx);
        stack.push_back(QPoint(a.x() - tx, a.y()));
        stack.push_back(QPoint(a.x() + tx, a.y()));
        stack.push_back(QPoint(a.x(), a.y() - 2));
        stack.push_back(QPoint(a.x(), a.y() + 2));
    }
    document_->touch();
}

// ---- Undone, done again, turned over ----

void EditTools::undo()
{
    endStroke();
    if (!canUndo())
        return;
    --done_;
    Picture& picture = document_->picture();
    const std::vector<Change>& step = steps_[static_cast<size_t>(done_)];
    for (auto change = step.rbegin(); change != step.rend(); ++change)
        picture.setPixel(change->x, change->y, change->before, change->width);
    document_->touch();
}

void EditTools::redo()
{
    endStroke();
    if (!canRedo())
        return;
    Picture& picture = document_->picture();
    for (const Change& change : steps_[static_cast<size_t>(done_)])
        picture.setPixel(change.x, change.y, change.after, change.width);
    ++done_;
    document_->touch();
}

void EditTools::flip(bool rows)
{
    endStroke();
    const Screen& screen = document_->screen;
    Picture& picture = document_->picture();
    const int width = screen.width(), height = screen.height();
    if (rows) {
        // Top to bottom.
        for (int y = 0; y < (height >> 1) - 1; y += 2) {
            const int other = height - 2 - y;
            if (other <= y)
                break;
            for (int x = 0; x < width; ++x) {
                const int pen = picture.pen(x, y);
                picture.setPixel(x, y, picture.pen(x, other), 1);
                picture.setPixel(x, other, pen, 1);
            }
        }
        for (std::vector<Change>& step : steps_)
            for (Change& change : step)
                change.y = height - 2 - (change.y & ~1);
    } else {
        // Left to right.
        for (int y = 0; y < height; y += 2)
            for (int x = 0; x < width >> 1; ++x) {
                const int other = width - 1 - x;
                const int pen = picture.pen(x, y);
                picture.setPixel(x, y, picture.pen(other, y), 1);
                picture.setPixel(other, y, pen, 1);
            }
        for (std::vector<Change>& step : steps_)
            for (Change& change : step)
                change.x = width - change.x - change.width;
    }
    document_->touch();
}

// ---- Blocks as files ----

std::vector<uint8_t> EditTools::blockBytes(bool columns) const
{
    const Screen& screen = document_->screen;
    const int realWidth = (blockWidth_ >> 3) + ((blockWidth_ % 8) > 0 ? 1 : 0);
    const auto byteAt = [&](int x, int y, int maxLen) {
        int byte = 0;
        const int tx = screen.pixelWidth(y);
        for (int p = 0; p < std::min(8, maxLen); ++p)
            if ((p % tx) == 0) {
                const int px = (x << 3) + p;
                const int pen = px < blockWidth_ ? blockPens_[static_cast<size_t>((y >> 1) * blockWidth_ + px)] : 0;
                byte |= kPenBits[pen & 15] >> (p / tx);
            }
        return static_cast<uint8_t>(byte);
    };
    std::vector<uint8_t> bytes;
    if (columns) {
        for (int x = 0; x < realWidth; ++x)
            for (int y = 0; y < blockHeight_; y += 2)
                bytes.push_back(byteAt(x, y, blockWidth_));
    } else {
        for (int y = 0; y < blockHeight_; y += 2)
            for (int x = 0; x < realWidth; ++x)
                bytes.push_back(byteAt(x, y, blockWidth_ - (x << 3)));
    }
    return bytes;
}

void EditTools::saveBlock()
{
    if (blockWidth_ <= 0)
        return;
    using namespace asmgen;
    int chosen = 0;
    const QString path = prompts::saveName(window_, QStringLiteral("Save Win"), QString::fromStdString(document_->param.lastSavePath),
                                           {QStringLiteral("OCP window (*.win)"), QStringLiteral("Raw assembler data (*.asm)"),
                                            QStringLiteral("Raw assembler data in column order (*.asm)")},
                                           &chosen);
    if (path.isEmpty())
        return;
    bool written = false;
    if (chosen == 0) {
        std::vector<uint8_t> data = blockBytes(false);
        data.push_back(static_cast<uint8_t>(blockWidth_ & 0xFF));
        data.push_back(static_cast<uint8_t>(blockWidth_ >> 8));
        data.push_back(static_cast<uint8_t>((blockHeight_ >> 1) & 0xFF));
        data.push_back(static_cast<uint8_t>(blockHeight_ >> 9));
        written = writeHostFile(path, withAmsdosHeader(path.toStdString(), 0x4000, 0xCACA, data));
    } else {
        const std::vector<uint8_t> data = blockBytes(chosen == 2);
        AsmText sw(document_->screen);
        WriteHeader(sw, Text(Document::stamp().version));
        const QRect rect = QRect(dragFrom_, dragTo_).normalized();
        sw.WriteLine(S(";Width:") + dec(blockWidth_) + S(", Height:") + dec(blockHeight_));
        sw.WriteLine(S(";Posx:") + dec(rect.left()) + S(", Posy:") + dec(rect.top()));
        if (!data.empty())
            GenereDatas(sw, data.data(), static_cast<int>(data.size()), chosen == 1 ? (blockWidth_ + 7) >> 3 : blockHeight_ >> 1);
        written = writeHostText(path, sw.text());
    }
    if (!written)
        document_->fail(both("Impossible d'écrire ", "Cannot write ") + path);
}

void EditTools::loadBlock()
{
    const QString path = prompts::openName(window_, QStringLiteral("Load Win"), QString::fromStdString(document_->param.lastReadPath),
                                           {QStringLiteral("OCP window (*.win)")});
    if (path.isEmpty())
        return;
    const auto file = readHostFile(path);
    if (!file || !hasAmsdosHeader(*file) || file->size() < kAmsdosHeaderSize + 5) {
        document_->fail(texts::text("Main.prg.TxtInfo6"));
        return;
    }
    const size_t l = file->size();
    const int width = (*file)[l - 4] + ((*file)[l - 3] << 8);
    const int lines = (*file)[l - 2];
    const int realWidth = (width >> 3) + ((width % 8) > 0 ? 1 : 0);
    if (width <= 0 || lines <= 0 || kAmsdosHeaderSize + static_cast<size_t>(realWidth * lines) + 4 > l) {
        document_->fail(texts::text("Main.prg.TxtInfo6"));
        return;
    }
    Picture pens;
    const std::vector<uint8_t> data(file->begin() + kAmsdosHeaderSize, file->end() - 4);
    drawSprite(data, realWidth, lines, document_->screen, &pens, width);
    blockWidth_ = width;
    blockHeight_ = lines * 2;
    blockPens_.assign(static_cast<size_t>(width * lines), 0);
    for (int y = 0; y < lines; ++y)
        for (int x = 0; x < width; ++x)
            blockPens_[static_cast<size_t>(y * width + x)] = static_cast<uint8_t>(pens.pen(x, y * 2));
    if (zoom_ != 1) {
        zoom_ = 1;
        offsetX_ = offsetY_ = 0;
        applyZoom();
    }
    blockAt_ = QPoint(0, 0);
    bpSaveWin_->setEnabled(true);
    view_->update();
}

void EditTools::saveTileset()
{
    using namespace asmgen;
    const QString path = prompts::saveName(window_, QStringLiteral("Save as Tileset"), QString::fromStdString(document_->param.lastSavePath),
                                           {QStringLiteral("Bitmapfont (*.asm)")});
    if (path.isEmpty())
        return;
    AsmLabels labels;
    if (!window_->askLabels(QStringLiteral("Bitmap tiles"), QFileInfo(path).completeBaseName(), labels))
        return;
    const Screen& screen = document_->screen;
    const Picture& picture = document_->picture();
    const int sizeX = tilesSizeX_->value() * screen.pixelWidth(0);
    const int sizeY = tilesSizeY_->value() * 2;
    AsmText sw(screen);
    WriteHeader(sw);
    sw.WriteLine(Text(labels.media));
    std::vector<uint8_t> all;
    const auto cell = [&](int x, int y) {
        const std::vector<uint8_t> tile = spriteBytes(screen, picture, x, y, sizeX, sizeY);
        if (chkPack_->isChecked()) {
            all.insert(all.end(), tile.begin(), tile.end());
        } else {
            if (!tile.empty())
                GenereDatas(sw, tile.data(), static_cast<int>(tile.size()), sizeX >> 3, 0, Text(), true);
            sw.WriteLine(S(";"));
        }
        return static_cast<int>(tile.size());
    };
    int total = 0;
    if (rbRows_->isChecked()) {
        for (int y = 0; y < screen.height(); y += sizeY)
            for (int x = 0; x < screen.width(); x += sizeX)
                total += cell(x, y);
    } else {
        for (int x = 0; x < screen.width(); x += sizeX)
            for (int y = 0; y < screen.height(); y += sizeY)
                total += cell(x, y);
    }
    if (chkPack_->isChecked()) {
        const PackMethod method = document_->packMethod == PackMethod::ZX0Ovs ? PackMethod::ZX0 : document_->packMethod;
        const std::vector<uint8_t> packed = method == PackMethod::None ? all : pack(all, method);
        if (!packed.empty())
            GenereDatas(sw, packed.data(), static_cast<int>(packed.size()), 16);
    } else {
        sw.WriteLine(S("; Taille totale ") + dec(total) + S(" octets"));
    }
    if (document_->param.withPalette)
        GenerePalette(sw, document_->param, false, false, Text(labels.palette));
    if (!writeHostText(path, sw.text())) {
        document_->fail(both("Impossible d'écrire ", "Cannot write ") + path);
        return;
    }
    document_->say(both("Sauvegarde tileset assembleur ok.", "Tileset saved as a source."));
}

void EditTools::copyToClipboard()
{
    QApplication::clipboard()->setImage(document_->shown());
}
