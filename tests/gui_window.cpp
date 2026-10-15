// The converter's window as a whole: every control has a text or a name in
// English and in French, a control that has no text of its own is told by a
// label or by its accessible name, and the colours the window chooses for
// what it writes on a pen's colour stand out from it. Runs without a
// display (QT_QPA_PLATFORM=offscreen).
//
//   gui_window [prefix]   also saves pictures of the window, in both
//                         languages and both looks, as <prefix>tuxscr_*.png

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTest>

#include "check.h"
#include "contrast.h"
#include "converterwindow.h"
#include "gui_looks.h"
#include "penbutton.h"
#include "prompts.h"
#include "texts.h"

namespace {

// Whether a label of the window says what the control is.
bool hasBuddy(const QWidget* window, const QWidget* control)
{
    for (const QLabel* label : window->findChildren<QLabel*>())
        if (label->buddy() == control && !label->text().isEmpty())
            return true;
    return false;
}

void checkNames(QWidget* window, const char* language)
{
    int unnamed = 0;
    for (const QAbstractButton* button : window->findChildren<QAbstractButton*>()) {
        if (qobject_cast<const PenButton*>(button) || button->objectName().startsWith(QLatin1String("qt_")))
            continue;
        if (button->text().isEmpty() && button->accessibleName().isEmpty() && button->toolTip().isEmpty()) {
            std::printf("  %s: a button with no text: %s\n", language, qPrintable(button->objectName()));
            ++unnamed;
        }
    }
    for (const QGroupBox* box : window->findChildren<QGroupBox*>())
        if (box->title().isEmpty()) {
            std::printf("  %s: a group with no title: %s\n", language, qPrintable(box->objectName()));
            ++unnamed;
        }
    // What is typed in or slid: told by a label, or by its own name.
    QList<QWidget*> inputs;
    for (QSpinBox* w : window->findChildren<QSpinBox*>())
        inputs << w;
    for (QSlider* w : window->findChildren<QSlider*>())
        inputs << w;
    for (QComboBox* w : window->findChildren<QComboBox*>())
        inputs << w;
    for (QLineEdit* w : window->findChildren<QLineEdit*>())
        if (!qobject_cast<QAbstractSpinBox*>(w->parentWidget()))
            inputs << w;
    for (QScrollBar* w : window->findChildren<QScrollBar*>())
        if (!w->objectName().isEmpty() && !w->objectName().startsWith(QLatin1String("qt_")))
            inputs << w;
    for (const QWidget* input : inputs)
        if (input->accessibleName().isEmpty() && !hasBuddy(window, input)) {
            std::printf("  %s: nothing says what this is: %s (%s)\n", language, qPrintable(input->objectName()), input->metaObject()->className());
            ++unnamed;
        }
    CHECK_EQ(unnamed, 0);
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    const QString prefix = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    prompts::setProblemHandler([](const QString&) {});

    ConverterWindow window(nullptr);
    window.show();
    // On a Plus, with every tool there is.
    window.findChild<QCheckBox*>("modePlus")->click();
    CHECK(window.tools()->count() >= 4);

    // English to start with; the texts are ConvImgCpc's.
    CHECK(texts::language() == "EN");
    CHECK(window.findChild<QPushButton*>("bpLoad")->text() == "Read");
    CHECK(window.findChild<QPushButton*>("bpSave")->text() == "Save");
    CHECK(window.findChild<QCheckBox*>("withCode")->text() == "Include display code in image");
    CHECK(window.findChild<QGroupBox*>("resoCPC")->title() == "CPC resolution");
    CHECK(window.findChild<QCheckBox*>("chkInterrupts")->text() == "Add Palette Interrupt");
    // A name two of ConvImgCpc's windows use has each window's own text.
    CHECK(window.findChild<QWidget*>("resoCPC")->findChild<QLabel*>("label1")->text() == "Nb Columns");
    CHECK(window.findChild<QWidget*>("tilesTab")->findChild<QLabel*>("label1")->text() == "Start NOP");
    checkNames(&window, "English");
    if (!prefix.isEmpty()) {
        for (int tab = 0; tab < window.tools()->count(); ++tab) {
            window.tools()->setCurrentIndex(tab);
            CHECK(saveInBothLooks(&window, prefix + QStringLiteral("tuxscr_en_tab%1").arg(tab)));
        }
    }

    window.findChild<QPushButton*>("bpFr")->click();
    CHECK(texts::language() == "FR");
    CHECK(window.findChild<QPushButton*>("bpLoad")->text() == "Lecture");
    CHECK(window.findChild<QPushButton*>("bpSave")->text() == "Enregistrement");
    CHECK(window.findChild<QCheckBox*>("withCode")->text() == "Inclure le code d'affichage dans l'image");
    CHECK(window.findChild<QGroupBox*>("resoCPC")->title() == QString::fromUtf8("Résolution CPC"));
    CHECK(window.findChild<QCheckBox*>("chkInterrupts")->text() == "Ajouter des interruptions de palette");
    CHECK(window.document()->param.langue == "FR");
    checkNames(&window, "French");
    if (!prefix.isEmpty()) {
        window.tools()->setCurrentIndex(0);
        CHECK(saveInBothLooks(&window, prefix + "tuxscr_fr"));
    }
    window.findChild<QPushButton*>("bpEn")->click();
    CHECK(window.findChild<QPushButton*>("bpLoad")->text() == "Read");

    // What is written on a pen's colour reads on it: black or white, the
    // better of the two, is 4.5 to 1 at worst on any colour, and the
    // numbers of the pens are on plates of the window's own colours.
    for (int r = 0; r < 256; r += 51)
        for (int g = 0; g < 256; g += 51)
            for (int b = 0; b < 256; b += 51) {
                const QColor colour(r, g, b);
                CHECK(contrastRatio(inkOn(colour), colour) >= 4.5);
                CHECK(contrastRatio(readable(QColor(128, 128, 128), colour, kGraphicContrast), colour) >= kGraphicContrast);
            }
    for (const bool dark : {false, true}) {
        const QPalette look = tuxapeLook(dark);
        CHECK(contrastRatio(look.color(QPalette::Text), look.color(QPalette::Base)) >= kTextContrast);
        CHECK(contrastRatio(look.color(QPalette::WindowText), look.color(QPalette::Window)) >= kTextContrast);
    }
    return checkSummary("gui_window");
}
