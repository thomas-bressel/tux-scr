#include "converterwindow.h"

#include <cstring>

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QTabWidget>
#include <QVBoxLayout>

#include "base/version.h"
#include "colourdialog.h"
#include "controls.h"
#include "animationpanel.h"
#include "edittools.h"
#include "internalsettings.h"
#include "patterndialog.h"
#include "convert/dither.h"
#include "formats/paramfile.h"
#include "interruptspanel.h"
#include "palettebar.h"
#include "pictures.h"
#include "pictureview.h"
#include "prompts.h"
#include "texts.h"

namespace tuxscr {

namespace {

const char* const kPackNames[] = {"ZX0 V2", "ZX0", "ZX1", "Standard", "ZX0Ovs"};
const PackMethod kPackMethods[] = {PackMethod::ZX0_V2, PackMethod::ZX0, PackMethod::ZX1, PackMethod::Standard, PackMethod::ZX0Ovs};

// The names of the six sliders of the colours and of their buttons.
struct SliderNames {
    const char* slider;
    const char* label;
    const char* text;
    const char* less;
    const char* more;
    const char* reset;
};
const SliderNames kSliders[6] = {
    {"lumi", "label8", QT_TRANSLATE_NOOP("tuxscr::ConverterWindow", "Lum."), "bpLumMoins", "bpLumPlus", "bpRazLumi"},
    {"sat", "label9", QT_TRANSLATE_NOOP("tuxscr::ConverterWindow", "Sat."), "bpSatMoins", "bpSatPlus", "bpRazSat"},
    {"contrast", "label10", QT_TRANSLATE_NOOP("tuxscr::ConverterWindow", "Ctrst."), "bpCtrstMoins", "bpCtrstPlus", "bpRazContrast"},
    {"red", "label6", QT_TRANSLATE_NOOP("tuxscr::ConverterWindow", "R"), "bpRmoins", "bpRplus", "RazR"},
    {"green", "label11", QT_TRANSLATE_NOOP("tuxscr::ConverterWindow", "G"), "bpVmoins", "bpVplus", "RazV"},
    {"blue", "label12", QT_TRANSLATE_NOOP("tuxscr::ConverterWindow", "B"), "bpBmoins", "bpBplus", "RazB"},
};

}  // namespace

ConverterWindow::ConverterWindow(tuxape::PluginHost* host, QWidget* parent)
    : QDialog(parent)
    , host_(host)
{
    setObjectName(QStringLiteral("TuxScrWindow"));
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
    setAcceptDrops(true);

    auto* layout = new QHBoxLayout(this);
    auto* settings = new QScrollArea;
    settings->setObjectName(QStringLiteral("saSettings"));
    settings->setWidgetResizable(true);
    settings->setFrameShape(QFrame::NoFrame);
    settings->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    settingsPanel_ = buildSettings();
    settings->setWidget(settingsPanel_);
    settings->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    settingsScroll_ = settings;
    layout->addWidget(settings);
    layout->addWidget(buildPicture(), 1);

    connect(&document_, &Document::changed, this, &ConverterWindow::pictureChanged);
    connect(&document_, &Document::screenChanged, this, &ConverterWindow::fromDocument);
    connect(&document_, &Document::framesChanged, this, &ConverterWindow::updateVisibility);
    connect(&document_, &Document::said, this, &ConverterWindow::info);
    connect(&document_, &Document::failed, this, [this](const QString& text) { prompts::problem(this, text); });
    connect(&document_, &Document::titleChanged, this,
            [this](const QString& name) { setWindowTitle(tr("TuxSCR – Screen Converter - %1").arg(name)); });
    connect(&document_, &Document::packing, this, [this](int percent) {
        // Packing takes its time: the mouse says so until it is done.
        if (!QApplication::overrideCursor())
            QApplication::setOverrideCursor(Qt::WaitCursor);
        lblInfo_->setText(tr("Packing: %1 %").arg(percent));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        if (percent >= 100)
            QApplication::restoreOverrideCursor();
    });

    editTools_ = new EditTools(this);
    animation_ = new AnimationPanel(this);
    tools_->addTab(animation_, tr("Animation"));
    connect(bpEditTrame_, &QPushButton::clicked, this, [this] {
        PatternDialog dialog(this);
        dialog.exec();
        document_.convert(false);
    });
    connect(chkParamInterne_, &QCheckBox::toggled, this, [this](bool on) {
        if (!on) {
            if (internalSettings_)
                internalSettings_->hide();
            return;
        }
        if (!internalSettings_) {
            internalSettings_ = new InternalSettings(this);
            connect(internalSettings_, &QDialog::finished, chkParamInterne_, [this] { chkParamInterne_->setChecked(false); });
        }
        internalSettings_->fromDocument();
        internalSettings_->show();
    });
    setWindowTitle(tr("TuxSCR – Screen Converter"));
    // The settings of the last time, if TuxAPE keeps them for us.
    const QString kept = settingsFile();
    if (!kept.isEmpty() && QFileInfo::exists(kept)) {
        loading_ = true;
        document_.readParam(kept);
        loading_ = false;
    }
    applyTexts();
    fromDocument();
    infoLines_.clear();
    lblInfo_->clear();
    resize(1280, 860);
}

ConverterWindow::~ConverterWindow() = default;

QString ConverterWindow::settingsFile() const
{
    if (!host_)
        return {};
    const QString folder = host_->dataFolder();
    return folder.isEmpty() ? QString() : QDir(folder).filePath(QStringLiteral("TuxSCR.xml"));
}

void ConverterWindow::closeEvent(QCloseEvent* event)
{
    const QString kept = settingsFile();
    if (!kept.isEmpty())
        writeHostText(kept, paramXml(document_.param, document_.screen));
    QDialog::closeEvent(event);
}

// ---- The settings ----

QWidget* ConverterWindow::buildSettings()
{
    auto* panel = new QWidget;
    auto* column = new QVBoxLayout(panel);
    column->setContentsMargins(0, 0, 6, 0);

    // Files.
    auto* files = new QGroupBox;
    files->setObjectName(QStringLiteral("groupFiles"));
    auto* filesGrid = new QGridLayout(files);
    bpLoad_ = namedButton("bpLoad");
    bpCreate_ = namedButton("bpCreate");
    bpImport_ = namedButton("bpImport");
    bpConvert_ = namedButton("bpConvert");
    bpSave_ = namedButton("bpSave");
    autoRecalc_ = namedCheck("autoRecalc");
    withCode_ = namedCheck("withCode");
    withPalette_ = namedCheck("withPalette");
    chkAllPics_ = namedCheck("chkAllPics");
    chk17K_ = namedCheck("chk17K");
    comboPack_ = new QComboBox;
    comboPack_->setObjectName(QStringLiteral("comboPackMethode"));
    for (const char* name : kPackNames)
        comboPack_->addItem(QLatin1String(name));
    auto* lblPack = namedLabel("lblPack");
    lblPack->setBuddy(comboPack_);
    bpShow_ = namedButton("bpShow");
    bpFromCpc_ = namedButton("bpFromCpc");
    filesGrid->addWidget(bpLoad_, 0, 0);
    filesGrid->addWidget(bpCreate_, 0, 1);
    filesGrid->addWidget(bpImport_, 1, 0);
    filesGrid->addWidget(bpFromCpc_, 1, 1);
    filesGrid->addWidget(bpConvert_, 2, 0);
    filesGrid->addWidget(chkAllPics_, 2, 1);
    filesGrid->addWidget(autoRecalc_, 3, 0, 1, 2);
    filesGrid->addWidget(bpSave_, 4, 0);
    filesGrid->addWidget(bpShow_, 4, 1);
    filesGrid->addWidget(withCode_, 5, 0, 1, 2);
    filesGrid->addWidget(withPalette_, 6, 0, 1, 2);
    auto* packRow = new QHBoxLayout;
    packRow->addWidget(lblPack);
    packRow->addWidget(comboPack_);
    packRow->addWidget(chk17K_);
    packRow->addStretch();
    filesGrid->addLayout(packRow, 7, 0, 1, 2);
    column->addWidget(files);
    bpShow_->setVisible(host_ != nullptr);
    bpFromCpc_->setVisible(host_ != nullptr);

    connect(bpLoad_, &QPushButton::clicked, this, [this] { load(false); });
    connect(bpImport_, &QPushButton::clicked, this, [this] { load(true); });
    connect(bpConvert_, &QPushButton::clicked, this, [this] { document_.convert(true); });
    connect(bpSave_, &QPushButton::clicked, this, &ConverterWindow::save);
    connect(bpShow_, &QPushButton::clicked, this, &ConverterWindow::showInTuxApe);
    connect(bpFromCpc_, &QPushButton::clicked, this, &ConverterWindow::fromCpcScreen);
    connect(bpCreate_, &QPushButton::clicked, this, [this] {
        // How many pictures the animation is to have.
        QDialog dialog(this);
        dialog.setObjectName(QStringLiteral("CreationImages"));
        dialog.setWindowTitle(texts::windowTitle(texts::text("Main.bpCreate")));
        auto* form = new QVBoxLayout(&dialog);
        auto* single = namedRadio("rbSingle", texts::text("CreationImages.rbSingle"));
        auto* several = namedRadio("rbMulti", texts::text("CreationImages.label1"));
        auto* count = namedSpin("nbImages", 2, 255, 2);
        single->setChecked(true);
        count->setEnabled(false);
        connect(several, &QRadioButton::toggled, count, &QWidget::setEnabled);
        auto* row = new QHBoxLayout;
        row->addWidget(several);
        row->addWidget(new QLabel(texts::text("CreationImages.lblNbImages")));
        row->addWidget(count);
        form->addWidget(single);
        form->addLayout(row);
        auto* buttons = new QDialogButtonBox;
        buttons->addButton(texts::text("CreationImages.bpCreer"), QDialogButtonBox::AcceptRole)
            ->setObjectName(QStringLiteral("bpCreer"));
        buttons->addButton(QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        form->addWidget(buttons);
        if (dialog.exec() != QDialog::Accepted)
            return;
        const int frames = single->isChecked() ? 1 : count->value();
        document_.setBlankFrames(frames);
        document_.say(frames == 1 ? texts::text("Main.prg.TxtInfo14") : tr("Animation creation with %n pictures.", nullptr, frames));
        document_.resetPictures(true);
        document_.convert(false);
    });
    connect(autoRecalc_, &QCheckBox::toggled, this, &ConverterWindow::interfaceChange);
    connect(withCode_, &QCheckBox::toggled, this, [this](bool on) {
        document_.param.withCode = on;
        // The routine needs the palette.
        if (on)
            withPalette_->setChecked(true);
        withPalette_->setEnabled(!on);
    });
    connect(withPalette_, &QCheckBox::toggled, this, [this](bool on) { document_.param.withPalette = on; });
    connect(comboPack_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0)
            document_.packMethod = kPackMethods[index];
    });
    connect(chk17K_, &QCheckBox::toggled, this, [this](bool on) { document_.inTwoFiles = on; });
    connect(chkAllPics_, &QCheckBox::toggled, this, [this](bool on) {
        document_.allPictures = on;
        if (on)
            autoRecalc_->setChecked(false);
    });

    // The CPC's resolution.
    auto* reso = new QGroupBox;
    reso->setObjectName(QStringLiteral("resoCPC"));
    auto* resoGrid = new QGridLayout(reso);
    nbCols_ = namedSpin("nbCols", 1, kMaxColumns, 80);
    nbLignes_ = namedSpin("nbLignes", 1, kMaxLines, 200);
    auto* label1 = namedLabel("label1");
    auto* label2 = namedLabel("label2");
    label1->setBuddy(nbCols_);
    label2->setBuddy(nbLignes_);
    auto* bpStandard = namedButton("bpStandard");
    auto* bpOverscan = namedButton("bpOverscan");
    mode_ = new QComboBox;
    mode_->setObjectName(QStringLiteral("mode"));
    auto* label3 = namedLabel("label3");
    label3->setBuddy(mode_);
    trackModeX_ = new QSlider(Qt::Horizontal);
    trackModeX_->setObjectName(QStringLiteral("trackModeX"));
    trackModeX_->setRange(1, 32);
    lblTrack_ = namedLabel("lblTrackModeX");
    lblTrack_->setBuddy(trackModeX_);
    chkSwapEgx_ = namedCheck("chkSwapEgx");
    bpEditTrame_ = namedButton("bpEditTrame");
    bpEditSplit_ = namedButton("bpEditSplit");
    bpRasterPlus_ = namedButton("bpRasterPlus");
    bpEditSprites_ = namedButton("bpEditSprites");
    resoGrid->addWidget(label1, 0, 0);
    resoGrid->addWidget(nbCols_, 0, 1);
    resoGrid->addWidget(bpStandard, 0, 2);
    resoGrid->addWidget(label2, 1, 0);
    resoGrid->addWidget(nbLignes_, 1, 1);
    resoGrid->addWidget(bpOverscan, 1, 2);
    resoGrid->addWidget(label3, 2, 0);
    resoGrid->addWidget(mode_, 2, 1, 1, 2);
    resoGrid->addWidget(lblTrack_, 3, 0);
    resoGrid->addWidget(trackModeX_, 3, 1, 1, 2);
    resoGrid->addWidget(chkSwapEgx_, 4, 0, 1, 3);
    auto* editRow = new QHBoxLayout;
    editRow->addWidget(bpEditTrame_);
    editRow->addWidget(bpEditSplit_);
    editRow->addWidget(bpRasterPlus_);
    editRow->addWidget(bpEditSprites_);
    editRow->addStretch();
    resoGrid->addLayout(editRow, 5, 0, 1, 3);
    column->addWidget(reso);

    connect(nbCols_, &QSpinBox::valueChanged, this, [this](int value) {
        if (loading_)
            return;
        document_.screen.columns = value;
        document_.resetPictures(true);
        document_.screen.tidyInterrupts();
        updateVisibility();
        document_.convert(false);
    });
    connect(nbLignes_, &QSpinBox::valueChanged, this, [this](int value) {
        if (loading_)
            return;
        document_.screen.lines = value;
        document_.resetPictures(true);
        document_.screen.tidyInterrupts();
        updateVisibility();
        document_.convert(false);
    });
    connect(bpStandard, &QPushButton::clicked, this, [this] {
        nbLignes_->setValue(200);
        nbCols_->setValue(80);
    });
    connect(bpOverscan, &QPushButton::clicked, this, [this] {
        nbLignes_->setValue(272);
        nbCols_->setValue(96);
    });
    connect(mode_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (loading_ || index < 0)
            return;
        document_.screen.mode = index;
        document_.resetPictures(true);
        updateVisibility();
        document_.convert(false);
    });
    connect(trackModeX_, &QSlider::valueChanged, this, &ConverterWindow::interfaceChange);
    connect(chkSwapEgx_, &QCheckBox::toggled, this, [this](bool on) {
        if (loading_)
            return;
        document_.screen.yEgx = on ? 2 : 0;
        document_.convert(false);
    });

    // The size of the source.
    auto* size = new QGroupBox;
    size->setObjectName(QStringLiteral("groupBox1"));
    auto* sizeColumn = new QVBoxLayout(size);
    radioFit_ = namedRadio("radioFit");
    radioKeepSmaller_ = namedRadio("radioKeepSmaller");
    radioKeepLarger_ = namedRadio("radioKeepLarger");
    radioUserSize_ = namedRadio("radioUserSize");
    radioOrigin_ = namedRadio("radioOrigin");
    auto* radios = new QGridLayout;
    radios->addWidget(radioFit_, 0, 0);
    radios->addWidget(radioKeepSmaller_, 0, 1);
    radios->addWidget(radioKeepLarger_, 0, 2);
    radios->addWidget(radioUserSize_, 1, 0);
    radios->addWidget(radioOrigin_, 1, 1);
    sizeColumn->addLayout(radios);
    sizeRow_ = new QWidget;
    auto* sizeGrid = new QGridLayout(sizeRow_);
    sizeGrid->setContentsMargins(0, 0, 0, 0);
    numPosX_ = namedSpin("numPosX", -1920, 1920, 0);
    numPosY_ = namedSpin("numPosY", -1080, 1080, 0);
    numSizeX_ = namedSpin("numSizeX", 0, 6000, 0);
    numSizeY_ = namedSpin("numSizeY", 0, 4000, 0);
    auto* label7 = namedLabel("label7");
    auto* label5 = namedLabel("label5");
    label7->setBuddy(numPosX_);
    label5->setBuddy(numSizeX_);
    sizeGrid->addWidget(label7, 0, 0);
    sizeGrid->addWidget(numPosX_, 0, 1);
    sizeGrid->addWidget(numPosY_, 0, 2);
    sizeGrid->addWidget(label5, 1, 0);
    sizeGrid->addWidget(numSizeX_, 1, 1);
    sizeGrid->addWidget(numSizeY_, 1, 2);
    scaleRow_ = new QWidget;
    auto* scaleLine = new QHBoxLayout(scaleRow_);
    scaleLine->setContentsMargins(0, 0, 0, 0);
    const struct {
        const char* name;
        const char* text;
        bool x;
        bool twice;
    } scales[4] = {{"bpXDiv2", "X /2", true, false}, {"bpXMul2", "X *2", true, true}, {"bpYDiv2", "Y /2", false, false}, {"bpYMul2", "Y *2", false, true}};
    for (const auto& scale : scales) {
        auto* button = namedButton(scale.name, QLatin1String(scale.text));
        scaleLine->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, scale] {
            QSpinBox* box = scale.x ? numSizeX_ : numSizeY_;
            box->setValue(scale.twice ? box->value() * 2 : box->value() / 2);
            interfaceChange();
        });
    }
    scaleLine->addStretch();
    sizeGrid->addWidget(scaleRow_, 2, 0, 1, 3);
    sizeColumn->addWidget(sizeRow_);
    column->addWidget(size);
    for (QRadioButton* radio : {radioFit_, radioKeepSmaller_, radioKeepLarger_})
        connect(radio, &QRadioButton::toggled, this, [this](bool on) {
            if (on) {
                sizeModeChanged();
                interfaceChange();
            }
        });
    for (QRadioButton* radio : {radioUserSize_, radioOrigin_})
        connect(radio, &QRadioButton::toggled, this, [this](bool on) {
            if (on)
                sizeModeChanged();
        });
    for (QSpinBox* box : {numPosX_, numPosY_, numSizeX_, numSizeY_})
        connect(box, &QSpinBox::valueChanged, this, &ConverterWindow::interfaceChange);

    // Dithering.
    auto* tramage = new QGroupBox;
    tramage->setObjectName(QStringLiteral("tramage"));
    auto* tramageGrid = new QGridLayout(tramage);
    methode_ = new QComboBox;
    methode_->setObjectName(QStringLiteral("methode"));
    methode_->addItem(QStringLiteral("Aucun"), QStringLiteral("Aucun"));
    for (const std::string& name : ditherNames())
        methode_->addItem(QString::fromStdString(name), QString::fromStdString(name));
    auto* label4 = namedLabel("label4");
    label4->setBuddy(methode_);
    pctTrame_ = namedSpin("pctTrame", 0, 400, 100);
    lblPct_ = namedLabel("lblPct", QStringLiteral("%"));
    chkDiffErr_ = namedCheck("chkDiffErr");
    chkLissage_ = namedCheck("chkLissage");
    chkTrameTC_ = namedCheck("chkTrameTC");
    chkImpDraw_ = namedCheck("chkImpDraw");
    tramageGrid->addWidget(label4, 0, 0);
    tramageGrid->addWidget(methode_, 0, 1, 1, 3);
    tramageGrid->addWidget(pctTrame_, 1, 1);
    tramageGrid->addWidget(lblPct_, 1, 2);
    tramageGrid->addWidget(chkDiffErr_, 1, 3);
    tramageGrid->addWidget(chkLissage_, 2, 0, 1, 2);
    tramageGrid->addWidget(chkTrameTC_, 2, 2, 1, 2);
    tramageGrid->addWidget(chkImpDraw_, 3, 0, 1, 4);
    column->addWidget(tramage);
    connect(methode_, &QComboBox::currentIndexChanged, this, &ConverterWindow::interfaceChange);
    connect(pctTrame_, &QSpinBox::valueChanged, this, [this](int value) {
        if (loading_)
            return;
        document_.param.pct = value;
        document_.convert(false);
    });
    for (QCheckBox* box : {chkDiffErr_, chkLissage_, chkTrameTC_})
        connect(box, &QCheckBox::toggled, this, &ConverterWindow::interfaceChange);
    connect(chkImpDraw_, &QCheckBox::toggled, this, [this](bool on) {
        if (loading_)
            return;
        document_.param.modeImpDraw = on;
        document_.convert(false);
    });

    // Colours.
    auto* colours = new QGroupBox;
    colours->setObjectName(QStringLiteral("groupBox2"));
    auto* coloursColumn = new QVBoxLayout(colours);
    auto* checks = new QGridLayout;
    sortPal_ = namedCheck("sortPal");
    nb_ = namedCheck("nb");
    newMethode_ = namedCheck("newMethode");
    chkNewReduc_ = namedCheck("chkNewReduc");
    chkPalCpc_ = namedCheck("chkPalCpc");
    modePlus_ = namedCheck("modePlus");
    chkGauss_ = namedCheck("chkGauss");
    checks->addWidget(modePlus_, 0, 0);
    checks->addWidget(sortPal_, 0, 1);
    checks->addWidget(nb_, 0, 2);
    checks->addWidget(newMethode_, 1, 0);
    checks->addWidget(chkNewReduc_, 1, 1);
    checks->addWidget(chkGauss_, 1, 2);
    checks->addWidget(chkPalCpc_, 2, 0, 1, 3);
    for (int i = 0; i < 4; ++i) {
        reducPal_[i] = namedCheck(i == 0 ? "reducPal1" : i == 1 ? "reducPal2" : i == 2 ? "reducPal3" : "reducPal4");
        checks->addWidget(reducPal_[i], 3 + i / 2, i % 2);
        connect(reducPal_[i], &QCheckBox::toggled, this, [this, i](bool on) {
            if (loading_)
                return;
            bool* flags[4] = {&document_.param.reductPal1, &document_.param.reductPal2, &document_.param.reductPal3, &document_.param.reductPal4};
            *flags[i] = on;
            document_.convert(false);
        });
    }
    coloursColumn->addLayout(checks);
    auto* bits = new QHBoxLayout;
    const int bitCounts[4] = {24, 12, 9, 6};
    for (int i = 0; i < 4; ++i) {
        rbBits_[i] = namedRadio(i == 0 ? "rb24bits" : i == 1 ? "rb12bits" : i == 2 ? "rb9bits" : "rb6bits");
        bits->addWidget(rbBits_[i]);
        connect(rbBits_[i], &QRadioButton::toggled, this, [this, i, bitCounts](bool on) {
            if (loading_ || !on)
                return;
            document_.param.bitsRVB = bitCounts[i];
            document_.convert(false);
        });
    }
    // The two sets of radio buttons are in the same panel: each is a group
    // of its own.
    auto* bitsGroup = new QButtonGroup(this);
    for (QRadioButton* radio : rbBits_)
        bitsGroup->addButton(radio);
    auto* sizeGroup = new QButtonGroup(this);
    for (QRadioButton* radio : {radioFit_, radioKeepSmaller_, radioKeepLarger_, radioUserSize_, radioOrigin_})
        sizeGroup->addButton(radio);
    bits->addStretch();
    coloursColumn->addLayout(bits);

    auto* sliderGrid = new QGridLayout;
    for (int i = 0; i < 6; ++i) {
        const SliderNames& names = kSliders[i];
        auto* label = namedLabel(names.label, tr(names.text));
        auto* less = namedButton(names.less, QStringLiteral("-"));
        auto* more = namedButton(names.more, QStringLiteral("+"));
        auto* reset = namedButton(names.reset, tr("Reset"));
        less->setFixedWidth(28);
        more->setFixedWidth(28);
        sliders_[i] = new QSlider(Qt::Horizontal);
        sliders_[i]->setObjectName(QLatin1String(names.slider));
        sliders_[i]->setRange(0, 200);
        sliders_[i]->setValue(100);
        sliders_[i]->setMinimumWidth(170);
        label->setBuddy(sliders_[i]);
        sliderGrid->addWidget(label, i, 0);
        sliderGrid->addWidget(less, i, 1);
        sliderGrid->addWidget(sliders_[i], i, 2);
        sliderGrid->addWidget(more, i, 3);
        sliderGrid->addWidget(reset, i, 4);
        QSlider* slider = sliders_[i];
        connect(less, &QPushButton::clicked, slider, [slider] { slider->setValue(slider->value() - 1); });
        connect(more, &QPushButton::clicked, slider, [slider] { slider->setValue(slider->value() + 1); });
        connect(reset, &QPushButton::clicked, slider, [slider] { slider->setValue(100); });
        connect(slider, &QSlider::valueChanged, this, [this, i](int value) {
            if (loading_)
                return;
            Param& param = document_.param;
            int* values[6] = {&param.pctLumi, &param.pctSat, &param.pctContrast, &param.pctRed, &param.pctGreen, &param.pctBlue};
            *values[i] = i == 1 && nb_->isChecked() ? 0 : value;
            sliders_[i]->setToolTip(QString::number(value) + QStringLiteral(" %"));
            document_.convert(false);
        });
    }
    coloursColumn->addLayout(sliderGrid);
    auto* resets = new QHBoxLayout;
    auto* bpRaz = namedButton("bpRaz");
    auto* bpRazAll = namedButton("bpRazAll");
    resets->addWidget(bpRaz);
    resets->addWidget(bpRazAll);
    resets->addStretch();
    coloursColumn->addLayout(resets);
    column->addWidget(colours);
    connect(bpRaz, &QPushButton::clicked, this, [this] { resetColours(false); });
    connect(bpRazAll, &QPushButton::clicked, this, [this] { resetColours(true); });
    connect(sortPal_, &QCheckBox::clicked, this, [this] {
        // Each click goes to the next of: as found, rising, as found,
        // falling.
        document_.param.newSortPal = (document_.param.newSortPal + 1) & 3;
        document_.convert(false);
    });
    connect(nb_, &QCheckBox::toggled, this, [this](bool on) {
        if (loading_)
            return;
        sliders_[1]->setEnabled(!on);
        document_.param.pctSat = on ? 0 : sliders_[1]->value();
        document_.convert(false);
    });
    connect(newMethode_, &QCheckBox::toggled, this, [this](bool on) {
        if (loading_)
            return;
        document_.param.newMethode = on;
        document_.convert(false);
    });
    for (QCheckBox* box : {chkNewReduc_, chkPalCpc_})
        connect(box, &QCheckBox::toggled, this, &ConverterWindow::interfaceChange);
    connect(chkGauss_, &QCheckBox::toggled, this, [this](bool on) {
        if (loading_)
            return;
        document_.param.filtre = on;
        document_.convert(false);
    });
    connect(modePlus_, &QCheckBox::toggled, this, [this](bool on) {
        if (loading_)
            return;
        document_.setPlus(on);
        fillModes();
        updateVisibility();
        document_.touch();
    });

    // The foot.
    auto* foot = new QGridLayout;
    lblVersion_ = namedLabel("lblInfoVersion");
    chkParamInterne_ = namedCheck("chkParamInterne");
    chkInfo_ = namedCheck("chkInfo");
    auto* bpTest = namedButton("bpTest");
    foot->addWidget(lblVersion_, 0, 0, 1, 3);
    foot->addWidget(chkParamInterne_, 1, 0);
    foot->addWidget(chkInfo_, 1, 1);
    foot->addWidget(bpTest, 1, 2);
    column->addLayout(foot);
    column->addStretch();
    lblVersion_->setText(QStringLiteral("TuxSCR %1 - ConvImgCpc %2").arg(QLatin1String(versionString()), QLatin1String(convImgCpcVersion())));
    connect(bpTest, &QPushButton::clicked, this, [this] {
        const QString path = prompts::saveName(this, texts::text("Main.bpTest"), QString::fromStdString(document_.param.lastSavePath),
                                               {tr("Matrix as a source (*.asm)")});
        if (!path.isEmpty() && document_.saveMatrix(path))
            document_.param.lastSavePath = QFileInfo(path).absolutePath().toStdString();
    });
    connect(chkInfo_, &QCheckBox::toggled, this, [this](bool on) {
        if (!on) {
            if (infoWindow_)
                infoWindow_->hide();
            return;
        }
        if (!infoWindow_) {
            infoWindow_ = new QDialog(this);
            infoWindow_->setObjectName(QStringLiteral("Informations"));
            infoWindow_->setWindowTitle(texts::windowTitle(texts::text("Main.chkInfo")));
            auto* box = new QVBoxLayout(infoWindow_);
            infoText_ = new QPlainTextEdit;
            infoText_->setObjectName(QStringLiteral("listInfo"));
            infoText_->setReadOnly(true);
            box->addWidget(infoText_);
            infoWindow_->resize(520, 300);
            connect(infoWindow_, &QDialog::finished, chkInfo_, [this] { chkInfo_->setChecked(false); });
        }
        infoText_->setPlainText(infoLines_.join(QLatin1Char('\n')));
        infoWindow_->show();
    });
    return panel;
}

// ---- The picture ----

QWidget* ConverterWindow::buildPicture()
{
    auto* panel = new QWidget;
    auto* column = new QVBoxLayout(panel);
    column->setContentsMargins(0, 0, 0, 0);

    auto* top = new QHBoxLayout;
    palette_ = new PaletteBar(&document_);
    top->addWidget(palette_, 1);
    auto* side = new QVBoxLayout;
    lockAllPal_ = namedCheck("lockAllPal");
    bpCopyPal_ = namedButton("bpCopyPal");
    bpGenPal_ = namedButton("bpGenPal", tr("Make Palette"));
    chkX2_ = namedCheck("chkX2", QStringLiteral("X2"));
    side->addWidget(lockAllPal_);
    auto* sideRow = new QHBoxLayout;
    sideRow->addWidget(bpCopyPal_);
    sideRow->addWidget(bpGenPal_);
    sideRow->addWidget(chkX2_);
    side->addLayout(sideRow);
    top->addLayout(side);
    column->addLayout(top);

    view_ = new PictureView;
    view_->setObjectName(QStringLiteral("pictureBox"));
    viewScroll_ = new QScrollArea;
    viewScroll_->setObjectName(QStringLiteral("pictureScroll"));
    viewScroll_->setWidget(view_);
    viewScroll_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    viewScroll_->setMinimumSize(660, 300);
    column->addWidget(viewScroll_, 3);

    tools_ = new QTabWidget;
    tools_->setObjectName(QStringLiteral("twTools"));
    interrupts_ = new InterruptsPanel(&document_);
    tools_->addTab(interrupts_, QString());
    tools_->setMinimumHeight(190);
    column->addWidget(tools_, 1);

    lblInfo_ = namedLabel("lblInfo");
    lblInfo_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    column->addWidget(lblInfo_);

    connect(palette_, &PaletteBar::penClicked, this, &ConverterWindow::penClicked);
    connect(lockAllPal_, &QCheckBox::clicked, this, [this](bool on) {
        document_.param.lockState.fill(on ? 1 : 0);
        document_.convert(false);
    });
    connect(bpCopyPal_, &QPushButton::clicked, this, &ConverterWindow::copyPalette);
    connect(chkX2_, &QCheckBox::toggled, view_, &PictureView::setDoubled);
    connect(view_, &PictureView::mouse, this, [this](QPoint at, Qt::MouseButtons buttons) {
        if (!mouseOwner_)
            moveOrSize(at, buttons);
    });
    connect(interrupts_, &InterruptsPanel::marksChanged, this,
            [this] { view_->setInterruptLines(interrupts_->lines(), interrupts_->currentLine()); });
    return panel;
}

void ConverterWindow::setImageProvider(std::function<QImage()> provider)
{
    imageProvider_ = std::move(provider);
    pictureChanged();
}

bool ConverterWindow::doubled() const
{
    return chkX2_->isChecked();
}

void ConverterWindow::holdSettings(QObject* tool, bool hold)
{
    settingsHolders_.removeIf([tool](const QPointer<QObject>& holder) { return holder.isNull() || holder == tool; });
    if (hold)
        settingsHolders_.append(tool);
    settingsPanel_->setEnabled(settingsHolders_.isEmpty());
}

void ConverterWindow::setPenHandler(std::function<void(int, bool)> handler)
{
    penHandler_ = std::move(handler);
}

// ---- Texts ----

void ConverterWindow::applyTexts()
{
    // ConvImgCpc names its controls window by window: "label1" is one thing
    // in its main window and another elsewhere. Each part is given the
    // texts of the window it comes from.
    texts::apply(settingsPanel_, "Main");
    lockAllPal_->setText(texts::text("ImageCpc.lockAllPal"));
    bpCopyPal_->setText(texts::text("ImageCpc.bpCopyPal"));
    findChild<QGroupBox*>(QStringLiteral("groupFiles"))->setTitle(tr("Files"));
    findChild<QGroupBox*>(QStringLiteral("groupBox2"))->setTitle(texts::text("Main.groupBox2", tr("Colours")));
    lblTrack_->setText(tr("Lines followed"));
    bpGenPal_->setText(tr("Make Palette"));
    methode_->setItemText(0, tr("None"));
    tools_->setTabText(tools_->indexOf(interrupts_), texts::text("Interrupts.title"));
    interrupts_->applyTexts();
    palette_->applyTexts();
    // What a control is, said to those who do not see its label.
    for (int i = 0; i < 6; ++i)
        sliders_[i]->setAccessibleName(tr(kSliders[i].text));
    numPosX_->setAccessibleName(tr("Position X"));
    numPosY_->setAccessibleName(tr("Position Y"));
    numSizeX_->setAccessibleName(tr("Size X"));
    numSizeY_->setAccessibleName(tr("Size Y"));
    pctTrame_->setAccessibleName(tr("Dithering, percent"));
    view_->setAccessibleName(tr("CPC picture"));
    fillModes();
    // The settings are as wide as their texts in the language, with room
    // for the bar that scrolls them.
    settingsPanel_->adjustSize();
    settingsScroll_->setFixedWidth(settingsPanel_->sizeHint().width() + style()->pixelMetric(QStyle::PM_ScrollBarExtent) + 12);
}

void ConverterWindow::fillModes()
{
    const bool was = loading_;
    loading_ = true;
    mode_->clear();
    // Capturing sprites is the Plus's.
    const int count = document_.screen.plus ? kModeCount : kModeCount - 1;
    for (int i = 0; i < count; ++i)
        mode_->addItem(QLatin1String(kModeNames[i]));
    if (document_.screen.mode >= count)
        document_.screen.mode = kMode1;
    mode_->setCurrentIndex(document_.screen.mode);
    loading_ = was;
}

// ---- The controls and the document ----

void ConverterWindow::fromDocument()
{
    const bool was = loading_;
    loading_ = true;
    const Param& param = document_.param;
    const Screen& screen = document_.screen;
    autoRecalc_->setChecked(param.autoRecalc);
    withCode_->setChecked(param.withCode);
    withPalette_->setChecked(param.withPalette || param.withCode);
    withPalette_->setEnabled(!param.withCode);
    for (int i = 0; i < 5; ++i)
        if (kPackMethods[i] == document_.packMethod)
            comboPack_->setCurrentIndex(i);
    chk17K_->setChecked(document_.inTwoFiles);
    chkAllPics_->setChecked(document_.allPictures);
    nbCols_->setValue(screen.columns);
    nbLignes_->setValue(screen.lines);
    modePlus_->setChecked(screen.plus);
    fillModes();
    trackModeX_->setValue(param.trackModeX);
    chkSwapEgx_->setChecked(screen.yEgx == 2);
    QRadioButton* sizes[5] = {radioFit_, radioKeepSmaller_, radioKeepLarger_, radioUserSize_, radioOrigin_};
    sizes[static_cast<int>(param.sizeMode)]->setChecked(true);
    numPosX_->setValue(param.posX);
    numPosY_->setValue(param.posY);
    numSizeX_->setValue(param.sizeX);
    numSizeY_->setValue(param.sizeY);
    const int method = methode_->findData(QString::fromStdString(param.method));
    methode_->setCurrentIndex(method < 0 ? 0 : method);
    pctTrame_->setValue(param.pct > 0 || method > 0 ? param.pct : 100);
    chkDiffErr_->setChecked(param.diffErr);
    chkLissage_->setChecked(param.lissage);
    chkTrameTC_->setChecked(param.trameTc);
    chkImpDraw_->setChecked(param.modeImpDraw);
    sortPal_->setChecked((param.newSortPal & 1) == 1);
    newMethode_->setChecked(param.newMethode);
    chkNewReduc_->setChecked(param.newReduc);
    chkPalCpc_->setChecked(param.setPalCpc);
    chkGauss_->setChecked(param.filtre);
    const bool flags[4] = {param.reductPal1, param.reductPal2, param.reductPal3, param.reductPal4};
    for (int i = 0; i < 4; ++i)
        reducPal_[i]->setChecked(flags[i]);
    rbBits_[param.bitsRVB == 12 ? 1 : param.bitsRVB == 9 ? 2 : param.bitsRVB == 6 ? 3 : 0]->setChecked(true);
    const int values[6] = {param.pctLumi, nb_->isChecked() ? sliders_[1]->value() : param.pctSat, param.pctContrast, param.pctRed, param.pctGreen, param.pctBlue};
    for (int i = 0; i < 6; ++i)
        sliders_[i]->setValue(values[i]);
    loading_ = was;
    document_.param.pct = pctTrame_->value();
    updateVisibility();
    pictureChanged();
    if (internalSettings_)
        internalSettings_->fromDocument();
}

void ConverterWindow::interfaceChange()
{
    if (loading_)
        return;
    Param& param = document_.param;
    if (autoRecalc_->isChecked())
        chkAllPics_->setChecked(false);
    const QString method = methode_->currentData().toString();
    const bool dithered = method != QLatin1String("Aucun");
    // Floyd-Steinberg's matrix is made to be diffused.
    if (!chkDiffErr_->isVisibleTo(this) && !chkDiffErr_->isVisible())
        chkDiffErr_->setChecked(method == QLatin1String("Floyd-Steinberg (2x2)"));
    param.method = method.toStdString();
    param.diffErr = chkDiffErr_->isChecked();
    param.lissage = chkLissage_->isChecked();
    param.trameTc = chkTrameTC_->isChecked();
    param.newReduc = chkNewReduc_->isChecked();
    param.setPalCpc = chkPalCpc_->isChecked();
    param.trackModeX = trackModeX_->value();
    param.autoRecalc = autoRecalc_->isChecked();
    param.pct = pctTrame_->value();
    param.sizeMode = radioKeepLarger_->isChecked() ? Param::SizeMode::KeepLarger
        : radioKeepSmaller_->isChecked() ? Param::SizeMode::KeepSmaller
        : radioFit_->isChecked() ? Param::SizeMode::Fit
        : radioOrigin_->isChecked() ? Param::SizeMode::Origin
        : Param::SizeMode::UserSize;
    param.posX = numPosX_->value();
    param.posY = numPosY_->value();
    param.sizeX = numSizeX_->value();
    param.sizeY = numSizeY_->value();
    lblPct_->setVisible(dithered);
    pctTrame_->setVisible(dithered);
    chkDiffErr_->setVisible(dithered);
    document_.convert(false);
}

void ConverterWindow::updateVisibility()
{
    const Screen& screen = document_.screen;
    const bool dithered = methode_->currentData().toString() != QLatin1String("Aucun");
    lblPct_->setVisible(dithered);
    pctTrame_->setVisible(dithered);
    chkDiffErr_->setVisible(dithered);
    const bool lineColours = screen.mode == kModeX || screen.mode == kModeSplit;
    trackModeX_->setVisible(lineColours);
    lblTrack_->setVisible(lineColours);
    bpEditTrame_->setVisible(screen.mode == kModeAscUt);
    chkSwapEgx_->setVisible(screen.mode == kModeEgx1 || screen.mode == kModeEgx2);
    bpRasterPlus_->setVisible(screen.lines == 272 && screen.columns == 96 && screen.plus);
    bpEditSplit_->setVisible(!screen.plus);
    bpEditSprites_->setVisible(screen.plus);
    newMethode_->setVisible(!screen.plus);
    bpGenPal_->setVisible(screen.plus);
    chkAllPics_->setVisible(document_.frameCount() > 1);
    const bool placed = radioUserSize_->isChecked() || radioOrigin_->isChecked();
    sizeRow_->setVisible(placed);
    scaleRow_->setVisible(radioUserSize_->isChecked());
    for (QSpinBox* box : {numPosX_, numPosY_, numSizeX_, numSizeY_})
        box->setEnabled(!radioOrigin_->isChecked());
}

void ConverterWindow::sizeModeChanged()
{
    if (loading_)
        return;
    const bool origin = radioOrigin_->isChecked();
    const bool user = radioUserSize_->isChecked();
    if (origin || (user && numSizeX_->value() == 0 && numSizeY_->value() == 0)) {
        loading_ = true;
        numPosX_->setValue(0);
        numPosY_->setValue(0);
        numSizeX_->setValue(document_.source().width());
        numSizeY_->setValue(document_.source().height());
        loading_ = false;
    }
    updateVisibility();
    if (origin || user)
        interfaceChange();
}

void ConverterWindow::resetColours(bool all)
{
    Param& param = document_.param;
    if (all) {
        const Param fresh;
        param.method = "Aucun";
        param.lissage = param.trameTc = param.newReduc = param.diffErr = param.modeImpDraw = param.filtre = false;
        param.coefR = fresh.coefR;
        param.coefV = fresh.coefV;
        param.coefB = fresh.coefB;
        param.cstR1 = param.cstV1 = param.cstB1 = 85;
        param.cstR2 = param.cstV2 = param.cstB2 = 170;
        param.cstR3 = param.cstV3 = param.cstB3 = 255;
        param.cstR4 = param.cstV4 = param.cstB4 = 340;
        param.lockState.fill(0);
        param.disableState.fill(0);
        param.kMeansDist = 0;
        param.kMeansColor = 16;
    }
    param.pctRed = param.pctGreen = param.pctBlue = param.pctLumi = param.pctSat = param.pctContrast = 100;
    param.newMethode = false;
    param.reductPal1 = param.reductPal2 = param.reductPal3 = param.reductPal4 = false;
    param.bitsRVB = 24;
    loading_ = true;
    nb_->setChecked(false);
    sliders_[1]->setEnabled(true);
    loading_ = false;
    fromDocument();
    document_.convert(false);
}

void ConverterWindow::pictureChanged()
{
    view_->setImage(imageProvider_ ? imageProvider_() : document_.shown());
    palette_->refresh();
    interrupts_->refresh();
    const bool was = lockAllPal_->blockSignals(true);
    bool allLocked = true;
    for (int i = 0; i < document_.screen.maxPen(document_.screen.yEgx ^ 2); ++i)
        allLocked = allLocked && document_.param.lockState[static_cast<size_t>(i)] != 0;
    lockAllPal_->setChecked(allLocked);
    lockAllPal_->blockSignals(was);
    // ConvImgCpc does not save while it converts at every change.
    bpSave_->setEnabled(!autoRecalc_->isChecked());
}

void ConverterWindow::info(const QString& text)
{
    infoLines_ << text;
    while (infoLines_.size() > 500)
        infoLines_.removeFirst();
    lblInfo_->setText(text);
    if (infoText_ && infoWindow_ && infoWindow_->isVisible())
        infoText_->appendPlainText(text);
}

// ---- The palette ----

void ConverterWindow::penClicked(int pen, bool right)
{
    if (penHandler_) {
        penHandler_(pen, right);
        return;
    }
    Screen& screen = document_.screen;
    int colour = screen.palette[static_cast<size_t>(pen)];
    if (!ColourDialog::choose(this, pen, colour, screen.plus))
        return;
    screen.palette[static_cast<size_t>(pen)] = colour;
    // A colour that was chosen is kept.
    document_.param.lockState[static_cast<size_t>(pen)] = 1;
    screen.tidyInterrupts();
    document_.convert(false);
}

void ConverterWindow::copyPalette()
{
    const Screen& screen = document_.screen;
    const int maxPen = screen.maxPen(screen.yEgx ^ 2);
    QString text;
    for (int i = 0; i < maxPen; ++i) {
        const int value = screen.palette[static_cast<size_t>(i)];
        text += screen.plus ? QStringLiteral("&%1").arg(asicWord(value), 3, 16, QLatin1Char('0')).toUpper() : QString::number(value);
        if (i < maxPen - 1)
            text += QLatin1Char(',');
    }
    text += QStringLiteral("\r\n");
    text += screen.plus ? QStringLiteral(";ASIC Values :\r\n\tDW\t") : QStringLiteral(";Gate Array Values :\r\n\tDB\t");
    for (int i = 0; i < maxPen; ++i) {
        int value = screen.palette[static_cast<size_t>(i)];
        if (value == kNoColour)
            value = 0;
        text += screen.plus ? QStringLiteral("#%1").arg(asicWord(value), 3, 16, QLatin1Char('0')).toUpper()
                            : QStringLiteral("#%1").arg(static_cast<int>(kCpcVga[value >= 0 && value < 27 ? value : 0]), 2, 16, QLatin1Char('0')).toUpper();
        if (i < maxPen - 1)
            text += QLatin1Char(',');
    }
    QApplication::clipboard()->setText(text);
    document_.say(tr("Palette copied to the clipboard"));
}

// ---- The source moved and sized with the mouse ----

void ConverterWindow::moveOrSize(QPoint at, Qt::MouseButtons buttons)
{
    const Screen& screen = document_.screen;
    if (!radioUserSize_->isChecked() || at.x() < 0 || at.y() < 0 || at.y() >= screen.height() || at.x() >= screen.width())
        return;
    const auto grab = [&](bool& flag) {
        if (flag)
            return false;
        posX_ = numPosX_->value();
        posY_ = numPosY_->value();
        sizeX_ = numSizeX_->value();
        sizeY_ = numSizeY_->value();
        memoX_ = at.x();
        memoY_ = at.y();
        flag = true;
        return true;
    };
    const auto place = [this](int posX, int posY, int sizeX, int sizeY) {
        loading_ = true;
        numPosX_->setValue(posX);
        numPosY_->setValue(posY);
        numSizeX_->setValue(sizeX);
        numSizeY_->setValue(sizeY);
        loading_ = false;
        interfaceChange();
    };
    if (buttons & Qt::LeftButton) {
        if (!grab(movePos_))
            place(posX_ + memoX_ - at.x(), posY_ + memoY_ - at.y(), sizeX_, sizeY_);
    } else if (buttons & Qt::RightButton) {
        if (!grab(moveSize_))
            place(posX_, posY_, sizeX_ - memoX_ + at.x(), sizeY_ - memoY_ + at.y());
    } else {
        movePos_ = moveSize_ = false;
    }
}

// ---- Files ----

void ConverterWindow::openFile(const QString& path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String("pal"))
        document_.readPalette(path);
    else if (ext == QLatin1String("xml"))
        document_.readParam(path);
    else if (ext == QLatin1String("kit") && document_.screen.plus)
        document_.readKit(path);
    else
        document_.readFile(path);
    if (ext == QLatin1String("pal") || ext == QLatin1String("kit")) {
        // The colours that were read are kept: with none locked, the next
        // conversion would search them again.
        const int maxPen = document_.screen.maxPen(document_.screen.yEgx ^ 2);
        bool oneLocked = false;
        for (int i = 0; i < maxPen; ++i)
            oneLocked = oneLocked || document_.param.lockState[static_cast<size_t>(i)] != 0;
        if (!oneLocked)
            document_.param.lockState.fill(1);
        document_.convert(false);
    }
    document_.param.lastReadPath = QFileInfo(path).absolutePath().toStdString();
}

void ConverterWindow::load(bool single)
{
    QStringList filters = {texts::text("Main.prg.TxtInfo16") + QStringLiteral(" (*.bmp *.gif *.png *.jpg *.jpeg *.jfif *.scr *.imp)"),
                           texts::text("Main.prg.TxtInfo18") + QStringLiteral(" (*.pal)"),
                           texts::text("Main.prg.TxtInfo19") + QStringLiteral(" (*.xml)")};
    if (document_.screen.plus)
        filters << texts::text("Main.prg.TxtInfo39") + QStringLiteral(" (*.kit)");
    filters << texts::text("Main.prg.TxtInfo17") + QStringLiteral(" (*)");
    if (single)
        filters = {filters.first(), filters.last()};
    const QString path = prompts::openName(this, texts::text(single ? "Main.bpImport" : "Main.bpLoad"),
                                           QString::fromStdString(document_.param.lastReadPath), filters);
    if (path.isEmpty())
        return;
    chkX2_->setChecked(false);
    if (single) {
        document_.readFile(path, true);
        document_.param.lastReadPath = QFileInfo(path).absolutePath().toStdString();
    } else {
        openFile(path);
    }
}

void ConverterWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void ConverterWindow::dropEvent(QDropEvent* event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty() || !urls.first().isLocalFile())
        return;
    event->acceptProposedAction();
    chkX2_->setChecked(false);
    openFile(urls.first().toLocalFile());
}

bool ConverterWindow::askLabels(const QString& kind, const QString& name, AsmLabels& labels)
{
    // ConvImgCpc's SaveMedia: the labels a source gives what it holds.
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("SaveMedia"));
    dialog.setWindowTitle(texts::windowTitle(tr("Save %1 (asm format)").arg(kind)));
    auto* form = new QGridLayout(&dialog);
    auto* chkMedia = namedCheck("chkLabelMedia", tr("%1 Label").arg(kind));
    auto* txbMedia = new QLineEdit(name);
    txbMedia->setObjectName(QStringLiteral("txbLabelMedia"));
    auto* chkPalette = namedCheck("chkLabelPalette", tr("%1 Palette Label").arg(kind));
    auto* txbPalette = new QLineEdit(QStringLiteral("Palette") + name);
    txbPalette->setObjectName(QStringLiteral("txbLabelPalette"));
    chkMedia->setChecked(true);
    chkPalette->setChecked(true);
    connect(chkMedia, &QCheckBox::toggled, txbMedia, &QWidget::setEnabled);
    connect(chkPalette, &QCheckBox::toggled, txbPalette, &QWidget::setEnabled);
    form->addWidget(chkMedia, 0, 0);
    form->addWidget(txbMedia, 0, 1);
    form->addWidget(chkPalette, 1, 0);
    form->addWidget(txbPalette, 1, 1);
    chkPalette->setVisible(document_.param.withPalette);
    txbPalette->setVisible(document_.param.withPalette);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setObjectName(QStringLiteral("bpOk"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addWidget(buttons, 2, 0, 1, 2);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    labels.media = chkMedia->isChecked() ? txbMedia->text().toStdString() : std::string();
    labels.palette = chkPalette->isChecked() && document_.param.withPalette ? txbPalette->text().toStdString() : std::string();
    return true;
}

void ConverterWindow::save()
{
    const Screen& screen = document_.screen;
    // What can be saved, in ConvImgCpc's order, each with what saves it.
    struct Format {
        QString filter;
        std::function<bool(const QString&)> save;
    };
    QList<Format> formats;
    const auto name = [](const QString& path) { return QFileInfo(path).completeBaseName(); };
    const auto asmSaver = [this, name](const char* kind, std::function<bool(const QString&, const AsmLabels&)> write) {
        return [this, name, kind, write](const QString& path) {
            AsmLabels labels;
            return askLabels(QLatin1String(kind), name(path), labels) && write(path, labels);
        };
    };
    formats << Format{texts::text("Main.prg.TxtInfo20") + QStringLiteral(" (*.scr)"), [this](const QString& path) { return document_.saveScreen(path, false); }};
    formats << Format{tr("Bitmap (*.png)"), [this](const QString& path) { return document_.savePng(path); }};
    formats << Format{texts::text("Main.prg.TxtInfo21") + QStringLiteral(" (*.asm)"),
                      asmSaver("Soft sprite", [this](const QString& path, const AsmLabels& labels) { return document_.saveSprite(path, labels, false); })};
    formats << Format{texts::text("Main.prg.TxtInfo22") + QStringLiteral(" (*.asm)"),
                      asmSaver("Soft sprite", [this](const QString& path, const AsmLabels& labels) { return document_.saveSprite(path, labels, true); })};
    formats << Format{texts::text("Main.prg.TxtInfo23") + QStringLiteral(" (*.cmp)"), [this](const QString& path) { return document_.savePacked(path, {}, 0); }};
    formats << Format{texts::text("Main.prg.TxtInfo24") + QStringLiteral(" (*.asm)"),
                      asmSaver("Image", [this](const QString& path, const AsmLabels& labels) { return document_.savePacked(path, labels, 1); })};
    formats << Format{texts::text("Main.prg.TxtInfo31") + QStringLiteral(" (*.dsk)"), [this](const QString& path) { return document_.saveScreen(path, true); }};
    formats << Format{texts::text("Main.prg.TxtInfo32") + QStringLiteral(" (*.dsk)"), [this](const QString& path) { return document_.savePacked(path, {}, 2); }};
    formats << Format{texts::text("Main.prg.TxtInfo25") + QStringLiteral(" (*.asm)"), [this](const QString& path) { return animation_->saveAnimation(path); }};
    formats << Format{texts::text("Main.prg.TxtInfo26") + QStringLiteral(" (*.imp)"), [this](const QString& path) { return document_.saveImp(path); }};
    formats << Format{texts::text("Main.prg.TxtInfo36") + QStringLiteral(" (*.imp)"), [this](const QString& path) { return document_.saveTiles(path, 16, 16); }};
    formats << Format{texts::text("Main.prg.TxtInfo37") + QStringLiteral(" (*.imp)"), [this](const QString& path) { return document_.saveTiles(path, 32, 16); }};
    formats << Format{texts::text("Main.prg.TxtInfo38") + QStringLiteral(" (*.imp)"), [this](const QString& path) { return document_.saveTiles(path, 32, 32); }};
    formats << Format{texts::text("Main.prg.TxtInfo19") + QStringLiteral(" (*.xml)"), [this](const QString& path) { return document_.saveParam(path); }};
    formats << Format{texts::text("Main.prg.TxtInfo18") + QStringLiteral(" (*.pal)"), [this](const QString& path) { return document_.savePalette(path); }};
    if (screen.plus)
        formats << Format{texts::text("Main.prg.TxtInfo27") + QStringLiteral(" (*.kit)"), [this](const QString& path) { return document_.saveKit(path, true); }};
    if (screen.mode == kModeEgx1 || screen.mode == kModeEgx2)
        formats << Format{texts::text("Main.prg.TxtInfo28") + QStringLiteral(" (*.scr)"), [this](const QString& path) { return document_.saveEgx(path); }};
    formats << Format{tr("Bump (*.asm)"), [this](const QString& path) { return document_.saveBump(path); }};
    formats << Format{tr("DiffAnim (*.asm)"), [this](const QString& path) { return document_.saveDiffImages(path); }};
    formats << Format{tr("Tunnel (*.asm)"), [this](const QString& path) { return document_.saveTunnel(path); }};
    if (screen.interruptsActive())
        formats << Format{texts::text("Interrupts.title") + QStringLiteral(" (*.txt)"), [this](const QString& path) { return document_.savePaletteInterrupts(path); }};
    for (const SaveFormat& extra : extraFormats_)
        formats << Format{extra.filter, extra.save};

    QStringList filters;
    for (const Format& format : formats)
        filters << format.filter;
    int chosen = 0;
    const QString path = prompts::saveName(this, texts::text("Main.bpSave"), QString::fromStdString(document_.param.lastSavePath), filters, &chosen);
    if (path.isEmpty() || chosen < 0 || chosen >= formats.size())
        return;
    const bool saved = formats[chosen].save(path);
    if (saved)
        document_.param.lastSavePath = QFileInfo(path).absolutePath().toStdString();
}

// ---- TuxAPE ----

void ConverterWindow::showInTuxApe()
{
    if (!host_)
        return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const std::string program = document_.showProgram();
    QApplication::restoreOverrideCursor();
    const QString folder = host_->dataFolder();
    const QString path = QDir(folder.isEmpty() ? QDir::tempPath() : folder).filePath(QStringLiteral("tuxscr_show.asm"));
    if (!writeHostText(path, program)) {
        document_.fail(tr("Cannot write %1").arg(QDir::toNativeSeparators(path)));
        return;
    }
    if (host_->assembleAndRun(path))
        document_.say(tr("Picture shown in TuxAPE (a key ends it)."));
}

void ConverterWindow::fromCpcScreen()
{
    if (!host_)
        return;
    const tuxape::PluginScreen cpc = host_->screen();
    Screen& screen = document_.screen;
    const int columns = std::clamp(cpc.columns * 2, 1, kMaxColumns);
    const int lines = std::clamp(cpc.rows * cpc.rowHeight, 1, kMaxLines);
    const bool plus = cpc.plusColours.size() >= 16;
    // The pens' colours: the Plus's twelve bits as they are, the Gate
    // Array's numbers as the firmware's.
    screen.plus = plus;
    for (int pen = 0; pen < 17 && pen < cpc.inks.size(); ++pen) {
        if (plus && pen < cpc.plusColours.size()) {
            const int grb = cpc.plusColours[pen];
            screen.palette[static_cast<size_t>(pen)] = plusColour((grb >> 4) & 15, (grb >> 8) & 15, grb & 15);
        } else {
            const char* found = strchr(kCpcVga, '@' + (cpc.inks[pen] & 31));
            screen.palette[static_cast<size_t>(pen)] = found && *found ? static_cast<int>(found - kCpcVga) : 0;
        }
    }
    screen.mode = std::clamp(cpc.mode, 0, 2);
    screen.columns = columns;
    screen.lines = lines;
    // The screen's memory as the CRTC reads it: a line of a row is 2K
    // further, and a row carries on where the one before stopped.
    const QByteArray memory = host_->readMemory(0, 0x10000);
    std::vector<uint8_t> sprite(static_cast<size_t>(columns * lines), 0);
    for (int line = 0; line < lines; ++line) {
        const int row = line / std::max(1, cpc.rowHeight);
        const int raster = line % std::max(1, cpc.rowHeight);
        for (int x = 0; x < columns; ++x) {
            const int ma = cpc.crtcStart + row * cpc.columns + (x >> 1);
            const int address = ((ma & 0x3000) << 2) | ((raster & 7) << 11) | ((ma & 0x3FF) << 1) | (x & 1);
            if (address < memory.size())
                sprite[static_cast<size_t>(line * columns + x)] = static_cast<uint8_t>(memory[address]);
        }
    }
    document_.setSources({toQImage(drawSprite(sprite, columns, lines, screen, nullptr))}, 0);
    drawSprite(sprite, columns, lines, screen, &document_.picture());
    document_.param.lockState.fill(1);
    fromDocument();
    document_.say(tr("Picture taken from the CPC's screen."));
    document_.touch();
}

}  // namespace tuxscr
