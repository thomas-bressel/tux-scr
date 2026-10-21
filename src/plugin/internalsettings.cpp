#include "internalsettings.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include "controls.h"
#include "converterwindow.h"
#include "texts.h"

namespace tuxscr {

InternalSettings::InternalSettings(ConverterWindow* window)
    : QDialog(window)
    , document_(window->document())
{
    setObjectName(QStringLiteral("ParamInterne"));
    auto* column = new QVBoxLayout(this);
    Param& param = document_->param;
    // A group, and in it rows of a label, a slider and a box for the same
    // number.
    const auto group = [&](const char* name, std::initializer_list<std::tuple<const char*, const char*, int*, int>> rows) {
        auto* box = new QGroupBox;
        box->setObjectName(QLatin1String(name));
        auto* grid = new QGridLayout(box);
        int row = 0;
        for (const auto& [label, control, value, maximum] : rows) {
            auto* text = namedLabel(label, QLatin1String(control));
            auto* slider = new QSlider(Qt::Horizontal);
            slider->setObjectName(QStringLiteral("track") + QLatin1String(control));
            slider->setRange(0, maximum);
            slider->setMinimumWidth(220);
            auto* spin = namedSpin((QByteArray("numeric") + control).constData(), 0, maximum, 0);
            text->setBuddy(spin);
            slider->setAccessibleName(QLatin1String(control));
            grid->addWidget(text, row, 0);
            grid->addWidget(slider, row, 1);
            grid->addWidget(spin, row++, 2);
            values_.append({value, slider, spin});
            const auto changed = [this, value, slider, spin](int now) {
                if (filling_)
                    return;
                filling_ = true;
                *value = now;
                slider->setValue(now);
                spin->setValue(now);
                filling_ = false;
                document_->convert(false);
            };
            connect(slider, &QSlider::valueChanged, this, changed);
            connect(spin, &QSpinBox::valueChanged, this, changed);
        }
        column->addWidget(box);
    };
    group("groupBox1", {{"label1", "LumR", &param.coefR, 32767}, {"label2", "LumV", &param.coefV, 32767}, {"label3", "LumB", &param.coefB, 32767}});
    group("groupBox2", {{"label4", "R1", &param.cstR1, 511}, {"label5", "R2", &param.cstR2, 511}, {"label6", "R3", &param.cstR3, 511}, {"label7", "R4", &param.cstR4, 511}});
    group("groupBox3", {{"label8", "V1", &param.cstV1, 511}, {"label9", "V2", &param.cstV2, 511}, {"label10", "V3", &param.cstV3, 511}, {"label11", "V4", &param.cstV4, 511}});
    group("groupBox4", {{"label12", "B1", &param.cstB1, 511}, {"label13", "B2", &param.cstB2, 511}, {"label14", "B3", &param.cstB3, 511}, {"label15", "B4", &param.cstB4, 511}});

    auto* kmeans = new QGroupBox(tr("KMeans"));
    kmeans->setObjectName(QStringLiteral("groupBox5"));
    auto* grid = new QGridLayout(kmeans);
    const char* const names[3] = {"rbDistanceSup", "rbDistanceEuclide", "rbDistanceManhattan"};
    const char* const captions[3] = {"Distance Sup", "Distance Euclide", "Distance Manhattan"};
    for (int i = 0; i < 3; ++i) {
        distances_[i] = namedRadio(names[i], QLatin1String(captions[i]));
        grid->addWidget(distances_[i], 0, i);
        connect(distances_[i], &QRadioButton::toggled, this, [this, i](bool on) {
            if (filling_ || !on)
                return;
            document_->param.kMeansDist = i;
            document_->convert(false);
        });
    }
    numColor_ = namedSpin("numColor", 2, 256, 16);
    numPass_ = namedSpin("numPass", 1, 256, 16);
    auto* label16 = namedLabel("label16");
    auto* label17 = namedLabel("label17");
    label16->setBuddy(numColor_);
    label17->setBuddy(numPass_);
    grid->addWidget(label16, 1, 0);
    grid->addWidget(numColor_, 1, 1);
    grid->addWidget(label17, 2, 0);
    grid->addWidget(numPass_, 2, 1);
    column->addWidget(kmeans);
    connect(numColor_, &QSpinBox::valueChanged, this, [this](int value) {
        if (filling_)
            return;
        document_->param.kMeansColor = value;
        document_->convert(false);
    });
    connect(numPass_, &QSpinBox::valueChanged, this, [this](int value) {
        if (filling_)
            return;
        document_->param.kMeansPass = value;
        document_->convert(false);
    });
    applyTexts();
    fromDocument();
}

void InternalSettings::applyTexts()
{
    setWindowTitle(texts::windowTitle(texts::text("Main.chkParamInterne")));
    texts::apply(this, "ParamInterne");
    // Red, green and blue.
    findChild<QLabel*>(QStringLiteral("label1"))->setText(tr("R"));
    findChild<QLabel*>(QStringLiteral("label2"))->setText(tr("G"));
    findChild<QLabel*>(QStringLiteral("label3"))->setText(tr("B"));
    findChild<QLabel*>(QStringLiteral("label16"))->setText(tr("Colours"));
    findChild<QLabel*>(QStringLiteral("label17"))->setText(tr("Passes"));
}

void InternalSettings::fromDocument()
{
    filling_ = true;
    for (const Value& value : values_) {
        value.slider->setValue(*value.value);
        value.box->setValue(*value.value);
    }
    const Param& param = document_->param;
    distances_[std::clamp(param.kMeansDist, 0, 2)]->setChecked(true);
    numColor_->setValue(param.kMeansColor >= 2 ? param.kMeansColor : 16);
    numPass_->setValue(param.kMeansPass >= 1 ? param.kMeansPass : 1);
    filling_ = false;
}

}  // namespace tuxscr
