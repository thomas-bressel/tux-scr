#include "palettebar.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>

#include "document.h"
#include "penbutton.h"
#include "pictures.h"
#include "texts.h"

PaletteBar::PaletteBar(Document* document, QWidget* parent)
    : QWidget(parent)
    , document_(document)
{
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(2);
    disableLabel_ = new QLabel;
    lockLabel_ = new QLabel;
    grid->addWidget(disableLabel_, 0, 0);
    grid->addWidget(lockLabel_, 2, 0);
    for (int i = 0; i < 16; ++i) {
        disables_[i] = new QCheckBox;
        disables_[i]->setObjectName(QStringLiteral("disableColor%1").arg(i));
        pens_[i] = new PenButton(i);
        pens_[i]->setObjectName(QStringLiteral("lblColor%1").arg(i));
        locks_[i] = new QCheckBox;
        locks_[i]->setObjectName(QStringLiteral("lockColor%1").arg(i));
        grid->addWidget(disables_[i], 0, i + 1, Qt::AlignHCenter);
        grid->addWidget(pens_[i], 1, i + 1);
        grid->addWidget(locks_[i], 2, i + 1, Qt::AlignHCenter);
        connect(pens_[i], &QAbstractButton::clicked, this, [this, i] { emit penClicked(i, false); });
        connect(pens_[i], &PenButton::rightClicked, this, [this, i] { emit penClicked(i, true); });
        connect(locks_[i], &QCheckBox::clicked, this, [this, i](bool on) {
            document_->param.lockState[static_cast<size_t>(i)] = on ? 1 : 0;
            document_->convert(false);
        });
        connect(disables_[i], &QCheckBox::clicked, this, [this, i](bool on) {
            document_->param.disableState[static_cast<size_t>(i)] = on ? 1 : 0;
            document_->convert(false);
        });
    }
    grid->setColumnStretch(17, 1);
    retranslate();
    refresh();
}

void PaletteBar::retranslate()
{
    const bool english = texts::language() == QLatin1String("EN");
    disableLabel_->setText(english ? QStringLiteral("Disable") : QStringLiteral("Désactiver"));
    lockLabel_->setText(english ? QStringLiteral("Lock") : QStringLiteral("Verrouiller"));
    for (int i = 0; i < 16; ++i) {
        disables_[i]->setToolTip(english ? QStringLiteral("Leave pen %1 out of the conversion").arg(i)
                                         : QStringLiteral("Ne pas utiliser l'encre %1 dans la conversion").arg(i));
        locks_[i]->setToolTip(english ? QStringLiteral("Keep the colour of pen %1").arg(i)
                                      : QStringLiteral("Garder la couleur de l'encre %1").arg(i));
        disables_[i]->setAccessibleName(disables_[i]->toolTip());
        locks_[i]->setAccessibleName(locks_[i]->toolTip());
    }
}

void PaletteBar::refresh()
{
    const tuxscr::Screen& screen = document_->screen;
    const int maxPen = screen.maxPen(screen.yEgx ^ 2);
    const std::array<bool, 16> used = document_->usedPens();
    for (int i = 0; i < 16; ++i) {
        const size_t pen = static_cast<size_t>(i);
        const bool disabled = document_->param.disableState[pen] != 0;
        pens_[i]->setColour(toQColor(screen.rgb(screen.palette[pen])));
        pens_[i]->setMark(used[pen] ? PenButton::Mark::Tick : PenButton::Mark::None);
        pens_[i]->setToolTip(texts::language() == QLatin1String("EN")
                                 ? QStringLiteral("Pen %1%2").arg(i).arg(used[pen] ? QStringLiteral(", used by the picture") : QStringLiteral(", not used"))
                                 : QStringLiteral("Encre %1%2").arg(i).arg(used[pen] ? QStringLiteral(", utilisée par l'image") : QStringLiteral(", non utilisée")));
        pens_[i]->setAccessibleName(pens_[i]->toolTip());
        pens_[i]->setVisible(i < maxPen && !disabled);
        locks_[i]->setVisible(i < maxPen && !disabled);
        disables_[i]->setVisible(i < maxPen);
        const bool wasLocked = locks_[i]->blockSignals(true);
        locks_[i]->setChecked(document_->param.lockState[pen] != 0);
        locks_[i]->blockSignals(wasLocked);
        const bool wasDisabled = disables_[i]->blockSignals(true);
        disables_[i]->setChecked(disabled);
        disables_[i]->blockSignals(wasDisabled);
    }
}
