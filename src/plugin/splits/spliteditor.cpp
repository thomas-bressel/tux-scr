#include "spliteditor.h"

#include <QCheckBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QVBoxLayout>

#include "colourdialog.h"
#include "controls.h"
#include "document.h"
#include "penbutton.h"
#include "pictures.h"
#include "pictureview.h"
#include "prompts.h"
#include "splits/splitasm.h"
#include "splittexts.h"
#include "swatch.h"
#include "texts.h"

namespace tuxscr {

namespace {

// The screen as a PC shows it.
constexpr int kWidth = SplitScreen::kColumns * 8;
constexpr int kHeight = SplitScreen::kLines * 2;
// The part of it shown enlarged eight times under it.
constexpr int kZoomWidth = kWidth / 8;
constexpr int kZoomHeight = 16;

// The colour a pen has for the Gate Array: the screen is a CPC's, and a
// colour of the Plus is brought to the nearest of the CPC's 27.
int cpcColour(const Screen& screen, int pen)
{
    const int colour = screen.palette[static_cast<size_t>(pen)];
    if (colour == kNoColour)
        return 0;
    return screen.plus ? cpcFromPlus(colour) : colour;
}

std::array<int, 17> cpcPalette(const Screen& screen)
{
    std::array<int, 17> colours{};
    for (int pen = 0; pen < 17; ++pen)
        colours[static_cast<size_t>(pen)] = cpcColour(screen, pen);
    return colours;
}

// A number that is typed whole before it is taken, as ConvImgCpc's are: the
// controls are set again from what is typed, which must not happen between
// two digits.
QSpinBox* wholeSpin(const char* name, int minimum, int maximum, int value, int step = 1)
{
    QSpinBox* spin = namedSpin(name, minimum, maximum, value);
    spin->setSingleStep(step);
    spin->setKeyboardTracking(false);
    return spin;
}

// A control that keeps its place when it is not shown: the others do not
// move.
void keepPlace(QWidget* widget)
{
    QSizePolicy policy = widget->sizePolicy();
    policy.setRetainSizeWhenHidden(true);
    widget->setSizePolicy(policy);
}

}  // namespace

// ---- Splits from a picture ----

ImportSplitDialog::ImportSplitDialog(SplitLines* lines, const QString& folder, QWidget* parent)
    : QDialog(parent)
    , lines_(lines)
    , folder_(folder)
{
    registerSplitTexts();
    setObjectName(QStringLiteral("ImportSplit"));
    setWindowTitle(texts::windowTitle(texts::text("ImportSplit.title")));
    auto* layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    auto* top = new QHBoxLayout;
    auto* side = new QVBoxLayout;
    QPushButton* bpLire = namedButton("bpLire", texts::text("ImportSplit.bpLire"));
    side->addWidget(bpLire);
    auto* penRow = new QHBoxLayout;
    QLabel* label1 = namedLabel("label1", texts::text("ImportSplit.label1"));
    numPen_ = namedSpin("numPen", 0, 15, 3);
    label1->setBuddy(numPen_);
    penRow->addWidget(label1);
    penRow->addWidget(numPen_);
    side->addLayout(penRow);
    side->addStretch();
    top->addLayout(side);
    view_ = new PictureView;
    view_->setObjectName(QStringLiteral("pictureSplit"));
    view_->setAccessibleName(texts::text("ImportSplit.pictureSplit"));
    view_->setFocusPolicy(Qt::NoFocus);
    // Until a picture is read there is nothing to show: the window's own
    // colour.
    QImage blank(kWidth, kHeight, QImage::Format_RGB32);
    blank.fill(palette().color(QPalette::Window));
    view_->setImage(blank);
    top->addWidget(view_);
    layout->addLayout(top);
    listErr_ = new QListWidget;
    listErr_->setObjectName(QStringLiteral("listErr"));
    listErr_->setAccessibleName(texts::text("ImportSplit.listErr"));
    listErr_->setFixedHeight(134);
    layout->addWidget(listErr_);
    connect(bpLire, &QPushButton::clicked, this, &ImportSplitDialog::readPicture);
}

void ImportSplitDialog::readPicture()
{
    const QString path = prompts::openName(this, texts::text("ImportSplit.bpLire"), folder_,
                                           {texts::text("ImportSplit.imageFiles"), texts::text("ImportSplit.allFiles")});
    if (path.isEmpty())
        return;
    folder_ = QFileInfo(path).absolutePath();
    const std::optional<std::vector<uint8_t>> file = readHostFile(path);
    QImage read;
    if (!file || !read.loadFromData(file->data(), static_cast<int>(file->size()))) {
        prompts::problem(this, texts::text("ImportSplit.unreadable"));
        return;
    }
    if (read.width() != kWidth / 2 || read.height() != kHeight / 2) {
        prompts::problem(this, texts::text("ImportSplit.badSize"));
        return;
    }
    // A pixel of the picture is one of mode 1: two across and two down on a
    // PC.
    const QImage doubled = read.convertToFormat(QImage::Format_RGB32).scaled(kWidth, kHeight, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    view_->setImage(doubled);
    listErr_->clear();
    for (const SplitImportNote& note : importSplits(toBitmap(doubled), numPen_->value(), *lines_)) {
        listErr_->addItem(texts::text(note.kind == SplitImportNote::Kind::TooMany ? "ImportSplit.tooMany" : "ImportSplit.tooShort").arg(note.line));
        listErr_->setCurrentRow(listErr_->count() - 1);
    }
}

// ---- The editor ----

SplitEditor::SplitEditor(Document* document, QWidget* parent)
    : QWidget(parent, Qt::Window)
    , document_(document)
    , screen_(cpcPalette(document->screen))
    , shown_(kWidth, kHeight)
    , folder_(QString::fromStdString(document->param.lastSavePath))
{
    registerSplitTexts();
    setObjectName(QStringLiteral("EditSplit"));
    auto* layout = new QHBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->addWidget(buildControls(), 0, Qt::AlignTop);
    layout->addWidget(buildPicture(), 0, Qt::AlignTop);
    applyTexts();
    updateScreen();
}

QWidget* SplitEditor::buildControls()
{
    auto* panel = new QWidget;
    auto* column = new QVBoxLayout(panel);
    column->setContentsMargins(0, 0, 0, 0);

    auto* files = new QHBoxLayout;
    QPushButton* bpLoad = namedButton("bpLoad");
    QPushButton* bpSave = namedButton("bpSave");
    files->addWidget(bpLoad);
    files->addWidget(bpSave);
    files->addStretch();
    column->addLayout(files);

    // The line, and what is its own: the pen, and where its first change
    // falls.
    auto* lineGrid = new QGridLayout;
    QLabel* label1 = namedLabel("label1");
    numLigne_ = wholeSpin("numLigne", 0, SplitScreen::kProgramLines - 1, 0);
    label1->setBuddy(numLigne_);
    QLabel* label15 = namedLabel("label15");
    numPen_ = wholeSpin("numPen", 0, 15, 0);
    label15->setBuddy(numPen_);
    QLabel* label14 = namedLabel("label14");
    retard_ = wholeSpin("retard", SplitScreen::kMinDelay, SplitScreen::kMinDelay + 32, SplitScreen::kMinDelay, 8);
    label14->setBuddy(retard_);
    chkChgt_ = namedCheck("chkChgt");
    lineGrid->addWidget(label1, 0, 0);
    lineGrid->addWidget(numLigne_, 0, 1);
    lineGrid->addWidget(label15, 0, 2, Qt::AlignRight);
    lineGrid->addWidget(numPen_, 0, 3);
    lineGrid->addWidget(label14, 1, 0);
    lineGrid->addWidget(retard_, 1, 1);
    lineGrid->addWidget(chkChgt_, 1, 2, 1, 2);
    column->addLayout(lineGrid);

    // The seven splits, one under the other.
    auto* group = new QGroupBox;
    group->setObjectName(QStringLiteral("grpSplit"));
    auto* grid = new QGridLayout(group);
    grid->setVerticalSpacing(10);
    const char* const widthLabels[kSplitsPerLine] = {"label2", "label3", "label4", "label5", "label6", "label7", "label16"};
    const char* const colourLabels[kSplitsPerLine] = {"label13", "label12", "label11", "label10", "label9", "label8", "label18"};
    for (int i = 0; i < kSplitsPerLine; ++i) {
        chkSplit_[i] = namedCheck(qPrintable(QStringLiteral("chkSplit%1").arg(i)));
        QLabel* widthLabel = namedLabel(widthLabels[i]);
        largSplit_[i] = wholeSpin(qPrintable(QStringLiteral("largSplit%1").arg(i)), 32, 384, 32, 8);
        widthLabel->setBuddy(largSplit_[i]);
        QLabel* colourLabel = namedLabel(colourLabels[i]);
        lblColor_[i] = new ColourSwatch(QSize(35, 35));
        lblColor_[i]->setObjectName(QStringLiteral("lblColor%1").arg(i));
        keepPlace(largSplit_[i]);
        keepPlace(lblColor_[i]);
        grid->addWidget(chkSplit_[i], i, 0);
        grid->addWidget(widthLabel, i, 1, Qt::AlignRight);
        grid->addWidget(largSplit_[i], i, 2);
        grid->addWidget(colourLabel, i, 3, Qt::AlignRight);
        grid->addWidget(lblColor_[i], i, 4);
        connect(chkSplit_[i], &QCheckBox::toggled, this, [this, i] {
            if (!loading_)
                enableSplit(i);
        });
        connect(largSplit_[i], &QSpinBox::valueChanged, this, [this, i] {
            if (!loading_)
                changeWidth(i);
        });
        connect(lblColor_[i], &QAbstractButton::clicked, this, [this, i] { changeColour(i); });
    }
    chkChangeMode_ = namedCheck("chkChangeMode");
    modeCpc_ = wholeSpin("modeCpc", 0, 2, 0);
    keepPlace(modeCpc_);
    modeCpc_->setVisible(false);
    grid->addWidget(chkChangeMode_, kSplitsPerLine, 0, 1, 4);
    grid->addWidget(modeCpc_, kSplitsPerLine, 4);
    column->addWidget(group);
    column->addStretch();

    connect(bpLoad, &QPushButton::clicked, this, &SplitEditor::load);
    connect(bpSave, &QPushButton::clicked, this, &SplitEditor::save);
    connect(numLigne_, &QSpinBox::valueChanged, this, [this] {
        if (!loading_)
            displayLine();
    });
    connect(numPen_, &QSpinBox::valueChanged, this, [this](int pen) {
        if (loading_)
            return;
        currentLine().pen = pen;
        render();
    });
    connect(retard_, &QSpinBox::valueChanged, this, [this](int delay) {
        if (loading_)
            return;
        currentLine().delay = delay;
        displayLine();
    });
    connect(chkChgt_, &QCheckBox::toggled, this, [this] { render(); });
    connect(chkChangeMode_, &QCheckBox::toggled, this, [this](bool on) {
        modeCpc_->setVisible(on);
        if (loading_)
            return;
        currentLine().changeMode = on;
        render();
    });
    connect(modeCpc_, &QSpinBox::valueChanged, this, [this](int mode) {
        if (loading_)
            return;
        currentLine().newMode = mode;
        render();
    });
    return panel;
}

QWidget* SplitEditor::buildPicture()
{
    auto* panel = new QWidget;
    auto* column = new QVBoxLayout(panel);
    column->setContentsMargins(0, 0, 0, 0);

    auto* top = new QHBoxLayout;
    QPushButton* bpCopieLigne = namedButton("bpCopieLigne");
    QPushButton* bpImportSplit = namedButton("bpImportSplit");
    QPushButton* bpGenAsm = namedButton("bpGenAsm");
    top->addWidget(bpCopieLigne);
    top->addStretch();
    top->addWidget(bpImportSplit);
    top->addWidget(bpGenAsm);
    column->addLayout(top);

    view_ = new PictureView;
    view_->setObjectName(QStringLiteral("pictureBox"));
    view_->setFocusPolicy(Qt::NoFocus);
    view_->setImage(toQImage(shown_));
    view_->addOverlay([this](QPainter& painter) { drawChanges(painter); });
    column->addWidget(view_);

    lblInfo_ = namedLabel("lblInfo", QStringLiteral("..."));
    column->addWidget(lblInfo_);

    // The colours the pens have when the screen starts.
    auto* palette = new QGroupBox;
    palette->setObjectName(QStringLiteral("groupPal"));
    auto* pens = new QHBoxLayout(palette);
    pens->setSpacing(8);
    for (int i = 0; i < 16; ++i) {
        colors_[i] = new PenButton(i);
        colors_[i]->setObjectName(QStringLiteral("colors%1").arg(i));
        pens->addWidget(colors_[i]);
        connect(colors_[i], &QAbstractButton::clicked, this, [this, i] { changeStartColour(i); });
    }
    pens->addStretch();
    column->addWidget(palette);

    zoom_ = new PictureView;
    zoom_->setObjectName(QStringLiteral("pictureZoom"));
    zoom_->setFocusPolicy(Qt::NoFocus);
    zoom_->setImage(QImage(kZoomWidth * 8, kZoomHeight * 8, QImage::Format_RGB32));
    column->addWidget(zoom_);
    hScrollZoom_ = new QScrollBar(Qt::Horizontal);
    hScrollZoom_->setObjectName(QStringLiteral("hScrollZoom"));
    hScrollZoom_->setRange(0, kWidth - kZoomWidth);
    hScrollZoom_->setSingleStep(8);
    hScrollZoom_->setPageStep(kZoomWidth);
    hScrollZoom_->setFocusPolicy(Qt::TabFocus);
    column->addWidget(hScrollZoom_);

    connect(bpCopieLigne, &QPushButton::clicked, this, &SplitEditor::copyLineAbove);
    connect(bpImportSplit, &QPushButton::clicked, this, &SplitEditor::importSplit);
    connect(bpGenAsm, &QPushButton::clicked, this, &SplitEditor::generate);
    connect(hScrollZoom_, &QScrollBar::valueChanged, this, [this] { render(); });
    connect(view_, &PictureView::mouse, this, [this](QPoint at, Qt::MouseButtons buttons) {
        lblInfo_->setText(texts::text("EditSplit.position").arg(at.x() >> 1, 3, 10, QLatin1Char('0')).arg(at.y() >> 1, 3, 10, QLatin1Char('0')));
        if (buttons != Qt::NoButton)
            pictureClicked(at);
    });
    connect(view_, &PictureView::mouseLeft, this, [this] { lblInfo_->setText(QStringLiteral("...")); });
    return panel;
}

void SplitEditor::applyTexts()
{
    texts::apply(this, "EditSplit");
    setWindowTitle(texts::windowTitle(texts::text("EditSplit.title")));
    // What a control is, said to those who do not see its label.
    for (int i = 0; i < kSplitsPerLine; ++i)
        largSplit_[i]->setAccessibleName(chkSplit_[i]->text() + QStringLiteral(", ") + texts::text("EditSplit.label2"));
    for (int i = 0; i < 16; ++i)
        colors_[i]->setAccessibleName(texts::text("EditSplit.groupPal") + QLatin1Char(' ') + QString::number(i));
    modeCpc_->setAccessibleName(texts::text("EditSplit.mode"));
    view_->setAccessibleName(texts::text("EditSplit.picture"));
    zoom_->setAccessibleName(texts::text("EditSplit.zoom"));
    hScrollZoom_->setAccessibleName(texts::text("EditSplit.zoomPlace"));
    if (!loading_)
        displayLine();
}

SplitLine& SplitEditor::currentLine()
{
    return screen_.lines[static_cast<size_t>(numLigne_->value())];
}

void SplitEditor::updateScreen()
{
    for (int pen = 0; pen < 16; ++pen)
        screen_.setStartColour(pen, cpcColour(document_->screen, pen));
    screen_.setPicture(document_->screen, document_->picture());
    displayLine();
}

void SplitEditor::updatePalette()
{
    for (int pen = 0; pen < 16; ++pen)
        colors_[pen]->setColour(toQColor(screen_.startRgb(pen)));
}

void SplitEditor::displayLine()
{
    loading_ = true;
    checkLastSplit();
    const SplitLine& line = currentLine();
    numPen_->setValue(line.pen);
    retard_->setValue(line.delay);
    // A split is in use if those before it are.
    bool inUse = true;
    for (int i = 0; i < kSplitsPerLine; ++i) {
        const Split& split = line.splits[static_cast<size_t>(i)];
        inUse = inUse && split.enabled;
        chkSplit_[i]->setChecked(inUse);
        largSplit_[i]->setVisible(inUse);
        lblColor_[i]->setVisible(inUse);
        largSplit_[i]->setValue(split.length);
        lblColor_[i]->setColour(toQColor(kCpcColours[split.colour >= 0 && split.colour < 27 ? split.colour : 0]));
        lblColor_[i]->setCaption(QString::number(split.colour));
        lblColor_[i]->setAccessibleName(chkSplit_[i]->text() + QStringLiteral(", ") + texts::text("EditSplit.label13") + QLatin1Char(' ')
                                        + QString::number(split.colour));
    }
    chkChangeMode_->setChecked(line.changeMode);
    modeCpc_->setVisible(line.changeMode);
    modeCpc_->setValue(line.newMode);
    loading_ = false;
    updatePalette();
    render();
}

// Six registers of the program are loaded with colours before the line: a
// seventh split whose colour is none of the six first has one of them
// loaded again, which takes two microseconds more of the sixth split.
void SplitEditor::checkLastSplit()
{
    SplitLine& line = currentLine();
    int minimum = 32;
    if (line.splits[6].enabled) {
        bool sameColour = false;
        for (size_t i = 0; i < 6; ++i)
            if (line.splits[6].colour == line.splits[i].colour)
                sameColour = true;
        if (!sameColour) {
            minimum = 48;
            if (line.splits[5].length < 48)
                line.splits[5].length = 48;
        }
    }
    largSplit_[5]->setMinimum(minimum);
}

void SplitEditor::render()
{
    if (loading_)
        return;
    screen_.calcPalette();
    screen_.render(shown_, 0);
    drawZoom();
    view_->setImage(toQImage(shown_));
}

void SplitEditor::drawZoom()
{
    // Sixteen rows around the line being set, and 96 pixels of them.
    const int top = std::max(0, std::min((numLigne_->value() << 1) - 7, shown_.height() - 14));
    QImage part(kZoomWidth, kZoomHeight, QImage::Format_RGB32);
    for (int y = 0; y < kZoomHeight; ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(part.scanLine(y));
        for (int x = 0; x < kZoomWidth; ++x)
            row[x] = 0xFF000000u | static_cast<QRgb>(shown_.colour(x + hScrollZoom_->value(), y + top).value());
    }
    zoom_->setImage(part.scaled(kZoomWidth * 8, kZoomHeight * 8, Qt::IgnoreAspectRatio, Qt::FastTransformation));
}

void SplitEditor::drawChanges(QPainter& painter)
{
    if (!chkChgt_->isChecked())
        return;
    // Where the colour changes on the line being set: a line down the
    // picture, in the window's two colours in turn, of which one at least
    // stands out from any colour of the picture.
    const QColor one = palette().color(QPalette::Text);
    const QColor other = palette().color(QPalette::Base);
    for (const int place : screen_.changes(numLigne_->value())) {
        const int x = place << 3;
        if (x < 0 || x >= kWidth)
            continue;
        for (int y = 0; y < kHeight; y += 4)
            painter.fillRect(x, y, 1, 4, (y & 4) ? one : other);
    }
}

// ---- Changes ----

void SplitEditor::changeWidth(int index)
{
    currentLine().splits[static_cast<size_t>(index)].length = largSplit_[index]->value() & 0xFFC;
    displayLine();
}

void SplitEditor::enableSplit(int index)
{
    if (index == 0 || chkSplit_[index - 1]->isChecked()) {
        currentLine().splits[static_cast<size_t>(index)].enabled = chkSplit_[index]->isChecked();
        displayLine();
    } else {
        // Not before the one above it.
        loading_ = true;
        chkSplit_[index]->setChecked(false);
        loading_ = false;
    }
}

void SplitEditor::changeColour(int index)
{
    Split& split = currentLine().splits[static_cast<size_t>(index)];
    int colour = split.colour;
    if (ColourDialog::choose(this, split.colour, colour, false)) {
        split.colour = colour;
        displayLine();
    }
}

void SplitEditor::changeStartColour(int pen)
{
    int colour = screen_.startColour(pen);
    if (colour < 0 || colour > 26)
        colour = 0;
    if (!ColourDialog::choose(this, pen, colour, false))
        return;
    screen_.setStartColour(pen, colour);
    updatePalette();
    render();
}

void SplitEditor::copyLineAbove()
{
    if (numLigne_->value() > 0) {
        currentLine() = screen_.lines[static_cast<size_t>(numLigne_->value()) - 1];
        displayLine();
    }
}

void SplitEditor::pictureClicked(QPoint at)
{
    numLigne_->setValue(std::clamp(at.y() >> 1, numLigne_->minimum(), numLigne_->maximum()));
    hScrollZoom_->setValue(std::clamp(at.x() - 48, hScrollZoom_->minimum(), hScrollZoom_->maximum()));
    render();
}

void SplitEditor::importSplit()
{
    ImportSplitDialog dialog(&screen_.lines, folder_, this);
    dialog.exec();
    displayLine();
}

// ---- Files ----

void SplitEditor::save()
{
    const QString path = prompts::saveName(this, texts::text("EditSplit.bpSave"), folder_, {texts::text("EditSplit.xmlFiles")});
    if (path.isEmpty())
        return;
    folder_ = QFileInfo(path).absolutePath();
    if (!writeHostText(path, splitXml(screen_.lines)))
        prompts::problem(this, texts::text("EditSplit.saveError"));
}

void SplitEditor::load()
{
    const QString path = prompts::openName(this, texts::text("EditSplit.bpLoad"), folder_, {texts::text("EditSplit.xmlFiles")});
    if (path.isEmpty())
        return;
    folder_ = QFileInfo(path).absolutePath();
    const std::optional<std::vector<uint8_t>> file = readHostFile(path);
    // What the file does not say is as in an editor just opened.
    SplitLines lines = freshSplitLines();
    if (file && readSplitXml(std::string(file->begin(), file->end()), lines))
        screen_.lines = lines;
    else
        prompts::problem(this, texts::text("EditSplit.readError"));
    displayLine();
}

void SplitEditor::generate()
{
    const QString path = prompts::saveName(this, texts::text("EditSplit.bpGenAsm"), folder_, {texts::text("EditSplit.asmFiles")});
    if (path.isEmpty())
        return;
    folder_ = QFileInfo(path).absolutePath();
    setEnabled(false);
    const std::string source = splitProgram(screen_, document_->packMethod, [this](int percent) { emit document_->packing(percent); });
    setEnabled(true);
    if (writeHostText(path, source))
        document_->say(texts::text("EditSplit.saved") + QFileInfo(path).fileName());
    else
        prompts::problem(this, texts::text("EditSplit.saveError"));
}

}  // namespace tuxscr
