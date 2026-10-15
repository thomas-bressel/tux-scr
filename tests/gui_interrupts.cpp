// The palette interrupts in the converter's window, driven as a user drives
// them: a picture is read and converted for a Plus in mode 1, the box "Add
// Palette Interrupt" is ticked, an interrupt is added, its line moved (the
// mark on the picture follows), a pen given another colour from that line
// on, the picture converted again with it, and the lines and their palettes
// exported as a text. Runs without a display (QT_QPA_PLATFORM=offscreen).
//
//   gui_interrupts [prefix]   also saves pictures of the window as
//                             <prefix>tuxscr_interrupts.png and _dark.png

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include "check.h"
#include "converterwindow.h"
#include "gui_looks.h"
#include "interruptspanel.h"
#include "penbutton.h"
#include "pictureview.h"
#include "prompts.h"

using namespace tuxscr;

namespace {

// A picture whose top and bottom call for different colours: a sunset over
// the sea.
QImage twoHalves()
{
    QImage picture(320, 200, QImage::Format_RGB32);
    QPainter painter(&picture);
    QLinearGradient sky(0, 0, 0, 110);
    sky.setColorAt(0, QColor(40, 0, 60));
    sky.setColorAt(0.6, QColor(255, 80, 0));
    sky.setColorAt(1, QColor(255, 230, 60));
    painter.fillRect(0, 0, 320, 110, sky);
    painter.setBrush(QColor(255, 255, 200));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPoint(160, 100), 34, 34);
    QLinearGradient sea(0, 110, 0, 200);
    sea.setColorAt(0, QColor(0, 150, 200));
    sea.setColorAt(1, QColor(0, 10, 70));
    painter.fillRect(0, 110, 320, 90, sea);
    painter.setPen(QPen(QColor(200, 255, 255), 2));
    for (int y = 122; y < 200; y += 14)
        for (int x = (y * 7) % 40; x < 320; x += 60)
            painter.drawLine(x, y, x + 22, y);
    return picture;
}

// The colour window that is about to ask is given a colour and accepted.
void answerColour(int red, int green, int blue)
{
    QTimer::singleShot(0, [=] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        CHECK(dialog && dialog->objectName() == "EditColor");
        if (!dialog)
            return;
        dialog->findChild<QSpinBox*>("valR")->setValue(red);
        dialog->findChild<QSpinBox*>("valV")->setValue(green);
        dialog->findChild<QSpinBox*>("valB")->setValue(blue);
        dialog->accept();
    });
}

QSpinBox* lineOf(QWidget* panel, int row)
{
    QWidget* holder = panel->findChild<QWidget*>(QStringLiteral("interrupt%1").arg(row));
    return holder ? holder->findChild<QSpinBox*>("numLine") : nullptr;
}

PenButton* penOf(QWidget* panel, int row, int pen)
{
    QWidget* holder = panel->findChild<QWidget*>(QStringLiteral("interrupt%1").arg(row));
    return holder ? holder->findChild<PenButton*>(QStringLiteral("pen%1").arg(pen)) : nullptr;
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    const QString prefix = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    QTemporaryDir scratch;
    QStringList told;
    prompts::setProblemHandler([&told](const QString& text) { told << text; });

    const QString source = scratch.filePath("sunset.png");
    CHECK(twoHalves().save(source));

    ConverterWindow window(nullptr);
    window.show();
    Document* document = window.document();
    InterruptsPanel* panel = window.interruptsPanel();
    PictureView* view = window.pictureView();
    auto* enable = panel->findChild<QCheckBox*>("chkInterrupts");
    auto* add = panel->findChild<QPushButton*>("bpAdd");
    auto* apply = panel->findChild<QPushButton*>("bpApply");
    auto* exportText = panel->findChild<QPushButton*>("bpExport");
    auto* plus = window.findChild<QCheckBox*>("modePlus");
    CHECK(enable && add && apply && exportText && plus);
    if (!enable || !add || !apply || !exportText || !plus)
        return checkSummary("gui_interrupts");
    CHECK(enable->text() == "Add Palette Interrupt" && add->text() == "Add");

    // A picture, converted for mode 1: four colours for all of it.
    window.openFile(source);
    window.findChild<QPushButton*>("bpConvert")->click();
    CHECK(document->picture().computed);
    CHECK_EQ(document->screen.mode, kMode1);
    CHECK(told.isEmpty());

    // The interrupts are the Plus's: on a CPC the box cannot be ticked, and
    // a line says why.
    CHECK(!enable->isEnabled());
    plus->click();
    CHECK(document->screen.plus);
    CHECK(enable->isEnabled() && !enable->isChecked() && !add->isEnabled());
    // The Plus has 4096 colours to choose the four from.
    window.findChild<QPushButton*>("bpConvert")->click();
    enable->click();
    CHECK(document->screen.interruptsOn && add->isEnabled() && !exportText->isEnabled());
    CHECK(view->interruptLines().isEmpty());

    // One is added, half way down: its line is marked on the picture, and
    // its palette is the picture's.
    add->click();
    CHECK_EQ(document->screen.interrupts.size(), 1);
    CHECK_EQ(document->screen.interrupts[0].line, 100);
    CHECK(view->interruptLines() == QList<int>{100});
    CHECK_EQ(view->currentInterruptLine(), 100);
    const std::array<int, 16> whole = document->screen.paletteAt(0);
    CHECK(document->screen.paletteAt(150) == whole);
    QSpinBox* line = lineOf(panel, 0);
    CHECK(line && line->value() == 100 && line->maximum() == 199);
    // The mark goes up and down with the number.
    if (line) {
        line->setValue(110);
        CHECK_EQ(document->screen.interrupts[0].line, 110);
        CHECK_EQ(view->currentInterruptLine(), 110);
        line->stepUp();
        CHECK_EQ(view->currentInterruptLine(), 111);
        line->setValue(110);
    }
    // Only the pens the mode has are shown.
    CHECK(penOf(panel, 0, 3) && penOf(panel, 0, 3)->isVisibleTo(panel));
    if (PenButton* pen = penOf(panel, 0, 3); pen && !pen->isVisibleTo(panel))
        for (QWidget* w = pen; w && w != panel; w = w->parentWidget())
            std::printf("  %s %s hidden=%d\n", w->metaObject()->className(), qPrintable(w->objectName()), w->isHidden());
    CHECK(penOf(panel, 0, 4) && !penOf(panel, 0, 4)->isVisibleTo(panel));

    // Pen 1 is given another colour from that line on: the picture's own
    // palette is left as it is, and the pen is marked.
    PenButton* pen1 = penOf(panel, 0, 1);
    CHECK(pen1 && pen1->mark() == PenButton::Mark::None);
    if (pen1) {
        answerColour(0, 12, 15);
        pen1->click();
        const int chosen = plusColour(0, 12, 15);
        CHECK(document->screen.interrupts[0].defined[1]);
        CHECK_EQ(document->screen.interrupts[0].colours[1], chosen);
        CHECK(document->screen.paletteAt(0) == whole);
        CHECK_EQ(document->screen.paletteAt(109)[1], whole[1]);
        CHECK_EQ(document->screen.paletteAt(110)[1], chosen);
        CHECK_EQ(document->screen.paletteAt(110)[2], whole[2]);
        CHECK(pen1->mark() == PenButton::Mark::Dot);
        CHECK(pen1->colour() == QColor(0, 12 * 17, 255));
    }

    // Pens 2 and 3 too, and the picture is converted again: every pixel
    // keeps its pen. A colour chosen for a pen colours again what the pen
    // draws; it sends none of the picture to another pen.
    const Picture before = document->picture();
    answerColour(0, 2, 6);
    penOf(panel, 0, 2)->click();
    answerColour(12, 15, 15);
    penOf(panel, 0, 3)->click();
    answerColour(0, 0, 3);
    penOf(panel, 0, 0)->click();
    apply->click();
    const Picture& after = document->picture();
    // How the picture's pens differ from what they were: above the line,
    // and from the line on.
    const auto changed = [&] {
        std::pair<int, int> count{0, 0};
        for (int y = 0; y < 400; y += 2)
            for (int x = 0; x < 640; x += 2)
                (y < 220 ? count.first : count.second) += after.pen(x, y) != before.pen(x, y);
        return count;
    };
    CHECK_EQ(changed().first, 0);
    CHECK_EQ(changed().second, 0);
    CHECK_EQ(document->screen.paletteAt(150)[0], plusColour(0, 0, 3));
    CHECK_EQ(document->screen.paletteAt(150)[2], plusColour(0, 2, 6));
    // What is shown has each line in its own colours.
    const QImage shown = document->shown();
    const auto shownColour = [&](int penNumber, int cpcLine) {
        return QColor::fromRgb(static_cast<QRgb>(document->screen.rgb(document->screen.paletteAt(cpcLine)[static_cast<size_t>(penNumber)])) | 0xFF000000);
    };
    CHECK(shown.pixelColor(10, 2 * 150) == shownColour(after.pen(10, 300), 150));
    CHECK(shown.pixelColor(10, 2 * 20) == shownColour(after.pen(10, 40), 20));

    // A right click gives a pen the colour it has above again; so does
    // Delete, for those who use the keyboard.
    QTest::mouseClick(penOf(panel, 0, 0), Qt::RightButton);
    CHECK(!document->screen.interrupts[0].defined[0]);
    CHECK_EQ(document->screen.paletteAt(150)[0], whole[0]);
    const int kept = document->screen.interrupts[0].colours[3];
    QTest::keyClick(penOf(panel, 0, 3), Qt::Key_Delete);
    CHECK(!document->screen.interrupts[0].defined[3]);
    // "Auto" searches the picture for the colours of the interrupt's own
    // lines: the sea's, which are not the sky's; every pen is then its own,
    // and those lines are converted with them. The button stays in.
    QPushButton* search = panel->findChild<QWidget*>("interrupt0")->findChild<QPushButton*>("bpAuto");
    CHECK(search->isCheckable() && !search->isChecked());
    search->click();
    CHECK(search->isChecked());
    CHECK(document->screen.interrupts[0].matched);
    CHECK_EQ(changed().first, 0);
    CHECK(changed().second > 2000);
    {
        const PaletteInterrupt& sea = document->screen.interrupts[0];
        CHECK(sea.defined[0] && sea.defined[1] && sea.defined[2] && sea.defined[3]);
        const std::array<int, 16> below = document->screen.paletteAt(150);
        CHECK(below != whole);
        // The sea has no red to speak of.
        for (int pen = 0; pen < 4; ++pen)
            CHECK(plusRed(below[static_cast<size_t>(pen)]) <= plusBlue(below[static_cast<size_t>(pen)]) + 3);
        CHECK(document->screen.paletteAt(0) == whole);

        // A colour chosen by hand after that colours again what the pen
        // found draws, and Apply leaves it so; a right click gives the
        // colour found back.
        const Picture found = document->picture();
        const int foundColour = sea.colours[1];
        answerColour(15, 0, 0);
        penOf(panel, 0, 1)->click();
        apply->click();
        CHECK_EQ(document->screen.paletteAt(150)[1], plusColour(15, 0, 0));
        CHECK_EQ(sea.matchColours[1], foundColour);
        bool samePens = true;
        for (int y = 0; y < 400; y += 2)
            for (int x = 0; x < 640; x += 2)
                samePens = samePens && document->picture().pen(x, y) == found.pen(x, y);
        CHECK(samePens);
        QTest::mouseClick(penOf(panel, 0, 1), Qt::RightButton);
        CHECK(sea.defined[1]);
        CHECK_EQ(document->screen.paletteAt(150)[1], foundColour);
    }
    // Let out, the button takes the colours found away, and the lines are
    // converted with the picture's palette again.
    search->click();
    CHECK(!search->isChecked());
    CHECK(!document->screen.interrupts[0].matched);
    CHECK(!document->screen.interrupts[0].defined[1]);
    CHECK(document->screen.paletteAt(150) == whole);
    CHECK_EQ(changed().second, 0);
    // Back to what was chosen by hand.
    answerColour(0, 12, 15);
    penOf(panel, 0, 1)->click();
    answerColour(0, 2, 6);
    penOf(panel, 0, 2)->click();
    answerColour(plusRed(kept), plusGreen(kept), plusBlue(kept));
    penOf(panel, 0, 3)->click();
    QTest::mouseClick(penOf(panel, 0, 0), Qt::RightButton);

    // A second one, under the first; moved above it, it comes first, and
    // the pens it does not set keep the picture's colours there.
    add->click();
    CHECK_EQ(document->screen.interrupts.size(), 2);
    CHECK_EQ(document->screen.interrupts[1].line, 155);
    CHECK_EQ(view->currentInterruptLine(), 155);
    answerColour(15, 15, 0);
    penOf(panel, 1, 1)->click();
    CHECK_EQ(document->screen.paletteAt(160)[1], plusColour(15, 15, 0));
    CHECK_EQ(document->screen.paletteAt(160)[2], plusColour(0, 2, 6));
    lineOf(panel, 1)->setValue(60);
    CHECK_EQ(document->screen.interrupts[0].line, 60);
    CHECK_EQ(document->screen.interrupts[1].line, 110);
    CHECK_EQ(view->currentInterruptLine(), 60);
    CHECK_EQ(document->screen.paletteAt(70)[1], plusColour(15, 15, 0));
    CHECK_EQ(document->screen.paletteAt(70)[2], whole[2]);
    CHECK_EQ(document->screen.paletteAt(120)[1], plusColour(0, 12, 15));
    CHECK(view->interruptLines() == (QList<int>{60, 110}));
    apply->click();

    if (!prefix.isEmpty())
        CHECK(saveInBothLooks(&window, prefix + "tuxscr_interrupts"));

    // The lines and their palettes, as a text.
    const QString text = scratch.filePath("sunset.txt");
    prompts::setSaveAnswerer([&](const QString&, const QStringList& filters, int* chosen) {
        CHECK(filters.size() == 2 && filters[0].contains("*.txt"));
        *chosen = 0;
        return text;
    });
    CHECK(exportText->isEnabled());
    exportText->click();
    QFile exported(text);
    CHECK(exported.open(QIODevice::ReadOnly));
    const QList<QByteArray> lines = exported.readAll().split('\n');
    const auto word = [](int colour) { return QByteArray("#") + QByteArray::number(asicWord(colour), 16).rightJustified(3, '0').toUpper(); };
    CHECK(lines.value(0).startsWith("; Interruptions de palette (CPC Plus) - TuxSCR "));
    CHECK(lines.value(1) == "; Mode \xC3\xA9" "cran - Mode 1 - 80x200");
    CHECK(lines.value(4) == "PalIntNb:");
    CHECK(lines.value(5).startsWith("\tDB\t2\t"));
    CHECK(lines.value(6).startsWith("PalIntLigne0:"));
    CHECK(lines.value(7) == "\tDW\t" + word(whole[0]) + "," + word(whole[1]) + "," + word(whole[2]) + "," + word(whole[3]));
    CHECK(lines.value(8) == "PalIntLigne1:");
    CHECK(lines.value(9).startsWith("\tDB\t60\t"));
    CHECK(lines.value(10) == "\tDW\t" + word(whole[0]) + "," + word(plusColour(15, 15, 0)) + "," + word(whole[2]) + "," + word(whole[3]));
    CHECK(lines.value(11) == "PalIntLigne2:");
    CHECK(lines.value(12).startsWith("\tDB\t110\t"));
    CHECK(lines.value(13) == "\tDW\t" + word(whole[0]) + "," + word(plusColour(0, 12, 15)) + "," + word(plusColour(0, 2, 6)) + "," + word(plusColour(12, 15, 15)));

    // The program that shows the picture has the interrupts.
    const QByteArray program = QByteArray::fromStdString(document->showProgram());
    CHECK(program.contains("PalInt:") && program.contains("LD\t(#6800),A") && program.contains("HALT"));
    CHECK(program.contains("; Ligne 60") && program.contains("; Ligne 110"));

    // Taken away, an interrupt leaves the picture's palette on its lines.
    panel->findChild<QWidget*>("interrupt0")->findChild<QPushButton*>("bpRemove")->click();
    CHECK_EQ(document->screen.interrupts.size(), 1);
    CHECK_EQ(document->screen.interrupts[0].line, 110);
    CHECK_EQ(document->screen.paletteAt(70)[1], whole[1]);

    // Out of mode 0 to 2, or back on a CPC, they no longer apply; they are
    // kept, and come back with the Plus.
    plus->click();
    CHECK(!document->screen.plus && !enable->isEnabled());
    CHECK(view->interruptLines().isEmpty());
    CHECK(!document->screen.interruptsActive());
    plus->click();
    CHECK(enable->isEnabled() && enable->isChecked());
    CHECK(view->interruptLines() == QList<int>{110});
    CHECK(document->screen.interruptsActive());

    // Unticked, the picture has its one palette again.
    enable->click();
    CHECK(!document->screen.interruptsActive());
    CHECK_EQ(document->screen.paletteAt(150)[1], document->screen.palette[1]);
    CHECK(told.isEmpty());
    return checkSummary("gui_interrupts");
}
