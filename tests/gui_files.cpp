// The files the converter reads and writes, through its window: a picture
// read and converted, saved as a screen with the routine that shows it and
// read back, packed each way and unpacked, put on a disc, written as
// sources; palettes, settings, animations, tiles. Runs without a display
// (QT_QPA_PLATFORM=offscreen).

#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QImageReader>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include "animationpanel.h"
#include "check.h"
#include "english.h"
#include "converterwindow.h"
#include "formats/amsdos.h"
#include "formats/dsk.h"
#include "formats/pack.h"
#include "patterndialog.h"
#include "prompts.h"

using namespace tuxscr;

namespace {

// Four bands of colours a CPC has, so that the conversion loses nothing.
QImage bands(int width, int height)
{
    QImage picture(width, height, QImage::Format_RGB32);
    QPainter painter(&picture);
    const QColor colours[4] = {QColor(0, 0, 0), QColor(255, 0, 0), QColor(0, 255, 0), QColor(255, 255, 255)};
    for (int band = 0; band < 4; ++band)
        painter.fillRect(band * width / 4, 0, width / 4, height, colours[band]);
    return picture;
}

std::vector<uint8_t> fileOf(const QString& path)
{
    const auto file = readHostFile(path);
    return file ? *file : std::vector<uint8_t>();
}

QByteArray textOf(const QString& path)
{
    const std::vector<uint8_t> bytes = fileOf(path);
    return QByteArray(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size()));
}

// The next window that asks is accepted as it is.
void acceptNext(const char* name)
{
    QTimer::singleShot(0, [name] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        CHECK(dialog && dialog->objectName() == QLatin1String(name));
        if (dialog)
            dialog->accept();
    });
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    const English english;
    QTemporaryDir scratch;
    QStringList told;
    prompts::setProblemHandler([&told](const QString& text) { told << text; });

    const QString source = scratch.filePath("bands.png");
    CHECK(bands(320, 200).save(source));

    ConverterWindow window(nullptr);
    window.show();
    Document* document = window.document();
    Screen& screen = document->screen;
    const auto convert = [&window] { window.findChild<QPushButton*>("bpConvert")->click(); };

    // ---- A picture, converted for mode 1 ----
    window.openFile(source);
    convert();
    CHECK(window.windowTitle() == "TuxSCR – Screen Converter - bands.png");
    const Picture& picture = document->picture();
    // The four bands, each of one pen, and the pens' colours the bands'.
    int pens[4];
    for (int band = 0; band < 4; ++band) {
        pens[band] = picture.pen(band * 160 + 80, 100);
        CHECK_EQ(picture.pen(band * 160, 0), pens[band]);
        CHECK_EQ(picture.pen(band * 160 + 158, 398), pens[band]);
    }
    CHECK(pens[0] != pens[1] && pens[1] != pens[2] && pens[2] != pens[3] && pens[0] != pens[3]);
    const int colours[4] = {0, 6, 18, 26};
    for (int band = 0; band < 4; ++band)
        CHECK_EQ(screen.palette[static_cast<size_t>(pens[band])], colours[band]);
    const std::array<int, 17> palette = screen.palette;
    const Picture converted = picture;

    // ---- A screen, with the routine that shows it and its palette ----
    const QString scr = scratch.filePath("bands.scr");
    CHECK(document->saveScreen(scr, false));
    const std::vector<uint8_t> screenFile = fileOf(scr);
    CHECK_EQ(screenFile.size(), 128 + 16336);
    CHECK(hasAmsdosHeader(screenFile));
    const AmsdosHeader header = readAmsdosHeader(screenFile);
    CHECK(header.name == "BANDS   SCR" && header.type == 2);
    CHECK_EQ(header.address, 0xC000);
    CHECK_EQ(header.entry, 0xC7D0);
    CHECK_EQ(header.length, 16336);
    const std::vector<uint8_t> memory(screenFile.begin() + 128, screenFile.end());
    CHECK(memory[0x7D0] == 0x3A && memory[0x7D1] == 0xD0 && memory[0x7D2] == 0xD7 && memory[0x7D3] == 0xCD);
    CHECK_EQ(memory[0x17D0], 1);
    for (size_t pen = 0; pen < 4; ++pen)
        CHECK_EQ(memory[0x17D1 + pen], palette[pen]);
    // A byte of the first band is four pixels of its pen.
    CHECK_EQ(memory[0], kPenBits[pens[0]] | kPenBits[pens[0]] >> 1 | kPenBits[pens[0]] >> 2 | kPenBits[pens[0]] >> 3);

    // Read back, it is the picture, its mode and its palette.
    screen.palette.fill(0);
    screen.mode = kMode0;
    window.openFile(scr);
    CHECK_EQ(screen.mode, kMode1);
    CHECK(!screen.plus && screen.columns == 80 && screen.lines == 200);
    for (size_t pen = 0; pen < 4; ++pen)
        CHECK_EQ(screen.palette[pen], palette[pen]);
    bool same = true;
    for (int y = 0; y < 400 && same; y += 2)
        for (int x = 0; x < 640; x += 2)
            // The routine and the palette are in bytes the picture leaves
            // unused: they are no pixel of it.
            same = same && document->picture().pen(x, y) == converted.pen(x, y);
    CHECK(same);
    CHECK_EQ(window.findChild<QComboBox*>("mode")->currentIndex(), 1);

    // ---- Packed, each way: unpacked, it is the screen ----
    const struct {
        const char* name;
        PackMethod method;
    } methods[] = {{"Standard", PackMethod::Standard}, {"ZX0", PackMethod::ZX0}, {"ZX0 V2", PackMethod::ZX0_V2}, {"ZX1", PackMethod::ZX1}};
    for (const auto& method : methods) {
        window.findChild<QComboBox*>("comboPackMethode")->setCurrentText(method.name);
        CHECK(document->packMethod == method.method);
        const QString cmp = scratch.filePath("bands.cmp");
        CHECK(document->savePacked(cmp, {}, 0));
        const std::vector<uint8_t> packed = fileOf(cmp);
        CHECK(hasAmsdosHeader(packed));
        const AmsdosHeader packedHeader = readAmsdosHeader(packed);
        // The file ends at #A657, and is entered at the routine that
        // unpacks it, which is after the packed screen.
        CHECK_EQ(packedHeader.address + packedHeader.length, 0xA657);
        CHECK(packedHeader.entry > packedHeader.address && packedHeader.entry < 0xA657);
        const std::vector<uint8_t> data(packed.begin() + 128, packed.end());
        const std::vector<uint8_t> unpacked = method.method == PackMethod::Standard ? unpackStandard(data)
            : method.method == PackMethod::ZX1 ? unpackZx1(data)
            : unpackZx0(data, method.method == PackMethod::ZX0_V2);
        CHECK(unpacked == memory);
        CHECK(packed.size() < 2000);
    }

    // ---- As sources ----
    const QString asmFile = scratch.filePath("bands.asm");
    prompts::setSaveAnswerer([&](const QString&, const QStringList& filters, int* chosen) {
        // "Packed screen as a source" is the sixth kind offered.
        CHECK(filters.size() >= 15 && filters[5].contains("*.asm"));
        *chosen = 5;
        return asmFile;
    });
    acceptNext("SaveMedia");
    window.save();
    const QByteArray program = textOf(asmFile);
    CHECK(program.startsWith("; G\xC3\xA9n\xC3\xA9r\xC3\xA9 par TuxSCR "));
    CHECK(program.contains("; Mode \xC3\xA9" "cran - Mode 1\n; Taille (nbColsxNbLignes) 80x200\n\tORG\t#"));
    CHECK(program.contains("\nbands\n\tDB\t#") && program.contains("\tRUN\t$\n_StartDepack:\n\tDI\n\tLD\tHL,Palettebands\n"));
    CHECK(program.contains("\tLD\tHL,bands\n\tLD\tDE,#C000\n\tCALL\tDepack\n") && program.contains("dzx1s_elias"));
    CHECK(program.endsWith("Palettebands\n\tDB\t#54,#4C,#52,#4B,#54,#54,#54,#54,#54,#54,#54,#54,#54,#54,#54,#54,#54,#8D\n")
          || program.contains("Palettebands\n\tDB\t#"));
    // A sprite: the picture's bytes line after line.
    acceptNext("SaveMedia");
    CHECK(window.askLabels("Soft sprite", "bands", *new AsmLabels));
    CHECK(document->saveSprite(asmFile, {"Sprite", ""}, false));
    CHECK(textOf(asmFile).contains("\nSprite\n\tDB\t#") && textOf(asmFile).contains("; Taille totale 16000 octets\n"));

    // ---- On a disc ----
    const QString dskFile = scratch.filePath("bands.dsk");
    window.findChild<QComboBox*>("comboPackMethode")->setCurrentText("ZX0 V2");
    CHECK(document->saveScreen(dskFile, true));
    CHECK(document->savePacked(dskFile, {}, 2));
    CHECK(document->saveScreen(dskFile, true));
    Dsk dsk;
    CHECK(dsk.load(fileOf(dskFile)));
    const std::vector<std::string> names = dsk.fileNames();
    CHECK_EQ(names.size(), 3);
    CHECK(names.size() == 3 && names[0] == "CNVIMG00.SCR" && names[1] == "CNVIMG00.CMP" && names[2] == "CNVIMG01.SCR");
    std::vector<uint8_t> onDisc = dsk.file("CNVIMG00.SCR");
    CHECK(onDisc.size() >= 128 + 16336);
    onDisc.resize(128 + 16336);
    // The same screen as the file's, under the disc's name.
    CHECK(std::equal(onDisc.begin() + 128, onDisc.end(), screenFile.begin() + 128));
    CHECK(readAmsdosHeader(onDisc).name == "CNVIMG00SCR");
    CHECK_EQ(fileOf(dskFile).size(), 256 + 40 * (256 + 9 * 512));

    // ---- Palettes and settings ----
    const QString pal = scratch.filePath("bands.pal");
    CHECK(document->savePalette(pal));
    CHECK_EQ(fileOf(pal).size(), 128 + 239);
    screen.palette.fill(13);
    window.openFile(pal);
    for (size_t pen = 0; pen < 4; ++pen)
        CHECK_EQ(screen.palette[pen], palette[pen]);
    // A palette that was read is kept.
    CHECK_EQ(document->param.lockState[0], 1);

    const QString xml = scratch.filePath("settings.xml");
    window.findChild<QSpinBox*>("pctTrame");
    document->param.pctLumi = 120;
    document->param.method = "Bayer 2 (4x4)";
    document->param.pct = 40;
    document->param.lockState = {1, 0, 1, 0};
    CHECK(document->saveParam(xml));
    CHECK(textOf(xml).contains("<methode>Bayer 2 (4x4)</methode>") && textOf(xml).contains("<pctLumi>120</pctLumi>"));
    document->param.pctLumi = 100;
    document->param.method = "Aucun";
    document->param.lockState.fill(0);
    window.openFile(xml);
    CHECK_EQ(document->param.pctLumi, 120);
    CHECK(document->param.method == "Bayer 2 (4x4)");
    CHECK(document->param.lockState[0] == 1 && document->param.lockState[1] == 0 && document->param.lockState[2] == 1);
    CHECK(window.findChild<QComboBox*>("methode")->currentText() == "Bayer 2 (4x4)");
    CHECK_EQ(window.findChild<QSpinBox*>("pctTrame")->value(), 40);
    window.findChild<QComboBox*>("methode")->setCurrentIndex(0);
    document->param.pctLumi = 100;
    document->param.lockState.fill(0);

    // ---- A Plus: its palette as a .KIT ----
    window.findChild<QCheckBox*>("modePlus")->click();
    CHECK(screen.plus);
    CHECK_EQ(screen.palette[static_cast<size_t>(pens[1])], plusColour(15, 0, 0));
    const QString kit = scratch.filePath("bands.kit");
    CHECK(document->saveKit(kit, true));
    CHECK_EQ(fileOf(kit).size(), 128 + 32);
    const std::array<int, 17> plusPalette = screen.palette;
    screen.palette.fill(0);
    window.openFile(kit);
    CHECK(std::equal(plusPalette.begin(), plusPalette.begin() + 16, screen.palette.begin()));
    CHECK(document->saveScreen(scr, false));
    const std::vector<uint8_t> plusScreen = fileOf(scr);
    CHECK(plusScreen[128 + 0x7D0] == 0xF3 && plusScreen[128 + 0x17D0] == (1 | 0x8C));
    window.openFile(scr);
    CHECK(screen.plus && screen.mode == kMode1);
    CHECK(std::equal(plusPalette.begin(), plusPalette.begin() + 16, screen.palette.begin()));
    window.findChild<QCheckBox*>("modePlus")->click();
    CHECK(!screen.plus);

    // ---- Overscan ----
    window.openFile(source);
    window.findChild<QPushButton*>("bpOverscan")->click();
    CHECK(screen.columns == 96 && screen.lines == 272);
    convert();
    const QString wide = scratch.filePath("wide.scr");
    CHECK(document->saveScreen(wide, false));
    const std::vector<uint8_t> wideFile = fileOf(wide);
    CHECK_EQ(wideFile.size(), 128 + 0x7CC0);
    CHECK_EQ(readAmsdosHeader(wideFile).address, 0x200);
    CHECK_EQ(readAmsdosHeader(wideFile).entry, 0x811);
    const Picture widePicture = document->picture();
    window.findChild<QPushButton*>("bpStandard")->click();
    window.openFile(wide);
    CHECK(screen.columns == 96 && screen.lines == 272);
    same = true;
    for (int y = 0; y < 544 && same; y += 2)
        for (int x = 0; x < 768; x += 2)
            same = same && document->picture().pen(x, y) == widePicture.pen(x, y);
    CHECK(same);
    window.findChild<QPushButton*>("bpStandard")->click();

    // ---- An animation: three frames ----
    document->setBlankFrames(3);
    document->resetPictures(true);
    CHECK_EQ(document->frameCount(), 3);
    for (int frame = 0; frame < 3; ++frame) {
        document->picture(frame).setPixel(100 + 40 * frame, 100, 1 + frame, 20);
        document->picture(frame).computed = true;
        document->setFrameTime(frame, 100 + 100 * frame);
    }
    document->touch();
    const QString imp = scratch.filePath("three.imp");
    CHECK(document->saveImp(imp));
    CHECK_EQ(fileOf(imp).size(), 128 + 3 * 16000 + 3);
    const QString gif = scratch.filePath("three.gif");
    CHECK(document->saveGif(gif));
    {
        QImageReader reader(gif);
        CHECK(reader.supportsAnimation());
        for (int frame = 0; frame < 3; ++frame) {
            const QImage read = reader.read().convertToFormat(QImage::Format_RGB32);
            CHECK(read == document->shown(frame));
            if (frame == 2)
                CHECK_EQ(reader.nextImageDelay(), 300);
        }
        CHECK(reader.read().isNull());
    }
    // As the source of the program that plays them: each frame is what
    // changes from the one before.
    const QString anim = scratch.filePath("three.asm");
    acceptNext("SaveAnim");
    CHECK(window.animationPanel()->saveAnimation(anim));
    const QByteArray animText = textOf(anim);
    CHECK(animText.contains("\tORG\t#0200\n\tRUN\t$\n") && animText.contains("Delta0:") && animText.contains("Delta2:") && !animText.contains("Delta3:"));
    CHECK(animText.contains("AnimDelta") && animText.contains("\tDW\tDelta2\n"));
    // Read back, the .IMP is the three frames.
    window.openFile(imp);
    CHECK_EQ(document->frameCount(), 3);
    CHECK_EQ(document->picture(0).pen(100, 100), 1);
    CHECK_EQ(document->picture(1).pen(140, 100), 2);
    CHECK_EQ(document->picture(2).pen(180, 100), 3);
    CHECK_EQ(document->picture(2).pen(100, 100), 0);
    CHECK(window.findChild<QCheckBox*>("chkAllPics")->isVisibleTo(&window));

    // ---- Tiles ----
    window.openFile(source);
    convert();
    const QString tiles = scratch.filePath("tiles.imp");
    CHECK(document->saveTiles(tiles, 16, 16));
    // Four bands: four different tiles of two bytes by eight lines.
    CHECK_EQ(fileOf(tiles).size(), 128 + 4 * 16 + 3);
    CHECK_EQ(fileOf(scratch.filePath("tiles.TIL")).size(), 128 + 40 * 25);
    CHECK(QFile::exists(scratch.filePath("tiles.PAL")));

    // ---- EGX: two screens, the lines of each mode ----
    window.findChild<QComboBox*>("mode")->setCurrentIndex(kModeEgx1);
    convert();
    CHECK(document->saveEgx(scratch.filePath("egx.scr")));
    const std::vector<uint8_t> egx0 = fileOf(scratch.filePath("egx0.SCR")), egx1 = fileOf(scratch.filePath("egx1.SCR"));
    CHECK(egx0.size() == 128 + 16336 && egx1.size() == 128 + 16336);
    // The first has the even lines, in mode 1; the second the odd ones.
    CHECK(egx0[128 + 0x17D0] == 1 && egx1[128 + 0x17D0] == 0);
    CHECK(egx0[128 + 0x0800 + 20] == 0 && egx1[128 + 20] == 0);
    window.findChild<QComboBox*>("mode")->setCurrentIndex(kMode1);

    // ---- Mode ASC-UT: its patterns ----
    window.findChild<QComboBox*>("mode")->setCurrentIndex(kModeAscUt);
    CHECK(window.findChild<QPushButton*>("bpEditTrame")->isVisibleTo(&window));
    {
        PatternDialog patterns(&window);
        patterns.show();
        // The fourth pattern is chosen, and its top left pixel given pen 2.
        QWidget* all = patterns.findChild<QWidget*>("pictAllMatrice");
        QWidget* edit = patterns.findChild<QWidget*>("pictEditMatrice");
        QTest::mouseClick(all, Qt::LeftButton, {}, patterns.patternCentre(3));
        CHECK_EQ(patterns.current(), 3);
        QTest::mouseClick(patterns.findChild<QWidget*>("lblPen2"), Qt::LeftButton);
        QTest::mouseClick(edit, Qt::LeftButton, {}, patterns.cellCentre(0, 0));
        CHECK_EQ(screen.patterns[3][0][0], 2);
        const QString file = scratch.filePath("patterns.xml");
        prompts::setSaveAnswerer([&](const QString&, const QStringList&, int*) { return file; });
        patterns.findChild<QPushButton*>("bpSave")->click();
        screen.patterns[3][0][0] = 0;
        prompts::setOpenAnswerer([&](const QString&, const QStringList&, int*) { return file; });
        patterns.findChild<QPushButton*>("bpRead")->click();
        CHECK_EQ(screen.patterns[3][0][0], 2);
        // The second ready-made set.
        patterns.findChild<QSpinBox*>("numTabTrame")->setValue(1);
        patterns.findChild<QPushButton*>("bpCopyTrame")->click();
        CHECK_EQ(screen.patterns[1][0][0], 1);
        CHECK_EQ(screen.patterns[1][3][0], 0);
    }
    convert();
    // A picture of an ASCII mode as a source: its characters, and the
    // routine that draws them.
    CHECK(document->savePacked(asmFile, {}, 1));
    const QByteArray asciiText = textOf(asmFile);
    CHECK(asciiText.contains("; Mode \xC3\xA9" "cran - Mode ASC-UT"));
    CHECK(asciiText.contains("; Type Frame ='") && asciiText.contains("Delta0:") && asciiText.contains("\tAlign\t256"));
    if (!asciiText.contains("Delta0:"))
        std::printf("%s\n", asciiText.left(600).constData());

    for (const QString& problem : told)
        std::printf("  told: %s\n", qPrintable(problem));
    CHECK(told.isEmpty());
    return checkSummary("gui_files");
}
