// The picture's editor, driven as a user drives it: the pencil with either
// button, undo and redo, the filler, the pen picker, the magnifying glass,
// a block copied and put elsewhere, saved as a window of OCP Art Studio and
// read back, and the picture turned over. Runs without a display
// (QT_QPA_PLATFORM=offscreen).
//
//   gui_edit [prefix]   also saves pictures of the window as
//                       <prefix>tuxscr_edit.png and _dark.png

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>

#include "check.h"
#include "english.h"
#include "converterwindow.h"
#include "edittools.h"
#include "formats/amsdos.h"
#include "gui_looks.h"
#include "palettebar.h"
#include "penbutton.h"
#include "pictureview.h"
#include "prompts.h"

using namespace tuxscr;

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    const English english;
    const QString prefix = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    QTemporaryDir scratch;
    QStringList told;
    prompts::setProblemHandler([&told](const QString& text) { told << text; });

    ConverterWindow window(nullptr);
    window.show();
    Document* document = window.document();
    PictureView* view = window.pictureView();
    EditTools* tools = window.editTools();
    Picture& picture = document->picture();
    const auto radio = [&window](const char* name) { return window.findChild<QRadioButton*>(name); };
    const auto button = [&window](const char* name) { return window.findChild<QPushButton*>(name); };
    auto* edit = window.findChild<QCheckBox*>("modeEdition");
    CHECK(edit && tools && radio("rbDraw") && button("bpUndo"));
    if (!edit || !tools)
        return checkSummary("gui_edit");
    window.tools()->setCurrentWidget(window.findChild<QWidget*>("editTab"));

    // Not editing, the mouse does not draw.
    emit view->mouse(QPoint(100, 100), Qt::LeftButton);
    emit view->mouse(QPoint(100, 100), Qt::NoButton);
    CHECK_EQ(picture.pen(100, 100), 0);
    CHECK(!button("bpUndo")->isEnabled());

    edit->click();
    CHECK(tools->editing());
    CHECK(window.mouseOwner() == tools);

    // The pencil: mode 1, a pixel is two wide; the left button draws with
    // pen 1, the right with pen 0. A stroke is undone as one.
    emit view->mouse(QPoint(101, 51), Qt::LeftButton);
    emit view->mouse(QPoint(103, 51), Qt::LeftButton);
    emit view->mouse(QPoint(103, 51), Qt::NoButton);
    CHECK_EQ(picture.pen(100, 50), 1);
    CHECK_EQ(picture.pen(101, 50), 1);
    CHECK_EQ(picture.pen(102, 50), 1);
    CHECK_EQ(picture.pen(104, 50), 0);
    CHECK_EQ(picture.pen(100, 52), 0);
    CHECK(tools->canUndo() && !tools->canRedo() && button("bpUndo")->isEnabled());
    // A pen of the palette clicked is the pencil's: left button and right.
    emit window.paletteBar()->penClicked(3, false);
    emit window.paletteBar()->penClicked(2, true);
    CHECK_EQ(tools->drawPen(), 3);
    CHECK_EQ(tools->undrawPen(), 2);
    CHECK(window.findChild<QLabel*>("drawColor")->text() == "3");
    emit view->mouse(QPoint(200, 60), Qt::RightButton);
    emit view->mouse(QPoint(200, 60), Qt::NoButton);
    CHECK_EQ(picture.pen(200, 60), 2);
    button("bpUndo")->click();
    CHECK_EQ(picture.pen(200, 60), 0);
    CHECK_EQ(picture.pen(100, 50), 1);
    button("bpUndo")->click();
    CHECK_EQ(picture.pen(100, 50), 0);
    CHECK(!tools->canUndo() && tools->canRedo());
    button("bpRedo")->click();
    CHECK_EQ(picture.pen(100, 50), 1);
    CHECK_EQ(picture.pen(200, 60), 0);
    // A pencil of 4: four pixels by four lines.
    window.findChild<QComboBox*>("tailleCrayon")->setCurrentText("4");
    emit view->mouse(QPoint(300, 100), Qt::LeftButton);
    emit view->mouse(QPoint(300, 100), Qt::NoButton);
    CHECK_EQ(picture.pen(300, 100), 3);
    CHECK_EQ(picture.pen(307, 106), 3);
    CHECK_EQ(picture.pen(308, 100), 0);
    CHECK_EQ(picture.pen(300, 108), 0);
    window.findChild<QComboBox*>("tailleCrayon")->setCurrentText("1");

    // The picker takes the pen under the mouse.
    radio("rbPickColor")->click();
    emit view->mouse(QPoint(100, 50), Qt::RightButton);
    CHECK_EQ(tools->undrawPen(), 1);
    emit view->mouse(QPoint(302, 102), Qt::LeftButton);
    CHECK_EQ(tools->drawPen(), 3);

    // The filler: a frame is drawn, and its inside filled; the outside is
    // left, and so is the frame.
    radio("rbDraw")->click();
    for (int x = 400; x <= 440; x += 2) {
        emit view->mouse(QPoint(x, 200), Qt::LeftButton);
        emit view->mouse(QPoint(x, 220), Qt::LeftButton);
    }
    for (int y = 200; y <= 220; y += 2) {
        emit view->mouse(QPoint(400, y), Qt::LeftButton);
        emit view->mouse(QPoint(440, y), Qt::LeftButton);
    }
    emit view->mouse(QPoint(440, 220), Qt::NoButton);
    radio("rbFill")->click();
    emit window.paletteBar()->penClicked(2, false);
    radio("rbFill")->click();
    emit view->mouse(QPoint(420, 210), Qt::LeftButton);
    emit view->mouse(QPoint(420, 210), Qt::NoButton);
    CHECK_EQ(picture.pen(420, 210), 2);
    CHECK_EQ(picture.pen(402, 202), 2);
    CHECK_EQ(picture.pen(438, 218), 2);
    CHECK_EQ(picture.pen(400, 210), 3);
    CHECK_EQ(picture.pen(444, 210), 0);
    button("bpUndo")->click();
    CHECK_EQ(picture.pen(420, 210), 0);
    button("bpRedo")->click();

    // The magnifying glass: a rectangle drawn with the mouse is what the
    // view then shows, and the pencil draws where the mouse is in it.
    radio("rbZoom")->click();
    emit view->mouse(QPoint(96, 40), Qt::LeftButton);
    emit view->mouse(QPoint(96 + 160, 40 + 100), Qt::LeftButton);
    emit view->mouse(QPoint(96 + 160, 40 + 100), Qt::NoButton);
    CHECK_EQ(tools->zoom(), 4);
    CHECK(window.findChild<QLabel*>("lblZoom")->text() == "4:1");
    // Pen 1 at (100, 50) is now at 4 * (100 - 96), 4 * (50 - 40).
    CHECK(view->image().pixelColor(4 * 4 + 1, 4 * 10 + 1) == document->shown().pixelColor(100, 50));
    radio("rbDraw")->click();
    emit window.paletteBar()->penClicked(1, false);
    emit view->mouse(QPoint(4 * 20, 4 * 30), Qt::LeftButton);
    emit view->mouse(QPoint(4 * 20, 4 * 30), Qt::NoButton);
    CHECK_EQ(picture.pen(96 + 20, 40 + 30), 1);
    // The right button of the glass goes back, a half at a time.
    radio("rbZoom")->click();
    emit view->mouse(QPoint(10, 10), Qt::RightButton);
    emit view->mouse(QPoint(10, 10), Qt::NoButton);
    CHECK_EQ(tools->zoom(), 2);
    emit view->mouse(QPoint(10, 10), Qt::RightButton);
    emit view->mouse(QPoint(10, 10), Qt::NoButton);
    CHECK_EQ(tools->zoom(), 1);

    // A block: the frame and what it holds, copied and put elsewhere.
    radio("rbCopy")->click();
    CHECK(button("bpLoadWin")->isEnabled() && !button("bpSaveWin")->isEnabled());
    emit view->mouse(QPoint(400, 200), Qt::LeftButton);
    emit view->mouse(QPoint(448, 224), Qt::LeftButton);
    emit view->mouse(QPoint(448, 224), Qt::NoButton);
    CHECK(tools->block() == QSize(48, 24));
    CHECK(button("bpSaveWin")->isEnabled());
    emit view->mouse(QPoint(100, 300), Qt::NoButton);
    if (!prefix.isEmpty())
        CHECK(saveInBothLooks(&window, prefix + "tuxscr_edit"));
    emit view->mouse(QPoint(100, 300), Qt::LeftButton);
    emit view->mouse(QPoint(100, 300), Qt::NoButton);
    CHECK_EQ(picture.pen(100, 300), 3);
    CHECK_EQ(picture.pen(120, 310), 2);
    CHECK_EQ(picture.pen(140, 320), 3);
    CHECK_EQ(picture.pen(146, 322), 0);

    // The block as a window of OCP Art Studio: its bytes, then its width in
    // pixels and its height in lines; read back, it is the same block.
    const QString win = scratch.filePath("frame.win");
    prompts::setSaveAnswerer([&](const QString&, const QStringList&, int* chosen) {
        *chosen = 0;
        return win;
    });
    button("bpSaveWin")->click();
    const auto file = readHostFile(win);
    CHECK(file && hasAmsdosHeader(*file));
    if (file) {
        CHECK_EQ(file->size(), 128 + 6 * 12 + 4);
        CHECK_EQ((*file)[128], 0xFF);                 // four pixels of pen 3
        CHECK_EQ((*file)[128 + 6], 0x8F);             // one of pen 3, then three of pen 2
        CHECK_EQ((*file)[file->size() - 4], 48);
        CHECK_EQ((*file)[file->size() - 2], 12);
        const AmsdosHeader header = readAmsdosHeader(*file);
        CHECK(header.name == "FRAME   WIN" && header.address == 0x4000 && header.length == 6 * 12 + 4);
    }
    emit view->mouse(QPoint(10, 10), Qt::RightButton);
    emit view->mouse(QPoint(10, 10), Qt::NoButton);
    CHECK(tools->block() == QSize(0, 0));
    prompts::setOpenAnswerer([&](const QString&, const QStringList&, int*) { return win; });
    button("bpLoadWin")->click();
    CHECK(tools->block() == QSize(48, 24));
    emit view->mouse(QPoint(500, 20), Qt::LeftButton);
    emit view->mouse(QPoint(500, 20), Qt::NoButton);
    CHECK_EQ(picture.pen(500, 20), 3);
    CHECK_EQ(picture.pen(520, 30), 2);

    // Turned over: top to bottom, then left to right; twice each, it is the
    // picture again.
    const Picture before = picture;
    button("bpHorFlip")->click();
    CHECK_EQ(picture.pen(100, 398 - 50), 1);
    CHECK_EQ(picture.pen(100, 50), 0);
    button("bpHorFlip")->click();
    CHECK(picture == before);
    button("bpVerFlip")->click();
    CHECK_EQ(picture.pen(639 - 100, 50), 1);
    button("bpVerFlip")->click();
    CHECK(picture == before);

    // Editing ended, the mouse and the palette are the window's again.
    edit->click();
    CHECK(!tools->editing() && window.mouseOwner() == nullptr);
    CHECK(told.isEmpty());
    return checkSummary("gui_edit");
}
