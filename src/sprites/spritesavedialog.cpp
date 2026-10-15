#include "spritesavedialog.h"

#include <algorithm>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

#include "controls.h"

SpriteSaveDialog::SpriteSaveDialog(const QString& kind, const QString& name, bool withPalette, bool withPointers,
                                   const QString& countLabel, int count, int most, QWidget* parent)
    : QDialog(parent)
{
    // The texts are ConvImgCpc's, which has them in English only.
    setObjectName(QStringLiteral("SaveMedia"));
    setWindowTitle(QStringLiteral("Save %1 (asm format)").arg(kind));
    auto* form = new QGridLayout(this);
    const auto row = [this, form](int line, const char* box, const QString& text, const char* field, const QString& value,
                                  QCheckBox*& check, QLineEdit*& edit, bool shown) {
        check = namedCheck(box, text);
        check->setChecked(true);
        edit = new QLineEdit(value);
        edit->setObjectName(QLatin1String(field));
        edit->setMinimumWidth(260);
        edit->setAccessibleName(text);
        connect(check, &QCheckBox::toggled, edit, &QWidget::setEnabled);
        form->addWidget(check, line, 0);
        form->addWidget(edit, line, 1);
        check->setVisible(shown);
        edit->setVisible(shown);
    };
    row(0, "chkLabelMedia", kind + QStringLiteral(" Label"), "txbLabelMedia", name, chkMedia_, txbMedia_, true);
    row(1, "chkLabelPalette", kind + QStringLiteral(" Palette Label"), "txbLabelPalette", QStringLiteral("Palette") + name,
        chkPalette_, txbPalette_, withPalette);
    row(2, "chkLabelPtr", kind + QStringLiteral(" Pointer Label"), "txbLabelPtr", name + QStringLiteral("Ptr"), chkPointers_,
        txbPointers_, withPointers);

    auto* lblCount = namedLabel("lblNbMedia", countLabel);
    numCount_ = namedSpin("numNbMedia", 1, std::max(1, most), std::clamp(count, 1, std::max(1, most)));
    lblCount->setBuddy(numCount_);
    form->addWidget(lblCount, 3, 0);
    form->addWidget(numCount_, 3, 1, Qt::AlignLeft);
    lblCount->setVisible(!countLabel.isEmpty());
    numCount_->setVisible(!countLabel.isEmpty());

    chkZero_ = namedCheck("chkZeroPtr", QStringLiteral("Add 0 at the end of the pointer list"));
    form->addWidget(chkZero_, 4, 0, 1, 2);
    chkZero_->setVisible(withPointers);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setObjectName(QStringLiteral("bpOk"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    form->addWidget(buttons, 5, 0, 1, 2);
}

tuxscr::SpriteLabels SpriteSaveDialog::labels() const
{
    tuxscr::SpriteLabels labels;
    // What is not shown is as ConvImgCpc has it: ticked, with its text.
    labels.media = chkMedia_->isChecked() ? txbMedia_->text().toStdString() : std::string();
    labels.palette = chkPalette_->isChecked() ? txbPalette_->text().toStdString() : std::string();
    labels.pointers = chkPointers_->isChecked() ? txbPointers_->text().toStdString() : std::string();
    labels.zeroPointer = chkZero_->isChecked();
    return labels;
}

int SpriteSaveDialog::count() const
{
    return numCount_->value();
}
