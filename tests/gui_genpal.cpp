// The maker of palettes from the converter's window, driven as a user
// drives it: on a Plus, "Make Palette" opens it on the picture's palette;
// from a pen to another, from a colour to another, the colours in between
// are written in the palette, as soon as a slider moves or when "Generate"
// is clicked; every pen is then locked and the picture converted again.
// Runs without a display (QT_QPA_PLATFORM=offscreen).
//
//   gui_genpal [prefix]   also saves pictures of the window as
//                         <prefix>genpal.png and _dark.png

#include <functional>

#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QTest>
#include <QTimer>

#include "check.h"
#include "english.h"
#include "french.h"
#include "converterwindow.h"
#include "gui_looks.h"
#include "palettebar.h"
#include "penbutton.h"
#include "prompts.h"
#include "splits/rasters.h"
#include "splittools.h"
#include "swatch.h"

using namespace tuxscr;

namespace {

// The window that is about to ask is given to `fill`, which answers and
// closes it.
void whenAsked(std::function<void(QDialog*)> fill)
{
    QTimer::singleShot(0, [fill = std::move(fill)] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        CHECK(dialog && dialog->objectName() == "GenPalette");
        if (dialog)
            fill(dialog);
    });
}

QLineEdit* box(QWidget* window, const char* name)
{
    auto* found = window->findChild<QLineEdit*>(QLatin1String(name));
    CHECK(found != nullptr);
    return found;
}

// Colours that go from one to another, as ConvImgCpc works them out: the
// figures below are what its sums give.
void checkSums()
{
    // Six pens from black to white: three sixteenths more at each.
    int pens[17] = {};
    gradientPens(pens, 0, 5, Shade{0, 0, 0}, Shade{15, 15, 15});
    for (int i = 0; i <= 5; ++i)
        CHECK_EQ(pens[i], plusColour(3 * i, 3 * i, 3 * i));
    CHECK_EQ(pens[6], 0);
    // Eight pens: fifteen sevenths more at each, less what is over a whole
    // number; the last is the colour asked.
    gradientPens(pens, 2, 9, Shade{0, 15, 4}, Shade{15, 0, 4});
    const int reds[8] = {0, 2, 4, 6, 8, 10, 12, 15};
    const int greens[8] = {15, 12, 10, 8, 6, 4, 2, 0};
    for (int i = 0; i < 8; ++i)
        CHECK_EQ(pens[2 + i], plusColour(reds[i], greens[i], 4));
    CHECK(pens[1] == plusColour(3, 3, 3) && pens[10] == 0);

    // Lines of a raster table have red and blue the other way round, and
    // go upwards as well as downwards.
    int lines[272] = {};
    gradientLines(lines, 10, 14, Shade{0, 8, 15}, Shade{8, 8, 3});
    const int wanted[5] = {rasterWord(0, 8, 15), rasterWord(2, 8, 12), rasterWord(4, 8, 9), rasterWord(6, 8, 6), rasterWord(8, 8, 3)};
    for (int i = 0; i < 5; ++i)
        CHECK_EQ(lines[10 + i], wanted[i]);
    CHECK(lines[9] == 0 && lines[15] == 0);
    gradientLines(lines, 104, 100, Shade{0, 8, 15}, Shade{8, 8, 3});
    for (int i = 0; i < 5; ++i)
        CHECK_EQ(lines[104 - i], wanted[i]);
    CHECK(lines[99] == 0 && lines[105] == 0);
    // A component is from 0 to under 16.
    const Shade most{15.5, 0, 0}, over{16, 0, 0}, under{0, -1, 0};
    CHECK(most.valid() && !over.valid() && !under.valid());
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    const English english;
    const QString prefix = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    QStringList told;
    prompts::setProblemHandler([&told](const QString& text) { told << text; });
    checkSums();

    ConverterWindow window(nullptr);
    installSplitTools(&window);
    window.show();
    Document* document = window.document();
    auto* open = window.findChild<QPushButton*>("bpGenPal");
    auto* plus = window.findChild<QCheckBox*>("modePlus");
    CHECK(open && plus);
    if (!open || !plus)
        return checkSummary("gui_genpal");

    // The maker is the Plus's: its colours are numbers, which the CPC's are
    // not.
    CHECK(!open->isVisibleTo(&window));
    plus->click();
    CHECK(document->screen.plus && open->isVisibleTo(&window));
    Screen& screen = document->screen;
    screen.palette[0] = plusColour(1, 2, 3);
    screen.palette[15] = plusColour(15, 14, 13);
    document->param.lockState.fill(0);
    document->touch();
    int changes = 0;
    QObject::connect(document, &Document::changed, &app, [&changes] { ++changes; });
    const std::array<int, 17> before = screen.palette;

    bool asked = false;
    whenAsked([&](QDialog* dialog) {
        asked = true;
        CHECK(dialog->windowTitle() == QString::fromUtf8("TuxSCR – Make Palette"));
        CHECK(dialog->findChild<QLabel*>("lblFrom")->text() == "From color" && dialog->findChild<QLabel*>("lblTo")->text() == "To color");
        CHECK(dialog->findChild<QLabel*>("lblStartText")->text() == "Start color" && dialog->findChild<QLabel*>("lblEndText")->text() == "End color");
        auto* bpGenerate = dialog->findChild<QPushButton*>("bpGenerate");
        auto* bpGetCol = dialog->findChild<QPushButton*>("bpGetCol");
        auto* lblError = dialog->findChild<QLabel*>("lblError");
        auto* lblStartColor = dialog->findChild<ColourSwatch*>("lblStartColor");
        auto* lblEndColor = dialog->findChild<ColourSwatch*>("lblEndColor");
        auto* trkStartR = dialog->findChild<QSlider*>("trkStartR");
        auto* trkEndR = dialog->findChild<QSlider*>("trkEndR");
        auto* trkEndB = dialog->findChild<QSlider*>("trkEndB");
        const bool all = bpGenerate && bpGetCol && lblError && lblStartColor && lblEndColor && trkStartR && trkEndR && trkEndB
            && dialog->findChild<QSlider*>("trkStartV") && dialog->findChild<QSlider*>("trkStartB") && dialog->findChild<QSlider*>("trkEndV");
        CHECK(all);
        if (!all) {
            dialog->reject();
            return;
        }
        CHECK(bpGenerate->text() == "Generate" && bpGetCol->text() == "Get color");
        // As opened: all the pens, from the first one's colour to the
        // last one's.
        CHECK(box(dialog, "txbFrom")->text() == "0" && box(dialog, "txbTo")->text() == "15");
        CHECK(box(dialog, "txbStartR")->text() == "1" && box(dialog, "txbStartV")->text() == "2" && box(dialog, "txbStartB")->text() == "3");
        CHECK(box(dialog, "txbEndR")->text() == "15" && box(dialog, "txbEndV")->text() == "14" && box(dialog, "txbEndB")->text() == "13");
        CHECK(trkStartR->value() == 1 && trkEndR->value() == 15 && trkEndB->value() == 13 && trkEndR->maximum() == 15);
        CHECK(lblStartColor->colour() == QColor(17, 34, 51) && lblEndColor->colour() == QColor(255, 238, 221));
        CHECK(lblError->text().isEmpty());

        // Numbers typed wait for "Generate": from black to white, each pen
        // a sixteenth more. The window stays for more.
        for (const char* name : {"txbStartR", "txbStartV", "txbStartB"})
            box(dialog, name)->setText("0");
        for (const char* name : {"txbEndR", "txbEndV", "txbEndB"})
            box(dialog, name)->setText("15");
        CHECK(lblStartColor->colour() == QColor(Qt::black) && lblEndColor->colour() == QColor(Qt::white));
        CHECK(screen.palette == before && changes == 0);
        bpGenerate->click();
        CHECK(dialog->isVisible());
        for (int i = 0; i < 16; ++i)
            CHECK_EQ(screen.palette[static_cast<size_t>(i)], plusColour(i, i, i));
        CHECK_EQ(screen.palette[16], before[16]);
        CHECK(lblError->text().isEmpty());
        // Every pen keeps the colour it was given, and the picture is
        // drawn again.
        for (const int lock : document->param.lockState)
            CHECK_EQ(lock, 1);
        CHECK_EQ(changes, 1);
        CHECK(window.paletteBar()->lockBox(0)->isChecked() && window.paletteBar()->penButton(3)->colour() == QColor(51, 51, 51));
        if (!prefix.isEmpty())
            CHECK(saveInBothLooks(dialog, prefix + "genpal"));

        // A slider moved writes the colours at once.
        trkEndR->setValue(0);
        CHECK(box(dialog, "txbEndR")->text() == "0" && lblEndColor->colour() == QColor(0, 255, 255));
        for (int i = 0; i < 16; ++i)
            CHECK_EQ(screen.palette[static_cast<size_t>(i)], plusColour(0, i, i));
        CHECK_EQ(changes, 2);

        // Some of the pens only, from the colour one of them has: pens 4
        // to 7, from pen 4's cyan to red.
        box(dialog, "txbFrom")->setText("4");
        box(dialog, "txbTo")->setText("7");
        bpGetCol->click();
        CHECK(box(dialog, "txbStartR")->text() == "0" && box(dialog, "txbStartV")->text() == "4" && box(dialog, "txbStartB")->text() == "4");
        box(dialog, "txbEndR")->setText("15");
        box(dialog, "txbEndV")->setText("1");
        box(dialog, "txbEndB")->setText("1");
        bpGenerate->click();
        CHECK(screen.palette[3] == plusColour(0, 3, 3) && screen.palette[8] == plusColour(0, 8, 8));
        CHECK_EQ(screen.palette[4], plusColour(0, 4, 4));
        CHECK_EQ(screen.palette[5], plusColour(5, 3, 3));
        CHECK_EQ(screen.palette[6], plusColour(10, 2, 2));
        CHECK_EQ(screen.palette[7], plusColour(15, 1, 1));
        CHECK(lblError->text().isEmpty());

        // Two pens of the same colour are told: here pens 0 to 3 are given
        // too few colours for them all.
        box(dialog, "txbFrom")->setText("0");
        box(dialog, "txbTo")->setText("3");
        for (const char* name : {"txbStartR", "txbStartV", "txbStartB", "txbEndV", "txbEndB"})
            box(dialog, name)->setText("0");
        box(dialog, "txbEndR")->setText("1");
        bpGenerate->click();
        CHECK(screen.palette[0] == 0 && screen.palette[1] == 0 && screen.palette[2] == 0 && screen.palette[3] == plusColour(1, 0, 0));
        CHECK(lblError->text() == "Color 1 is the same as color 2");

        // Pens there are not, a first pen after the last, a colour that is
        // none: nothing is written, and it is said.
        const std::array<int, 17> kept = screen.palette;
        box(dialog, "txbTo")->setText("20");
        bpGenerate->click();
        CHECK(screen.palette == kept && lblError->text() == "Color 1 is the same as color 2");
        screen.palette[1] = plusColour(9, 9, 9);
        screen.palette[2] = plusColour(9, 9, 8);
        const std::array<int, 17> distinct = screen.palette;
        bpGenerate->click();
        CHECK(screen.palette == distinct && lblError->text() == "These values cannot be used.");
        box(dialog, "txbTo")->setText("15");
        box(dialog, "txbFrom")->setText("15");
        bpGenerate->click();
        box(dialog, "txbFrom")->setText("0");
        box(dialog, "txbEndV")->setText("16");
        bpGenerate->click();
        box(dialog, "txbEndV")->setText("abc");
        trkEndB->setValue(9);
        CHECK(screen.palette == distinct && lblError->text() == "These values cannot be used.");
        // "Get color" of a pen there is not takes nothing.
        box(dialog, "txbFrom")->setText("99");
        bpGetCol->click();
        box(dialog, "txbFrom")->setText("x");
        bpGetCol->click();
        CHECK(box(dialog, "txbStartR")->text() == "0");
        dialog->reject();
    });
    open->click();
    CHECK(asked);
    CHECK(told.isEmpty());

    // In French, in a window made while TuxSCR's translation is installed.
    {
        French french;
        CHECK(french.loaded());
        ConverterWindow other(nullptr);
        installSplitTools(&other);
        other.show();
        other.findChild<QCheckBox*>("modePlus")->click();
        whenAsked([&](QDialog* dialog) {
            CHECK(dialog->windowTitle() == QString::fromUtf8("TuxSCR – Générer palette"));
            CHECK(dialog->findChild<QLabel*>("lblFrom")->text() == "De la couleur" && dialog->findChild<QPushButton*>("bpGenerate")->text() == QString::fromUtf8("Générer"));
            CHECK(dialog->findChild<QLabel*>("label3")->text() == "V");
            if (!prefix.isEmpty())
                CHECK(saveInBothLooks(dialog, prefix + "genpal_fr"));
            dialog->reject();
        });
        other.findChild<QPushButton*>("bpGenPal")->click();
    }

    // Back on a CPC, the maker is not offered.
    plus->click();
    CHECK(!document->screen.plus && !open->isVisibleTo(&window));

    return checkSummary("gui_genpal");
}
