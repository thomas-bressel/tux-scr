#include "colourdialog.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include "controls.h"
#include "convert/screen.h"
#include "penbutton.h"
#include "pictures.h"
#include "texts.h"

namespace tuxscr {

ColourDialog::ColourDialog(int pen, int colour, bool plus, QWidget* parent)
    : QDialog(parent)
    , colour_(colour)
    , plus_(plus)
{
    setObjectName(QStringLiteral("EditColor"));
    setWindowTitle(texts::windowTitle(tr("Colour %1").arg(pen)));
    auto* layout = new QVBoxLayout(this);

    auto* top = new QHBoxLayout;
    top->addWidget(new QLabel(tr("Colour %1").arg(pen)));
    sample_ = new QLabel;
    sample_->setObjectName(QStringLiteral("selColor"));
    sample_->setFixedSize(64, 32);
    sample_->setFrameShape(QFrame::Box);
    sample_->setAutoFillBackground(true);
    top->addWidget(sample_);
    value_ = new QLabel;
    value_->setObjectName(QStringLiteral("lblValColor"));
    top->addWidget(value_);
    top->addStretch();
    layout->addLayout(top);

    if (plus) {
        // Green, red, blue: the order ConvImgCpc has them in.
        auto* grid = new QGridLayout;
        const char* const names[3] = {"V", "R", "B"};
        const int start[3] = {plusGreen(colour) & 15, plusRed(colour), plusBlue(colour)};
        for (int i = 0; i < 3; ++i) {
            grid->addWidget(new QLabel(QLatin1String(names[i])), i, 0);
            boxes_[i] = namedSpin(i == 0 ? "valV" : i == 1 ? "valR" : "valB", 0, 15, start[i]);
            sliders_[i] = new QSlider(Qt::Horizontal);
            sliders_[i]->setObjectName(QLatin1String(i == 0 ? "trackV" : i == 1 ? "trackR" : "trackB"));
            sliders_[i]->setRange(0, 15);
            sliders_[i]->setValue(start[i]);
            sliders_[i]->setMinimumWidth(160);
            grid->addWidget(boxes_[i], i, 1);
            grid->addWidget(sliders_[i], i, 2);
            connect(sliders_[i], &QSlider::valueChanged, boxes_[i], &QSpinBox::setValue);
            connect(boxes_[i], &QSpinBox::valueChanged, this, [this, i](int value) {
                sliders_[i]->setValue(value);
                colour_ = plusColour(boxes_[1]->value(), boxes_[0]->value(), boxes_[2]->value());
                showColour();
            });
        }
        layout->addLayout(grid);
    } else {
        // The CPC's 27 colours, nine to a row.
        auto* grid = new QGridLayout;
        grid->setSpacing(4);
        for (int i = 0; i < 27; ++i) {
            auto* button = new PenButton(i);
            button->setObjectName(QStringLiteral("colour%1").arg(i));
            button->setColour(toQColor(kCpcColours[i]));
            grid->addWidget(button, i / 9, i % 9);
            connect(button, &QAbstractButton::clicked, this, [this, i] {
                colour_ = i;
                showColour();
            });
        }
        layout->addLayout(grid);
    }

    auto* buttons = new QDialogButtonBox;
    QPushButton* ok = buttons->addButton(texts::text("EditColor.bpValide"), QDialogButtonBox::AcceptRole);
    ok->setObjectName(QStringLiteral("bpValide"));
    QPushButton* cancel = buttons->addButton(texts::text("EditColor.bpAnnule"), QDialogButtonBox::RejectRole);
    cancel->setObjectName(QStringLiteral("bpAnnule"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    showColour();
}

void ColourDialog::showColour()
{
    Screen screen;
    screen.plus = plus_;
    QPalette look = sample_->palette();
    look.setColor(QPalette::Window, toQColor(screen.rgb(colour_)));
    sample_->setPalette(look);
    value_->setText(plus_ ? QStringLiteral("#%1").arg(asicWord(colour_), 3, 16, QLatin1Char('0')).toUpper()
                          : QStringLiteral("=%1").arg(colour_));
}

bool ColourDialog::choose(QWidget* parent, int pen, int& colour, bool plus)
{
    ColourDialog dialog(pen, colour == kNoColour ? 0 : colour, plus, parent);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    colour = dialog.colour();
    return true;
}

}  // namespace tuxscr
