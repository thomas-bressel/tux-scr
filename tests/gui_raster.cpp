// The Plus's raster table in the converter's window, driven as a user
// drives it: "Raster+" asks which of the picture's colours the rasters take
// the place of, then shows the table on the picture; lines are given one
// colour for so many lines, downwards or upwards, taken back, or colours
// going from one to another; the table is moved up and down, given a
// palette's colours, read and saved, copied and written as sources, with
// the sprites drawn over it if asked. Runs without a display
// (QT_QPA_PLATFORM=offscreen).
//
//   gui_raster [prefix]   also saves pictures of the windows as
//                         <prefix>raster_*.png and _dark.png

#include <functional>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialog>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include "check.h"
#include "english.h"
#include "french.h"
#include "converterwindow.h"
#include "formats/screenfile.h"
#include "gui_looks.h"
#include "penbutton.h"
#include "pictureview.h"
#include "prompts.h"
#include "rastertable.h"
#include "splittools.h"
#include "swatch.h"

using namespace tuxscr;

namespace {

QString g_openPath, g_savePath;
QStringList g_filters;

// The window that is about to ask is given to `fill`, which answers and
// closes it.
void whenAsked(const char* name, std::function<void(QDialog*)> fill)
{
    QTimer::singleShot(0, [name, fill = std::move(fill)] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        CHECK(dialog && dialog->objectName() == QLatin1String(name));
        if (dialog)
            fill(dialog);
    });
}

QByteArray fileText(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QLineEdit* box(QWidget* window, const char* name)
{
    auto* found = window->findChild<QLineEdit*>(QLatin1String(name));
    CHECK(found != nullptr);
    return found;
}

QPushButton* button(QWidget* window, const char* name)
{
    auto* found = window->findChild<QPushButton*>(QLatin1String(name));
    CHECK(found != nullptr);
    return found;
}

// The words of the lines as ConvImgCpc writes them: sixteen to a line of
// the source, then how many there are.
QString words(const Raster& raster, int start, int end)
{
    QString text;
    for (int i = start; i <= end; ++i) {
        text += (i - start) % 16 == 0 ? QStringLiteral("\tDW\t") : QStringLiteral(",");
        text += QStringLiteral("#%1").arg(raster.line[static_cast<size_t>(i)], 3, 16, QLatin1Char('0')).toUpper();
        if ((i - start) % 16 == 15 || i == end)
            text += QLatin1Char('\n');
    }
    return text + QStringLiteral("; Taille totale %1 mots\r\n").arg(end - start + 1);
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    const English english;
    const QString prefix = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    QTemporaryDir scratch;
    QStringList told;
    prompts::setProblemHandler([&told](const QString& text) { told << text; });
    prompts::setOpenAnswerer([](const QString&, const QStringList& filters, int*) {
        g_filters = filters;
        return g_openPath;
    });
    prompts::setSaveAnswerer([](const QString&, const QStringList& filters, int*) {
        g_filters = filters;
        return g_savePath;
    });

    ConverterWindow window(nullptr);
    installSplitTools(&window);
    window.show();
    Document* document = window.document();
    auto* open = window.findChild<QPushButton*>("bpRasterPlus");
    auto* plus = window.findChild<QCheckBox*>("modePlus");
    auto* overscan = window.findChild<QPushButton*>("bpOverscan");
    CHECK(open && plus && overscan);
    if (!open || !plus || !overscan)
        return checkSummary("gui_raster");

    // The table is the Plus's, for an overscan picture.
    CHECK(!open->isVisibleTo(&window));
    plus->click();
    CHECK(!open->isVisibleTo(&window));
    overscan->click();
    CHECK(document->screen.plus && document->screen.columns == 96 && document->screen.lines == 272);
    CHECK(open->isVisibleTo(&window));

    // A picture of the Plus's own: a sky of pen 0 with a wide band of pen 1
    // down the middle and a narrow one of pen 2.
    Screen& screen = document->screen;
    screen.palette[0] = plusColour(1, 2, 3);
    screen.palette[1] = plusColour(15, 0, 15);
    screen.palette[2] = plusColour(0, 15, 0);
    screen.palette[3] = plusColour(15, 15, 15);
    Picture& picture = document->picture();
    for (int y = 0; y < 544; y += 2)
        for (int x = 0; x < 768; x += 2)
            picture.setPixel(x, y, x >= 200 && x < 500 ? 1 : (x >= 600 && x < 640 ? 2 : 0), 2);
    picture.computed = true;
    document->touch();
    const QColor sky(17, 34, 51), band(255, 0, 255), narrow(0, 255, 0);

    // Which colour: the pens the mode has, in their colours; the band's is
    // chosen.
    whenAsked("SelectColor", [&](QDialog* dialog) {
        CHECK(dialog->windowTitle() == QString::fromUtf8("TuxSCR – Raster Colour"));
        CHECK(dialog->findChild<QLabel*>("label1")->text() == "Choose the color that will be rasterized :");
        CHECK_EQ(dialog->findChildren<PenButton*>().size(), 4);
        auto* pen1 = dialog->findChild<PenButton*>("color1");
        CHECK(pen1 && pen1->colour() == band);
        if (!prefix.isEmpty())
            CHECK(saveInBothLooks(dialog, prefix + "raster_colour"));
        if (pen1)
            pen1->click();
        else
            dialog->reject();
    });
    open->click();
    auto* table = window.findChild<RasterTable*>("RasterTablePlus");
    CHECK(table && table->isVisible() && table->isWindow());
    if (!table)
        return checkSummary("gui_raster");
    CHECK(table->windowTitle() == QString::fromUtf8("TuxSCR – Raster+"));
    CHECK_EQ(table->rastered(), 0xFF00FF);
    // Clicked again, it is the same window, and nothing is asked.
    open->click();
    CHECK_EQ(window.findChildren<RasterTable*>().size(), 1);

    PictureView* view = table->pictureView();
    const auto shown = [view](int x, int y) { return view->image().pixelColor(x, y); };
    QLineEdit* txbStart = box(table, "txbStart");
    QLineEdit* txbConstant = box(table, "txbConstant");
    QLineEdit* txbScroll = box(table, "txbScroll");
    QLineEdit* txbLineStart = box(table, "txbLineStart");
    QLineEdit* txbLineEnd = box(table, "txbLineEnd");
    QLineEdit* txbStartR = box(table, "txbStartR");
    QLineEdit* txbStartV = box(table, "txbStartV");
    QLineEdit* txbStartB = box(table, "txbStartB");
    auto* trkStartR = table->findChild<QSlider*>("trkStartR");
    auto* trkStartV = table->findChild<QSlider*>("trkStartV");
    auto* trkStartB = table->findChild<QSlider*>("trkStartB");
    auto* lblStartColor = table->findChild<ColourSwatch*>("lblStartColor");
    auto* rbPlus = table->findChild<QRadioButton*>("rbPlus");
    auto* rbMoins = table->findChild<QRadioButton*>("rbMoins");
    auto* chkSprites = table->findChild<QCheckBox*>("chkSprites");
    auto* lblLine = table->findChild<QLabel*>("lblLine");
    QPushButton* bpAddConstant = button(table, "bpAddConstant");
    QPushButton* bpUndo = button(table, "bpUndo");
    QPushButton* bpAddLine = button(table, "bpAddLine");
    QPushButton* bpClearAll = button(table, "bpClearAll");
    QPushButton* bpGenerate = button(table, "bpGenerate");
    QPushButton* bpGenFade = button(table, "bpGenFade");
    QPushButton* bpCopy = button(table, "bpCopyToClipboard");
    QPushButton* bpImportKit = button(table, "bpImportKit");
    QPushButton* bpLoad = button(table, "bpLoad");
    QPushButton* bpSave = button(table, "bpSave");
    const bool all = txbStart && txbConstant && txbScroll && txbLineStart && txbLineEnd && txbStartR && txbStartV && txbStartB && trkStartR
        && trkStartV && trkStartB && lblStartColor && rbPlus && rbMoins && chkSprites && lblLine && bpAddConstant && bpUndo && bpAddLine
        && bpClearAll && bpGenerate && bpGenFade && bpCopy && bpImportKit && bpLoad && bpSave && button(table, "bpScrollUp")
        && button(table, "bpScrollDown") && button(table, "bpRPlus") && button(table, "bpRMoins") && button(table, "bpVPlus")
        && button(table, "bpVMoins") && button(table, "bpBPlus") && button(table, "bpBMoins");
    CHECK(all);
    if (!all)
        return checkSummary("gui_raster");
    const Raster& raster = table->raster();

    // As opened: every line black, shown where the picture has the colour
    // chosen and nowhere else.
    CHECK(bpLoad->text() == "Load rasters" && bpAddConstant->text() == "Add for" && bpGenFade->text() == "Generate asm fade");
    CHECK(view->image().size() == QSize(768, 544));
    CHECK(shown(0, 0) == sky && shown(199, 0) == sky && shown(200, 0) == QColor(Qt::black) && shown(499, 543) == QColor(Qt::black));
    CHECK(shown(500, 543) == sky && shown(610, 100) == narrow);
    CHECK(txbStart->text() == "0" && txbLineStart->text() == "0" && txbLineEnd->text() == "0");
    CHECK(!bpUndo->isEnabled() && !bpGenerate->isEnabled() && rbPlus->isChecked());

    // The colour to give: a number typed moves its slider, the buttons move
    // it one step, and the square shows the colour.
    txbStartR->setText("15");
    CHECK_EQ(trkStartR->value(), 15);
    CHECK(lblStartColor->colour() == QColor(255, 0, 0));
    button(table, "bpRPlus")->click();
    CHECK_EQ(trkStartR->value(), 15);
    button(table, "bpVPlus")->click();
    button(table, "bpVPlus")->click();
    CHECK(trkStartV->value() == 2 && txbStartV->text() == "2");
    button(table, "bpVMoins")->click();
    button(table, "bpVMoins")->click();
    button(table, "bpVMoins")->click();
    CHECK(trkStartV->value() == 0 && txbStartV->text() == "0");
    trkStartB->setValue(7);
    CHECK(txbStartB->text() == "7" && lblStartColor->colour() == QColor(255, 0, 7 * 17));
    button(table, "bpBMoins")->click();
    CHECK(txbStartB->text() == "6");
    // A number that is no component leaves the slider where it is.
    txbStartB->setText("99");
    CHECK_EQ(trkStartB->value(), 6);
    txbStartB->setText("0");
    CHECK_EQ(trkStartB->value(), 0);

    // Ten lines of red from the top: the band is red there, the next lines
    // will go under.
    txbConstant->setText("10");
    bpAddConstant->click();
    for (size_t i = 0; i < 272; ++i)
        CHECK_EQ(raster.line[i], i < 10 ? 0x0F0 : 0);
    CHECK(txbStart->text() == "10" && txbLineEnd->text() == "9" && bpUndo->isEnabled() && bpGenerate->isEnabled());
    CHECK(shown(200, 0) == QColor(255, 0, 0) && shown(499, 19) == QColor(255, 0, 0) && shown(300, 20) == QColor(Qt::black));
    CHECK(shown(0, 0) == sky && shown(610, 0) == narrow);
    // Five lines of blue upwards from line 30, then taken back: they are
    // black again, and the next lines go where they were.
    txbStartR->setText("0");
    txbStartB->setText("15");
    rbMoins->click();
    txbStart->setText("30");
    txbConstant->setText("5");
    bpAddConstant->click();
    for (size_t i = 10; i < 40; ++i)
        CHECK_EQ(raster.line[i], i >= 26 && i <= 30 ? 0x00F : 0);
    CHECK(txbStart->text() == "25" && shown(300, 60) == QColor(0, 0, 255) && shown(300, 50) == QColor(Qt::black));
    bpUndo->click();
    for (size_t i = 10; i < 40; ++i)
        CHECK_EQ(raster.line[i], 0);
    CHECK(txbStart->text() == "30" && !bpUndo->isEnabled());
    // The same downwards.
    rbPlus->click();
    txbStart->setText("10");
    txbConstant->setText("4");
    bpAddConstant->click();
    CHECK(raster.line[10] == 0x00F && raster.line[13] == 0x00F && raster.line[14] == 0 && txbStart->text() == "14");
    bpUndo->click();
    CHECK(raster.line[10] == 0 && raster.line[13] == 0 && raster.line[9] == 0x0F0 && txbStart->text() == "10" && txbLineEnd->text() == "9");
    // Lines that are not all in the table are told, and nothing is given.
    txbStart->setText("270");
    txbConstant->setText("5");
    CHECK(told.isEmpty());
    bpAddConstant->click();
    CHECK(told.size() == 1 && told.value(0) == "These lines are not all between 0 and 271.");
    CHECK(raster.line[270] == 0 && raster.line[271] == 0);
    rbMoins->click();
    txbStart->setText("300");
    txbConstant->setText("40");
    bpAddConstant->click();
    CHECK_EQ(told.size(), 2);
    told.clear();
    // No line at all, from line 10: the next go from there.
    rbPlus->click();
    txbStart->setText("10");
    txbConstant->setText("0");
    bpAddConstant->click();
    CHECK(told.isEmpty() && txbStart->text() == "10" && raster.line[10] == 0 && raster.line[9] == 0x0F0);

    // Colours that go from one to another: the maker of palettes, for
    // lines. It offers the lines under the last given and starts from the
    // last colour; "Generate" writes the lines and closes it.
    whenAsked("GenPalette", [&](QDialog* dialog) {
        CHECK(dialog->findChild<QLabel*>("lblFrom")->text() == "From line" && dialog->findChild<QLabel*>("lblTo")->text() == "To line");
        CHECK(box(dialog, "txbFrom")->text() == "10" && box(dialog, "txbTo")->text() == "25");
        CHECK(box(dialog, "txbStartR")->text() == "0" && box(dialog, "txbStartV")->text() == "0" && box(dialog, "txbStartB")->text() == "0");
        box(dialog, "txbFrom")->setText("20");
        box(dialog, "txbTo")->setText("35");
        box(dialog, "txbEndR")->setText("15");
        box(dialog, "txbEndV")->setText("15");
        CHECK(dialog->findChild<QSlider*>("trkEndR")->value() == 15);
        CHECK(dialog->findChild<ColourSwatch*>("lblEndColor")->colour() == QColor(255, 255, 0));
        // Nothing is written while numbers are typed.
        CHECK_EQ(raster.line[30], 0);
        if (!prefix.isEmpty())
            CHECK(saveInBothLooks(dialog, prefix + "raster_gradient"));
        button(dialog, "bpGenerate")->click();
        CHECK(!dialog->isVisible());
    });
    bpAddLine->click();
    for (int i = 0; i < 16; ++i)
        CHECK_EQ(raster.line[static_cast<size_t>(20 + i)], rasterWord(i, i, 0));
    CHECK(raster.line[19] == 0 && raster.line[36] == 0);
    CHECK(txbStart->text() == "36" && txbLineStart->text() == "0" && txbLineEnd->text() == "35" && !bpUndo->isEnabled());
    CHECK(shown(300, 70) == QColor(255, 255, 0) && shown(300, 50) == QColor(5 * 17, 5 * 17, 0));
    // Again: the lines under, as many of them, from the last colour.
    // "Get color" takes the colour a line has; the lines follow a slider
    // as it moves, and the window closed leaves them written.
    whenAsked("GenPalette", [&](QDialog* dialog) {
        CHECK(box(dialog, "txbFrom")->text() == "36" && box(dialog, "txbTo")->text() == "51");
        CHECK(box(dialog, "txbStartR")->text() == "15" && box(dialog, "txbStartV")->text() == "15" && box(dialog, "txbStartB")->text() == "0");
        box(dialog, "txbFrom")->setText("25");
        button(dialog, "bpGetCol")->click();
        CHECK(box(dialog, "txbStartR")->text() == "5" && box(dialog, "txbStartV")->text() == "5" && box(dialog, "txbStartB")->text() == "0");
        // Upwards: from line 60 to line 50.
        box(dialog, "txbFrom")->setText("60");
        box(dialog, "txbTo")->setText("50");
        box(dialog, "txbStartR")->setText("15");
        box(dialog, "txbStartV")->setText("0");
        CHECK_EQ(raster.line[55], 0);
        dialog->findChild<QSlider*>("trkEndR")->setValue(5);
        CHECK(box(dialog, "txbEndR")->text() == "5");
        for (int k = 0; k <= 10; ++k)
            CHECK_EQ(raster.line[static_cast<size_t>(60 - k)], rasterWord(15 - k, 0, 0));
        CHECK(dialog->findChild<QLabel*>("lblError")->text().isEmpty());
        // The same line twice is no run of lines.
        box(dialog, "txbTo")->setText("60");
        button(dialog, "bpGenerate")->click();
        CHECK(dialog->isVisible() && dialog->findChild<QLabel*>("lblError")->text() == "These values cannot be used.");
        dialog->reject();
    });
    bpAddLine->click();
    CHECK(raster.line[60] == 0x0F0 && raster.line[50] == 0x050 && raster.line[49] == 0 && raster.line[61] == 0);
    // The next lines are offered from under the last line typed.
    CHECK(txbStart->text() == "51");
    CHECK(txbLineEnd->text() == "50");
    // Closed without anything written, nothing has changed.
    const Raster before = raster;
    whenAsked("GenPalette", [&](QDialog* dialog) { dialog->reject(); });
    bpAddLine->click();
    CHECK(raster == before && txbStart->text() == "51");

    // The table moved down, and back up: lines that come in are black.
    txbScroll->setText("3");
    button(table, "bpScrollDown")->click();
    CHECK(raster.line[0] == 0 && raster.line[2] == 0 && raster.line[3] == 0x0F0 && raster.line[12] == 0x0F0 && raster.line[13] == 0);
    CHECK(raster.line[63] == 0x0F0 && shown(300, 6) == QColor(255, 0, 0) && shown(300, 4) == QColor(Qt::black));
    button(table, "bpScrollUp")->click();
    CHECK(raster == before);
    txbScroll->setText("0");
    button(table, "bpScrollUp")->click();
    txbScroll->setText("x");
    button(table, "bpScrollDown")->click();
    CHECK(raster == before);

    // A palette of the Plus, a colour to a line from the start line on: its
    // sixteen colours, or fifteen that leave the first line.
    std::array<int, 16> colours{};
    for (int i = 0; i < 16; ++i)
        colours[static_cast<size_t>(i)] = plusColour(i, 15 - i, (i * 3) & 15);
    const auto writeFile = [&scratch](const char* name, const std::vector<uint8_t>& bytes) {
        QFile file(scratch.filePath(QLatin1String(name)));
        CHECK(file.open(QIODevice::WriteOnly));
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<qint64>(bytes.size()));
        return file.fileName();
    };
    g_openPath = writeFile("sixteen.kit", kitFile("SIXTEEN.KIT", colours, true));
    txbStart->setText("100");
    bpImportKit->click();
    CHECK(g_filters == QStringList{"Palette files (*.kit)"});
    for (int i = 0; i < 16; ++i)
        CHECK_EQ(raster.line[static_cast<size_t>(100 + i)], rasterWord(i, 15 - i, (i * 3) & 15));
    CHECK(raster.line[99] == 0 && raster.line[116] == 0);
    CHECK(shown(300, 2 * 105) == QColor(5 * 17, 10 * 17, 15 * 17));
    g_openPath = writeFile("fifteen.kit", kitFile("FIFTEEN.KIT", colours, false));
    txbStart->setText("120");
    bpImportKit->click();
    CHECK(raster.line[120] == 0 && raster.line[121] == rasterWord(1, 14, 3) && raster.line[135] == rasterWord(15, 0, 13) && raster.line[136] == 0);
    // A file that is no palette, and lines that do not fit, are told.
    g_openPath = writeFile("other.kit", std::vector<uint8_t>(160, 0x55));
    bpImportKit->click();
    CHECK(told.size() == 1 && told.value(0) == "This file is not a palette of the Plus (.KIT).");
    txbStart->setText("256");
    bpImportKit->click();
    CHECK(told.size() == 2 && told.value(1) == "These lines are not all between 0 and 271.");
    told.clear();

    // The sprites drawn over the table where they are put on the picture:
    // sprite 0, twice as wide as high, of pen 5 but for a corner.
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            screen.sprites[0][0][x][y] = x < 2 && y < 2 ? 0 : 5;
    screen.spritePalette[5] = plusColour(0, 15, 15);
    document->spritePlaces.x[0] = 40;
    document->spritePlaces.y[0] = 100;
    document->spritePlaces.zoomX[0] = 2;
    document->spritePlaces.zoomY[0] = 1;
    const QColor sprite(0, 255, 255);
    CHECK(shown(50, 210) == sky);
    chkSprites->click();
    CHECK(shown(50, 210) == sprite && shown(71, 231) == sprite && shown(72, 210) == sky && shown(50, 232) == sky && shown(50, 199) == sky);
    // Its pen 0 lets the picture through.
    CHECK(shown(40, 200) == sky && shown(43, 203) == sky && shown(44, 200) == sprite);
    // A sprite that is not shown has no size.
    document->spritePlaces.zoomX[0] = 0;
    document->touch();
    CHECK(shown(50, 210) == sky);
    document->spritePlaces.zoomX[0] = 2;
    chkSprites->click();
    CHECK(shown(50, 210) == sky);

    // The line under the mouse.
    QMouseEvent move(QEvent::MouseMove, QPointF(300, 101), view->mapToGlobal(QPointF(300, 101)), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(view, &move);
    CHECK(lblLine->text() == "line 50");

    // The picture drawn again is followed: what takes the colour chosen
    // takes the lines' colours.
    for (int x = 0; x < 100; x += 2)
        picture.setPixel(x, 2 * 25, 1, 2);
    document->touch();
    CHECK(shown(50, 50) == QColor(5 * 17, 5 * 17, 0) && shown(50, 52) == sky);

    if (!prefix.isEmpty()) {
        chkSprites->click();
        CHECK(saveInBothLooks(table, prefix + "raster_table"));
        chkSprites->click();
    }

    // The lines to write out are typed: copied as the words of a source...
    const Raster kept = raster;
    txbLineStart->setText("0");
    txbLineEnd->setText("35");
    QApplication::clipboard()->clear();
    bpCopy->click();
    CHECK(QApplication::clipboard()->text() == words(kept, 0, 35));
    CHECK(QApplication::clipboard()->text().startsWith("\tDW\t#0F0,#0F0,#0F0,#0F0,#0F0,#0F0,#0F0,#0F0,#0F0,#0F0,#000,#000,#000,#000,#000,#000\n\tDW\t#000,"));
    CHECK(QApplication::clipboard()->text().endsWith("\tDW\t#CC0,#DD0,#EE0,#FF0\n; Taille totale 36 mots\r\n"));
    CHECK(raster.minLine == 0 && raster.maxLine == 35);
    // ... written as a source...
    g_savePath = scratch.filePath("rasters.asm");
    bpGenerate->click();
    CHECK(g_filters == QStringList{"Assembler file (*.asm)"});
    CHECK(QString::fromUtf8(fileText(g_savePath)) == QString::fromUtf8("; Mode écran - Mode 1\n") + words(kept, 0, 35));
    // ... or as the sixteen tables that take them from black to their
    // colours: of full red, a step of red at a time.
    txbLineEnd->setText("1");
    g_savePath = scratch.filePath("fade.asm");
    bpGenFade->click();
    QString fade = QString::fromUtf8("; Mode écran - Mode 1\n");
    for (int step = 0; step < 16; ++step) {
        const QString word = QStringLiteral("#0") + QString::number(step, 16).toUpper() + QLatin1Char('0');
        fade += QStringLiteral("Fade_") + QString::number(step) + QStringLiteral("\n\tDW\t") + word + QLatin1Char(',') + word
            + QStringLiteral("\n; Taille totale 2 mots\r\n");
    }
    CHECK(QString::fromUtf8(fileText(g_savePath)) == fade);
    // Lines that are not a run of the table are told, and nothing is
    // written.
    txbLineStart->setText("50");
    txbLineEnd->setText("40");
    g_savePath = scratch.filePath("none.asm");
    bpGenFade->click();
    bpCopy->click();
    txbLineEnd->setText("272");
    bpSave->click();
    CHECK(told.size() == 3 && told.value(0) == "The lines to write are from 0 to 271, the first before the last.");
    CHECK(!QFile::exists(g_savePath));
    told.clear();

    // Saved: the file C#'s serializer writes, which is read back the same.
    txbLineStart->setText("0");
    txbLineEnd->setText("135");
    g_savePath = scratch.filePath("rasters.xml");
    bpSave->click();
    CHECK(g_filters == QStringList{"XML files (*.xml)"});
    const QByteArray xml = fileText(g_savePath);
    CHECK(xml.startsWith("<?xml version=\"1.0\"?>\r\n<Raster xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
                         "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\">\r\n  <line>\r\n    <int>240</int>\r\n    <int>240</int>\r\n"));
    CHECK(xml.endsWith("    <int>0</int>\r\n  </line>\r\n  <minLine>0</minLine>\r\n  <maxLine>135</maxLine>\r\n</Raster>"));
    CHECK_EQ(xml.count("<int>"), 272);
    const Raster saved = raster;
    CHECK(saved.maxLine == 135 && saved.line == kept.line);

    // Everything cleared: black lines, and the next go from the top.
    bpClearAll->click();
    for (size_t i = 0; i < 272; ++i)
        CHECK_EQ(raster.line[i], 0);
    CHECK(txbStart->text() == "0" && txbLineStart->text() == "0" && txbLineEnd->text() == "0" && !bpGenerate->isEnabled());
    CHECK(shown(300, 0) == QColor(Qt::black));
    g_openPath = g_savePath;
    bpLoad->click();
    CHECK(raster == saved);
    CHECK(txbLineEnd->text() == "135" && bpGenerate->isEnabled() && shown(300, 0) == QColor(255, 0, 0));
    // A file that is not one is told, and the table is left.
    g_openPath = scratch.filePath("fade.asm");
    bpLoad->click();
    CHECK(told.size() == 1 && told.value(0) == "The rasters could not be read.");
    CHECK(raster == saved);
    told.clear();
    // Given up, nothing happens.
    g_openPath.clear();
    g_savePath.clear();
    bpLoad->click();
    bpSave->click();
    bpGenerate->click();
    bpGenFade->click();
    bpImportKit->click();
    CHECK(told.isEmpty() && raster.line == saved.line);

    // In French, in a window made while TuxSCR's translation is installed.
    {
        French french;
        CHECK(french.loaded());
        ConverterWindow other(nullptr);
        installSplitTools(&other);
        other.show();
        other.findChild<QCheckBox*>("modePlus")->click();
        other.findChild<QPushButton*>("bpOverscan")->click();
        whenAsked("SelectColor", [](QDialog* dialog) { dialog->reject(); });
        other.findChild<QPushButton*>("bpRasterPlus")->click();
        auto* otherTable = other.findChild<RasterTable*>("RasterTablePlus");
        CHECK(otherTable != nullptr);
        if (otherTable) {
            CHECK(otherTable->windowTitle() == "TuxSCR – Rasters Plus");
            CHECK(otherTable->findChild<QPushButton*>("bpLoad")->text() == "Lire rasters" && otherTable->findChild<QPushButton*>("bpUndo")->text() == "Annuler");
            CHECK(otherTable->findChild<QLabel*>("label1")->text() == QString::fromUtf8("De la ligne :"));
            if (!prefix.isEmpty())
                CHECK(saveInBothLooks(otherTable, prefix + "raster_table_fr"));
            otherTable->close();
        }
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(bpLoad->text() == "Load rasters" && lblLine->text() == "line 50");

    // Closed, the table is gone. Opened again with no colour chosen, the
    // lines' colours are shown everywhere.
    table->close();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(window.findChild<RasterTable*>("RasterTablePlus") == nullptr);
    document->touch();
    whenAsked("SelectColor", [](QDialog* dialog) { dialog->reject(); });
    open->click();
    table = window.findChild<RasterTable*>("RasterTablePlus");
    CHECK(table && table->isVisible() && table->rastered() == -1);
    if (table) {
        CHECK(table->pictureView()->image().pixelColor(0, 0) == QColor(Qt::black));
        CHECK(table->pictureView()->image().pixelColor(610, 300) == QColor(Qt::black));
        CHECK(table->raster() == Raster());
        table->close();
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(told.isEmpty());

    return checkSummary("gui_raster");
}
