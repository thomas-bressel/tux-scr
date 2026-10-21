// The split editor in the converter's window, driven as a user drives it:
// "Split Editor" opens it on the picture; a line is given splits, their
// lengths and their colours, a later first change, another pen, another
// mode for the line under it; the screen is shown as they make it, with the
// places of the changes and the line enlarged; a line is copied from the
// one above; the lines are saved and read back, taken from a picture of
// splits, and written as the program that shows them. Runs without a
// display (QT_QPA_PLATFORM=offscreen).
//
//   gui_split [prefix]   also saves pictures of the windows as
//                        <prefix>split_*.png and _dark.png

#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include "check.h"
#include "english.h"
#include "french.h"
#include "converterwindow.h"
#include "gui_looks.h"
#include "penbutton.h"
#include "pictures.h"
#include "pictureview.h"
#include "prompts.h"
#include "splits/splitasm.h"
#include "spliteditor.h"
#include "splittools.h"
#include "swatch.h"

using namespace tuxscr;

namespace {

QString g_openPath, g_savePath;
QStringList g_filters;

// Two colours for the splits that the picture's palette does not have:
// bright green and orange.
constexpr int kFirst = 18, kSecond = 15;

// The colour window that is about to ask is given one of the CPC's colours
// and accepted, or closed.
void answerColour(int colour)
{
    QTimer::singleShot(0, [colour] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        CHECK(dialog && dialog->objectName() == "EditColor");
        if (!dialog)
            return;
        if (colour < 0) {
            dialog->reject();
            return;
        }
        auto* button = dialog->findChild<PenButton*>(QStringLiteral("colour%1").arg(colour));
        CHECK(button != nullptr);
        if (button)
            button->click();
        dialog->accept();
    });
}

QColor cpc(int colour)
{
    return toQColor(kCpcColours[colour]);
}

QByteArray fileText(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// A picture of splits, a pixel for each pixel of mode 1: red, then red and
// blue, black, and a line with more colours than a line takes.
QImage splitPicture()
{
    QImage picture(384, 272, QImage::Format_RGB32);
    picture.fill(Qt::black);
    for (int y = 0; y < 20; ++y)
        for (int x = 0; x < 384; ++x)
            picture.setPixelColor(x, y, y >= 10 && x >= 100 ? QColor(0, 0, 255) : QColor(255, 0, 0));
    for (int x = 0; x < 384; ++x)
        picture.setPixelColor(x, 40, (x / 40) % 2 == 0 ? QColor(255, 0, 0) : QColor(0, 0, 255));
    return picture;
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
    auto* open = window.findChild<QPushButton*>("bpEditSplit");
    CHECK(open && open->isVisibleTo(&window));
    if (!open)
        return checkSummary("gui_split");

    // A picture of the CPC's own: stripes of the four pens of mode 1, pen 3
    // being the widest, on a standard screen.
    Picture& picture = document->picture();
    for (int y = 0; y < 400; y += 2)
        for (int x = 0; x < 640; x += 2)
            picture.setPixel(x, y, x < 480 ? 3 : (x / 32) % 3, 2);
    picture.computed = true;
    document->touch();

    CHECK(window.findChild<QWidget*>("EditSplit") == nullptr);
    open->click();
    auto* editor = window.findChild<SplitEditor*>("EditSplit");
    CHECK(editor && editor->isVisible() && editor->isWindow());
    if (!editor)
        return checkSummary("gui_split");
    CHECK(editor->windowTitle() == QString::fromUtf8("TuxSCR – Split Editor"));
    // Clicked again, it is the same window.
    open->click();
    CHECK_EQ(window.findChildren<SplitEditor*>().size(), 1);

    auto* numLigne = editor->findChild<QSpinBox*>("numLigne");
    auto* numPen = editor->findChild<QSpinBox*>("numPen");
    auto* retard = editor->findChild<QSpinBox*>("retard");
    auto* chkChgt = editor->findChild<QCheckBox*>("chkChgt");
    auto* chkChangeMode = editor->findChild<QCheckBox*>("chkChangeMode");
    auto* modeCpc = editor->findChild<QSpinBox*>("modeCpc");
    auto* bpCopieLigne = editor->findChild<QPushButton*>("bpCopieLigne");
    auto* bpImportSplit = editor->findChild<QPushButton*>("bpImportSplit");
    auto* bpGenAsm = editor->findChild<QPushButton*>("bpGenAsm");
    auto* bpLoad = editor->findChild<QPushButton*>("bpLoad");
    auto* bpSave = editor->findChild<QPushButton*>("bpSave");
    auto* hScrollZoom = editor->findChild<QScrollBar*>("hScrollZoom");
    auto* lblInfo = editor->findChild<QLabel*>("lblInfo");
    QCheckBox* chkSplit[7];
    QSpinBox* largSplit[7];
    ColourSwatch* lblColor[7];
    bool all = numLigne && numPen && retard && chkChgt && chkChangeMode && modeCpc && bpCopieLigne && bpImportSplit && bpGenAsm && bpLoad
        && bpSave && hScrollZoom && lblInfo;
    for (int i = 0; i < 7; ++i) {
        chkSplit[i] = editor->findChild<QCheckBox*>(QStringLiteral("chkSplit%1").arg(i));
        largSplit[i] = editor->findChild<QSpinBox*>(QStringLiteral("largSplit%1").arg(i));
        lblColor[i] = editor->findChild<ColourSwatch*>(QStringLiteral("lblColor%1").arg(i));
        all = all && chkSplit[i] && largSplit[i] && lblColor[i];
    }
    CHECK(all);
    if (!all)
        return checkSummary("gui_split");
    SplitScreen& screen = editor->screen();
    PictureView* view = editor->pictureView();
    const auto shown = [view](int x, int y) { return view->image().pixelColor(x, y); };

    // As opened: the first line, no split, the earliest first change; the
    // picture on an overscan screen, with the palette it has.
    CHECK(bpLoad->text() == "Load Splits" && chkSplit[6]->text() == "Split 7" && bpGenAsm->text() == "Generate assembler");
    CHECK_EQ(numLigne->value(), 0);
    CHECK_EQ(numLigne->maximum(), 261);
    CHECK_EQ(retard->value(), -4);
    CHECK(retard->minimum() == -4 && retard->maximum() == 28 && retard->singleStep() == 8);
    CHECK(largSplit[0]->minimum() == 32 && largSplit[0]->maximum() == 384 && largSplit[0]->singleStep() == 8);
    CHECK(!chkSplit[0]->isChecked() && !largSplit[0]->isVisibleTo(editor) && !lblColor[0]->isVisibleTo(editor));
    CHECK(!modeCpc->isVisibleTo(editor));
    CHECK(view->image().size() == QSize(768, 544));
    CHECK(shown(0, 0) == cpc(document->screen.palette[3]));
    CHECK(shown(700, 0) == cpc(document->screen.palette[0]));
    CHECK(shown(0, 500) == cpc(document->screen.palette[0]));
    for (int pen = 0; pen < 16; ++pen) {
        auto* start = editor->findChild<PenButton*>(QStringLiteral("colors%1").arg(pen));
        CHECK(start && start->colour() == cpc(document->screen.palette[static_cast<size_t>(pen)]));
    }

    // A split cannot be used before the one above it.
    chkSplit[1]->click();
    CHECK(!chkSplit[1]->isChecked() && !screen.lines[0].splits[1].enabled);
    // The first split of the line, for pen 3: green for 64 pixels...
    numPen->setValue(3);
    CHECK_EQ(screen.lines[0].pen, 3);
    chkSplit[0]->click();
    CHECK(screen.lines[0].splits[0].enabled && largSplit[0]->isVisibleTo(editor) && lblColor[0]->isVisibleTo(editor));
    largSplit[0]->setValue(64);
    CHECK_EQ(screen.lines[0].splits[0].length, 64);
    answerColour(kFirst);
    lblColor[0]->click();
    CHECK_EQ(screen.lines[0].splits[0].colour, kFirst);
    CHECK(lblColor[0]->colour() == cpc(kFirst) && lblColor[0]->caption() == "18");
    // ... the colour is the pen's from there on, on this line and under it.
    // The palette the screen starts with is not changed by it.
    CHECK_EQ(screen.startColour(3), document->screen.palette[3]);
    CHECK(editor->findChild<PenButton*>("colors3")->colour() == cpc(document->screen.palette[3]));
    CHECK(shown(0, 0) == cpc(kFirst) && shown(400, 1) == cpc(kFirst) && shown(100, 300) == cpc(kFirst));
    CHECK(shown(700, 0) == cpc(document->screen.palette[0]));
    // A colour window closed leaves the colour.
    answerColour(-1);
    lblColor[0]->click();
    CHECK_EQ(screen.lines[0].splits[0].colour, kFirst);
    // Then orange. A length is a multiple of four.
    chkSplit[1]->click();
    CHECK(screen.lines[0].splits[1].enabled);
    answerColour(kSecond);
    lblColor[1]->click();
    largSplit[1]->setValue(47);
    CHECK_EQ(largSplit[1]->value(), 44);
    CHECK_EQ(screen.lines[0].splits[1].length, 44);
    CHECK(shown(119, 0) == cpc(kFirst) && shown(120, 0) == cpc(kSecond) && shown(300, 1) == cpc(kSecond) && shown(100, 300) == cpc(kSecond));
    // The first change falls later: what is before it keeps the colour the
    // pen had.
    retard->setValue(12);
    CHECK_EQ(screen.lines[0].delay, 12);
    CHECK(shown(23, 0) == cpc(document->screen.palette[3]) && shown(24, 0) == cpc(kFirst) && shown(151, 0) == cpc(kFirst) && shown(152, 0) == cpc(kSecond));
    retard->setValue(-4);

    // The places of the changes are marked down the picture in the window's
    // own two colours, of which one stands out from any colour.
    const QColor ink = editor->palette().color(QPalette::Text), paper = editor->palette().color(QPalette::Base);
    QImage seen = view->grab().toImage();
    CHECK(seen.pixelColor(120, 0) == cpc(kSecond) && seen.pixelColor(120, 200) == cpc(kSecond));
    chkChgt->click();
    seen = view->grab().toImage();
    int inks = 0, papers = 0;
    for (int y = 0; y < 544; ++y) {
        inks += seen.pixelColor(120, y) == ink;
        papers += seen.pixelColor(120, y) == paper;
    }
    CHECK_EQ(inks, 272);
    CHECK_EQ(papers, 272);
    // 44 pixels are eleven bytes: the second colour ends at byte 26.
    CHECK(seen.pixelColor(208, 0) == paper && seen.pixelColor(208, 4) == ink);
    CHECK(seen.pixelColor(121, 100) == cpc(kSecond) && seen.pixelColor(300, 100) == cpc(kSecond));

    // The line under is asked to be in mode 0: the same bytes are other
    // pens there.
    CHECK(!chkChangeMode->isChecked());
    chkChangeMode->click();
    CHECK(screen.lines[0].changeMode && modeCpc->isVisibleTo(editor));
    modeCpc->setValue(0);
    CHECK_EQ(screen.lines[0].newMode, 0);
    CHECK_EQ(screen.lineModes[1], 0);
    CHECK(shown(0, 2) == cpc(document->screen.palette[15]) && shown(0, 4) == cpc(kSecond));
    modeCpc->setValue(2);
    CHECK(shown(0, 2) == cpc(document->screen.palette[1]));
    chkChangeMode->click();
    CHECK(!screen.lines[0].changeMode && !modeCpc->isVisibleTo(editor));
    CHECK(shown(0, 2) == cpc(kSecond));

    // A click on the picture goes to its line, and the part enlarged under
    // it is around the place clicked.
    CHECK(lblInfo->text() == "...");
    QMouseEvent move(QEvent::MouseMove, QPointF(300, 101), view->mapToGlobal(QPointF(300, 101)), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(view, &move);
    CHECK(lblInfo->text() == "Position X:150, Y:050");
    CHECK_EQ(numLigne->value(), 0);
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(view, &leave);
    CHECK(lblInfo->text() == "...");
    QTest::mouseClick(view, Qt::LeftButton, {}, QPoint(300, 101));
    CHECK_EQ(numLigne->value(), 50);
    CHECK_EQ(hScrollZoom->value(), 252);
    CHECK(!chkSplit[0]->isChecked() && !largSplit[0]->isVisibleTo(editor));
    const QImage zoomed = editor->zoomView()->image();
    CHECK(zoomed.size() == QSize(768, 128));
    CHECK(zoomed.pixelColor(0, 0) == shown(252, 93) && zoomed.pixelColor(767, 127) == shown(252 + 95, 93 + 15));
    // The last line is the last the program does.
    QTest::mouseClick(view, Qt::LeftButton, {}, QPoint(10, 543));
    CHECK_EQ(numLigne->value(), 261);
    CHECK_EQ(hScrollZoom->value(), 0);

    // The line above, copied: its pen, its splits and its delay.
    numLigne->setValue(1);
    CHECK_EQ(numPen->value(), 0);
    bpCopieLigne->click();
    CHECK(screen.lines[1] == screen.lines[0]);
    CHECK(numPen->value() == 3 && chkSplit[1]->isChecked() && largSplit[1]->value() == 44 && lblColor[1]->colour() == cpc(kSecond));
    CHECK(shown(0, 2) == cpc(kFirst) && shown(120, 2) == cpc(kSecond));
    // The first line has none above it.
    numLigne->setValue(0);
    const SplitLine first = screen.lines[0];
    bpCopieLigne->click();
    CHECK(screen.lines[0] == first);

    // Seven splits: the seventh, of a colour none of the six before has,
    // takes sixteen pixels more of the sixth.
    numLigne->setValue(3);
    numPen->setValue(3);
    for (int i = 0; i < 7; ++i) {
        chkSplit[i]->click();
        answerColour(i == 6 ? 26 : i + 1);
        lblColor[i]->click();
    }
    CHECK(screen.lines[3].splits[6].enabled && screen.lines[3].splits[6].colour == 26);
    CHECK_EQ(largSplit[5]->minimum(), 48);
    CHECK_EQ(screen.lines[3].splits[5].length, 48);
    answerColour(2);
    lblColor[6]->click();
    CHECK_EQ(largSplit[5]->minimum(), 32);
    // A split taken away takes those after it out of use, which keep what
    // they were given.
    chkSplit[2]->click();
    CHECK(!screen.lines[3].splits[2].enabled && screen.lines[3].splits[3].enabled);
    CHECK(!chkSplit[3]->isChecked() && !largSplit[3]->isVisibleTo(editor) && chkSplit[1]->isChecked());
    CHECK(screen.changes(3) == (std::vector<int>{-1, 7, 15}));
    chkSplit[2]->click();
    CHECK(chkSplit[6]->isChecked() && largSplit[6]->isVisibleTo(editor));

    // A colour of the palette the screen starts with is the editor's own:
    // the picture's palette is left, and takes its place again when the
    // picture is drawn again.
    auto* start0 = editor->findChild<PenButton*>("colors0");
    const int pen0 = document->screen.palette[0];
    answerColour(11);
    start0->click();
    CHECK_EQ(screen.startColour(0), 11);
    CHECK(start0->colour() == cpc(11) && shown(700, 0) == cpc(11));
    CHECK_EQ(document->screen.palette[0], pen0);
    document->touch();
    CHECK(screen.startColour(0) == pen0 && shown(700, 0) == cpc(pen0));
    // The picture's changes are followed.
    for (int x = 0; x < 64; x += 2)
        picture.setPixel(x, 20, 1, 2);
    document->touch();
    CHECK(shown(10, 20) == cpc(document->screen.palette[1]) && shown(70, 20) == cpc(2));

    if (!prefix.isEmpty()) {
        numLigne->setValue(0);
        QTest::mouseClick(view, Qt::LeftButton, {}, QPoint(140, 6));
        CHECK(saveInBothLooks(editor, prefix + "split_editor"));
    }

    // Saved: the file C#'s serializer writes, which is read back the same.
    const SplitLines kept = screen.lines;
    g_savePath = scratch.filePath("splits.xml");
    bpSave->click();
    CHECK(g_filters == QStringList{"SplitEditor files (*.xml)"});
    const QByteArray xml = fileText(g_savePath);
    CHECK(xml.startsWith("<?xml version=\"1.0\"?>\r\n<SplitEcran xmlns:xsi="));
    CHECK(xml.toStdString() == splitXml(kept));
    numLigne->setValue(0);
    chkSplit[0]->click();
    numLigne->setValue(3);
    retard->setValue(20);
    CHECK(!(screen.lines == kept));
    g_openPath = g_savePath;
    bpLoad->click();
    CHECK(screen.lines == kept);
    CHECK_EQ(retard->value(), -4);
    numLigne->setValue(0);
    CHECK(chkSplit[0]->isChecked() && largSplit[0]->value() == 64);
    CHECK(shown(0, 0) == cpc(kFirst));
    // A file that is not one is told, and the lines are left.
    QFile other(scratch.filePath("other.xml"));
    CHECK(other.open(QIODevice::WriteOnly));
    other.write("<?xml version=\"1.0\"?>\r\n<Param><sizex>3</sizex></Param>");
    other.close();
    g_openPath = other.fileName();
    CHECK(told.isEmpty());
    bpLoad->click();
    CHECK(told.size() == 1 && told.value(0) == "These splits could not be read.");
    CHECK(screen.lines == kept);
    // Given up, nothing happens.
    g_openPath.clear();
    g_savePath.clear();
    bpLoad->click();
    bpSave->click();
    bpGenAsm->click();
    CHECK(told.size() == 1 && screen.lines == kept);
    told.clear();

    // The program that shows the screen: ConvImgCpc's, for these lines.
    document->packMethod = PackMethod::Standard;
    g_savePath = scratch.filePath("splits.asm");
    bpGenAsm->click();
    CHECK(g_filters == QStringList{"Assembler source (*.asm)"});
    const QByteArray program = fileText(g_savePath);
    CHECK(program.startsWith("\tORG\t#"));
    CHECK(program.contains("; ---- Ligne 0 ----\n\tLD\tC,3\t\t\t; (2 NOPs)\n\tOUT\t(C),C\t\t\t; (4 NOPs)\n\tLD\tC,#52\t\t\t; (2 NOPs)\n"));
    CHECK(program.contains("; ---- Ligne 2 (vide) ----\n") && program.contains("; ---- Ligne 261 (vide) ----\n\tJP\tBoucle\n"));
    CHECK(program.toStdString() == splitProgram(screen, PackMethod::Standard));
    CHECK(editor->isEnabled());

    // Splits taken from a picture of them: the lines are given theirs as
    // soon as it is read, and what they cannot hold is listed.
    const QString bands = scratch.filePath("bands.png");
    CHECK(splitPicture().save(bands));
    const QString small = scratch.filePath("small.png");
    CHECK(QImage(320, 200, QImage::Format_RGB32).save(small));
    bool asked = false;
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        CHECK(dialog && dialog->objectName() == "ImportSplit");
        if (!dialog)
            return;
        asked = true;
        CHECK(dialog->windowTitle() == QString::fromUtf8("TuxSCR – Import a Split Picture"));
        auto* bpLire = dialog->findChild<QPushButton*>("bpLire");
        auto* pen = dialog->findChild<QSpinBox*>("numPen");
        auto* listErr = dialog->findChild<QListWidget*>("listErr");
        auto* pictureSplit = dialog->findChild<PictureView*>("pictureSplit");
        CHECK(bpLire && pen && listErr && pictureSplit);
        if (bpLire && pen && listErr && pictureSplit) {
            CHECK(bpLire->text() == "Load picture" && pen->value() == 3 && pen->maximum() == 15);
            // A picture of another size is not one of splits.
            g_openPath = small;
            bpLire->click();
            CHECK(told.size() == 1 && told.value(0) == "The picture must be 384 pixels by 272");
            CHECK(screen.lines == kept);
            g_openPath = other.fileName();
            bpLire->click();
            CHECK(told.size() == 2 && told.value(1) == "The picture could not be read (unknown format?)");
            pen->setValue(2);
            g_openPath = bands;
            bpLire->click();
            CHECK(g_filters.size() == 2 && g_filters.value(0) == "Pictures (*.bmp *.gif *.png *.jpg)");
            CHECK(pictureSplit->image().size() == QSize(768, 544));
            CHECK(pictureSplit->image().pixelColor(199, 21) == QColor(255, 0, 0) && pictureSplit->image().pixelColor(200, 20) == QColor(0, 0, 255));
            CHECK_EQ(listErr->count(), 1);
            CHECK(listErr->count() == 1 && listErr->item(0)->text() == "More than 7 splits found on line 40: only 7 will be kept.");
            CHECK_EQ(listErr->currentRow(), 0);
            if (!prefix.isEmpty())
                CHECK(saveInBothLooks(dialog, prefix + "split_import"));
        }
        dialog->close();
    });
    bpImportSplit->click();
    CHECK(asked);
    CHECK(screen.lines[0].splits[0] == (Split{380, 6, true}) && !screen.lines[0].splits[1].enabled && screen.lines[0].pen == 2);
    CHECK(!screen.lines[1].splits[0].enabled);
    CHECK(screen.lines[10].splits[0] == (Split{100, 6, true}) && screen.lines[10].splits[1] == (Split{280, 2, true}));
    CHECK(screen.lines[21].splits[0] == (Split{380, 0, true}));
    CHECK(screen.lines[40].splits[6] == (Split{40, 6, true}));
    // The editor shows the line it is on as the picture made it.
    CHECK(numLigne->value() == 0 && numPen->value() == 2 && chkSplit[0]->isChecked() && !chkSplit[1]->isChecked());
    CHECK_EQ(largSplit[0]->value(), 380);
    CHECK(lblColor[0]->colour() == cpc(6));
    told.clear();

    // In French, in a window made while TuxSCR's translation is installed.
    {
        French french;
        CHECK(french.loaded());
        ConverterWindow other(nullptr);
        installSplitTools(&other);
        other.show();
        other.findChild<QPushButton*>("bpEditSplit")->click();
        auto* otherEditor = other.findChild<SplitEditor*>("EditSplit");
        CHECK(otherEditor != nullptr);
        if (otherEditor) {
            CHECK(otherEditor->windowTitle() == QString::fromUtf8("TuxSCR – Éditeur de splits"));
            CHECK(otherEditor->findChild<QPushButton*>("bpLoad")->text() == "Lire Splits" && otherEditor->findChild<QPushButton*>("bpSave")->text() == "Sauver Splits");
            CHECK(otherEditor->findChild<QPushButton*>("bpCopieLigne")->text() == QString::fromUtf8("Copier ligne précédente"));
            CHECK(otherEditor->findChild<QLabel*>("label1")->text() == "Ligne" && otherEditor->findChild<QLabel*>("label13")->text() == "Couleur");
            CHECK(otherEditor->findChild<QLabel*>("label2")->text() == "Largeur");
            CHECK(otherEditor->findChild<QCheckBox*>("chkChangeMode")->text() == "Changement de mode ligne suivante");
            if (!prefix.isEmpty())
                CHECK(saveInBothLooks(otherEditor, prefix + "split_editor_fr"));
            otherEditor->close();
        }
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(bpLoad->text() == "Load Splits" && editor->findChild<QLabel*>("label1")->text() == "Line");
    // The main window's own controls of the same names kept their texts.
    CHECK(window.findChild<QPushButton*>("bpLoad") != bpLoad);

    // Closed, the editor is gone; opened again it is a new one.
    editor->close();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(window.findChild<SplitEditor*>("EditSplit") == nullptr);
    open->click();
    auto* again = window.findChild<SplitEditor*>("EditSplit");
    CHECK(again && again->isVisible());
    CHECK(again && !again->screen().lines[0].splits[0].enabled);
    // A change of the picture with no editor open is nothing to it.
    if (again)
        again->close();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    document->touch();
    CHECK(told.isEmpty());

    // The editor is not offered on a Plus, whose colours are not the Gate
    // Array's.
    auto* plus = window.findChild<QCheckBox*>("modePlus");
    CHECK(plus != nullptr);
    if (plus) {
        plus->click();
        CHECK(document->screen.plus && !open->isVisibleTo(&window));
        plus->click();
        CHECK(!document->screen.plus && open->isVisibleTo(&window));
    }

    return checkSummary("gui_split");
}
