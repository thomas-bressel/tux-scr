#include "rastertable.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QVBoxLayout>

#include "controls.h"
#include "document.h"
#include "formats/screenfile.h"
#include "palettemaker.h"
#include "penbutton.h"
#include "pictures.h"
#include "pictureview.h"
#include "prompts.h"
#include "shadecontrols.h"
#include "splittexts.h"
#include "swatch.h"
#include "texts.h"

using namespace tuxscr;

namespace {

// The picture the table is for: an overscan one, as a PC shows it.
constexpr int kWidth = 768;
constexpr int kHeight = kMaxLines * 2;

// A box a number of a few digits is typed in.
QLineEdit* numberBox(const char* name, const QString& text, int digits)
{
    auto* box = new QLineEdit(text);
    box->setObjectName(QLatin1String(name));
    box->setMaxLength(digits);
    box->setAlignment(Qt::AlignRight);
    box->setFixedWidth(box->fontMetrics().horizontalAdvance(QString(digits + 1, QLatin1Char('0'))) + 16);
    return box;
}

}  // namespace

// ---- Which colour ----

SelectColourDialog::SelectColourDialog(const Screen& screen, QWidget* parent)
    : QDialog(parent)
{
    registerSplitTexts();
    setObjectName(QStringLiteral("SelectColor"));
    setWindowTitle(texts::text("SelectColor.title"));
    auto* layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->addWidget(namedLabel("label1", texts::text("SelectColor.label1")), 0, Qt::AlignHCenter);
    auto* pens = new QHBoxLayout;
    pens->setSpacing(8);
    pens->addStretch();
    const int count = screen.maxPen(0);
    for (int i = 0; i < count; ++i) {
        auto* pen = new PenButton(i);
        pen->setObjectName(QStringLiteral("color%1").arg(i));
        pen->setColour(toQColor(screen.rgb(screen.palette[static_cast<size_t>(i)])));
        pen->setAccessibleName(texts::text("SelectColor.pen").arg(i));
        pens->addWidget(pen);
        connect(pen, &QAbstractButton::clicked, this, [this, i] {
            chosen_ = i;
            accept();
        });
    }
    pens->addStretch();
    layout->addLayout(pens);
    auto* hint = namedLabel("lblHint", texts::text("SelectColor.lblHint"));
    hint->setWordWrap(true);
    hint->setMinimumWidth(360);
    layout->addWidget(hint);
}

// ---- The table ----

RasterTable::RasterTable(Document* document, int rastered, QWidget* parent)
    : QWidget(parent, Qt::Window)
    , document_(document)
    , rastered_(rastered)
    , folder_(QString::fromStdString(document->param.lastSavePath))
{
    registerSplitTexts();
    setObjectName(QStringLiteral("RasterTablePlus"));
    auto* layout = new QHBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->addWidget(buildControls(), 0, Qt::AlignTop);

    view_ = new PictureView;
    view_->setObjectName(QStringLiteral("pictureBox"));
    view_->setImage(QImage(kWidth, kHeight, QImage::Format_RGB32));
    layout->addWidget(view_, 0, Qt::AlignTop);
    connect(view_, &PictureView::mouse, this, [this](QPoint at, Qt::MouseButtons) {
        shownLine_ = at.y() / 2;
        showLine();
    });

    retranslate();
    drawLines();
}

QWidget* RasterTable::buildControls()
{
    auto* panel = new QWidget;
    auto* column = new QVBoxLayout(panel);
    column->setContentsMargins(0, 0, 0, 0);

    auto* files = new QHBoxLayout;
    QPushButton* bpLoad = namedButton("bpLoad");
    QPushButton* bpSave = namedButton("bpSave");
    files->addWidget(bpLoad);
    files->addWidget(bpSave);
    column->addLayout(files);

    auto* all = new QHBoxLayout;
    QPushButton* bpClearAll = namedButton("bpClearAll");
    QPushButton* bpAddLine = namedButton("bpAddLine");
    all->addWidget(bpClearAll);
    all->addWidget(bpAddLine);
    column->addLayout(all);

    auto* shown = new QHBoxLayout;
    chkSprites_ = namedCheck("chkSprites");
    lblLine_ = namedLabel("lblLine");
    lblLine_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    shown->addWidget(chkSprites_);
    shown->addStretch();
    shown->addWidget(lblLine_);
    column->addLayout(shown);

    // One colour for so many lines, from a line on, downwards or upwards.
    auto* constant = new QGroupBox;
    constant->setObjectName(QStringLiteral("groupBox2"));
    auto* constantColumn = new QVBoxLayout(constant);
    auto* addRow = new QHBoxLayout;
    QPushButton* bpAddConstant = namedButton("bpAddConstant");
    txbConstant_ = numberBox("txbConstant", QStringLiteral("1"), 3);
    QLabel* label6 = namedLabel("label6");
    auto* directions = new QVBoxLayout;
    directions->setSpacing(0);
    rbPlus_ = namedRadio("rbPlus", QStringLiteral("+"));
    QRadioButton* rbMoins = namedRadio("rbMoins", QStringLiteral("-"));
    rbPlus_->setChecked(true);
    directions->addWidget(rbPlus_);
    directions->addWidget(rbMoins);
    QLabel* label7 = namedLabel("label7");
    txbStart_ = numberBox("txbStart", QStringLiteral("1"), 3);
    label7->setBuddy(txbStart_);
    addRow->addWidget(bpAddConstant);
    addRow->addWidget(txbConstant_);
    addRow->addWidget(label6);
    addRow->addLayout(directions);
    addRow->addStretch();
    addRow->addWidget(label7);
    addRow->addWidget(txbStart_);
    constantColumn->addLayout(addRow);
    bpUndo_ = namedButton("bpUndo");
    bpUndo_->setEnabled(false);
    constantColumn->addWidget(bpUndo_, 0, Qt::AlignLeft);

    // Green, red, blue: the order ConvImgCpc has them in.
    shade_ = new ShadeControls(QStringLiteral("Start"), this);
    struct Row {
        ShadeControls::Component component;
        const char* label;
        const char* less;
        const char* more;
    };
    const Row rows[3] = {{ShadeControls::Green, "label3", "bpVMoins", "bpVPlus"},
                         {ShadeControls::Red, "label4", "bpRMoins", "bpRPlus"},
                         {ShadeControls::Blue, "label5", "bpBMoins", "bpBPlus"}};
    auto* grid = new QGridLayout;
    for (int r = 0; r < 3; ++r) {
        QSlider* slider = shade_->slider(rows[r].component);
        QLabel* letter = namedLabel(rows[r].label);
        letter->setBuddy(slider);
        QPushButton* less = namedButton(rows[r].less, QStringLiteral("-"));
        QPushButton* more = namedButton(rows[r].more, QStringLiteral("+"));
        for (QPushButton* button : {less, more}) {
            QFont bold = button->font();
            bold.setBold(true);
            button->setFont(bold);
            button->setFixedWidth(30);
        }
        grid->addWidget(letter, r, 0);
        grid->addWidget(less, r, 1);
        grid->addWidget(slider, r, 2);
        grid->addWidget(more, r, 3);
        grid->addWidget(shade_->box(rows[r].component), r, 4);
        connect(less, &QPushButton::clicked, slider, [slider] {
            if (slider->value() > 0)
                slider->setValue(slider->value() - 1);
        });
        connect(more, &QPushButton::clicked, slider, [slider] {
            if (slider->value() < 15)
                slider->setValue(slider->value() + 1);
        });
    }
    constantColumn->addLayout(grid);
    constantColumn->addWidget(shade_->swatch(), 0, Qt::AlignHCenter);
    column->addWidget(constant);

    auto* scrollRow = new QGridLayout;
    QPushButton* bpScrollUp = namedButton("bpScrollUp");
    QPushButton* bpScrollDown = namedButton("bpScrollDown");
    txbScroll_ = numberBox("txbScroll", QStringLiteral("1"), 3);
    scrollRow->setColumnStretch(0, 1);
    scrollRow->addWidget(bpScrollUp, 0, 1);
    scrollRow->addWidget(bpScrollDown, 1, 1);
    scrollRow->addWidget(txbScroll_, 0, 2, 2, 1);
    scrollRow->addWidget(namedLabel("label8"), 0, 3, 2, 1);
    column->addLayout(scrollRow);

    auto* output = new QGroupBox;
    output->setObjectName(QStringLiteral("groupBox1"));
    auto* outputGrid = new QGridLayout(output);
    QLabel* label1 = namedLabel("label1");
    QLabel* label2 = namedLabel("label2");
    txbLineStart_ = numberBox("txbLineStart", QString(), 3);
    txbLineEnd_ = numberBox("txbLineEnd", QString(), 3);
    label1->setBuddy(txbLineStart_);
    label2->setBuddy(txbLineEnd_);
    bpGenerate_ = namedButton("bpGenerate");
    QPushButton* bpGenFade = namedButton("bpGenFade");
    QPushButton* bpCopyToClipboard = namedButton("bpCopyToClipboard");
    QPushButton* bpImportKit = namedButton("bpImportKit");
    outputGrid->addWidget(label1, 0, 0);
    outputGrid->addWidget(txbLineStart_, 0, 1, Qt::AlignLeft);
    outputGrid->addWidget(label2, 1, 0);
    outputGrid->addWidget(txbLineEnd_, 1, 1, Qt::AlignLeft);
    outputGrid->addWidget(bpGenerate_, 2, 0);
    outputGrid->addWidget(bpGenFade, 2, 1);
    outputGrid->addWidget(bpCopyToClipboard, 3, 0);
    outputGrid->addWidget(bpImportKit, 3, 1);
    column->addWidget(output);
    column->addStretch();

    connect(bpLoad, &QPushButton::clicked, this, &RasterTable::load);
    connect(bpSave, &QPushButton::clicked, this, &RasterTable::save);
    connect(bpClearAll, &QPushButton::clicked, this, &RasterTable::clearAll);
    connect(bpAddLine, &QPushButton::clicked, this, &RasterTable::addLines);
    connect(chkSprites_, &QCheckBox::toggled, this, &RasterTable::drawLines);
    connect(bpAddConstant, &QPushButton::clicked, this, &RasterTable::addConstant);
    connect(bpUndo_, &QPushButton::clicked, this, &RasterTable::undo);
    connect(bpScrollUp, &QPushButton::clicked, this, [this] { scroll(true); });
    connect(bpScrollDown, &QPushButton::clicked, this, [this] { scroll(false); });
    connect(bpGenerate_, &QPushButton::clicked, this, [this] { generate(false); });
    connect(bpGenFade, &QPushButton::clicked, this, [this] { generate(true); });
    connect(bpCopyToClipboard, &QPushButton::clicked, this, &RasterTable::copyToClipboard);
    connect(bpImportKit, &QPushButton::clicked, this, &RasterTable::importKit);
    return panel;
}

void RasterTable::retranslate()
{
    texts::apply(this, "RasterTablePlus");
    setWindowTitle(texts::text("RasterTablePlus.title"));
    // Room for the longest line there is.
    lblLine_->setMinimumWidth(lblLine_->fontMetrics().horizontalAdvance(texts::text("RasterTablePlus.lblLine").arg(271)) + 4);
    showLine();
    // What a control is, said to those who do not see its label.
    const QString colour = texts::text("RasterTablePlus.groupBox2");
    const struct {
        ShadeControls::Component component;
        const char* letter;
        const char* less;
        const char* more;
    } rows[3] = {{ShadeControls::Green, "V", "bpVMoins", "bpVPlus"}, {ShadeControls::Red, "R", "bpRMoins", "bpRPlus"}, {ShadeControls::Blue, "B", "bpBMoins", "bpBPlus"}};
    for (const auto& row : rows) {
        const QString name = colour + QLatin1Char(' ') + QLatin1String(row.letter);
        shade_->slider(row.component)->setAccessibleName(name);
        shade_->box(row.component)->setAccessibleName(name);
        findChild<QPushButton*>(QLatin1String(row.less))->setAccessibleName(name + QStringLiteral(" -"));
        findChild<QPushButton*>(QLatin1String(row.more))->setAccessibleName(name + QStringLiteral(" +"));
    }
    shade_->swatch()->setAccessibleName(colour);
    txbConstant_->setAccessibleName(texts::text("RasterTablePlus.bpAddConstant") + QLatin1Char(' ') + texts::text("RasterTablePlus.label6"));
    txbScroll_->setAccessibleName(texts::text("RasterTablePlus.bpScrollUp") + QLatin1Char(' ') + texts::text("RasterTablePlus.label8"));
    view_->setAccessibleName(texts::text("RasterTablePlus.title"));
}

void RasterTable::showLine()
{
    lblLine_->setText(shownLine_ < 0 ? QString() : texts::text("RasterTablePlus.lblLine").arg(shownLine_));
}

// ---- The picture ----

void RasterTable::drawLines()
{
    txbStart_->setText(QString::number(newStart_));
    const QImage shown = document_->shown().convertToFormat(QImage::Format_RGB32);
    QImage image(kWidth, kHeight, QImage::Format_RGB32);
    for (int i = 0; i < kMaxLines; ++i) {
        const QRgb lineColour = static_cast<QRgb>(rasterRgb(raster_.line[static_cast<size_t>(i)])) | 0xFF000000u;
        const QRgb* source = i * 2 < shown.height() ? reinterpret_cast<const QRgb*>(shown.constScanLine(i * 2)) : nullptr;
        QRgb* first = reinterpret_cast<QRgb*>(image.scanLine(i * 2));
        QRgb* second = reinterpret_cast<QRgb*>(image.scanLine(i * 2 + 1));
        for (int x = 0; x < kWidth; x += 2) {
            // What the picture does not cover is black.
            QRgb colour = source && x < shown.width() ? (source[x] | 0xFF000000u) : 0xFF000000u;
            if (rastered_ == -1 || static_cast<QRgb>(rastered_) == (colour & 0xFFFFFF))
                colour = lineColour;
            first[x] = first[x + 1] = second[x] = second[x + 1] = colour;
        }
    }
    txbLineStart_->setText(QString::number(raster_.minLine));
    txbLineEnd_->setText(QString::number(raster_.maxLine));
    bpGenerate_->setEnabled(raster_.minLine < raster_.maxLine);

    if (chkSprites_->isChecked())
        drawSprites(image);
    view_->setImage(image);
}

void RasterTable::drawSprites(QImage& image) const
{
    // The sprites of the first bank, each where it is put and as large as
    // it is asked: a pixel of a sprite is so many pixels of mode 2 across
    // and so many lines down. Their pen 0 is not drawn.
    const Screen& screen = document_->screen;
    const Document::SpritePlaces& places = document_->spritePlaces;
    const int bank = 0;
    for (int sprite = 0; sprite < 16; ++sprite) {
        if (places.zoomX[sprite] == 0 || places.zoomY[sprite] == 0)
            continue;
        // The ASIC's zoom 3 is four times the size.
        const auto times = [](int zoom) { return zoom >= 3 ? 4 : zoom; };
        const int sizeX = times(places.zoomX[sprite]);
        const int sizeY = times(places.zoomY[sprite]) << 1;
        const int left = places.x[sprite];
        const int top = places.y[sprite] << 1;
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                const int pen = screen.sprites[bank][sprite][x][y];
                if (pen == 0)
                    continue;
                const QRgb colour = static_cast<QRgb>(screen.rgb(screen.spritePalette[static_cast<size_t>(pen & 0x0F)])) | 0xFF000000u;
                for (int zx = 0; zx < sizeX; ++zx)
                    for (int zy = 0; zy < sizeY; ++zy) {
                        const int px = zx + (x * sizeX) + left;
                        const int py = zy + (y * sizeY) + top;
                        if (px >= 0 && py >= 0 && px < screen.width() && py < screen.height() && px < image.width() && py < image.height())
                            image.setPixel(px, py, colour);
                    }
            }
    }
}

// ---- The lines ----

void RasterTable::addConstant()
{
    const int direction = rbPlus_->isChecked() ? 1 : -1;
    if (!(typedInt(txbConstant_->text(), lastAddConstant_) && typedInt(txbStart_->text(), newStart_) && lastAddConstant_ >= 0
          && lastAddConstant_ < kMaxLines && lastAddConstant_ + direction * newStart_ <= kMaxLines
          && lastAddConstant_ * direction + newStart_ >= 0)) {
        prompts::problem(this, texts::text("RasterTablePlus.badLines"));
        return;
    }
    // The first and the last of the lines are both in the table.
    const int last = newStart_ + (lastAddConstant_ - 1) * direction;
    if (lastAddConstant_ > 0 && (newStart_ < 0 || newStart_ >= kMaxLines || last < 0 || last >= kMaxLines)) {
        prompts::problem(this, texts::text("RasterTablePlus.badLines"));
        return;
    }
    Shade shade;
    if (!shade_->read(shade))
        return;
    if (shade.valid())
        for (int i = 0; i < lastAddConstant_; ++i)
            raster_.line[static_cast<size_t>(newStart_ + i * direction)]
                = static_cast<int>(shade.blue + (static_cast<int>(shade.red) << 4) + (static_cast<int>(shade.green) << 8));
    newStart_ += direction * lastAddConstant_;
    newEnd_ += direction * lastAddConstant_;
    lastDirection_ = direction;
    raster_.maxLine = newStart_ - 1;
    drawLines();
    bpUndo_->setEnabled(true);
}

void RasterTable::undo()
{
    // The lines last added are black again, and the next go where they
    // were.
    newStart_ -= lastDirection_ * lastAddConstant_;
    newEnd_ -= lastDirection_ * lastAddConstant_;
    raster_.maxLine = newStart_ - 1;
    for (int i = 0; i < lastAddConstant_; ++i) {
        const int line = newStart_ + i * lastDirection_;
        if (line >= 0 && line < kMaxLines)
            raster_.line[static_cast<size_t>(line)] = 0;
    }
    drawLines();
    bpUndo_->setEnabled(false);
}

void RasterTable::addLines()
{
    PaletteMaker maker(raster_.line.data(), newStart_, newEnd_, lastColour_, this);
    maker.exec();
    if (maker.done()) {
        // The next lines go under these, as many of them.
        newStart_ = maker.end() + 1;
        newEnd_ = maker.end() + (maker.end() - maker.start()) + 1;
        raster_.minLine = std::min(raster_.minLine, maker.start());
        raster_.maxLine = std::max(raster_.maxLine, maker.end());
        const int last = raster_.line[static_cast<size_t>(maker.end())];
        lastColour_ = Rgb(static_cast<uint8_t>(rasterRed(last) * 17), static_cast<uint8_t>(rasterGreen(last) * 17),
                          static_cast<uint8_t>(rasterBlue(last) * 17));
        drawLines();
    }
    bpUndo_->setEnabled(false);
}

void RasterTable::clearAll()
{
    raster_.line.fill(0);
    newStart_ = 0;
    newEnd_ = 15;
    raster_.minLine = raster_.maxLine = 0;
    lastColour_ = Rgb();
    drawLines();
}

void RasterTable::scroll(bool up)
{
    int count = 0;
    typedInt(txbScroll_->text(), count);
    scrollRaster(raster_, count, up);
    drawLines();
}

void RasterTable::importKit()
{
    if (!typedInt(txbStart_->text(), newStart_) || newStart_ < 0 || newStart_ + 16 >= kMaxLines) {
        prompts::problem(this, texts::text("RasterTablePlus.badLines"));
        return;
    }
    const QString path = prompts::openName(this, texts::text("RasterTablePlus.bpImportKit"), folder_, {texts::text("RasterTablePlus.kitFiles")});
    if (!path.isEmpty()) {
        folder_ = QFileInfo(path).absolutePath();
        // The palette's colours, one to a line: a file of fifteen starts a
        // line down.
        const std::optional<std::vector<uint8_t>> file = readHostFile(path);
        std::array<int, 16> colours{};
        if (file && readKitFile(*file, colours)) {
            for (size_t i = file->size() == 160 ? 0 : 1; i < 16; ++i)
                raster_.line[static_cast<size_t>(newStart_) + i] = asicWord(colours[i]) & 0xFFF;
        } else {
            prompts::problem(this, texts::text("RasterTablePlus.badKit"));
        }
    }
    drawLines();
}

// ---- Files ----

void RasterTable::load()
{
    const QString path = prompts::openName(this, texts::text("RasterTablePlus.bpLoad"), folder_, {texts::text("RasterTablePlus.xmlFiles")});
    if (path.isEmpty())
        return;
    folder_ = QFileInfo(path).absolutePath();
    const std::optional<std::vector<uint8_t>> file = readHostFile(path);
    if (!file || !readRasterXml(std::string(file->begin(), file->end()), raster_)) {
        prompts::problem(this, texts::text("RasterTablePlus.readError"));
        return;
    }
    drawLines();
}

bool RasterTable::outputRange(int& start, int& end)
{
    if (typedInt(txbLineStart_->text(), start) && typedInt(txbLineEnd_->text(), end) && start < end && start >= 0 && end < kMaxLines) {
        raster_.minLine = start;
        raster_.maxLine = end;
        return true;
    }
    prompts::problem(this, texts::text("RasterTablePlus.badRange"));
    return false;
}

void RasterTable::save()
{
    int start = 0, end = 0;
    if (!outputRange(start, end))
        return;
    const QString path = prompts::saveName(this, texts::text("RasterTablePlus.bpSave"), folder_, {texts::text("RasterTablePlus.xmlFiles")});
    if (path.isEmpty())
        return;
    folder_ = QFileInfo(path).absolutePath();
    if (!writeHostText(path, rasterXml(raster_)))
        prompts::problem(this, texts::text("RasterTablePlus.saveError"));
}

void RasterTable::copyToClipboard()
{
    int start = 0, end = 0;
    if (outputRange(start, end))
        QApplication::clipboard()->setText(QString::fromStdString(rasterData(raster_, start, end)));
}

void RasterTable::generate(bool fade)
{
    int start = 0, end = 0;
    if (!outputRange(start, end))
        return;
    const QString path = prompts::saveName(this, texts::text(fade ? "RasterTablePlus.bpGenFade" : "RasterTablePlus.bpGenerate"), folder_,
                                           {texts::text("RasterTablePlus.asmFiles")});
    if (path.isEmpty())
        return;
    folder_ = QFileInfo(path).absolutePath();
    const std::string source = fade ? rasterFadeAsm(document_->screen, raster_, start, end) : rasterAsm(document_->screen, raster_, start, end);
    if (!writeHostText(path, source))
        prompts::problem(this, texts::text("RasterTablePlus.saveError"));
}
