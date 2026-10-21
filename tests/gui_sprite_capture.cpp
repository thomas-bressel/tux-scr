// Hardware sprites captured from the picture, as a user does it: with the
// mode "Capture Sprites" its two boxes are there under the picture; the
// grid is shown; "Sprites capture" opens the capture window and gives the
// picture's mouse to the capture, whose rectangle follows it and the arrow
// keys; a click shows what the rectangle holds, and "Capture" makes sprites
// of it, one or several, with the picture's palette. Runs without a display
// (QT_QPA_PLATFORM=offscreen).
//
//   gui_sprite_capture [prefix]   also saves pictures of the windows as
//                                 <prefix>sprites_capture.png and so on

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>

#include "check.h"
#include "english.h"
#include "french.h"
#include "converterwindow.h"
#include "gui_looks.h"
#include "pictures.h"
#include "pictureview.h"
#include "prompts.h"
#include "spritecapture.h"
#include "spriteeditor.h"
#include "spritefiles.h"
#include "spritetools.h"
#include "spriteviews.h"

using namespace tuxscr;

namespace {

// The pen the test's picture has at a place of it: bands that slant, so that
// no two sprites of it are the same.
int penAt(int x, int y)
{
    return ((x / 2) / 3 + (y / 2) / 5 + ((x / 2) * (y / 2)) / 97) & 15;
}

// Whether a sprite is what the picture has from (left, top) on, each pen
// being the first of its colour.
bool spriteIs(const Screen& screen, int bank, int sprite, int left, int top, int (*first)(int))
{
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            if (screen.sprites[bank][sprite][x][y] != first(penAt(left + x * 2, top + y * 2)))
                return false;
    return true;
}

bool spriteEmpty(const Screen& screen, int bank, int sprite)
{
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            if (screen.sprites[bank][sprite][x][y] != 0)
                return false;
    return true;
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    const English english;
    const QString prefix = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    QStringList told;
    prompts::setProblemHandler([&told](const QString& text) { told << text; });

    ConverterWindow window(nullptr);
    installSpriteTools(&window);
    window.show();
    Document* document = window.document();
    Screen& screen = document->screen;
    PictureView* view = window.pictureView();
    QTabWidget* tools = window.tools();
    auto* panel = window.findChild<SpriteCapturePanel*>("captureSprites");
    auto* plus = window.findChild<QCheckBox*>("modePlus");
    auto* modes = window.findChild<QComboBox*>("mode");
    CHECK(panel && plus && modes);
    if (!panel || !plus || !modes)
        return checkSummary("gui_sprite_capture");
    auto* capture = panel->findChild<QCheckBox*>("modeCaptureSprites");
    auto* grid = panel->findChild<QCheckBox*>("chkGrilleSprite");
    CHECK(capture && grid);
    if (!capture || !grid)
        return checkSummary("gui_sprite_capture");
    QStringList said;
    QObject::connect(document, &Document::said, &app, [&said](const QString& text) { said << text; });

    // The capture's boxes are the mode's: not there until it is chosen,
    // which a Plus only can do.
    const int tab = tools->indexOf(panel);
    CHECK(tab >= 0 && !tools->isTabVisible(tab));
    CHECK_EQ(modes->count(), kModeCount - 1);
    plus->click();
    CHECK_EQ(modes->count(), kModeCount);
    CHECK(!tools->isTabVisible(tab));
    modes->setCurrentIndex(kModeCaptureSprites);
    CHECK_EQ(screen.mode, kModeCaptureSprites);
    CHECK(tools->isTabVisible(tab) && tools->currentWidget() == panel);
    CHECK(tools->tabText(tab) == "Sprites capture");
    CHECK(capture->text() == "Sprites capture" && grid->text() == "Sprites grid");
    CHECK(!capture->isChecked() && !grid->isChecked());

    // A picture of sixteen colours as wide as mode 1's, two of its pens
    // having the same colour.
    for (size_t pen = 0; pen < 16; ++pen)
        screen.palette[pen] = plusColour(static_cast<int>(pen), static_cast<int>(15 - pen), static_cast<int>((pen * 7) & 15));
    screen.palette[11] = screen.palette[4];
    const auto first = [](int pen) { return pen == 11 ? 4 : pen; };
    Picture& picture = document->picture();
    for (int y = 0; y < screen.height(); y += 2)
        for (int x = 0; x < screen.width(); x += 2)
            picture.setPixel(x, y, penAt(x, y), 2);
    picture.computed = true;
    document->touch();
    CHECK_EQ(screen.width(), 640);
    CHECK_EQ(picture.pen(100, 50), penAt(100, 50));
    const QColor plain = view->grab().toImage().pixelColor(64, 10);

    // The grid: a line every 32 pixels, across and down, in two tones that
    // show on any colour. While it is shown the settings are out of reach.
    CHECK(window.settingsPanel()->isEnabled());
    grid->click();
    CHECK(!window.settingsPanel()->isEnabled());
    {
        const QImage shown = view->grab().toImage();
        bool lines = true;
        for (const QPoint& at : {QPoint(64, 10), QPoint(64, 14), QPoint(10, 96), QPoint(14, 96), QPoint(608, 390)})
            lines = lines && (shown.pixelColor(at) == QColor(Qt::white) || shown.pixelColor(at) == QColor(Qt::black));
        CHECK(lines);
        CHECK(shown.pixelColor(64, 10) != shown.pixelColor(64, 14));
        CHECK(shown.pixelColor(70, 10) == toQColor(screen.rgb(screen.palette[static_cast<size_t>(penAt(70, 10))])));
    }
    grid->click();
    CHECK(window.settingsPanel()->isEnabled());
    CHECK(view->grab().toImage().pixelColor(64, 10) == plain);

    // The sprite editor, which stays open beside the capture.
    window.findChild<QPushButton*>("bpEditSprites")->click();
    auto* editor = window.findChild<SpriteEditor*>("EditSprites");
    CHECK(editor && editor->isVisible());
    if (!editor)
        return checkSummary("gui_sprite_capture");
    auto* strip = editor->findChild<SpriteStrip*>("pictAllSprites");
    const QColor empty = strip->grab().toImage().pixelColor(5 * 64 + 2, 2);

    // "Sprites capture": the capture window, and the picture's mouse.
    CHECK(panel->captureWindow() == nullptr && window.mouseOwner() == nullptr);
    capture->click();
    SpriteCaptureWindow* box = panel->captureWindow();
    CHECK(box && box->isVisible() && !box->isModal() && box->objectName() == "Capture");
    if (!box)
        return checkSummary("gui_sprite_capture");
    CHECK(window.mouseOwner() == panel && !window.settingsPanel()->isEnabled());
    CHECK(box->windowTitle() == QString::fromUtf8("TuxSCR – Hard sprites capture"));
    const auto radio = [&box](const char* name) { return box->findChild<QRadioButton*>(name); };
    auto* preview = box->findChild<SpriteCaptureView*>("pictCapture");
    auto* number = box->findChild<QSpinBox*>("numSprite");
    auto* bank = box->findChild<QComboBox*>("comboBanque");
    auto* store = box->findChild<QPushButton*>("bpCapture");
    auto* automatic = box->findChild<QCheckBox*>("chkAutoCapture");
    auto* across = box->findChild<QSpinBox*>("txbNbX");
    auto* down = box->findChild<QSpinBox*>("txbNbY");
    CHECK(preview && number && bank && store && automatic && across && down && radio("rbCapt1") && radio("rbCapt2") && radio("rbCapt4") && radio("rbCaptUser"));
    if (!preview || !number || !bank || !store || !automatic || !across || !down || !radio("rbCaptUser"))
        return checkSummary("gui_sprite_capture");
    CHECK(radio("rbCapt1")->text() == "Capture 1 sprite" && radio("rbCapt1")->isChecked() && radio("rbCaptUser")->text() == "Choose nb sprites");
    CHECK(store->text() == "Capture" && automatic->text() == "Auto capture" && box->findChild<QLabel*>("label1")->text() == "Sprite #");
    CHECK(bank->count() == 8 && bank->itemText(0) == "Bank 1" && number->value() == 0 && number->maximum() == 15);
    CHECK(!across->isVisibleTo(box) && !down->isVisibleTo(box));
    CHECK(box->across() == 1 && box->down() == 1 && preview->pen(0, 0) == SpriteCaptureView::kNothing);

    // The rectangle follows the mouse, its corner on a pixel of mode 1 and
    // on a line: a sprite is 32 by 32 of the picture's pixels.
    CHECK(panel->corner() == QPoint(-1, -1));
    emit view->mouse(QPoint(101, 51), Qt::NoButton);
    CHECK(panel->corner() == QPoint(100, 50));
    {
        const QImage shown = view->grab().toImage();
        const auto mark = [&shown](int x, int y) { return shown.pixelColor(x, y) == QColor(Qt::white) || shown.pixelColor(x, y) == QColor(Qt::black); };
        CHECK(mark(100, 50) && mark(110, 50) && mark(100, 70) && mark(131, 60) && mark(120, 81));
        CHECK(!mark(110, 60) && !mark(140, 60) && !mark(96, 50));
    }
    // Moving shows nothing; a click shows what the rectangle holds.
    CHECK_EQ(preview->pen(0, 0), SpriteCaptureView::kNothing);
    emit view->mouse(QPoint(101, 51), Qt::LeftButton);
    emit view->mouse(QPoint(101, 51), Qt::NoButton);
    bool right = true;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            right = right && preview->pen(x, y) == penAt(100 + x * 2, 50 + y * 2);
    CHECK(right);
    CHECK(said.value(said.size() - 1) == "Sprite capture x:100, y:50");
    CHECK(preview->grab().toImage().pixelColor(3, 3) == toQColor(screen.rgb(screen.palette[static_cast<size_t>(penAt(100, 50))])));
    CHECK(preview->grab().toImage().pixelColor(8 * 15 + 4, 8 * 15 + 4) == toQColor(screen.rgb(screen.palette[static_cast<size_t>(penAt(130, 80))])));
    CHECK(preview->grab().toImage().pixelColor(8 * 16 + 4, 4) == QColor(Qt::black));
    // Nothing is a sprite until "Capture" says so: the sprite the window
    // names takes the pens, the sprites take the picture's palette, and the
    // next sprite is named.
    CHECK(spriteEmpty(screen, 0, 0));
    screen.spritePalette.fill(0);
    store->click();
    CHECK(spriteIs(screen, 0, 0, 100, 50, first));
    CHECK(spriteEmpty(screen, 0, 1));
    for (size_t pen = 0; pen < 16; ++pen)
        CHECK_EQ(screen.spritePalette[pen], screen.palette[pen]);
    CHECK(number->value() == 1 && bank->currentIndex() == 0);
    CHECK(said.value(said.size() - 1) == "Sprite #0 copied, bank 1");

    // With "Auto capture" a click makes the sprite at once; held and
    // dragged, it shows what the rectangle passes over without making more.
    automatic->click();
    emit view->mouse(QPoint(200, 120), Qt::NoButton);
    emit view->mouse(QPoint(200, 120), Qt::LeftButton);
    CHECK(spriteIs(screen, 0, 1, 200, 120, first) && number->value() == 2);
    emit view->mouse(QPoint(240, 130), Qt::LeftButton);
    emit view->mouse(QPoint(260, 140), Qt::LeftButton);
    CHECK(number->value() == 2 && spriteEmpty(screen, 0, 2));
    CHECK_EQ(preview->pen(5, 5), penAt(240 + 10, 130 + 10));
    emit view->mouse(QPoint(260, 140), Qt::NoButton);
    automatic->click();

    // Four sprites at once: two across and two down, which start a row of
    // four at the latest.
    radio("rbCapt2")->click();
    CHECK(box->across() == 2 && box->down() == 2 && number->maximum() == 12 && number->value() == 2);
    CHECK(preview->across() == 2 && preview->pen(0, 0) == SpriteCaptureView::kNothing);
    number->setValue(12);
    emit view->mouse(QPoint(320, 64), Qt::NoButton);
    {
        const QImage shown = view->grab().toImage();
        CHECK(shown.pixelColor(383, 100) == QColor(Qt::white) || shown.pixelColor(383, 100) == QColor(Qt::black));
    }
    emit view->mouse(QPoint(320, 64), Qt::LeftButton);
    emit view->mouse(QPoint(320, 64), Qt::NoButton);
    store->click();
    CHECK(spriteIs(screen, 0, 12, 320, 64, first) && spriteIs(screen, 0, 13, 352, 64, first));
    CHECK(spriteIs(screen, 0, 14, 320, 96, first) && spriteIs(screen, 0, 15, 352, 96, first));
    // They filled the bank: the next sprites go to the bank after it.
    CHECK(number->value() == 0 && bank->currentIndex() == 1);

    // Sixteen: a whole bank.
    radio("rbCapt4")->click();
    CHECK(box->across() == 4 && box->down() == 4 && number->maximum() == 0);
    emit view->mouse(QPoint(0, 0), Qt::NoButton);
    emit view->mouse(QPoint(0, 0), Qt::LeftButton);
    emit view->mouse(QPoint(0, 0), Qt::NoButton);
    CHECK(preview->grab().toImage().pixelColor(511, 511) == toQColor(screen.rgb(screen.palette[static_cast<size_t>(penAt(126, 126))])));
    store->click();
    CHECK(spriteIs(screen, 1, 0, 0, 0, first) && spriteIs(screen, 1, 5, 32, 32, first) && spriteIs(screen, 1, 15, 96, 96, first));
    CHECK(number->value() == 0 && bank->currentIndex() == 2);

    // As many as wanted across and down, if the banks have room for them.
    radio("rbCaptUser")->click();
    CHECK(across->isVisibleTo(box) && down->isVisibleTo(box) && box->across() == 1 && box->down() == 1);
    across->setValue(3);
    down->setValue(2);
    CHECK(box->across() == 3 && box->down() == 2 && number->maximum() == 10);
    // In the last bank there is room for sixteen: a box that asks for more
    // is put back.
    bank->setCurrentIndex(7);
    across->setValue(16);
    CHECK(box->across() == 3 && across->value() == 3);
    across->setValue(8);
    CHECK(box->across() == 8 && number->maximum() == 0);
    down->setValue(3);
    CHECK(box->down() == 2 && down->value() == 2);
    bank->setCurrentIndex(2);
    across->setValue(3);
    number->setValue(10);
    // Past the right of the picture there is pen 0.
    emit view->mouse(QPoint(580, 300), Qt::NoButton);
    emit view->mouse(QPoint(580, 300), Qt::LeftButton);
    emit view->mouse(QPoint(580, 300), Qt::NoButton);
    store->click();
    CHECK(spriteIs(screen, 2, 10, 580, 300, first) && spriteIs(screen, 2, 13, 580, 332, first));
    CHECK(screen.sprites[2][11][13][0] == first(penAt(580 + 32 + 26, 300)) && screen.sprites[2][11][14][0] == 0 && spriteEmpty(screen, 2, 12));
    CHECK(number->value() == 0 && bank->currentIndex() == 3);

    // The arrow keys move the rectangle a pixel of mode 1 or a line at a
    // time, and Space shows what it holds.
    radio("rbCapt1")->click();
    emit view->mouse(QPoint(40, 40), Qt::NoButton);
    QTest::keyClick(view, Qt::Key_Right);
    QTest::keyClick(view, Qt::Key_Down);
    QTest::keyClick(view, Qt::Key_Down);
    CHECK(panel->corner() == QPoint(42, 44));
    QTest::keyClick(view, Qt::Key_Left);
    QTest::keyClick(view, Qt::Key_Up);
    CHECK(panel->corner() == QPoint(40, 42));
    CHECK_EQ(preview->pen(0, 0), SpriteCaptureView::kNothing);
    QTest::keyClick(view, Qt::Key_Space);
    CHECK_EQ(preview->pen(15, 15), penAt(40 + 30, 42 + 30));
    for (int i = 0; i < 30; ++i)
        QTest::keyClick(view, Qt::Key_Left);
    CHECK(panel->corner() == QPoint(0, 42));
    // Out of the picture, the rectangle is gone; a key brings it back.
    emit view->mouseLeft();
    CHECK(panel->corner() == QPoint(-1, -1));
    QTest::keyClick(view, Qt::Key_Space);
    CHECK(panel->corner() == QPoint(0, 0));

    // After the last sprite of the last bank there is no room: what does
    // not fit is left out, and said.
    radio("rbCapt2")->click();
    bank->setCurrentIndex(7);
    number->setMaximum(15);
    number->setValue(15);
    emit view->mouse(QPoint(64, 200), Qt::NoButton);
    emit view->mouse(QPoint(64, 200), Qt::LeftButton);
    emit view->mouse(QPoint(64, 200), Qt::NoButton);
    store->click();
    CHECK(spriteIs(screen, 7, 15, 64, 200, first));
    CHECK(number->value() == 15 && bank->currentIndex() == 7);
    CHECK(said.value(said.size() - 1).startsWith("There is no room after bank 8"));
    CHECK(told.isEmpty());

    // The sprite editor shows what was captured.
    radio("rbCapt1")->click();
    bank->setCurrentIndex(0);
    number->setValue(5);
    emit view->mouse(QPoint(300, 300), Qt::NoButton);
    emit view->mouse(QPoint(300, 300), Qt::LeftButton);
    emit view->mouse(QPoint(300, 300), Qt::NoButton);
    store->click();
    {
        const QColor now = strip->grab().toImage().pixelColor(5 * 64 + 2, 2);
        CHECK(now == spriteColour(screen.palette[static_cast<size_t>(penAt(300, 300))]) && now != empty);
    }
    editor->close();

    if (!prefix.isEmpty()) {
        radio("rbCapt4")->click();
        grid->click();
        emit view->mouse(QPoint(96, 64), Qt::NoButton);
        emit view->mouse(QPoint(96, 64), Qt::LeftButton);
        emit view->mouse(QPoint(96, 64), Qt::NoButton);
        CHECK(saveInBothLooks(box, prefix + "sprites_capture"));
        CHECK(saveInBothLooks(&window, prefix + "sprites_capture_picture"));
        radio("rbCaptUser")->click();
        bank->setCurrentIndex(0);
        across->setValue(6);
        down->setValue(3);
        emit view->mouse(QPoint(32, 32), Qt::NoButton);
        emit view->mouse(QPoint(32, 32), Qt::LeftButton);
        emit view->mouse(QPoint(32, 32), Qt::NoButton);
        CHECK(saveInBothLooks(box, prefix + "sprites_capture_user"));
        grid->click();
    }

    // Another tool that has the picture's mouse keeps it: the box does not
    // stay ticked.
    capture->click();
    CHECK(!capture->isChecked() && panel->captureWindow() == nullptr && window.mouseOwner() == nullptr && window.settingsPanel()->isEnabled());
    QObject other;
    window.setMouseOwner(&other);
    capture->click();
    CHECK(!capture->isChecked() && panel->captureWindow() == nullptr && window.mouseOwner() == &other);
    CHECK(said.value(said.size() - 1) == "Another tool is using the picture's mouse.");
    window.setMouseOwner(nullptr);

    // The capture window closed, the box is unticked and the mouse given
    // back.
    capture->click();
    CHECK(capture->isChecked() && panel->captureWindow() && window.mouseOwner() == panel);
    panel->captureWindow()->close();
    CHECK(!capture->isChecked() && window.mouseOwner() == nullptr && window.settingsPanel()->isEnabled());
    emit view->mouse(QPoint(50, 50), Qt::NoButton);
    CHECK(panel->corner() == QPoint(-1, -1));

    // The converter's window closed, the capture ends and the sprites' own
    // windows go with it.
    capture->click();
    editor->show();
    CHECK(panel->captureWindow() && panel->captureWindow()->isVisible() && editor->isVisible());
    window.close();
    CHECK(!capture->isChecked() && window.mouseOwner() == nullptr && window.settingsPanel()->isEnabled() && !editor->isVisible());
    window.show();
    CHECK(!editor->isVisible() && !capture->isChecked());

    // The mode left while capturing (a file that is read says another):
    // the capture ends, and its boxes go.
    capture->click();
    grid->click();
    CHECK(capture->isChecked() && grid->isChecked() && !window.settingsPanel()->isEnabled());
    screen.mode = kMode1;
    document->touch();
    CHECK(!capture->isChecked() && !grid->isChecked() && window.mouseOwner() == nullptr && window.settingsPanel()->isEnabled());
    CHECK(!tools->isTabVisible(tab));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(panel->captureWindow() == nullptr && window.findChild<QDialog*>("Capture") == nullptr);
    CHECK(told.isEmpty());

    // In French, in a window made while TuxSCR's translation is installed.
    {
        French french;
        CHECK(french.loaded());
        ConverterWindow other(nullptr);
        installSpriteTools(&other);
        other.show();
        other.findChild<QCheckBox*>("modePlus")->click();
        other.findChild<QComboBox*>("mode")->setCurrentIndex(kModeCaptureSprites);
        auto* otherPanel = other.findChild<SpriteCapturePanel*>("captureSprites");
        auto* otherCapture = otherPanel ? otherPanel->findChild<QCheckBox*>("modeCaptureSprites") : nullptr;
        CHECK(otherPanel && otherCapture);
        if (otherPanel && otherCapture) {
            CHECK(otherCapture->text() == "Capture de sprites" && other.tools()->tabText(other.tools()->indexOf(otherPanel)) == "Capture de sprites");
            otherCapture->click();
            SpriteCaptureWindow* otherBox = otherPanel->captureWindow();
            CHECK(otherBox != nullptr);
            if (otherBox) {
                CHECK(otherBox->windowTitle() == "TuxSCR – Capture sprites hard");
                CHECK(otherBox->findChild<QRadioButton*>("rbCaptUser")->text() == "Choisir nb sprites");
                CHECK(otherBox->findChild<QPushButton*>("bpCapture")->text() == "Capturer");
                if (!prefix.isEmpty())
                    CHECK(saveInBothLooks(otherBox, prefix + "sprites_capture_fr"));
            }
            otherCapture->click();
        }
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    // A window that goes while sprites are captured takes it all with it.
    {
        ConverterWindow second(nullptr);
        installSpriteTools(&second);
        second.show();
        second.findChild<QCheckBox*>("modePlus")->click();
        second.findChild<QComboBox*>("mode")->setCurrentIndex(kModeCaptureSprites);
        second.findChild<QPushButton*>("bpEditSprites")->click();
        second.findChild<QCheckBox*>("chkGrilleSprite")->click();
        second.findChild<QCheckBox*>("modeCaptureSprites")->click();
        CHECK(second.mouseOwner() != nullptr && second.findChild<QDialog*>("Capture") && second.findChild<QDialog*>("EditSprites"));
        emit second.pictureView()->mouse(QPoint(10, 10), Qt::NoButton);
    }
    return checkSummary("gui_sprite_capture");
}
