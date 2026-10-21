#include "palettemaker.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include "controls.h"
#include "convert/screen.h"
#include "splits/rasters.h"
#include "shadecontrols.h"
#include "splittexts.h"
#include "swatch.h"
#include "texts.h"

namespace tuxscr {

PaletteMaker::PaletteMaker(int* palette, int minStart, std::function<void()> changed, QWidget* parent)
    : QDialog(parent)
    , palette_(palette)
    , minStart_(minStart)
    , changed_(std::move(changed))
{
    build();
    txbFrom_->setText(QString::number(minStart_));
    const int first = palette_[minStart_];
    first_->set(first & 0x0F, first >> 8, (first & 0xF0) >> 4);
    const int last = palette_[15];
    last_->set(last & 0x0F, last >> 8, (last & 0xF0) >> 4);
}

PaletteMaker::PaletteMaker(int* lines, int from, int to, Rgb last, QWidget* parent)
    : QDialog(parent)
    , palette_(lines)
    , rasters_(true)
{
    build();
    txbFrom_->setText(QString::number(from));
    txbTo_->setText(QString::number(to));
    // A component of the Plus is its four high bits on a PC.
    first_->set(last.r >> 4, last.g >> 4, last.b >> 4);
}

void PaletteMaker::build()
{
    registerSplitTexts();
    setObjectName(QStringLiteral("GenPalette"));
    auto* layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    auto* sides = new QHBoxLayout;
    sides->setSpacing(24);
    layout->addLayout(sides);

    first_ = new ShadeControls(QStringLiteral("Start"), this);
    last_ = new ShadeControls(QStringLiteral("End"), this);
    txbFrom_ = new QLineEdit(QStringLiteral("0"));
    txbFrom_->setObjectName(QStringLiteral("txbFrom"));
    txbTo_ = new QLineEdit(QStringLiteral("15"));
    txbTo_->setObjectName(QStringLiteral("txbTo"));
    for (QLineEdit* box : {txbFrom_, txbTo_}) {
        box->setMaxLength(3);
        box->setAlignment(Qt::AlignRight);
        box->setFixedWidth(box->fontMetrics().horizontalAdvance(QStringLiteral("0000")) + 16);
    }
    bpGetCol_ = namedButton("bpGetCol");

    // The first colour on the left, the last on the right: the pen or the
    // line, the colour itself, then its red, green and blue.
    const char* const componentLabels[2][3] = {{"label1", "label3", "label5"}, {"label2", "label4", "label6"}};
    for (int side = 0; side < 2; ++side) {
        ShadeControls* shade = side == 0 ? first_ : last_;
        auto* grid = new QGridLayout;
        QLabel* which = namedLabel(side == 0 ? "lblFrom" : "lblTo");
        QLineEdit* number = side == 0 ? txbFrom_ : txbTo_;
        which->setBuddy(number);
        grid->addWidget(which, 0, 0);
        auto* numberRow = new QHBoxLayout;
        numberRow->addWidget(number);
        if (side == 0)
            numberRow->addWidget(bpGetCol_);
        numberRow->addStretch();
        grid->addLayout(numberRow, 0, 1, 1, 2);
        grid->addWidget(namedLabel(side == 0 ? "lblStartText" : "lblEndText"), 1, 0);
        grid->addWidget(shade->swatch(), 1, 1, 1, 2, Qt::AlignLeft);
        for (int c = 0; c < 3; ++c) {
            const auto component = static_cast<ShadeControls::Component>(c);
            QLabel* letter = namedLabel(componentLabels[side][c]);
            letter->setBuddy(shade->slider(component));
            grid->addWidget(letter, 2 + c, 0, Qt::AlignRight);
            grid->addWidget(shade->slider(component), 2 + c, 1);
            grid->addWidget(shade->box(component), 2 + c, 2);
        }
        sides->addLayout(grid);
    }

    lblError_ = namedLabel("lblError");
    lblError_->setWordWrap(true);
    lblError_->setMinimumHeight(lblError_->fontMetrics().height() * 2);
    layout->addWidget(lblError_);
    bpGenerate_ = namedButton("bpGenerate");
    layout->addWidget(bpGenerate_, 0, Qt::AlignHCenter);

    // The colours follow the sliders as they move; a number typed waits for
    // "Generate".
    connect(first_, &ShadeControls::moved, this, &PaletteMaker::calcPalette);
    connect(last_, &ShadeControls::moved, this, &PaletteMaker::calcPalette);
    connect(bpGetCol_, &QPushButton::clicked, this, &PaletteMaker::takeColour);
    connect(bpGenerate_, &QPushButton::clicked, this, [this] {
        const bool used = calcPalette();
        if (!rasters_)
            done_ = true;
        else if (used)
            accept();
    });
    applyTexts();
}

void PaletteMaker::applyTexts()
{
    texts::apply(this, "GenPalette");
    setWindowTitle(texts::windowTitle(texts::text("GenPalette.title")));
    if (rasters_) {
        findChild<QLabel*>(QStringLiteral("lblFrom"))->setText(texts::text("GenPalette.lblFromLine"));
        findChild<QLabel*>(QStringLiteral("lblTo"))->setText(texts::text("GenPalette.lblToLine"));
    }
    // What a control is, said to those who do not see its label.
    const char* const letters[3] = {"R", "V", "B"};
    for (int c = 0; c < 3; ++c) {
        const auto component = static_cast<ShadeControls::Component>(c);
        const QString first = texts::text("GenPalette.lblStartText") + QLatin1Char(' ') + QLatin1String(letters[c]);
        const QString last = texts::text("GenPalette.lblEndText") + QLatin1Char(' ') + QLatin1String(letters[c]);
        first_->slider(component)->setAccessibleName(first);
        first_->box(component)->setAccessibleName(first);
        last_->slider(component)->setAccessibleName(last);
        last_->box(component)->setAccessibleName(last);
    }
    first_->swatch()->setAccessibleName(texts::text("GenPalette.lblStartText"));
    last_->swatch()->setAccessibleName(texts::text("GenPalette.lblEndText"));
}

bool PaletteMaker::calcPens()
{
    bool used = false;
    int start = 0, end = 0;
    if (typedInt(txbFrom_->text(), start) && typedInt(txbTo_->text(), end) && start >= minStart_ && start < end && end < 16) {
        Shade from, to;
        if (first_->read(from) && last_->read(to) && from.valid() && to.valid()) {
            gradientPens(palette_, start, end, from, to);
            start_ = start;
            end_ = end;
            used = true;
        }
    }
    if (changed_)
        changed_();
    return used;
}

bool PaletteMaker::calcLines()
{
    int start = 0, end = 0;
    if (typedInt(txbFrom_->text(), start) && typedInt(txbTo_->text(), end) && start >= 0 && start < kMaxLines && end >= 0
        && end < kMaxLines && start != end) {
        Shade from, to;
        if (first_->read(from) && last_->read(to) && from.valid() && to.valid()) {
            gradientLines(palette_, start, end, from, to);
            start_ = start;
            end_ = end;
            done_ = true;
            return true;
        }
    }
    return false;
}

bool PaletteMaker::calcPalette()
{
    const bool used = rasters_ ? calcLines() : calcPens();
    QString problem = used ? QString() : texts::text("GenPalette.invalid");
    // Two pens of a palette that have the same colour are one pen lost.
    if (!rasters_)
        for (int i = 0; i < 16; ++i)
            for (int j = i + 1; j < 16; ++j)
                if (palette_[i] == palette_[j]) {
                    problem = texts::text("GenPalette.same").arg(i).arg(j);
                    break;
                }
    lblError_->setText(problem);
    return used;
}

void PaletteMaker::takeColour()
{
    int start = 0;
    if (!typedInt(txbFrom_->text(), start) || start < 0 || start >= (rasters_ ? kMaxLines : 16))
        return;
    const int colour = palette_[start];
    if (rasters_)
        first_->set(rasterRed(colour), rasterGreen(colour), rasterBlue(colour));
    else
        first_->set(colour & 0x0F, colour >> 8, (colour & 0xF0) >> 4);
}

}  // namespace tuxscr
