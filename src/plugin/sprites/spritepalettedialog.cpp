#include "spritepalettedialog.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include "controls.h"
#include "convert/screen.h"
#include "texts.h"

namespace tuxscr {

SpritePaletteDialog::SpritePaletteDialog(std::array<int, 16>* palette, int firstPen, QWidget* parent)
    : QDialog(parent)
    , palette_(palette)
    , firstPen_(firstPen)
{
    // The texts are ConvImgCpc's, which has them in English only.
    setObjectName(QStringLiteral("GenPalette"));
    setWindowTitle(texts::windowTitle(tr("Make Palette")));
    auto* layout = new QVBoxLayout(this);
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(8);
    layout->addLayout(grid);

    // The pens the shades go from and to.
    auto* lblFrom = namedLabel("lblFrom", tr("From color"));
    from_ = namedSpin("txbFrom", firstPen, 14, firstPen);
    lblFrom->setBuddy(from_);
    auto* getColour = namedButton("bpGetCol", tr("Get color"));
    auto* lblTo = namedLabel("lblTo", tr("To color"));
    to_ = namedSpin("txbTo", firstPen + 1, 15, 15);
    lblTo->setBuddy(to_);
    grid->addWidget(lblFrom, 0, 0, 1, 2);
    grid->addWidget(from_, 0, 2);
    grid->addWidget(getColour, 0, 3);
    grid->addWidget(lblTo, 0, 5, 1, 2);
    grid->addWidget(to_, 0, 7);
    grid->setColumnMinimumWidth(4, 24);

    // The two colours, each with its sample.
    const char* const endNames[2] = {"Start", "End"};
    const char* const endTexts[2] = {"Start color", "End color"};
    const char* const parts[3] = {"R", "V", "B"};
    const int ends[2] = {(*palette_)[static_cast<size_t>(firstPen)], (*palette_)[15]};
    for (int end = 0; end < 2; ++end) {
        const int column = end * 5;
        auto* text = namedLabel(end == 0 ? "lblStartText" : "lblEndText", QLatin1String(endTexts[end]));
        samples_[end] = namedLabel(end == 0 ? "lblStartColor" : "lblEndColor");
        samples_[end]->setFixedSize(64, 64);
        samples_[end]->setFrameShape(QFrame::Box);
        samples_[end]->setAutoFillBackground(true);
        samples_[end]->setAccessibleName(QLatin1String(endTexts[end]));
        grid->addWidget(text, 1, column, 1, 2);
        grid->addWidget(samples_[end], 1, column + 2, 1, 2, Qt::AlignLeft);
        const int values[3] = {plusRed(ends[end]), plusGreen(ends[end]), plusBlue(ends[end])};
        for (int part = 0; part < 3; ++part) {
            const int i = end * 3 + part;
            const QString name = QLatin1String(endNames[end]) + QLatin1String(parts[part]);
            auto* label = new QLabel(QLatin1String(parts[part]));
            sliders_[i] = new QSlider(Qt::Horizontal);
            sliders_[i]->setObjectName(QStringLiteral("trk") + name);
            sliders_[i]->setRange(0, 15);
            sliders_[i]->setPageStep(1);
            sliders_[i]->setMinimumWidth(110);
            sliders_[i]->setValue(values[part]);
            sliders_[i]->setAccessibleName(QLatin1String(endTexts[end]) + QLatin1Char(' ') + QLatin1String(parts[part]));
            boxes_[i] = new QSpinBox;
            boxes_[i]->setObjectName(QStringLiteral("txb") + name);
            boxes_[i]->setRange(0, 15);
            boxes_[i]->setValue(values[part]);
            boxes_[i]->setAccessibleName(sliders_[i]->accessibleName());
            label->setBuddy(boxes_[i]);
            grid->addWidget(label, 2 + part, column);
            grid->addWidget(sliders_[i], 2 + part, column + 1, 1, 2);
            grid->addWidget(boxes_[i], 2 + part, column + 3, Qt::AlignLeft);
            // A box sets its slider and the sample; a slider that is moved
            // sets its box and makes the shades at once.
            connect(boxes_[i], &QSpinBox::valueChanged, this, [this, i, end](int value) {
                setting_ = true;
                sliders_[i]->setValue(value);
                setting_ = false;
                showSample(end);
            });
            connect(sliders_[i], &QSlider::valueChanged, this, [this, i](int value) {
                if (setting_)
                    return;
                boxes_[i]->setValue(value);
                makePalette();
            });
        }
        showSample(end);
    }

    error_ = namedLabel("lblError");
    error_->setWordWrap(true);
    layout->addWidget(error_);
    auto* generate = namedButton("bpGenerate", tr("Generate"));
    auto* foot = new QHBoxLayout;
    foot->addStretch();
    foot->addWidget(generate);
    foot->addStretch();
    layout->addLayout(foot);

    connect(getColour, &QPushButton::clicked, this, &SpritePaletteDialog::takeColour);
    connect(generate, &QPushButton::clicked, this, &SpritePaletteDialog::makePalette);
}

void SpritePaletteDialog::showSample(int end)
{
    QPalette look = samples_[end]->palette();
    look.setColor(QPalette::Window,
                  QColor(boxes_[end * 3]->value() * 17, boxes_[end * 3 + 1]->value() * 17, boxes_[end * 3 + 2]->value() * 17));
    samples_[end]->setPalette(look);
}

void SpritePaletteDialog::makePalette()
{
    error_->clear();
    const int start = from_->value();
    const int end = to_->value();
    if (start >= firstPen_ && start < end && end < 16) {
        // As ConvImgCpc counts them: each part goes from one colour's to
        // the other's by equal steps, and what is over a whole is dropped.
        double rs = boxes_[0]->value() * 17, vs = boxes_[1]->value() * 17, bs = boxes_[2]->value() * 17;
        const double re = boxes_[3]->value() * 17, ve = boxes_[4]->value() * 17, be = boxes_[5]->value() * 17;
        const double kr = (re - rs) / (end - start);
        const double kv = (ve - vs) / (end - start);
        const double kb = (be - bs) / (end - start);
        for (int i = start; i < end; ++i) {
            (*palette_)[static_cast<size_t>(i)]
                = static_cast<int>((rs / 17) + (static_cast<int>(bs / 17) << 4) + (static_cast<int>(vs / 17) << 8));
            rs += kr;
            bs += kb;
            vs += kv;
        }
        (*palette_)[static_cast<size_t>(end)]
            = static_cast<int>((re / 17) + (static_cast<int>(be / 17) << 4) + (static_cast<int>(ve / 17) << 8));
    }
    emit paletteChanged();

    // Two pens of the same colour are told of.
    for (size_t i = 0; i < 16; ++i)
        for (size_t j = i + 1; j < 16; ++j)
            if ((*palette_)[i] == (*palette_)[j]) {
                error_->setText(texts::text("EditSprites.sameColour").arg(i).arg(j));
                break;
            }
}

void SpritePaletteDialog::takeColour()
{
    const int colour = (*palette_)[static_cast<size_t>(from_->value())];
    boxes_[0]->setValue(plusRed(colour));
    boxes_[1]->setValue(plusGreen(colour));
    boxes_[2]->setValue(plusBlue(colour));
}

}  // namespace tuxscr
