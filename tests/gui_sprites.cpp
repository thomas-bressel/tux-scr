// The editor of the Plus's hardware sprites, driven as a user drives it: it
// is opened from the converter's window, sprites are drawn with the point,
// the line and the filler, copied, flipped, turned, moved and changed
// places, their palette is changed, reversed and made of shades, they are
// tried at the Plus's sizes, and saved and read back in each of the
// editor's formats, whose bytes are checked. Runs without a display
// (QT_QPA_PLATFORM=offscreen).
//
//   gui_sprites [prefix]   also saves pictures of the windows as
//                          <prefix>sprites_editor.png, _dark.png and so on

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include "check.h"
#include "french.h"
#include "converterwindow.h"
#include "formats/amsdos.h"
#include "formats/pack.h"
#include "gui_looks.h"
#include "penbutton.h"
#include "prompts.h"
#include "spriteeditor.h"
#include "spritefiles.h"
#include "spritetools.h"
#include "spriteviews.h"

using namespace tuxscr;

namespace {

void sendMouse(QWidget* widget, QEvent::Type type, QPoint at, Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent event(type, QPointF(at), QPointF(widget->mapToGlobal(at)), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
}

// The middle of a pixel of the grid the sprite is drawn in.
QPoint cell(int x, int y)
{
    return QPoint(x * SpriteGrid::kCell + 20, y * SpriteGrid::kCell + 20);
}

void press(QWidget* widget, QPoint at, Qt::MouseButton button = Qt::LeftButton)
{
    sendMouse(widget, QEvent::MouseButtonPress, at, button, button);
}

void drag(QWidget* widget, QPoint at, Qt::MouseButton button = Qt::LeftButton)
{
    sendMouse(widget, QEvent::MouseMove, at, Qt::NoButton, button);
}

void release(QWidget* widget, QPoint at, Qt::MouseButton button = Qt::LeftButton)
{
    sendMouse(widget, QEvent::MouseButtonRelease, at, button, Qt::NoButton);
}

void click(QWidget* widget, QPoint at, Qt::MouseButton button = Qt::LeftButton)
{
    press(widget, at, button);
    release(widget, at, button);
}

// The window that is about to ask (one shown with exec()) is given to
// `fill`, which fills it in and closes it.
void whenAsked(const char* name, std::function<void(QDialog*)> fill)
{
    QTimer::singleShot(0, [name, fill = std::move(fill)] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        CHECK(dialog && dialog->objectName() == QLatin1String(name));
        if (!dialog) {
            std::printf("  no window \"%s\" asked anything\n", name);
            return;
        }
        fill(dialog);
    });
}

std::vector<uint8_t> fileBytes(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray all = file.readAll();
    return std::vector<uint8_t>(all.begin(), all.end());
}

QStringList fileLines(const QString& path)
{
    const std::vector<uint8_t> bytes = fileBytes(path);
    return QString::fromUtf8(QByteArray(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size()))).split('\n');
}

// The bytes of the lines of data from `from` on, up to the line that says
// how many they are; `from` is left on that line.
std::vector<uint8_t> dataBytes(const QStringList& lines, qsizetype& from)
{
    std::vector<uint8_t> bytes;
    for (; from < lines.size() && lines[from].startsWith("\tDB\t"); ++from)
        for (const QString& value : lines[from].mid(4).split(','))
            bytes.push_back(static_cast<uint8_t>(value.mid(1).toInt(nullptr, 16)));
    return bytes;
}

// A sprite's bytes as a file has them: line after line.
std::vector<uint8_t> spriteOf(const Screen& screen, int bank, int sprite)
{
    std::vector<uint8_t> bytes;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            bytes.push_back(screen.sprites[bank][sprite][x][y]);
    return bytes;
}

// The line of a source that holds the sprites' fifteen colours.
QString paletteLine(const Screen& screen)
{
    QStringList words;
    for (size_t i = 1; i < 16; ++i)
        words << QStringLiteral("#%1").arg(asicWord(screen.spritePalette[i]), 3, 16, QLatin1Char('0')).toUpper();
    return QStringLiteral("\tDW\t") + words.join(',');
}

int count(const Screen& screen, int bank, int sprite, int pen)
{
    int found = 0;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            found += screen.sprites[bank][sprite][x][y] == pen;
    return found;
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    const QString prefix = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    QTemporaryDir scratch;
    QStringList told;
    prompts::setProblemHandler([&told](const QString& text) { told << text; });
    QString answer;
    int answerFilter = 0;
    QStringList askedFilters;
    const prompts::FileAnswerer answerer = [&](const QString&, const QStringList& filters, int* chosen) {
        askedFilters = filters;
        if (chosen)
            *chosen = answerFilter;
        return answer;
    };
    prompts::setSaveAnswerer(answerer);
    prompts::setOpenAnswerer(answerer);

    ConverterWindow window(nullptr);
    installSpriteTools(&window);
    window.show();
    Document* document = window.document();
    Screen& screen = document->screen;

    // The editor is the Plus's: its button is there with the Plus's palette.
    auto* open = window.findChild<QPushButton*>("bpEditSprites");
    auto* plus = window.findChild<QCheckBox*>("modePlus");
    CHECK(open && plus);
    if (!open || !plus)
        return checkSummary("gui_sprites");
    CHECK(!open->isVisibleTo(&window));
    CHECK(window.findChild<SpriteEditor*>("EditSprites") == nullptr);
    plus->click();
    CHECK(screen.plus && open->isVisibleTo(&window));
    open->click();
    auto* editor = window.findChild<SpriteEditor*>("EditSprites");
    CHECK(editor && editor->isVisible() && !editor->isModal());
    if (!editor)
        return checkSummary("gui_sprites");
    // Asked for again, it is the same window.
    open->click();
    CHECK_EQ(window.findChildren<SpriteEditor*>().size(), 1);
    CHECK(editor->windowTitle() == QString::fromUtf8("TuxSCR – Editing Hard Sprites"));
    // Sprites that had no palette take the picture's.
    for (size_t pen = 0; pen < 16; ++pen)
        CHECK_EQ(screen.spritePalette[pen], screen.palette[pen]);

    const auto button = [editor](const char* name) { return editor->findChild<QAbstractButton*>(name); };
    const auto label = [editor](const char* name) { return editor->findChild<QLabel*>(name); };
    auto* grid = editor->findChild<SpriteGrid*>("pictEditSprite");
    auto* strip = editor->findChild<SpriteStrip*>("pictAllSprites");
    auto* test = editor->findChild<SpriteTestView*>("pictTest");
    auto* banks = editor->findChild<QComboBox*>("comboBanque");
    PenButton* pens[16];
    bool allPens = true;
    for (int pen = 0; pen < 16; ++pen) {
        pens[pen] = editor->findChild<PenButton*>(QStringLiteral("lblColors%1").arg(pen));
        allPens = allPens && pens[pen];
    }
    CHECK(grid && strip && test && banks && allPens);
    for (const char* name : {"bpPrev", "bpSuiv", "bpCopySprite", "bpPasteSprite", "bpHorizontalFlip", "bpVerticalFlip", "bpRotateSprite",
                             "bpClearSprite", "bpScrUp", "bpScrDown", "bpScrLeft", "bpScrRight", "bpCopyBank", "bpPasteBank", "bpClearBank",
                             "bpRead", "bpSave", "bpSaveAll", "chkWithPal", "bpReadPal", "bpSavePal", "bpGenPal", "bpInversePalette",
                             "rbPt", "rbLine", "rbFill", "rb1Sprite", "rb2Sprite", "rb4Sprite", "rb42Sprite", "rb28sprite", "rbPosSpr",
                             "bpTest"}) {
        CHECK(button(name) != nullptr);
        if (!button(name)) {
            std::printf("  no control \"%s\"\n", name);
            return checkSummary("gui_sprites");
        }
        // What a control is, said to those who do not see it.
        CHECK(!button(name)->text().isEmpty() || !button(name)->accessibleName().isEmpty());
        CHECK(button(name)->text().isEmpty() || !button(name)->text().contains('\r'));
    }
    if (!grid || !strip || !test || !banks || !allPens)
        return checkSummary("gui_sprites");

    // Its texts, in English at first.
    CHECK(label("label1")->text() == "Left bp" && label("label2")->text() == "Right bp");
    CHECK(button("bpSave")->text() == "current bank" && button("bpSaveAll")->text() == "all banks");
    CHECK(button("chkWithPal")->text() == "With palette" && button("chkWithPal")->isChecked());
    CHECK(button("rbPt")->text() == "Point" && button("rbPt")->isChecked() && button("rbLine")->text() == "Line");
    CHECK(button("bpTest")->text() == "Test!" && button("rb1Sprite")->isChecked());
    CHECK(label("lblSelSprite")->text() == "Sprite #: 0");
    CHECK_EQ(banks->count(), 8);
    CHECK(banks->itemText(0) == "Bank 1" && banks->itemText(7) == "Bank 8" && banks->currentIndex() == 0);
    // At the first sprite there is none before it.
    CHECK(!button("bpPrev")->isVisible() && button("bpSuiv")->isVisible());

    // ---- Drawing ----
    // The left button draws with pen 1 at first, the right one rubs out.
    CHECK_EQ(editor->drawingPen(), 1);
    click(grid, cell(3, 2));
    CHECK_EQ(screen.sprites[0][0][3][2], 1);
    CHECK(pens[1]->mark() == PenButton::Mark::Tick && pens[0]->mark() == PenButton::Mark::Tick && pens[2]->mark() == PenButton::Mark::None);
    click(grid, cell(3, 2), Qt::RightButton);
    CHECK_EQ(screen.sprites[0][0][3][2], 0);
    CHECK(pens[1]->mark() == PenButton::Mark::None);
    // A pen of the palette is chosen with a click: the square on the left
    // shows it, with its green, red and blue.
    screen.spritePalette[5] = plusColour(3, 12, 7);
    pens[5]->click();
    CHECK_EQ(editor->drawingPen(), 5);
    CHECK(label("lblPenLeft")->palette().color(QPalette::Window) == QColor(3 * 17, 12 * 17, 7 * 17));
    CHECK(label("lblColSelV")->text() == "V:12" && label("lblColSelR")->text() == "R:3" && label("lblColSelB")->text() == "B:7");
    CHECK(label("lblPenRight")->palette().color(QPalette::Window) == spriteColour(screen.spritePalette[0]));
    // Dragged, the point draws all along.
    press(grid, cell(0, 0));
    drag(grid, cell(1, 0));
    drag(grid, cell(2, 0));
    release(grid, cell(2, 0));
    CHECK(screen.sprites[0][0][0][0] == 5 && screen.sprites[0][0][1][0] == 5 && screen.sprites[0][0][2][0] == 5);
    CHECK_EQ(count(screen, 0, 0, 5), 3);
    // The colour of the pixel under the mouse is framed in the palette.
    sendMouse(grid, QEvent::MouseMove, cell(1, 0), Qt::NoButton, Qt::NoButton);
    for (int pen = 0; pen < 16; ++pen) {
        auto* holder = qobject_cast<PenHolder*>(pens[pen]->parentWidget());
        CHECK(holder && holder->framed() == (pen == 5));
    }
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(grid, &leave);
    CHECK(!qobject_cast<PenHolder*>(pens[5]->parentWidget())->framed());

    // The line follows the mouse and is drawn when the button is let go:
    // what it passed over on its way is as it was.
    button("bpClearSprite")->click();
    CHECK_EQ(count(screen, 0, 0, 0), 256);
    button("rbLine")->click();
    press(grid, cell(1, 1));
    CHECK_EQ(count(screen, 0, 0, 0), 256);
    drag(grid, cell(10, 10));
    CHECK(screen.sprites[0][0][10][10] == 5 && screen.sprites[0][0][5][5] == 5);
    drag(grid, cell(6, 3));
    release(grid, cell(6, 3));
    const QPoint line[6] = {{1, 1}, {2, 1}, {3, 2}, {4, 2}, {5, 3}, {6, 3}};
    for (const QPoint& at : line)
        CHECK_EQ(screen.sprites[0][0][at.x()][at.y()], 5);
    CHECK_EQ(count(screen, 0, 0, 5), 6);
    // Steeper than it is long, going up, and straight ones.
    button("bpClearSprite")->click();
    press(grid, cell(2, 9));
    drag(grid, cell(4, 4));
    release(grid, cell(4, 4));
    const QPoint steep[6] = {{2, 9}, {2, 8}, {3, 7}, {3, 6}, {4, 5}, {4, 4}};
    for (const QPoint& at : steep)
        CHECK_EQ(screen.sprites[0][0][at.x()][at.y()], 5);
    CHECK_EQ(count(screen, 0, 0, 5), 6);
    button("bpClearSprite")->click();
    press(grid, cell(8, 12));
    drag(grid, cell(8, 3));
    release(grid, cell(8, 3));
    press(grid, cell(12, 15));
    drag(grid, cell(0, 15));
    release(grid, cell(0, 15));
    CHECK_EQ(count(screen, 0, 0, 5), 10 + 13);
    CHECK(screen.sprites[0][0][8][3] == 5 && screen.sprites[0][0][8][12] == 5 && screen.sprites[0][0][0][15] == 5);
    // One that ends where it starts draws nothing.
    click(grid, cell(14, 1));
    CHECK_EQ(count(screen, 0, 0, 5), 23);

    // The filler: all that touches the pixel and has its pen.
    button("bpClearSprite")->click();
    press(grid, cell(8, 0));
    drag(grid, cell(8, 15));
    release(grid, cell(8, 15));
    CHECK_EQ(count(screen, 0, 0, 5), 16);
    pens[9]->click();
    button("rbFill")->click();
    click(grid, cell(2, 7));
    CHECK_EQ(count(screen, 0, 0, 9), 8 * 16);
    CHECK_EQ(count(screen, 0, 0, 0), 7 * 16);
    CHECK(screen.sprites[0][0][7][15] == 9 && screen.sprites[0][0][9][0] == 0);
    CHECK(pens[9]->mark() == PenButton::Mark::Tick && pens[5]->mark() == PenButton::Mark::Tick);
    button("rbPt")->click();
    CHECK(told.isEmpty());

    // ---- A sprite turned and moved ----
    // A sprite whose every pixel tells where it was: its pen is its column
    // on even lines, its line on odd ones.
    const auto mark = [&screen](int bank, int sprite) {
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
                screen.sprites[bank][sprite][x][y] = static_cast<uint8_t>((y & 1) ? y : x);
    };
    const auto marked = [](int x, int y) { return (y & 1) ? y : x; };
    const auto same = [&screen, &marked](int bank, int sprite, int (*fromX)(int, int), int (*fromY)(int, int)) {
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
                if (screen.sprites[bank][sprite][x][y] != marked(fromX(x, y), fromY(x, y)))
                    return false;
        return true;
    };
    mark(0, 0);
    editor->refresh();
    button("bpHorizontalFlip")->click();
    CHECK(same(0, 0, [](int x, int) { return x; }, [](int, int y) { return 15 - y; }));
    button("bpHorizontalFlip")->click();
    button("bpVerticalFlip")->click();
    CHECK(same(0, 0, [](int x, int) { return 15 - x; }, [](int, int y) { return y; }));
    button("bpVerticalFlip")->click();
    // A quarter turn the way a clock's hands go: the top left corner goes to
    // the top right.
    button("bpRotateSprite")->click();
    CHECK(same(0, 0, [](int, int y) { return y; }, [](int x, int) { return 15 - x; }));
    CHECK_EQ(screen.sprites[0][0][15][0], marked(0, 0));
    for (int i = 0; i < 3; ++i)
        button("bpRotateSprite")->click();
    CHECK(same(0, 0, [](int x, int) { return x; }, [](int, int y) { return y; }));
    // A pixel each way: what goes out on a side comes back on the other.
    button("bpScrUp")->click();
    CHECK(same(0, 0, [](int x, int) { return x; }, [](int, int y) { return (y + 1) & 15; }));
    button("bpScrDown")->click();
    button("bpScrDown")->click();
    CHECK(same(0, 0, [](int x, int) { return x; }, [](int, int y) { return (y + 15) & 15; }));
    button("bpScrUp")->click();
    button("bpScrLeft")->click();
    CHECK(same(0, 0, [](int x, int) { return (x + 1) & 15; }, [](int, int y) { return y; }));
    button("bpScrRight")->click();
    button("bpScrRight")->click();
    CHECK(same(0, 0, [](int x, int) { return (x + 15) & 15; }, [](int, int y) { return y; }));
    button("bpScrLeft")->click();
    CHECK(same(0, 0, [](int x, int) { return x; }, [](int, int y) { return y; }));

    // ---- From a sprite to another ----
    // Copied, and pasted in the next one.
    button("bpCopySprite")->click();
    button("bpSuiv")->click();
    CHECK_EQ(editor->sprite(), 1);
    CHECK(label("lblSelSprite")->text() == "Sprite #: 1" && button("bpPrev")->isVisible());
    CHECK_EQ(count(screen, 0, 1, 0), 256);
    button("bpPasteSprite")->click();
    CHECK(same(0, 1, [](int x, int) { return x; }, [](int, int y) { return y; }));
    // A click on a sprite of the row chooses it; the arrows of the keyboard
    // go to the one before and the one after.
    click(strip, QPoint(5 * 64 + 30, 20));
    CHECK_EQ(editor->sprite(), 5);
    QTest::keyClick(grid, Qt::Key_Right);
    CHECK_EQ(editor->sprite(), 6);
    QTest::keyClick(button("bpTest"), Qt::Key_Left);
    QTest::keyClick(editor, Qt::Key_Left);
    CHECK_EQ(editor->sprite(), 4);
    // The boxes that are chosen with the arrows keep them.
    QTest::keyClick(button("rbPt"), Qt::Key_Right);
    CHECK_EQ(editor->sprite(), 4);
    button("rbPt")->click();
    // A right click on a sprite, then on another: they change places.
    screen.sprites[0][7][0][0] = 3;
    editor->refresh();
    click(strip, QPoint(1 * 64 + 10, 10), Qt::RightButton);
    CHECK_EQ(strip->marked(), 1);
    click(strip, QPoint(7 * 64 + 10, 10), Qt::RightButton);
    CHECK_EQ(strip->marked(), -1);
    CHECK(same(0, 7, [](int x, int) { return x; }, [](int, int y) { return y; }));
    CHECK(screen.sprites[0][1][0][0] == 3 && count(screen, 0, 1, 0) == 255);
    CHECK_EQ(editor->sprite(), 4);
    // After the last sprite of a bank comes the first of the next; the
    // last of all has none after it.
    click(strip, QPoint(15 * 64 + 30, 20));
    button("bpSuiv")->click();
    CHECK(editor->bank() == 1 && editor->sprite() == 0 && banks->currentIndex() == 1);
    button("bpPrev")->click();
    CHECK(editor->bank() == 0 && editor->sprite() == 15 && banks->currentIndex() == 0);
    banks->setCurrentIndex(7);
    CHECK(editor->bank() == 7 && editor->sprite() == 15 && !button("bpSuiv")->isVisible());
    QTest::keyClick(grid, Qt::Key_Right);
    CHECK(editor->bank() == 7 && editor->sprite() == 15);

    // A bank copied, pasted in another, and cleared.
    banks->setCurrentIndex(0);
    button("bpCopyBank")->click();
    banks->setCurrentIndex(2);
    button("bpPasteBank")->click();
    for (int sprite = 0; sprite < 16; ++sprite)
        CHECK(spriteOf(screen, 2, sprite) == spriteOf(screen, 0, sprite));
    CHECK(same(2, 7, [](int x, int) { return x; }, [](int, int y) { return y; }));
    banks->setCurrentIndex(0);
    button("bpClearBank")->click();
    for (int sprite = 0; sprite < 16; ++sprite)
        CHECK_EQ(count(screen, 0, sprite, 0), 256);
    CHECK(same(2, 0, [](int x, int) { return x; }, [](int, int y) { return y; }));

    // ---- The palette ----
    // A right click on a pen: its colour is chosen.
    whenAsked("EditColor", [](QDialog* dialog) {
        dialog->findChild<QSpinBox*>("valR")->setValue(9);
        dialog->findChild<QSpinBox*>("valV")->setValue(4);
        dialog->findChild<QSpinBox*>("valB")->setValue(14);
        dialog->accept();
    });
    press(pens[3], QPoint(5, 5), Qt::RightButton);
    CHECK_EQ(screen.spritePalette[3], plusColour(9, 4, 14));
    CHECK(pens[3]->colour() == QColor(9 * 17, 4 * 17, 14 * 17));
    CHECK_EQ(editor->drawingPen(), 9);
    // Given up, it stays.
    whenAsked("EditColor", [](QDialog* dialog) { dialog->reject(); });
    press(pens[3], QPoint(5, 5), Qt::RightButton);
    CHECK_EQ(screen.spritePalette[3], plusColour(9, 4, 14));

    // Reversed: the fifteen colours change ends, the transparent pen stays.
    for (size_t pen = 0; pen < 16; ++pen)
        screen.spritePalette[pen] = static_cast<int>(pen) * 0x101;
    button("bpInversePalette")->click();
    CHECK_EQ(screen.spritePalette[0], 0);
    for (size_t pen = 1; pen < 16; ++pen)
        CHECK_EQ(screen.spritePalette[pen], static_cast<int>(16 - pen) * 0x101);
    CHECK(pens[1]->colour() == spriteColour(15 * 0x101));

    // Shades from a colour to another, from pen 1 to pen 15.
    whenAsked("GenPalette", [&](QDialog* dialog) {
        const auto box = [dialog](const char* name) { return dialog->findChild<QSpinBox*>(name); };
        CHECK(box("txbFrom") && box("txbTo") && box("txbStartR") && box("txbEndB") && dialog->findChild<QPushButton*>("bpGenerate"));
        if (!box("txbFrom") || !box("txbEndB")) {
            dialog->reject();
            return;
        }
        // It starts with the colours of the first pen and of the last.
        CHECK(box("txbFrom")->value() == 1 && box("txbTo")->value() == 15);
        CHECK(box("txbStartR")->value() == 15 && box("txbStartV")->value() == 15 && box("txbStartB")->value() == 0);
        CHECK(box("txbEndR")->value() == 1 && box("txbEndV")->value() == 1 && box("txbEndB")->value() == 0);
        for (const char* name : {"txbStartR", "txbStartV", "txbStartB"})
            box(name)->setValue(0);
        for (const char* name : {"txbEndR", "txbEndV", "txbEndB"})
            box(name)->setValue(15);
        CHECK(dialog->findChild<QSlider*>("trkEndV")->value() == 15);
        CHECK(dialog->findChild<QLabel*>("lblEndColor")->palette().color(QPalette::Window) == QColor(255, 255, 255));
        // The boxes wait for "Generate".
        CHECK_EQ(screen.spritePalette[15], 0x101);
        dialog->findChild<QPushButton*>("bpGenerate")->click();
        const int shades[15] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15};
        for (size_t pen = 1; pen < 16; ++pen)
            CHECK_EQ(screen.spritePalette[pen], shades[pen - 1] * 0x111);
        CHECK(pens[15]->colour() == QColor(255, 255, 255));
        // Two pens of the same colour are told of.
        CHECK(dialog->findChild<QLabel*>("lblError")->text() == "Color 0 is the same as color 1");
        // A slider that moves makes the shades at once.
        dialog->findChild<QSlider*>("trkEndB")->setValue(0);
        CHECK(box("txbEndB")->value() == 0);
        CHECK_EQ(screen.spritePalette[15], plusColour(15, 15, 0));
        // Part of the palette only, from the colour a pen has.
        box("txbFrom")->setValue(8);
        dialog->findChild<QPushButton*>("bpGetCol")->click();
        CHECK(box("txbStartR")->value() == plusRed(screen.spritePalette[8]) && box("txbStartB")->value() == plusBlue(screen.spritePalette[8]));
        const int kept = screen.spritePalette[7];
        box("txbTo")->setValue(10);
        for (const char* name : {"txbEndR", "txbEndV", "txbEndB"})
            box(name)->setValue(1);
        for (const char* name : {"txbStartR", "txbStartV", "txbStartB"})
            box(name)->setValue(3);
        dialog->findChild<QPushButton*>("bpGenerate")->click();
        CHECK(screen.spritePalette[8] == 0x333 && screen.spritePalette[9] == 0x222 && screen.spritePalette[10] == 0x111);
        CHECK(screen.spritePalette[7] == kept && screen.spritePalette[11] != 0x111);
        dialog->reject();
    });
    button("bpGenPal")->click();
    CHECK(editor->findChild<QDialog*>("GenPalette") == nullptr);
    CHECK(told.isEmpty());

    // ---- The test ----
    for (size_t pen = 0; pen < 16; ++pen)
        screen.spritePalette[pen] = plusColour(static_cast<int>(pen), 15 - static_cast<int>(pen), static_cast<int>(pen) / 2);
    for (int sprite = 0; sprite < 16; ++sprite)
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
                screen.sprites[0][sprite][x][y] = static_cast<uint8_t>((x + y + sprite) & 15);
    banks->setCurrentIndex(0);
    click(strip, QPoint(6 * 64 + 30, 20));
    const auto shown = [&](int x, int y) { return QColor(test->image().pixel(x, y)); };
    const auto colourOf = [&screen](int sprite, int x, int y) { return spriteColour(screen.spritePalette[screen.sprites[0][sprite][x][y]]); };
    CHECK(shown(0, 0) == QColor(Qt::black));
    // One sprite, at the Plus's first size: a pixel is 2 across and 4 down.
    button("bpTest")->click();
    CHECK(shown(0, 0) == colourOf(6, 0, 0) && shown(1, 3) == colourOf(6, 0, 0));
    CHECK(shown(2, 0) == colourOf(6, 1, 0) && shown(0, 4) == colourOf(6, 0, 1) && shown(31, 63) == colourOf(6, 15, 15));
    CHECK(shown(32, 0) == QColor(Qt::black) && shown(0, 64) == QColor(Qt::black));
    // Twice as wide, four times as high.
    editor->findChild<QSpinBox*>("zoomX")->setValue(2);
    editor->findChild<QSpinBox*>("zoomY")->setValue(3);
    button("bpTest")->click();
    CHECK(shown(3, 15) == colourOf(6, 0, 0) && shown(4, 16) == colourOf(6, 1, 1) && shown(63, 255) == colourOf(6, 15, 15));
    CHECK(shown(64, 0) == QColor(Qt::black));
    editor->findChild<QSpinBox*>("zoomX")->setValue(1);
    editor->findChild<QSpinBox*>("zoomY")->setValue(1);
    // Four sprites are those of the row of four the sprite is in.
    button("rb2Sprite")->click();
    button("bpTest")->click();
    CHECK(shown(0, 0) == colourOf(4, 0, 0) && shown(32, 0) == colourOf(5, 0, 0) && shown(0, 64) == colourOf(6, 0, 0) && shown(32, 64) == colourOf(7, 0, 0));
    CHECK(shown(64, 0) == QColor(Qt::black));
    // Sixteen are the bank.
    button("rb4Sprite")->click();
    button("bpTest")->click();
    CHECK(shown(0, 0) == colourOf(0, 0, 0) && shown(96, 0) == colourOf(3, 0, 0) && shown(0, 64) == colourOf(4, 0, 0) && shown(96, 192) == colourOf(15, 0, 0));
    // Two blocks of two by four, and two of four by two.
    button("rb42Sprite")->click();
    button("bpTest")->click();
    CHECK(shown(32, 0) == colourOf(1, 0, 0) && shown(0, 192) == colourOf(6, 0, 0) && shown(64, 0) == colourOf(8, 0, 0) && shown(96, 192) == colourOf(15, 0, 0));
    button("rb28sprite")->click();
    button("bpTest")->click();
    CHECK(shown(96, 0) == colourOf(3, 0, 0) && shown(0, 64) == colourOf(4, 0, 0) && shown(128, 0) == colourOf(8, 0, 0) && shown(224, 64) == colourOf(15, 0, 0));
    // "SetPos": a frame follows the mouse, a click puts the sprite there,
    // and the test shows the sprites that were put.
    button("rbPosSpr")->click();
    button("bpTest")->click();
    CHECK(shown(0, 0) == QColor(Qt::black));
    sendMouse(test, QEvent::MouseMove, QPoint(70, 140), Qt::NoButton, Qt::NoButton);
    CHECK(test->frame() == QRect(64, 128, 32, 64));
    click(test, QPoint(70, 140));
    CHECK(shown(64, 128) == colourOf(6, 0, 0) && shown(95, 191) == colourOf(6, 15, 15));
    click(strip, QPoint(2 * 64 + 30, 20));
    click(test, QPoint(200, 10));
    button("bpTest")->click();
    CHECK(shown(64, 128) == colourOf(6, 0, 0) && shown(192, 0) == colourOf(2, 0, 0) && shown(0, 0) == QColor(Qt::black));
    QApplication::sendEvent(test, &leave);
    CHECK(test->frame().isNull());
    button("rb1Sprite")->click();

    // ---- Files ----
    // The bank as a file of Impdraw's: 4096 bytes after the header, a pen
    // for each pixel line after line, and the palette beside it.
    for (int bank = 1; bank < 8; ++bank)
        for (int sprite = 0; sprite < 16; ++sprite)
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 16; ++x)
                    screen.sprites[bank][sprite][x][y] = 0;
    const Screen before = screen;
    answer = scratch.filePath("heros.spr");
    answerFilter = 0;
    button("bpSave")->click();
    CHECK_EQ(askedFilters.size(), 4);
    CHECK(askedFilters.value(0) == "Sprites model (*.spr)" && askedFilters.value(3) == "Assembler file with packed sprites full (*.asm)");
    std::vector<uint8_t> file = fileBytes(answer);
    CHECK_EQ(file.size(), 128 + 4096);
    CHECK(hasAmsdosHeader(file));
    if (file.size() == 128 + 4096) {
        const AmsdosHeader header = readAmsdosHeader(file);
        CHECK(header.name == "HEROS   SPR" && header.address == 0x4000 && header.length == 0x1000 && header.entry == 0);
        CHECK(file[64] == 0x00 && file[65] == 0x10);
        bool right = true;
        for (int sprite = 0; sprite < 16; ++sprite)
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 16; ++x)
                    right = right && file[static_cast<size_t>(128 + sprite * 256 + y * 16 + x)] == screen.sprites[0][sprite][x][y];
        CHECK(right);
    }
    file = fileBytes(scratch.filePath("heros.kit"));
    CHECK_EQ(file.size(), 128 + 30);
    CHECK(hasAmsdosHeader(file));
    if (file.size() == 128 + 30)
        for (size_t pen = 1; pen < 16; ++pen) {
            const int colour = screen.spritePalette[pen];
            CHECK_EQ(file[126 + pen * 2], (plusRed(colour) << 4) | plusBlue(colour));
            CHECK_EQ(file[127 + pen * 2], plusGreen(colour));
        }
    CHECK(told.isEmpty());

    // Read back in another bank, with its palette.
    button("bpClearBank")->click();
    screen.spritePalette.fill(0x123);
    banks->setCurrentIndex(3);
    button("bpRead")->click();
    CHECK_EQ(askedFilters.size(), 1);
    for (int sprite = 0; sprite < 16; ++sprite) {
        CHECK(spriteOf(screen, 3, sprite) == spriteOf(before, 0, sprite));
        CHECK_EQ(count(screen, 0, sprite, 0), 256);
    }
    CHECK_EQ(screen.spritePalette[0], 0);
    for (size_t pen = 1; pen < 16; ++pen)
        CHECK_EQ(screen.spritePalette[pen], before.spritePalette[pen]);
    CHECK(pens[4]->colour() == spriteColour(before.spritePalette[4]));
    CHECK(told.isEmpty());

    // All the banks: as many as go to the last sprite that is not empty.
    // Here they are the first four, the sprites being in the first of the
    // fourth.
    banks->setCurrentIndex(0);
    click(strip, QPoint(30, 20));
    click(grid, cell(4, 4));
    answer = scratch.filePath("all.spr");
    button("chkWithPal")->click();
    button("bpSaveAll")->click();
    file = fileBytes(answer);
    CHECK_EQ(file.size(), 128 + 4 * 4096);
    CHECK(!QFile::exists(scratch.filePath("all.kit")));
    button("chkWithPal")->click();
    if (file.size() == 128 + 4 * 4096) {
        CHECK(readAmsdosHeader(file).length == 0x4000);
        CHECK(file[128 + 4 * 16 + 4] == 9 && file[128 + 3 * 4096 + 15 * 256 + 255] == screen.sprites[3][15][15][15]);
    }
    // Several banks are read from the first on, whichever is shown.
    const Screen fourBanks = screen;
    for (int bank = 0; bank < 4; ++bank)
        for (int sprite = 0; sprite < 16; ++sprite)
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 16; ++x)
                    screen.sprites[bank][sprite][x][y] = 0;
    banks->setCurrentIndex(5);
    button("bpRead")->click();
    for (int bank = 0; bank < 8; ++bank)
        for (int sprite = 0; sprite < 16; ++sprite)
            CHECK(spriteOf(screen, bank, sprite) == spriteOf(fourBanks, bank, sprite));
    // A sprite alone in the first of a bank is saved with its bank.
    Screen lone;
    lone.sprites[1][0][3][3] = 7;
    CHECK(spriteSpan(lone, 0, true).banks == 2 && spriteSpan(lone, 0, true).sprites == 17);
    CHECK(spriteSpan(Screen(), 0, true).banks == 1 && spriteSpan(Screen(), 4, false).firstBank == 4);
    lone.sprites[7][15][0][0] = 1;
    CHECK(spriteSpan(lone, 0, true).banks == 8 && hardSpriteFile("a.spr", lone, spriteSpan(lone, 0, true)).size() == 128 + 0x8000);

    // What is no file of sprites is told of, and nothing changes.
    answer = scratch.filePath("all.asm");
    QFile junk(answer);
    CHECK(junk.open(QIODevice::WriteOnly) && junk.write("; not sprites\n") > 0);
    junk.close();
    button("bpRead")->click();
    CHECK(told.size() == 1 && told.value(0) == "Unable to read sprites.");
    told.clear();
    for (int sprite = 0; sprite < 16; ++sprite)
        CHECK(spriteOf(screen, 3, sprite) == spriteOf(fourBanks, 3, sprite));

    // The sprites as a source: a label before each, sixteen lines of
    // sixteen bytes, up to the last sprite that is not empty, then the
    // palette under its label.
    banks->setCurrentIndex(3);
    for (int sprite = 3; sprite < 16; ++sprite)
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
                screen.sprites[3][sprite][x][y] = 0;
    editor->refresh();
    answer = scratch.filePath("bank4.asm");
    answerFilter = 1;
    whenAsked("SaveMedia", [](QDialog* dialog) {
        CHECK(dialog->windowTitle() == QString::fromUtf8("TuxSCR – Save Sprites (asm format)"));
        auto* media = dialog->findChild<QLineEdit*>("txbLabelMedia");
        auto* colours = dialog->findChild<QLineEdit*>("txbLabelPalette");
        CHECK(media && colours && media->text() == "bank4" && colours->text() == "Palettebank4");
        CHECK(media && media->isVisibleTo(dialog) && colours && colours->isVisibleTo(dialog));
        // The table of addresses and the number of sprites are other
        // formats'.
        CHECK(!dialog->findChild<QWidget*>("txbLabelPtr")->isVisibleTo(dialog) && !dialog->findChild<QWidget*>("chkZeroPtr")->isVisibleTo(dialog));
        CHECK(!dialog->findChild<QWidget*>("numNbMedia")->isVisibleTo(dialog));
        if (media)
            media->setText("Spr");
        dialog->accept();
    });
    button("bpSave")->click();
    QStringList lines = fileLines(answer);
    CHECK(lines.value(0).startsWith("; Généré par TuxSCR ") && lines.value(1) == "; Mode écran - Mode 1");
    {
        QStringList want;
        for (int sprite = 0; sprite < 3; ++sprite) {
            want << QStringLiteral("Spr%1").arg(sprite, 2, 10, QLatin1Char('0'));
            for (int y = 0; y < 16; ++y) {
                QStringList values;
                for (int x = 0; x < 16; ++x)
                    values << QStringLiteral("#%1").arg(screen.sprites[3][sprite][x][y], 2, 16, QLatin1Char('0')).toUpper();
                want << QStringLiteral("\tDB\t") + values.join(',');
            }
        }
        want << "; Taille totale 768 octets"
             << "Palettebank4" << paletteLine(screen) << "";
        CHECK(lines.mid(2) == want);
        if (lines.mid(2) != want)
            std::printf("%s\n", qPrintable(lines.join('\n')));
    }
    // Given up at the labels, nothing is written.
    answer = scratch.filePath("none.asm");
    whenAsked("SaveMedia", [](QDialog* dialog) { dialog->reject(); });
    button("bpSave")->click();
    CHECK(!QFile::exists(answer));

    // Each sprite packed by itself under its label, then the table of their
    // addresses, which may end with a zero; the palette without its label.
    answer = scratch.filePath("packed.asm");
    answerFilter = 2;
    whenAsked("SaveMedia", [](QDialog* dialog) {
        auto* table = dialog->findChild<QLineEdit*>("txbLabelPtr");
        auto* zero = dialog->findChild<QCheckBox*>("chkZeroPtr");
        CHECK(table && table->isVisibleTo(dialog) && table->text() == "packedPtr" && zero && zero->isVisibleTo(dialog) && !zero->isChecked());
        if (zero)
            zero->click();
        dialog->findChild<QCheckBox*>("chkLabelPalette")->click();
        CHECK(!dialog->findChild<QLineEdit*>("txbLabelPalette")->isEnabled());
        dialog->accept();
    });
    button("bpSave")->click();
    lines = fileLines(answer);
    {
        qsizetype at = 2;
        for (int sprite = 0; sprite < 3; ++sprite) {
            CHECK(lines.value(at) == QStringLiteral("packed%1").arg(sprite, 2, 10, QLatin1Char('0')));
            ++at;
            const std::vector<uint8_t> packed = dataBytes(lines, at);
            CHECK(lines.value(at) == QStringLiteral("; Taille totale %1 octets").arg(packed.size()));
            ++at;
            CHECK(document->packMethod == PackMethod::ZX0_V2 && unpackZx0(packed, true) == spriteOf(screen, 3, sprite));
        }
        const QStringList want = {"", "packedPtr", "\tDW\tpacked00,packed01,packed02", "\tDW\t0", "", paletteLine(screen), ""};
        CHECK(lines.mid(at) == want);
        if (lines.mid(at) != want)
            std::printf("%s\n", qPrintable(lines.join('\n')));
    }
    // Four addresses to a line; with the window's other way of packing.
    for (int sprite = 3; sprite < 6; ++sprite)
        screen.sprites[3][sprite][sprite][1] = 2;
    document->packMethod = PackMethod::Standard;
    whenAsked("SaveMedia", [](QDialog* dialog) { dialog->accept(); });
    button("bpSave")->click();
    lines = fileLines(answer);
    {
        qsizetype at = 2;
        for (int sprite = 0; sprite < 6; ++sprite) {
            ++at;
            const std::vector<uint8_t> packed = dataBytes(lines, at);
            ++at;
            CHECK(unpackStandard(packed) == spriteOf(screen, 3, sprite));
        }
        const QStringList want = {"", "packedPtr", "\tDW\tpacked00,packed01,packed02,packed03", "\tDW\tpacked04,packed05", "",
                                  "Palettepacked", paletteLine(screen), ""};
        CHECK(lines.mid(at) == want);
    }
    document->packMethod = PackMethod::ZX0_V2;

    // The sprites of the bank packed as one block: how many is asked, the
    // last that is not empty being offered.
    answer = scratch.filePath("block.asm");
    answerFilter = 3;
    whenAsked("SaveMedia", [](QDialog* dialog) {
        auto* number = dialog->findChild<QSpinBox*>("numNbMedia");
        CHECK(number && number->isVisibleTo(dialog) && number->value() == 6 && number->maximum() == 16);
        CHECK(dialog->findChild<QLabel*>("lblNbMedia")->text() == "Number of sprites to save");
        if (number)
            number->setValue(2);
        dialog->accept();
    });
    button("chkWithPal")->click();
    button("bpSave")->click();
    button("chkWithPal")->click();
    lines = fileLines(answer);
    {
        CHECK(lines.value(2) == "; 2 sprites" && lines.value(3) == "block");
        qsizetype at = 4;
        const std::vector<uint8_t> packed = dataBytes(lines, at);
        std::vector<uint8_t> two = spriteOf(screen, 3, 0);
        const std::vector<uint8_t> second = spriteOf(screen, 3, 1);
        two.insert(two.end(), second.begin(), second.end());
        CHECK(unpackZx0(packed, true) == two);
        // Without the palette, the source ends there.
        const QStringList want = {QStringLiteral("; Taille totale %1 octets").arg(packed.size()), ""};
        CHECK(lines.mid(at) == want);
    }
    // All the banks as one block: all the sprites up to the last.
    whenAsked("SaveMedia", [](QDialog* dialog) {
        CHECK(!dialog->findChild<QWidget*>("numNbMedia")->isVisibleTo(dialog));
        dialog->accept();
    });
    button("bpSaveAll")->click();
    lines = fileLines(answer);
    {
        CHECK(lines.value(2) == "; 54 sprites");
        qsizetype at = 4;
        const std::vector<uint8_t> packed = dataBytes(lines, at);
        const std::vector<uint8_t> all = hardSpriteBytes(screen, spriteSpan(screen, 0, true));
        CHECK(unpackZx0(packed, true) == std::vector<uint8_t>(all.begin(), all.begin() + 54 * 256));
        CHECK(lines.value(at + 1) == "Paletteblock" && lines.value(at + 2) == paletteLine(screen));
    }
    CHECK(told.isEmpty());

    // The palette by itself, saved and read back.
    answer = scratch.filePath("colours.kit");
    answerFilter = 0;
    button("bpSavePal")->click();
    CHECK(askedFilters.value(0) == "CPC+ palette (*.kit)");
    CHECK_EQ(fileBytes(answer).size(), 128 + 30);
    const std::array<int, 16> saved = screen.spritePalette;
    screen.spritePalette.fill(0x456);
    button("bpReadPal")->click();
    CHECK_EQ(screen.spritePalette[0], 0);
    for (size_t pen = 1; pen < 16; ++pen)
        CHECK_EQ(screen.spritePalette[pen], saved[pen]);
    // A picture's palette has sixteen colours: each pen takes its own.
    for (size_t pen = 0; pen < 16; ++pen)
        screen.palette[pen] = 0x800 + static_cast<int>(pen);
    CHECK(document->saveKit(answer, true));
    button("bpReadPal")->click();
    for (size_t pen = 0; pen < 16; ++pen)
        CHECK_EQ(screen.spritePalette[pen], 0x800 + static_cast<int>(pen));
    CHECK(pens[0]->colour() == spriteColour(0x800));
    // What is no palette is told of.
    answer = scratch.filePath("all.asm");
    button("bpReadPal")->click();
    CHECK(told.size() == 1 && told.value(0) == "Unable to read palette.");
    told.clear();
    // A file that cannot be written is told of too.
    answer = scratch.filePath("no/such/folder/x.spr");
    button("bpSave")->click();
    CHECK(told.size() == 1 && told.value(0) == "Unable to save sprites.");
    told.clear();

    // ---- In French, in a window made while TuxSCR's translation is installed ----
    {
        French french;
        CHECK(french.loaded());
        ConverterWindow other(nullptr);
        installSpriteTools(&other);
        other.show();
        other.findChild<QCheckBox*>("modePlus")->click();
        other.findChild<QPushButton*>("bpEditSprites")->click();
        auto* otherEditor = other.findChild<SpriteEditor*>("EditSprites");
        CHECK(otherEditor != nullptr);
        if (otherEditor) {
            const auto frenchButton = [otherEditor](const char* name) { return otherEditor->findChild<QAbstractButton*>(name); };
            const auto frenchLabel = [otherEditor](const char* name) { return otherEditor->findChild<QLabel*>(name); };
            CHECK(otherEditor->windowTitle() == "TuxSCR – Edition Sprites Hard");
            CHECK(frenchLabel("label1")->text() == "Bp gauche" && frenchLabel("label2")->text() == "Bp droite");
            CHECK(frenchButton("bpSave")->text() == "bank courante" && frenchButton("bpSaveAll")->text() == "toutes les banks");
            CHECK(frenchButton("chkWithPal")->text() == "Avec palette" && frenchButton("rbLine")->text() == "Ligne" && frenchButton("bpTest")->text() == "Test !");
            CHECK(frenchLabel("label3")->text() == "Zoom X" && frenchLabel("lblSelSprite")->text().startsWith("Sprite n° : "));
            CHECK(frenchButton("bpCopySprite")->toolTip() == "Copier le sprite");
            if (!prefix.isEmpty())
                CHECK(saveInBothLooks(otherEditor, prefix + "sprites_editor_fr"));
            otherEditor->close();
        }
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(button("bpSave")->text() == "current bank" && button("bpCopySprite")->toolTip() == "Copy the sprite");

    // ---- Pictures of the windows ----
    if (!prefix.isEmpty()) {
        // Something to look at: sprites of all colours, one framed to change
        // places, a pixel's colour framed, and sprites in the test.
        banks->setCurrentIndex(0);
        for (size_t pen = 0; pen < 16; ++pen)
            screen.spritePalette[pen] = plusColour(static_cast<int>((pen * 5) & 15), static_cast<int>((pen * 3 + 2) & 15), static_cast<int>(15 - pen));
        screen.spritePalette[0] = 0;
        for (int sprite = 0; sprite < 16; ++sprite)
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 16; ++x) {
                    const int dx = x - 8, dy = y - 8;
                    screen.sprites[0][sprite][x][y] = static_cast<uint8_t>(dx * dx + dy * dy < 50 ? 1 + ((x / 2 + y / 2 + sprite) % 15) : 0);
                }
        click(strip, QPoint(3 * 64 + 30, 20));
        click(strip, QPoint(9 * 64 + 10, 10), Qt::RightButton);
        sendMouse(grid, QEvent::MouseMove, cell(8, 8), Qt::NoButton, Qt::NoButton);
        button("rb4Sprite")->click();
        button("bpTest")->click();
        button("rbPosSpr")->click();
        sendMouse(test, QEvent::MouseMove, QPoint(300, 300), Qt::NoButton, Qt::NoButton);
        CHECK(saveInBothLooks(editor, prefix + "sprites_editor"));
        click(strip, QPoint(9 * 64 + 10, 10), Qt::RightButton);

        whenAsked("GenPalette", [&prefix](QDialog* dialog) {
            dialog->findChild<QPushButton*>("bpGenerate")->click();
            CHECK(saveInBothLooks(dialog, prefix + "sprites_genpalette"));
            dialog->reject();
        });
        button("bpGenPal")->click();
        answer = scratch.filePath("picture.asm");
        answerFilter = 2;
        whenAsked("SaveMedia", [&prefix](QDialog* dialog) {
            CHECK(saveInBothLooks(dialog, prefix + "sprites_savemedia"));
            dialog->reject();
        });
        button("bpSave")->click();
        answerFilter = 3;
        whenAsked("SaveMedia", [&prefix](QDialog* dialog) {
            CHECK(saveInBothLooks(dialog, prefix + "sprites_savemedia_count"));
            dialog->reject();
        });
        button("bpSave")->click();
    }

    // Closed, the editor keeps what it was showing for the next time.
    editor->close();
    CHECK(!editor->isVisible());
    open->click();
    CHECK(editor->isVisible() && window.findChildren<SpriteEditor*>().size() == 1);
    CHECK(told.isEmpty());
    return checkSummary("gui_sprites");
}
