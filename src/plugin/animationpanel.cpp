#include "animationpanel.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "controls.h"
#include "converterwindow.h"
#include "prompts.h"
#include "texts.h"

using namespace tuxscr;

namespace {

QString both(const char* french, const char* englishText)
{
    return QString::fromUtf8(texts::language() == QLatin1String("EN") ? englishText : french);
}

// An address as ConvImgCpc's windows take it: "#200", "&200" or "512".
int address(const QString& text)
{
    const QString typed = text.trimmed();
    bool ok = false;
    const int value = typed.startsWith('#') || typed.startsWith('&') ? typed.mid(1).toInt(&ok, 16) : typed.toInt(&ok, 10);
    return ok ? value : -1;
}

}  // namespace

AnimationPanel::AnimationPanel(ConverterWindow* window)
    : QWidget(window)
    , window_(window)
    , document_(window->document())
{
    setObjectName(QStringLiteral("Animation"));
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(6, 6, 6, 6);
    auto* top = new QHBoxLayout;
    lblNumImage_ = namedLabel("lblNumImage");
    numImage_ = namedSpin("numImage", 0, 0, 0);
    lblNumImage_->setBuddy(numImage_);
    lblMaxImage_ = namedLabel("lblMaxImage");
    rbSource_ = namedRadio("rbSource");
    rvCalculee_ = namedRadio("rvCalculee");
    rbSource_->setChecked(true);
    bpPlay_ = namedButton("bpPlay");
    bpStop_ = namedButton("bpStop");
    bpSaveGif_ = namedButton("bpSaveGif");
    bpDel1_2_ = namedButton("bpDel1_2");
    for (QWidget* widget : std::initializer_list<QWidget*>{lblNumImage_, numImage_, lblMaxImage_, rbSource_, rvCalculee_, bpPlay_, bpStop_, bpSaveGif_, bpDel1_2_})
        top->addWidget(widget);
    top->addStretch();
    column->addLayout(top);

    auto* strip = new QGridLayout;
    for (int i = 0; i < 5; ++i) {
        pictures_[i] = new QToolButton;
        pictures_[i]->setObjectName(QStringLiteral("pictureBox%1").arg(i + 1));
        pictures_[i]->setIconSize(QSize(128, 80));
        pictures_[i]->setFixedSize(136, 88);
        times_[i] = new QLineEdit;
        times_[i]->setObjectName(QStringLiteral("txbTps%1").arg(i + 1));
        times_[i]->setMaximumWidth(60);
        removes_[i] = namedButton(i == 0 ? "bpSup1" : i == 1 ? "bpSup2" : i == 2 ? "bpSup3" : i == 3 ? "bpSup4" : "bpSup5");
        strip->addWidget(pictures_[i], 0, i * 2, 1, 2);
        strip->addWidget(times_[i], 1, i * 2);
        strip->addWidget(removes_[i], 1, i * 2 + 1);
        connect(pictures_[i], &QToolButton::clicked, this, [this, i] {
            const int frame = numImage_->value() + i;
            if (frame <= numImage_->maximum())
                numImage_->setValue(frame);
        });
        connect(times_[i], &QLineEdit::textEdited, this, [this, i](const QString& text) {
            bool ok = false;
            const int time = text.toInt(&ok);
            const int frame = numImage_->value() + i;
            if (ok && time > 0 && time <= 5000 && frame < document_->frameCount())
                document_->setFrameTime(frame, time);
        });
        connect(removes_[i], &QPushButton::clicked, this, [this, i] {
            const int frame = numImage_->value() + i;
            document_->say(both("Suppression image ", "Frame removed: ") + QString::number(frame));
            document_->removeFrame(frame);
        });
    }
    strip->setColumnStretch(10, 1);
    column->addLayout(strip);
    scroll_ = new QScrollBar(Qt::Horizontal);
    scroll_->setObjectName(QStringLiteral("hScrollBar1"));
    column->addWidget(scroll_);
    column->addStretch();

    timer_ = new QTimer(this);
    timer_->setInterval(20);
    connect(numImage_, &QSpinBox::valueChanged, this, [this](int value) {
        if (filling_)
            return;
        document_->select(value);
        document_->convert(false);
    });
    connect(scroll_, &QScrollBar::valueChanged, numImage_, &QSpinBox::setValue);
    connect(rbSource_, &QRadioButton::toggled, this, &AnimationPanel::refresh);
    connect(bpDel1_2_, &QPushButton::clicked, this, [this] {
        // One frame in two is taken away, from the last.
        for (int i = document_->frameCount() - 1; i > 0; i -= 2) {
            document_->removeFrame(i);
            document_->say(both("Suppression image ", "Frame removed: ") + QString::number(i));
        }
        document_->select(0);
    });
    connect(bpPlay_, &QPushButton::clicked, this, [this] {
        bpPlay_->setEnabled(false);
        bpStop_->setEnabled(true);
        window_->holdSettings(this, true);
        shownSince_ = QDateTime::currentMSecsSinceEpoch();
        numImage_->setValue(0);
        timer_->start();
    });
    connect(bpStop_, &QPushButton::clicked, this, [this] {
        timer_->stop();
        bpPlay_->setEnabled(document_->frameCount() > 1);
        bpStop_->setEnabled(false);
        window_->holdSettings(this, false);
    });
    connect(timer_, &QTimer::timeout, this, [this] {
        const int frame = numImage_->value();
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - shownSince_ > document_->frameTime(frame)) {
            shownSince_ = now;
            numImage_->setValue(frame + 1 > numImage_->maximum() ? 0 : frame + 1);
        }
    });
    connect(bpSaveGif_, &QPushButton::clicked, this, [this] {
        const QString path = prompts::saveName(this, texts::text("Animation.bpSaveGif"), QString::fromStdString(document_->param.lastSavePath),
                                               {QStringLiteral("Gif anim (*.gif)")});
        if (!path.isEmpty())
            document_->saveGif(path);
    });
    connect(document_, &Document::framesChanged, this, &AnimationPanel::refresh);
    connect(document_, &Document::changed, this, &AnimationPanel::refresh);
    connect(window_, &ConverterWindow::languageChanged, this, &AnimationPanel::retranslate);
    bpStop_->setEnabled(false);
    retranslate();
}

void AnimationPanel::retranslate()
{
    texts::apply(this, "Animation");
    lblNumImage_->setText(both("N° image", "Frame"));
    bpDel1_2_->setText(both("Supprimer 1 image sur 2", "Remove 1 Frame in 2"));
    for (int i = 0; i < 5; ++i) {
        times_[i]->setAccessibleName(both("Durée de l'image, en millisecondes", "Time the frame stays, in milliseconds"));
        times_[i]->setToolTip(times_[i]->accessibleName());
    }
    scroll_->setAccessibleName(both("Image courante", "Current frame"));
    QTabWidget* tools = window_->tools();
    if (tools->indexOf(this) >= 0)
        tools->setTabText(tools->indexOf(this), QStringLiteral("Animation"));
    refresh();
}

void AnimationPanel::refresh()
{
    if (filling_)
        return;
    filling_ = true;
    const int count = document_->frameCount();
    const int current = std::min(document_->current(), count - 1);
    const bool several = count > 1;
    numImage_->setMaximum(count - 1);
    numImage_->setValue(current);
    scroll_->setRange(0, count - 1);
    scroll_->setValue(current);
    scroll_->setVisible(count > 5);
    lblMaxImage_->setText(texts::text("Animation.lblMaxImage") + QString::number(count));
    for (QWidget* widget : std::initializer_list<QWidget*>{lblMaxImage_, lblNumImage_, numImage_, bpDel1_2_})
        widget->setVisible(several);
    bpPlay_->setEnabled(several && !timer_->isActive());
    // The GIF is of the converted frames.
    bpSaveGif_->setVisible(several && rvCalculee_->isChecked());
    const bool source = rbSource_->isChecked();
    for (int i = 0; i < 5; ++i) {
        const int frame = current + i;
        const bool there = frame < count;
        pictures_[i]->setToolTip(both("Image ", "Frame ") + QString::number(frame));
        pictures_[i]->setAccessibleName(pictures_[i]->toolTip());
        if (there) {
            const QImage picture = source ? document_->source(frame) : document_->shown(frame);
            pictures_[i]->setIcon(QIcon(QPixmap::fromImage(picture.scaled(128, 80, Qt::KeepAspectRatio, Qt::FastTransformation))));
            if (!times_[i]->hasFocus())
                times_[i]->setText(QString::number(document_->frameTime(frame)));
        }
        pictures_[i]->setVisible(there && (several || i == 0));
        times_[i]->setVisible(there && several);
        removes_[i]->setVisible(there && several);
    }
    filling_ = false;
}

bool AnimationPanel::saveAnimation(const QString& path)
{
    // ConvImgCpc's SaveAnim: how the frames are to be packed and played.
    const Screen& screen = document_->screen;
    const bool ascii = screen.mode >= kModeAscUt;
    QDialog dialog(window_);
    dialog.setObjectName(QStringLiteral("SaveAnim"));
    dialog.setWindowTitle(texts::text("Main.prg.TxtInfo25", QStringLiteral("Animation")));
    auto* grid = new QGridLayout(&dialog);
    auto* label1 = namedLabel("label1");
    auto* txbAdrDeb = new QLineEdit(QStringLiteral("#200"));
    txbAdrDeb->setObjectName(QStringLiteral("txbAdrDeb"));
    label1->setBuddy(txbAdrDeb);
    auto* chkBoucle = namedCheck("chkBoucle");
    auto* chk128Ko = namedCheck("chk128Ko");
    auto* chkMaxMem = namedCheck("chkMaxMem");
    auto* tbxAdrMax = new QLineEdit(QStringLiteral("#A600"));
    tbxAdrMax->setObjectName(QStringLiteral("tbxAdrMax"));
    auto* chkDelai = namedCheck("chkDelai");
    auto* chkDataBrut = namedCheck("chkDataBrut");
    auto* comboMethode = new QComboBox;
    comboMethode->setObjectName(QStringLiteral("comboMethode"));
    comboMethode->addItems({QStringLiteral("Standard"), QStringLiteral("DirectMemory"), QStringLiteral("Blocks")});
    auto* lblMethode = namedLabel("lblMethode", both("Méthode", "Method"));
    lblMethode->setBuddy(comboMethode);
    auto* grpLines = new QGroupBox;
    grpLines->setObjectName(QStringLiteral("grpGenereLigne"));
    auto* lines = new QVBoxLayout(grpLines);
    QRadioButton* rbLines[4] = {namedRadio("rb1L"), namedRadio("rb2L"), namedRadio("rb4L"), namedRadio("rb8L")};
    for (QRadioButton* radio : rbLines)
        lines->addWidget(radio);
    rbLines[0]->setChecked(true);
    auto* chk2Zone = namedCheck("chk2Zone");
    auto* chkZoneVert = namedCheck("chkZoneVert");
    auto* chkCol = namedCheck("chkCol");
    auto* grpAscii = new QGroupBox;
    grpAscii->setObjectName(QStringLiteral("grpAscii"));
    auto* asciiBox = new QVBoxLayout(grpAscii);
    QRadioButton* rbAscii[3] = {namedRadio("rbFrameFull"), namedRadio("rbFrameD"), namedRadio("rbFrameO")};
    for (QRadioButton* radio : rbAscii)
        asciiBox->addWidget(radio);
    rbAscii[0]->setChecked(true);
    int row = 0;
    grid->addWidget(label1, row, 0);
    grid->addWidget(txbAdrDeb, row++, 1);
    grid->addWidget(chkBoucle, row++, 0, 1, 2);
    grid->addWidget(chkDelai, row++, 0, 1, 2);
    grid->addWidget(chk128Ko, row++, 0, 1, 2);
    grid->addWidget(chkMaxMem, row, 0);
    grid->addWidget(tbxAdrMax, row++, 1);
    grid->addWidget(lblMethode, row, 0);
    grid->addWidget(comboMethode, row++, 1);
    grid->addWidget(grpLines, row++, 0, 1, 2);
    grid->addWidget(chk2Zone, row++, 0, 1, 2);
    grid->addWidget(chkZoneVert, row++, 0, 1, 2);
    grid->addWidget(chkCol, row++, 0, 1, 2);
    grid->addWidget(grpAscii, row++, 0, 1, 2);
    grid->addWidget(chkDataBrut, row++, 0, 1, 2);
    auto* buttons = new QDialogButtonBox;
    buttons->addButton(texts::text("SaveAnim.bpSave", QStringLiteral("Sauver")), QDialogButtonBox::AcceptRole)->setObjectName(QStringLiteral("bpSave"));
    buttons->addButton(QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    grid->addWidget(buttons, row, 0, 1, 2);
    texts::apply(&dialog, "SaveAnim");
    grpLines->setTitle(both("Lignes générées", "Lines written"));
    grpAscii->setTitle(both("Images des modes ASCII", "Frames of the ASCII modes"));

    // What applies depends on the mode and on what is ticked.
    const auto show = [&] {
        const bool window0 = comboMethode->currentIndex() == 0 && !ascii;
        grpLines->setVisible(window0);
        chk2Zone->setVisible(window0);
        chkZoneVert->setVisible(window0 && chk2Zone->isChecked());
        comboMethode->setVisible(!ascii);
        lblMethode->setVisible(!ascii);
        chkCol->setVisible(!ascii);
        grpAscii->setVisible(ascii);
        chkMaxMem->setVisible(chk128Ko->isChecked());
        tbxAdrMax->setVisible(chk128Ko->isChecked());
        tbxAdrMax->setEnabled(chkMaxMem->isChecked());
        const bool program = !chkDataBrut->isChecked();
        for (QWidget* widget : std::initializer_list<QWidget*>{chk128Ko, chk2Zone, chkBoucle, chkCol, chkDelai, comboMethode, chkMaxMem})
            widget->setEnabled(program);
    };
    for (QCheckBox* box : {chk128Ko, chkMaxMem, chk2Zone, chkDataBrut})
        connect(box, &QCheckBox::toggled, &dialog, show);
    connect(comboMethode, &QComboBox::currentIndexChanged, &dialog, show);
    show();
    if (dialog.exec() != QDialog::Accepted)
        return false;

    AnimOptions options;
    options.startAddress = address(txbAdrDeb->text());
    if (options.startAddress <= 0) {
        document_->fail(both("L'adresse saisie [", "The address typed [") + txbAdrDeb->text() + both("] est erronée.", "] is wrong."));
        return false;
    }
    if (chk128Ko->isChecked() && chkMaxMem->isChecked()) {
        options.maxAddress = address(tbxAdrMax->text());
        if (options.maxAddress <= 0) {
            document_->fail(both("L'adresse saisie [", "The address typed [") + tbxAdrMax->text() + both("] est erronée.", "] is wrong."));
            return false;
        }
    }
    options.loop = chkBoucle->isChecked();
    options.memory128K = chk128Ko->isChecked();
    options.withDelay = chkDelai->isChecked();
    options.lineStep = rbLines[3]->isChecked() ? 8 : rbLines[2]->isChecked() ? 4 : rbLines[1]->isChecked() ? 2 : 1;
    options.twoZones = chk2Zone->isChecked() && comboMethode->currentIndex() == 0;
    options.verticalZones = chkZoneVert->isChecked();
    options.byColumns = chkCol->isChecked();
    options.method = comboMethode->currentIndex();
    options.ascii = rbAscii[1]->isChecked() ? AnimOptions::Ascii::Changes : rbAscii[2]->isChecked() ? AnimOptions::Ascii::Whole : AnimOptions::Ascii::Shortest;
    options.rawData = chkDataBrut->isChecked();
    return document_->saveAnimation(path, options);
}
