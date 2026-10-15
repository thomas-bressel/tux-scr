#include "spriteeditor.h"

#include <algorithm>
#include <stack>

#include <QAbstractSpinBox>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include "colourdialog.h"
#include "controls.h"
#include "document.h"
#include "penbutton.h"
#include "prompts.h"
#include "spritefiles.h"
#include "spriteicons.h"
#include "spritepalettedialog.h"
#include "spritesavedialog.h"
#include "spritetexts.h"
#include "spriteviews.h"
#include "texts.h"

using namespace tuxscr;

namespace {

// A button with a pictogram, and a text or none.
QPushButton* iconButton(const char* name)
{
    QPushButton* button = namedButton(name);
    button->setIconSize(QSize(24, 24));
    return button;
}

// A square of a colour of the palette.
QLabel* swatch(const char* name)
{
    QLabel* label = namedLabel(name);
    label->setFixedSize(48, 48);
    label->setFrameShape(QFrame::Box);
    label->setAutoFillBackground(true);
    return label;
}

void paintSwatch(QLabel* label, const QColor& colour)
{
    QPalette look = label->palette();
    look.setColor(QPalette::Window, colour);
    label->setPalette(look);
}

// The file of the same name with the extension .kit, however it is written:
// the palette that goes with sprites. Empty if there is none.
QString kitBeside(const QString& path)
{
    const QFileInfo info(path);
    const QString exact = info.dir().filePath(info.completeBaseName() + QStringLiteral(".kit"));
    if (QFileInfo::exists(exact))
        return exact;
    const QStringList files = info.dir().entryList(QDir::Files);
    for (const QString& name : files) {
        const QFileInfo other(name);
        if (other.completeBaseName() == info.completeBaseName() && other.suffix().compare(QLatin1String("kit"), Qt::CaseInsensitive) == 0)
            return info.dir().filePath(name);
    }
    return {};
}

QString folderOf(const std::string& path)
{
    return QString::fromStdString(path);
}

}  // namespace

SpriteEditor::SpriteEditor(Document* document, QWidget* parent)
    : QDialog(parent)
    , document_(document)
{
    setObjectName(QStringLiteral("EditSprites"));
    const Screen* screen = &document_->screen;
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);
    // The window is as large as what it holds, as ConvImgCpc's is.
    layout->setSizeConstraint(QLayout::SetFixedSize);

    // The sprites of the bank, and the bank.
    auto* top = new QHBoxLayout;
    top->setSpacing(6);
    strip_ = new SpriteStrip(screen);
    strip_->setObjectName(QStringLiteral("pictAllSprites"));
    top->addWidget(strip_, 0, Qt::AlignTop);
    auto* bankBox = new QVBoxLayout;
    bankBox->setSpacing(4);
    comboBanque_ = new QComboBox;
    comboBanque_->setObjectName(QStringLiteral("comboBanque"));
    for (int bank = 0; bank < kSpriteBanks; ++bank)
        comboBanque_->addItem(QString());
    bankBox->addWidget(comboBanque_, 0, Qt::AlignLeft);
    auto* bankRow = new QHBoxLayout;
    bankRow->setSpacing(4);
    bpCopyBank_ = iconButton("bpCopyBank");
    bpPasteBank_ = iconButton("bpPasteBank");
    bpClearBank_ = iconButton("bpClearBank");
    bankRow->addWidget(bpCopyBank_);
    bankRow->addWidget(bpPasteBank_);
    bankRow->addWidget(bpClearBank_);
    bankBox->addLayout(bankRow);
    bankBox->addStretch();
    top->addLayout(bankBox);
    top->addStretch();
    layout->addLayout(top);

    // Under them: the tools, the sprite, its palette, the files and the test.
    auto* main = new QHBoxLayout;
    main->setSpacing(4);
    main->addWidget(buildTools(), 0, Qt::AlignTop);
    grid_ = new SpriteGrid(screen);
    grid_->setObjectName(QStringLiteral("pictEditSprite"));
    main->addWidget(grid_, 0, Qt::AlignTop);
    auto* palette = new QVBoxLayout;
    palette->setSpacing(0);
    for (int pen = 0; pen < 16; ++pen) {
        pens_[pen] = new PenButton(pen);
        pens_[pen]->setObjectName(QStringLiteral("lblColors%1").arg(pen));
        holders_[pen] = new PenHolder(pens_[pen]);
        palette->addWidget(holders_[pen]);
        connect(pens_[pen], &QAbstractButton::clicked, this, [this, pen] { penClicked(pen, false); });
        connect(pens_[pen], &PenButton::rightClicked, this, [this, pen] { penClicked(pen, true); });
    }
    palette->addStretch();
    main->addLayout(palette);
    main->addWidget(buildFiles(), 0, Qt::AlignTop);
    layout->addLayout(main);

    connect(strip_, &SpriteStrip::chosen, this, [this](int sprite) {
        if (sprite != sprite_)
            select(bank_, sprite);
    });
    connect(strip_, &SpriteStrip::pointed, this, &SpriteEditor::stripPointed);
    connect(grid_, &SpriteGrid::pointed, this, &SpriteEditor::gridPointed);
    connect(grid_, &SpriteGrid::left, this, [this] {
        for (PenHolder* holder : holders_)
            holder->setFramed(false);
    });
    connect(comboBanque_, &QComboBox::currentIndexChanged, this, [this](int bank) {
        if (bank >= 0 && bank != bank_)
            select(bank, sprite_);
    });
    connect(bpCopyBank_, &QPushButton::clicked, this, [this] {
        for (int sprite = 0; sprite < kSpritesPerBank; ++sprite)
            for (int y = 0; y < kSpriteSize; ++y)
                for (int x = 0; x < kSpriteSize; ++x)
                    copied_[sprite][x + y * kSpriteSize] = document_->screen.sprites[bank_][sprite][x][y];
    });
    connect(bpPasteBank_, &QPushButton::clicked, this, [this] {
        for (int sprite = 0; sprite < kSpritesPerBank; ++sprite)
            for (int y = 0; y < kSpriteSize; ++y)
                for (int x = 0; x < kSpriteSize; ++x)
                    document_->screen.sprites[bank_][sprite][x][y] = copied_[sprite][x + y * kSpriteSize];
        changed();
    });
    connect(bpClearBank_, &QPushButton::clicked, this, [this] {
        for (int sprite = 0; sprite < kSpritesPerBank; ++sprite)
            for (int y = 0; y < kSpriteSize; ++y)
                for (int x = 0; x < kSpriteSize; ++x)
                    document_->screen.sprites[bank_][sprite][x][y] = 0;
        changed();
    });

    connect(document_, &Document::changed, this, &SpriteEditor::refresh);
    const QList<QWidget*> controls = findChildren<QWidget*>();
    for (QWidget* control : controls)
        control->installEventFilter(this);
    installEventFilter(this);
    setIcons();
    retranslate();
}

// ---- The window's parts ----

QWidget* SpriteEditor::buildTools()
{
    auto* panel = new QWidget;
    auto* column = new QVBoxLayout(panel);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(4);

    auto* steps = new QHBoxLayout;
    steps->setSpacing(4);
    bpPrev_ = iconButton("bpPrev");
    bpSuiv_ = iconButton("bpSuiv");
    for (QPushButton* button : {bpPrev_, bpSuiv_}) {
        button->setFixedSize(40, 36);
        // It keeps its place when there is no sprite to go to.
        QSizePolicy policy = button->sizePolicy();
        policy.setRetainSizeWhenHidden(true);
        button->setSizePolicy(policy);
        steps->addWidget(button);
    }
    steps->addStretch();
    column->addLayout(steps);
    lblSelSprite_ = namedLabel("lblSelSprite");
    column->addWidget(lblSelSprite_);

    // The colour each button of the mouse draws with.
    label1_ = namedLabel("label1");
    column->addWidget(label1_);
    auto* leftRow = new QHBoxLayout;
    leftRow->setSpacing(4);
    lblPenLeft_ = swatch("lblPenLeft");
    leftRow->addWidget(lblPenLeft_);
    auto* parts = new QVBoxLayout;
    parts->setSpacing(0);
    const char* const names[3] = {"lblColSelV", "lblColSelR", "lblColSelB"};
    for (int i = 0; i < 3; ++i) {
        lblColSel_[i] = namedLabel(names[i]);
        parts->addWidget(lblColSel_[i]);
    }
    leftRow->addLayout(parts);
    leftRow->addStretch();
    column->addLayout(leftRow);
    label2_ = namedLabel("label2");
    column->addWidget(label2_);
    lblPenRight_ = swatch("lblPenRight");
    column->addWidget(lblPenRight_);

    grpEdition_ = new QGroupBox;
    grpEdition_->setObjectName(QStringLiteral("grpEdition"));
    auto* edition = new QGridLayout(grpEdition_);
    edition->setContentsMargins(4, 4, 4, 4);
    edition->setSpacing(4);
    bpCopySprite_ = iconButton("bpCopySprite");
    bpPasteSprite_ = iconButton("bpPasteSprite");
    bpHorizontalFlip_ = iconButton("bpHorizontalFlip");
    bpVerticalFlip_ = iconButton("bpVerticalFlip");
    bpRotateSprite_ = iconButton("bpRotateSprite");
    edition->addWidget(bpCopySprite_, 0, 0);
    edition->addWidget(bpPasteSprite_, 0, 1);
    edition->addWidget(bpHorizontalFlip_, 1, 0);
    edition->addWidget(bpVerticalFlip_, 1, 1);
    edition->addWidget(bpRotateSprite_, 2, 0);
    rbPt_ = namedRadio("rbPt");
    rbLine_ = namedRadio("rbLine");
    rbFill_ = namedRadio("rbFill");
    rbPt_->setChecked(true);
    edition->addWidget(rbPt_, 3, 0, 1, 2);
    edition->addWidget(rbLine_, 4, 0, 1, 2);
    edition->addWidget(rbFill_, 5, 0, 1, 2);
    column->addWidget(grpEdition_);

    // The sprite moved a pixel, each way.
    auto* cross = new QGridLayout;
    cross->setSpacing(2);
    const char* const scrolls[4] = {"bpScrUp", "bpScrDown", "bpScrLeft", "bpScrRight"};
    const Qt::ArrowType arrows[4] = {Qt::UpArrow, Qt::DownArrow, Qt::LeftArrow, Qt::RightArrow};
    for (int i = 0; i < 4; ++i) {
        bpScr_[i] = new QToolButton;
        bpScr_[i]->setObjectName(QLatin1String(scrolls[i]));
        bpScr_[i]->setArrowType(arrows[i]);
        bpScr_[i]->setFixedSize(24, 22);
    }
    cross->addWidget(bpScr_[2], 0, 0, 2, 1, Qt::AlignVCenter);
    cross->addWidget(bpScr_[0], 0, 1);
    cross->addWidget(bpScr_[1], 1, 1);
    cross->addWidget(bpScr_[3], 0, 2, 2, 1, Qt::AlignVCenter);
    auto* crossRow = new QHBoxLayout;
    crossRow->addStretch();
    crossRow->addLayout(cross);
    crossRow->addStretch();
    column->addLayout(crossRow);
    bpClearSprite_ = iconButton("bpClearSprite");
    column->addWidget(bpClearSprite_);
    column->addStretch();

    connect(bpPrev_, &QPushButton::clicked, this, &SpriteEditor::previous);
    connect(bpSuiv_, &QPushButton::clicked, this, &SpriteEditor::next);
    connect(bpCopySprite_, &QPushButton::clicked, this, [this] { keepSprite(copied_[0]); });
    connect(bpPasteSprite_, &QPushButton::clicked, this, [this] {
        putSprite(copied_[0]);
        changed();
    });
    connect(bpHorizontalFlip_, &QPushButton::clicked, this, [this] {
        move([](int x, int) { return x; }, [](int, int y) { return 15 - y; });
    });
    connect(bpVerticalFlip_, &QPushButton::clicked, this, [this] {
        move([](int x, int) { return 15 - x; }, [](int, int y) { return y; });
    });
    connect(bpRotateSprite_, &QPushButton::clicked, this, [this] {
        move([](int, int y) { return y; }, [](int x, int) { return 15 - x; });
    });
    connect(bpScr_[0], &QToolButton::clicked, this, [this] {
        move([](int x, int) { return x; }, [](int, int y) { return (y + 1) & 15; });
    });
    connect(bpScr_[1], &QToolButton::clicked, this, [this] {
        move([](int x, int) { return x; }, [](int, int y) { return (y + 15) & 15; });
    });
    connect(bpScr_[2], &QToolButton::clicked, this, [this] {
        move([](int x, int) { return (x + 1) & 15; }, [](int, int y) { return y; });
    });
    connect(bpScr_[3], &QToolButton::clicked, this, [this] {
        move([](int x, int) { return (x + 15) & 15; }, [](int, int y) { return y; });
    });
    connect(bpClearSprite_, &QPushButton::clicked, this, [this] {
        for (int y = 0; y < kSpriteSize; ++y)
            for (int x = 0; x < kSpriteSize; ++x)
                pixel(x, y) = 0;
        changed();
    });
    return panel;
}

QWidget* SpriteEditor::buildFiles()
{
    auto* panel = new QWidget;
    auto* column = new QVBoxLayout(panel);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(4);

    auto* files = new QHBoxLayout;
    files->setSpacing(4);
    bpRead_ = iconButton("bpRead");
    bpSave_ = iconButton("bpSave");
    bpSaveAll_ = iconButton("bpSaveAll");
    chkWithPal_ = namedCheck("chkWithPal");
    chkWithPal_->setChecked(true);
    files->addWidget(bpRead_);
    files->addWidget(bpSave_);
    files->addWidget(bpSaveAll_);
    files->addWidget(chkWithPal_);
    files->addStretch();
    column->addLayout(files);

    auto* colours = new QHBoxLayout;
    colours->setSpacing(4);
    bpReadPal_ = iconButton("bpReadPal");
    bpSavePal_ = iconButton("bpSavePal");
    bpGenPal_ = namedButton("bpGenPal");
    bpInversePalette_ = namedButton("bpInversePalette");
    colours->addWidget(bpReadPal_);
    colours->addWidget(bpSavePal_);
    colours->addWidget(bpGenPal_);
    colours->addWidget(bpInversePalette_);
    colours->addStretch();
    column->addLayout(colours);

    // The test: which sprites, how large, and where they are shown.
    auto* which = new QHBoxLayout;
    which->setSpacing(8);
    rb1Sprite_ = namedRadio("rb1Sprite");
    rb2Sprite_ = namedRadio("rb2Sprite");
    rb4Sprite_ = namedRadio("rb4Sprite");
    rb42Sprite_ = namedRadio("rb42Sprite");
    rb28sprite_ = namedRadio("rb28sprite");
    rbPosSpr_ = namedRadio("rbPosSpr");
    rb1Sprite_->setChecked(true);
    // The kinds of test are one choice, the tools of the drawing another.
    auto* tests = new QButtonGroup(this);
    for (QRadioButton* radio : {rb1Sprite_, rb2Sprite_, rb4Sprite_, rb42Sprite_, rb28sprite_, rbPosSpr_}) {
        tests->addButton(radio);
        which->addWidget(radio);
    }
    which->addStretch();
    column->addLayout(which);
    auto* sizes = new QHBoxLayout;
    sizes->setSpacing(6);
    label3_ = namedLabel("label3");
    zoomX_ = namedSpin("zoomX", 1, 3, 1);
    label3_->setBuddy(zoomX_);
    label4_ = namedLabel("label4");
    zoomY_ = namedSpin("zoomY", 1, 3, 1);
    label4_->setBuddy(zoomY_);
    bpTest_ = namedButton("bpTest");
    sizes->addWidget(label3_);
    sizes->addWidget(zoomX_);
    sizes->addSpacing(8);
    sizes->addWidget(label4_);
    sizes->addWidget(zoomY_);
    sizes->addStretch();
    sizes->addWidget(bpTest_);
    column->addLayout(sizes);
    test_ = new SpriteTestView;
    test_->setObjectName(QStringLiteral("pictTest"));
    column->addWidget(test_);

    connect(bpRead_, &QPushButton::clicked, this, &SpriteEditor::readSprites);
    connect(bpSave_, &QPushButton::clicked, this, [this] { saveSprites(false); });
    connect(bpSaveAll_, &QPushButton::clicked, this, [this] { saveSprites(true); });
    connect(bpReadPal_, &QPushButton::clicked, this, &SpriteEditor::readPalette);
    connect(bpSavePal_, &QPushButton::clicked, this, &SpriteEditor::savePalette);
    connect(bpGenPal_, &QPushButton::clicked, this, [this] {
        SpritePaletteDialog dialog(&document_->screen.spritePalette, 1, this);
        connect(&dialog, &SpritePaletteDialog::paletteChanged, this, &SpriteEditor::changed);
        dialog.exec();
    });
    connect(bpInversePalette_, &QPushButton::clicked, this, [this] {
        // Pen 0 is the transparent one: the fifteen others change ends.
        std::array<int, 16>& colours = document_->screen.spritePalette;
        std::reverse(colours.begin() + 1, colours.end());
        changed();
    });
    connect(bpTest_, &QPushButton::clicked, this, &SpriteEditor::runTest);
    connect(test_, &SpriteTestView::hovered, this, &SpriteEditor::testHovered);
    connect(test_, &SpriteTestView::pressed, this, &SpriteEditor::testPressed);
    connect(test_, &SpriteTestView::left, this, [this] { test_->setFrame(QRect()); });
    return panel;
}

void SpriteEditor::setIcons()
{
    const QColor ink = palette().color(QPalette::ButtonText);
    bpPrev_->setIcon(spriteIcon(SpriteIcon::Previous, ink));
    bpSuiv_->setIcon(spriteIcon(SpriteIcon::Next, ink));
    for (QPushButton* button : {bpCopySprite_, bpCopyBank_})
        button->setIcon(spriteIcon(SpriteIcon::Copy, ink));
    for (QPushButton* button : {bpPasteSprite_, bpPasteBank_})
        button->setIcon(spriteIcon(SpriteIcon::Paste, ink));
    for (QPushButton* button : {bpClearSprite_, bpClearBank_})
        button->setIcon(spriteIcon(SpriteIcon::Clear, ink));
    bpHorizontalFlip_->setIcon(spriteIcon(SpriteIcon::FlipDown, ink));
    bpVerticalFlip_->setIcon(spriteIcon(SpriteIcon::FlipAcross, ink));
    bpRotateSprite_->setIcon(spriteIcon(SpriteIcon::Rotate, ink));
    for (QPushButton* button : {bpRead_, bpReadPal_})
        button->setIcon(spriteIcon(SpriteIcon::Load, ink));
    for (QPushButton* button : {bpSave_, bpSaveAll_, bpSavePal_})
        button->setIcon(spriteIcon(SpriteIcon::Save, ink));
}

void SpriteEditor::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    // Not while the window is being made: its buttons are not all there.
    if (event->type() == QEvent::PaletteChange && layout() && layout()->count() > 1)
        setIcons();
}

void SpriteEditor::retranslate()
{
    setWindowTitle(texts::text("Main.bpEditSprites"));
    texts::apply(this, "EditSprites");
    // ConvImgCpc breaks these in two lines for its narrow buttons.
    bpSave_->setText(spriteText("EditSprites.bpSave"));
    bpSaveAll_->setText(spriteText("EditSprites.bpSaveAll"));
    for (int bank = 0; bank < kSpriteBanks; ++bank)
        comboBanque_->setItemText(bank, texts::text("EditSprites.bank") + QString::number(bank + 1));

    // What a button with a pictogram does, for those who point at it and for
    // those who do not see it.
    const std::pair<QWidget*, const char*> tips[] = {
        {bpPrev_, "EditSprites.tipPrev"},
        {bpSuiv_, "EditSprites.tipSuiv"},
        {comboBanque_, "EditSprites.tipBank"},
        {bpCopySprite_, "EditSprites.tipCopySprite"},
        {bpPasteSprite_, "EditSprites.tipPasteSprite"},
        {bpHorizontalFlip_, "EditSprites.tipHorizontalFlip"},
        {bpVerticalFlip_, "EditSprites.tipVerticalFlip"},
        {bpRotateSprite_, "EditSprites.tipRotateSprite"},
        {bpClearSprite_, "EditSprites.tipClearSprite"},
        {bpScr_[0], "EditSprites.tipScrUp"},
        {bpScr_[1], "EditSprites.tipScrDown"},
        {bpScr_[2], "EditSprites.tipScrLeft"},
        {bpScr_[3], "EditSprites.tipScrRight"},
        {bpCopyBank_, "EditSprites.tipCopyBank"},
        {bpPasteBank_, "EditSprites.tipPasteBank"},
        {bpClearBank_, "EditSprites.tipClearBank"},
        {bpRead_, "EditSprites.tipRead"},
        {bpSave_, "EditSprites.tipSave"},
        {bpSaveAll_, "EditSprites.tipSaveAll"},
        {chkWithPal_, "EditSprites.tipWithPal"},
        {bpReadPal_, "EditSprites.tipReadPal"},
        {bpSavePal_, "EditSprites.tipSavePal"},
        {bpGenPal_, "EditSprites.tipGenPal"},
        {bpInversePalette_, "EditSprites.tipInversePalette"},
        {bpTest_, "EditSprites.tipTest"},
        {rbPosSpr_, "EditSprites.tipPosSpr"},
        {strip_, "EditSprites.tipStrip"},
        {grid_, "EditSprites.tipGrid"},
    };
    for (const auto& [widget, name] : tips) {
        widget->setToolTip(texts::text(name));
        widget->setAccessibleName(texts::text(name));
    }
    strip_->setAccessibleName(texts::text("EditSprites.nameStrip"));
    grid_->setAccessibleName(texts::text("EditSprites.nameGrid"));
    test_->setAccessibleName(texts::text("EditSprites.nameTest"));
    lblPenLeft_->setAccessibleName(texts::text("EditSprites.namePenLeft"));
    lblPenRight_->setAccessibleName(texts::text("EditSprites.namePenRight"));
    refresh();
}

// ---- What is shown ----

uint8_t& SpriteEditor::pixel(int x, int y)
{
    return document_->screen.sprites[bank_][sprite_][x][y];
}

void SpriteEditor::present()
{
    // Sprites that were given no palette take the picture's.
    Screen& screen = document_->screen;
    const bool none = std::all_of(screen.spritePalette.begin(), screen.spritePalette.end(), [](int colour) { return colour == 0; });
    if (none) {
        for (size_t pen = 0; pen < 16; ++pen) {
            const int colour = screen.palette[pen];
            screen.spritePalette[pen] = colour >= kNoColour ? 0 : (screen.plus ? colour & 0xFFF : plusFromCpc(colour));
        }
        changed();
    } else {
        refresh();
    }
    show();
    raise();
    activateWindow();
}

void SpriteEditor::refresh()
{
    const Screen& screen = document_->screen;
    strip_->setBank(bank_);
    strip_->setCurrent(sprite_);
    grid_->setSprite(bank_, sprite_);
    lblSelSprite_->setText(texts::text("EditSprites.selSprite") + QString::number(sprite_));
    bpPrev_->setVisible(sprite_ > 0 || bank_ > 0);
    bpSuiv_->setVisible(sprite_ < kSpritesPerBank - 1 || bank_ < kSpriteBanks - 1);
    setQuietly<QComboBox, int>(comboBanque_, &QComboBox::setCurrentIndex, bank_);

    // The pens the sprite uses are ticked.
    bool used[16] = {};
    for (int y = 0; y < kSpriteSize; ++y)
        for (int x = 0; x < kSpriteSize; ++x)
            used[screen.sprites[bank_][sprite_][x][y] & 0x0F] = true;
    for (int pen = 0; pen < 16; ++pen) {
        pens_[pen]->setColour(spriteColour(screen.spritePalette[static_cast<size_t>(pen)]));
        pens_[pen]->setMark(used[pen] ? PenButton::Mark::Tick : PenButton::Mark::None);
        const QString tip = (pen == 0 ? texts::text("EditSprites.tipPen0") : texts::text("EditSprites.tipPen").arg(pen));
        pens_[pen]->setToolTip(tip);
        pens_[pen]->setAccessibleName(tip + (used[pen] ? texts::text("EditSprites.used") : QString()));
    }
    const int left = screen.spritePalette[static_cast<size_t>(pen_)];
    paintSwatch(lblPenLeft_, spriteColour(left));
    paintSwatch(lblPenRight_, spriteColour(screen.spritePalette[0]));
    lblColSel_[0]->setText(QStringLiteral("V:%1").arg(plusGreen(left)));
    lblColSel_[1]->setText(QStringLiteral("R:%1").arg(plusRed(left)));
    lblColSel_[2]->setText(QStringLiteral("B:%1").arg(plusBlue(left)));
}

void SpriteEditor::changed()
{
    // The other windows that show the sprites hear of it, and so does this
    // one.
    document_->touch();
}

void SpriteEditor::select(int bank, int sprite)
{
    bank_ = std::clamp(bank, 0, kSpriteBanks - 1);
    sprite_ = std::clamp(sprite, 0, kSpritesPerBank - 1);
    // A line that was being drawn belongs to the sprite that is left.
    lineStart_ = QPoint(-1, -1);
    refresh();
}

void SpriteEditor::previous()
{
    if (sprite_ > 0)
        select(bank_, sprite_ - 1);
    else if (bank_ > 0)
        select(bank_ - 1, kSpritesPerBank - 1);
}

void SpriteEditor::next()
{
    if (sprite_ < kSpritesPerBank - 1)
        select(bank_, sprite_ + 1);
    else if (bank_ < kSpriteBanks - 1)
        select(bank_ + 1, 0);
}

bool SpriteEditor::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        const bool plain = !(key->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
        // Those that take the arrows for themselves keep them.
        const bool taken = qobject_cast<QRadioButton*>(watched) || qobject_cast<QAbstractSpinBox*>(watched)
            || qobject_cast<QComboBox*>(watched) || qobject_cast<QLineEdit*>(watched);
        if (plain && !taken && (key->key() == Qt::Key_Left || key->key() == Qt::Key_Right)) {
            if (key->key() == Qt::Key_Left)
                previous();
            else
                next();
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

// ---- Drawing ----

void SpriteEditor::keepSprite(uint8_t* store)
{
    for (int y = 0; y < kSpriteSize; ++y)
        for (int x = 0; x < kSpriteSize; ++x)
            store[x + y * kSpriteSize] = pixel(x, y);
}

void SpriteEditor::putSprite(const uint8_t* store)
{
    for (int y = 0; y < kSpriteSize; ++y)
        for (int x = 0; x < kSpriteSize; ++x)
            pixel(x, y) = store[x + y * kSpriteSize];
}

void SpriteEditor::move(int (*fromX)(int x, int y), int (*fromY)(int x, int y))
{
    uint8_t before[kSpriteBytes];
    keepSprite(before);
    for (int y = 0; y < kSpriteSize; ++y)
        for (int x = 0; x < kSpriteSize; ++x)
            pixel(x, y) = before[fromX(x, y) + fromY(x, y) * kSpriteSize];
    changed();
}

void SpriteEditor::gridPointed(QPoint at, Qt::MouseButtons buttons)
{
    const int x = at.x(), y = at.y();
    if (x < 0 || y < 0 || x >= kSpriteSize || y >= kSpriteSize) {
        for (PenHolder* holder : holders_)
            holder->setFramed(false);
        // Let go outside the sprite, a line stays as it was last shown.
        if (buttons == Qt::NoButton)
            lineStart_ = QPoint(-1, -1);
        return;
    }
    // The colour under the mouse, before it is drawn over.
    const int under = pixel(x, y) & 0x0F;
    bool drawn = false;
    if (buttons == Qt::LeftButton) {
        if (rbFill_->isChecked()) {
            if (pixel(x, y) != pen_) {
                fill(x, y);
                drawn = true;
            }
        } else if (rbPt_->isChecked()) {
            drawn = pixel(x, y) != pen_;
            pixel(x, y) = static_cast<uint8_t>(pen_);
        } else if (lineStart_.x() < 0) {
            lineStart_ = at;
            keepSprite(beforeLine_);
        } else {
            // The line follows the mouse until the button is let go.
            putSprite(beforeLine_);
            drawLine(lineStart_.x(), lineStart_.y(), x, y);
            drawn = true;
        }
    } else if (buttons == Qt::RightButton) {
        drawn = pixel(x, y) != 0;
        pixel(x, y) = 0;
    } else if (lineStart_.x() >= 0) {
        putSprite(beforeLine_);
        drawLine(lineStart_.x(), lineStart_.y(), x, y);
        lineStart_ = QPoint(-1, -1);
        drawn = true;
    }
    for (int pen = 0; pen < 16; ++pen)
        holders_[pen]->setFramed(pen == under);
    if (drawn)
        changed();
}

// The line as ConvImgCpc draws it, pixel for pixel. One that ends where it
// starts draws nothing.
void SpriteEditor::drawLine(int x1, int y1, int x2, int y2)
{
    const auto plot = [this](int x, int y) {
        if (x >= 0 && y >= 0 && x < kSpriteSize && y < kSpriteSize)
            pixel(x, y) = static_cast<uint8_t>(pen_);
    };
    if (x2 < x1) {
        std::swap(x1, x2);
        std::swap(y1, y2);
    }
    int dx = x2 - x1;
    if (dx == 0 && y1 > y2)
        std::swap(y1, y2);
    int dy = y2 - y1;
    int e;
    if (dx != 0) {
        if (dy != 0) {
            if (dy > 0) {
                if (dx >= dy) {
                    e = dx;
                    dx *= 2;
                    dy *= 2;
                    do {
                        plot(x1, y1);
                        e -= dy;
                        if (e < 0) {
                            y1++;
                            e += dx;
                        }
                    } while (x1++ != x2);
                } else {
                    e = dy;
                    dy *= 2;
                    dx *= 2;
                    do {
                        plot(x1, y1);
                        e -= dx;
                        if (e < 0) {
                            x1++;
                            e += dy;
                        }
                    } while (y1++ != y2);
                }
            } else {
                if (dx >= -dy) {
                    e = dx;
                    dy *= 2;
                    dx *= 2;
                    do {
                        plot(x1, y1);
                        e += dy;
                        if (e < 0) {
                            y1--;
                            e += dx;
                        }
                    } while (x1++ != x2);
                } else {
                    e = dy;
                    dy *= 2;
                    dx *= 2;
                    do {
                        plot(x1, y1);
                        e += dx;
                        if (e > 0) {
                            x1++;
                            e += dy;
                        }
                    } while (y1-- != y2);
                }
            }
        } else {
            do {
                plot(x1, y1);
            } while (x1++ != x2);
        }
    } else if (dy != 0) {
        do {
            plot(x1, y1);
        } while (y1++ != y2);
    }
}

// The pixels of the same pen that touch each other, from this one on, take
// the drawing pen.
void SpriteEditor::fill(int x, int y)
{
    const uint8_t old = pixel(x, y);
    std::stack<QPoint> pixels;
    pixels.push(QPoint(x, y));
    while (!pixels.empty()) {
        const QPoint at = pixels.top();
        pixels.pop();
        if (at.x() < 0 || at.y() < 0 || at.x() >= kSpriteSize || at.y() >= kSpriteSize || pixel(at.x(), at.y()) != old)
            continue;
        pixel(at.x(), at.y()) = static_cast<uint8_t>(pen_);
        pixels.push(QPoint(at.x() - 1, at.y()));
        pixels.push(QPoint(at.x() + 1, at.y()));
        pixels.push(QPoint(at.x(), at.y() - 1));
        pixels.push(QPoint(at.x(), at.y() + 1));
    }
}

void SpriteEditor::stripPointed(int sprite)
{
    // The first one pointed at waits, framed; the second changes places
    // with it.
    const int waiting = strip_->marked();
    if (waiting < 0) {
        strip_->setMarked(sprite);
        return;
    }
    strip_->setMarked(-1);
    if (sprite != waiting) {
        Screen& screen = document_->screen;
        for (int y = 0; y < kSpriteSize; ++y)
            for (int x = 0; x < kSpriteSize; ++x)
                std::swap(screen.sprites[bank_][waiting][x][y], screen.sprites[bank_][sprite][x][y]);
    }
    changed();
}

void SpriteEditor::penClicked(int pen, bool right)
{
    if (!right) {
        pen_ = pen;
        refresh();
        return;
    }
    int colour = document_->screen.spritePalette[static_cast<size_t>(pen)];
    if (!ColourDialog::choose(this, pen, colour, true))
        return;
    document_->screen.spritePalette[static_cast<size_t>(pen)] = colour;
    changed();
}

// ---- The test ----

// A sprite's size in the test: a pixel is 2, 4 or 8 across and twice that
// down.
int SpriteEditor::testWidth() const
{
    return (1 << zoomX_->value()) << 4;
}

int SpriteEditor::testHeight() const
{
    return (2 << zoomY_->value()) << 4;
}

void SpriteEditor::drawTest(int sprite, int left, int top)
{
    const Screen& screen = document_->screen;
    const int width = 1 << zoomX_->value();
    const int height = 2 << zoomY_->value();
    QPainter painter(&test_->image());
    for (int y = 0; y < kSpriteSize; ++y)
        for (int x = 0; x < kSpriteSize; ++x)
            painter.fillRect(left + x * width, top + y * height, width, height,
                             spriteColour(screen.spritePalette[screen.sprites[bank_][sprite & 0x0F][x][y] & 0x0F]));
}

void SpriteEditor::runTest()
{
    const int count = rb1Sprite_->isChecked() ? 1 : rb2Sprite_->isChecked() ? 2 : 4;
    // One sprite is the one being drawn; four are those of its row of
    // four; sixteen are the whole bank.
    const int mask = rb1Sprite_->isChecked() ? 0x0F : rb2Sprite_->isChecked() ? 0x0C : 0x00;
    const int width = testWidth();
    const int height = testHeight();
    int sprite = sprite_ & mask;
    test_->image().fill(Qt::black);
    if (rbPosSpr_->isChecked()) {
        for (int s = 0; s < kSpritesPerBank; ++s)
            if (testPlaced_[s])
                drawTest(s, testPlaces_[s].x() * width, testPlaces_[s].y() * height);
    } else if (rb28sprite_->isChecked()) {
        // Two blocks of four by two, side by side.
        for (int block = 0; block < 2; ++block)
            for (int y = 0; y < 2; ++y)
                for (int x = block * 4; x < block * 4 + 4; ++x)
                    drawTest(sprite++, x * width, y * height);
    } else if (rb42Sprite_->isChecked()) {
        // Two blocks of two by four, side by side.
        for (int block = 0; block < 2; ++block)
            for (int y = 0; y < 4; ++y)
                for (int x = block * 2; x < block * 2 + 2; ++x)
                    drawTest(sprite++, x * width, y * height);
    } else {
        for (int y = 0; y < count; ++y)
            for (int x = 0; x < count; ++x)
                drawTest(sprite++, x * width, y * height);
    }
    test_->update();
}

void SpriteEditor::testHovered(QPoint at)
{
    if (!rbPosSpr_->isChecked() || at.x() < 0 || at.y() < 0) {
        test_->setFrame(QRect());
        return;
    }
    const int width = testWidth();
    const int height = testHeight();
    test_->setFrame(QRect((at.x() / width) * width, (at.y() / height) * height, width, height));
}

void SpriteEditor::testPressed(QPoint at)
{
    if (!rbPosSpr_->isChecked() || at.x() < 0 || at.y() < 0)
        return;
    // The sprite being drawn is put there, and stays there for the tests
    // that follow.
    const int width = testWidth();
    const int height = testHeight();
    testPlaces_[sprite_] = QPoint(at.x() / width, at.y() / height);
    testPlaced_[sprite_] = true;
    drawTest(sprite_, testPlaces_[sprite_].x() * width, testPlaces_[sprite_].y() * height);
    test_->update();
}

// ---- Files ----

void SpriteEditor::readSprites()
{
    const QString path = prompts::openName(this, texts::text("EditSprites.tipRead"), folderOf(document_->param.lastReadPath),
                                           {texts::text("EditSprites.TxtInfo1") + QStringLiteral(" (*.spr)")});
    if (path.isEmpty())
        return;
    const std::optional<std::vector<uint8_t>> file = readHostFile(path);
    if (!file || !readHardSpriteFile(*file, document_->screen, bank_)) {
        document_->fail(texts::text("EditSprites.TxtInfo2"));
        return;
    }
    // Their palette, if it is beside them.
    const QString kit = kitBeside(path);
    if (!kit.isEmpty())
        readKit(kit);
    document_->param.lastReadPath = QFileInfo(path).absolutePath().toStdString();
    changed();
}

bool SpriteEditor::readKit(const QString& path)
{
    const std::optional<std::vector<uint8_t>> file = readHostFile(path);
    // The transparent pen is shown black, unless the file says otherwise.
    std::array<int, 16> colours = document_->screen.spritePalette;
    colours[0] = 0;
    if (!file || !readKitFile(*file, colours))
        return false;
    document_->screen.spritePalette = colours;
    document_->say(texts::text("Main.prg.TxtInfo12"));
    return true;
}

void SpriteEditor::readPalette()
{
    const QString path = prompts::openName(this, texts::text("EditSprites.tipReadPal"), folderOf(document_->param.lastReadPath),
                                           {texts::text("EditSprites.paletteFilter")});
    if (path.isEmpty())
        return;
    if (!readKit(path)) {
        document_->fail(texts::text("EditSprites.TxtInfo6"));
        return;
    }
    changed();
}

void SpriteEditor::savePalette()
{
    const QString path = prompts::saveName(this, texts::text("EditSprites.tipSavePal"), folderOf(document_->param.lastSavePath),
                                           {texts::text("EditSprites.paletteFilter")});
    if (path.isEmpty())
        return;
    document_->saveKit(path, false);
}

void SpriteEditor::saveSprites(bool allBanks)
{
    const QStringList filters = {texts::text("EditSprites.TxtInfo1") + QStringLiteral(" (*.spr)"),
                                 texts::text("EditSprites.TxtInfo3") + QStringLiteral(" (*.asm)"),
                                 texts::text("EditSprites.TxtInfo4") + QStringLiteral(" (*.asm)"),
                                 texts::text("EditSprites.TxtInfo8") + QStringLiteral(" (*.asm)")};
    int chosen = 0;
    const QString path = prompts::saveName(this, texts::text(allBanks ? "EditSprites.tipSaveAll" : "EditSprites.tipSave"),
                                           folderOf(document_->param.lastSavePath), filters, &chosen);
    if (path.isEmpty() || chosen < 0 || chosen > 3)
        return;
    const Screen& screen = document_->screen;
    const SpriteSpan span = spriteSpan(screen, bank_, allBanks);
    const QFileInfo info(path);
    const bool withPalette = chkWithPal_->isChecked();
    bool saved = false;
    if (chosen == 0) {
        saved = writeHostFile(path, hardSpriteFile(path.toStdString(), screen, span));
        // The palette goes beside the sprites, as Impdraw wants it.
        if (saved && withPalette)
            document_->saveKit(info.dir().filePath(info.completeBaseName() + QStringLiteral(".kit")), false);
    } else {
        // The sprites of one bank packed as one block: how many is asked.
        const bool askCount = chosen == 3 && !allBanks;
        SpriteSaveDialog dialog(QStringLiteral("Sprites"), info.completeBaseName(), true, chosen == 2,
                                askCount ? QStringLiteral("Number of sprites to save") : QString(), span.sprites,
                                kSpritesPerBank, this);
        if (dialog.exec() != QDialog::Accepted)
            return;
        const SpriteLabels labels = dialog.labels();
        const AsmStamp stamp = Document::stamp();
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const std::string text = chosen == 1 ? hardSpritesAsm(screen, span, labels, withPalette, stamp)
            : chosen == 2 ? packedSpritesAsm(screen, span, labels, withPalette, document_->packMethod, stamp)
            : packedSpriteSetAsm(screen, span, askCount ? dialog.count() : span.sprites, labels, withPalette, document_->packMethod, stamp);
        QApplication::restoreOverrideCursor();
        saved = writeHostText(path, text);
    }
    if (!saved) {
        document_->fail(texts::text("EditSprites.TxtInfo5"));
        return;
    }
    document_->param.lastSavePath = info.absolutePath().toStdString();
}
