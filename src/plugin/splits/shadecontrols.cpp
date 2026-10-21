#include "shadecontrols.h"

#include <QLineEdit>
#include <QSlider>

#include "swatch.h"

namespace tuxscr {

bool typedInt(const QString& text, int& value)
{
    bool ok = false;
    const int typed = text.trimmed().toInt(&ok);
    value = ok ? typed : 0;
    return ok;
}

bool typedNumber(const QString& text, double& value)
{
    bool ok = false;
    QString plain = text.trimmed();
    plain.replace(QLatin1Char(','), QLatin1Char('.'));
    const double typed = plain.toDouble(&ok);
    value = ok ? typed : 0;
    return ok;
}

ShadeControls::ShadeControls(const QString& stem, QObject* parent)
    : QObject(parent)
{
    const char* const letters[3] = {"R", "V", "B"};
    for (int i = 0; i < 3; ++i) {
        sliders_[i] = new QSlider(Qt::Horizontal);
        sliders_[i]->setObjectName(QStringLiteral("trk") + stem + QLatin1String(letters[i]));
        sliders_[i]->setRange(0, 15);
        sliders_[i]->setPageStep(5);
        sliders_[i]->setTickPosition(QSlider::TicksBelow);
        sliders_[i]->setTickInterval(1);
        sliders_[i]->setMinimumWidth(104);
        boxes_[i] = new QLineEdit(QStringLiteral("0"));
        boxes_[i]->setObjectName(QStringLiteral("txb") + stem + QLatin1String(letters[i]));
        boxes_[i]->setMaxLength(2);
        boxes_[i]->setAlignment(Qt::AlignRight);
        boxes_[i]->setFixedWidth(boxes_[i]->fontMetrics().horizontalAdvance(QStringLiteral("000")) + 16);

        connect(sliders_[i], &QSlider::valueChanged, this, [this, i](int value) {
            showColour();
            if (fromBox_)
                return;
            boxes_[i]->setText(QString::number(value));
            emit moved();
        });
        connect(boxes_[i], &QLineEdit::textChanged, this, [this, i](const QString& text) {
            int typed = 0;
            if (!typedInt(text, typed) || typed < 0 || typed > 15)
                return;
            fromBox_ = true;
            sliders_[i]->setValue(typed);
            fromBox_ = false;
            showColour();
        });
    }
    swatch_ = new ColourSwatch(QSize(64, 64));
    swatch_->setObjectName(QStringLiteral("lbl") + stem + QStringLiteral("Color"));
    swatch_->setShowOnly(true);
    showColour();
}

void ShadeControls::set(int red, int green, int blue)
{
    boxes_[Red]->setText(QString::number(red));
    boxes_[Green]->setText(QString::number(green));
    boxes_[Blue]->setText(QString::number(blue));
}

bool ShadeControls::read(tuxscr::Shade& shade) const
{
    return typedNumber(boxes_[Red]->text(), shade.red) && typedNumber(boxes_[Green]->text(), shade.green)
        && typedNumber(boxes_[Blue]->text(), shade.blue);
}

void ShadeControls::showColour()
{
    if (swatch_)
        swatch_->setColour(QColor(sliders_[Red]->value() * 17, sliders_[Green]->value() * 17, sliders_[Blue]->value() * 17));
}

}  // namespace tuxscr
