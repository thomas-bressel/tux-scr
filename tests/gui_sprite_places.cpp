// The hardware sprites shown on the picture, as a user sets them: on a Plus
// in mode 0, 1 or 2 their part is there under the picture; a sprite is given
// a zoom and a place and "Enable" draws it over the picture, its pen 0
// letting the picture show; a right click on the picture puts it elsewhere;
// the places are copied as a source and kept in a file as ConvImgCpc writes
// it. Runs without a display (QT_QPA_PLATFORM=offscreen).
//
//   gui_sprite_places [prefix]   also saves pictures of the window as
//                                <prefix>sprites_places.png and _dark.png

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>

#include "check.h"
#include "french.h"
#include "converterwindow.h"
#include "gui_looks.h"
#include "pictures.h"
#include "pictureview.h"
#include "prompts.h"
#include "spritefiles.h"
#include "spriteplaces.h"
#include "spritetools.h"

using namespace tuxscr;

namespace {

void sendMouse(QWidget* widget, QEvent::Type type, QPoint at, Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent event(type, QPointF(at), QPointF(widget->mapToGlobal(at)), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
}

QByteArray fileText(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
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
    QStringList askedFilters;
    const prompts::FileAnswerer answerer = [&](const QString&, const QStringList& filters, int* chosen) {
        askedFilters = filters;
        if (chosen)
            *chosen = 0;
        return answer;
    };
    prompts::setSaveAnswerer(answerer);
    prompts::setOpenAnswerer(answerer);

    ConverterWindow window(nullptr);
    installSpriteTools(&window);
    window.show();
    Document* document = window.document();
    Screen& screen = document->screen;
    Document::SpritePlaces& places = document->spritePlaces;
    PictureView* view = window.pictureView();
    QTabWidget* tools = window.tools();
    auto* panel = window.findChild<SpritePlacesPanel*>("groupBoxPosSprite");
    auto* plus = window.findChild<QCheckBox*>("modePlus");
    auto* modes = window.findChild<QComboBox*>("mode");
    CHECK(panel && plus && modes);
    if (!panel || !plus || !modes)
        return checkSummary("gui_sprite_places");
    const auto spin = [panel](const char* name) { return panel->findChild<QSpinBox*>(name); };
    auto* enable = panel->findChild<QCheckBox*>("chkAfficheSH");
    auto* copyZoom = panel->findChild<QCheckBox*>("chkCopyZoom");
    auto* copy = panel->findChild<QPushButton*>("bpCopiePosSprites");
    auto* read = panel->findChild<QPushButton*>("bpReadCoord");
    auto* save = panel->findChild<QPushButton*>("bpSaveCoord");
    CHECK(enable && copyZoom && copy && read && save && spin("numSprite") && spin("numZoomX") && spin("numZoomY") && spin("numPosX") && spin("numPosY"));
    if (!enable || !copyZoom || !copy || !read || !save || !spin("numSprite") || !spin("numZoomX") || !spin("numZoomY") || !spin("numPosX") || !spin("numPosY"))
        return checkSummary("gui_sprite_places");

    // The sprites are the Plus's, in the modes the CPC has itself.
    const int tab = tools->indexOf(panel);
    CHECK(tab >= 0 && !tools->isTabVisible(tab));
    plus->click();
    CHECK(screen.plus && screen.mode == kMode1 && tools->isTabVisible(tab));
    modes->setCurrentIndex(kModeEgx1);
    CHECK(!tools->isTabVisible(tab));
    modes->setCurrentIndex(kMode1);
    CHECK(tools->isTabVisible(tab));
    tools->setCurrentIndex(tab);

    // Its texts; the window's own, which have the same names, are theirs.
    CHECK(tools->tabText(tab) == "Hard sprites on the picture");
    CHECK(enable->text() == "Enable" && copyZoom->text() == "Copy zoom to the other sprites" && copy->text() == "Copy positions to the clipboard");
    CHECK(save->text() == "Save" && read->text() == "Read");
    CHECK(panel->findChild<QLabel*>("label3")->text() == "Sprite #" && panel->findChild<QLabel*>("label7")->text() == "Pos X");
    CHECK(panel->findChild<QLabel*>("label5")->text() == "Zoom X" && panel->findChild<QLabel*>("label8")->text() == "Pos Y");
    CHECK(window.settingsPanel()->findChild<QLabel*>("label3")->text() == "Mode");
    CHECK(spin("numSprite")->maximum() == 15 && spin("numZoomX")->maximum() == 3 && spin("numPosX")->maximum() == 1024 && spin("numPosY")->maximum() == 272);

    // A sprite: its left half of a pen, a pixel of another in its last
    // corner, the rest transparent.
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 8; ++x)
            screen.sprites[0][2][x][y] = 3;
    screen.sprites[0][2][15][15] = 12;
    screen.spritePalette[3] = plusColour(15, 0, 0);
    screen.spritePalette[12] = plusColour(0, 0, 15);
    const QColor red(255, 0, 0), blue(0, 0, 255);
    const auto shown = [view](int x, int y) { return view->grab().toImage().pixelColor(x, y); };
    // The picture is of one colour all over.
    const QColor behind = toQColor(screen.rgb(screen.palette[static_cast<size_t>(document->picture().pen(100, 100))]));
    CHECK(behind != red && behind != blue && shown(100, 100) == behind && shown(630, 390) == behind);

    // It has a place and a zoom; it is drawn once "Enable" is ticked.
    spin("numSprite")->setValue(2);
    spin("numZoomX")->setValue(1);
    spin("numZoomY")->setValue(1);
    spin("numPosX")->setValue(100);
    spin("numPosY")->setValue(50);
    CHECK(places.zoomX[2] == 1 && places.zoomY[2] == 1 && places.x[2] == 100 && places.y[2] == 50);
    CHECK(places.zoomX[3] == 0 && places.x[0] == 0);
    CHECK(shown(100, 100) == behind);
    enable->click();
    CHECK(document->showSprites);
    // A pixel of a sprite is one of mode 2 across and a line (two rows)
    // down; pen 0 lets the picture show.
    CHECK(shown(100, 100) == red && shown(107, 101) == red && shown(108, 100) == behind && shown(99, 100) == behind);
    CHECK(shown(100, 99) == behind && shown(100, 131) == red && shown(100, 132) == behind);
    CHECK(shown(115, 130) == blue && shown(115, 131) == blue && shown(114, 130) == behind);
    // Twice as wide; then four times: the ASIC's third size.
    spin("numZoomX")->setValue(2);
    CHECK(shown(115, 100) == red && shown(116, 100) == behind && shown(130, 130) == blue);
    spin("numZoomX")->setValue(3);
    CHECK(shown(131, 100) == red && shown(132, 100) == behind && shown(160, 130) == blue && shown(163, 131) == blue);
    spin("numZoomY")->setValue(3);
    CHECK(shown(100, 100 + 127) == red && shown(100, 100 + 128) == behind && shown(160, 100 + 120) == blue);
    // Zoom 0 hides it.
    spin("numZoomY")->setValue(0);
    CHECK(shown(100, 100) == behind);
    spin("numZoomX")->setValue(1);
    spin("numZoomY")->setValue(1);
    // Past the picture's edge, what does not fit is not drawn.
    spin("numPosX")->setValue(636);
    spin("numPosY")->setValue(195);
    CHECK(shown(639, 399) == red);
    spin("numPosX")->setValue(100);
    spin("numPosY")->setValue(50);

    // The boxes show the sprite that is chosen.
    spin("numSprite")->setValue(5);
    CHECK(spin("numZoomX")->value() == 0 && spin("numPosX")->value() == 0);
    // A zoom may be given to the sprites that follow too.
    copyZoom->click();
    spin("numZoomX")->setValue(2);
    spin("numZoomY")->setValue(1);
    for (int i = 5; i < 16; ++i)
        CHECK(places.zoomX[i] == 2 && places.zoomY[i] == 1);
    CHECK(places.zoomX[4] == 0 && places.zoomX[2] == 1 && places.zoomY[3] == 0);
    // Choosing another sprite gives its zoom to none.
    spin("numSprite")->setValue(2);
    CHECK(spin("numZoomX")->value() == 1 && spin("numPosX")->value() == 100 && spin("numPosY")->value() == 50);
    CHECK(places.zoomX[5] == 2 && places.zoomX[3] == 0);
    copyZoom->click();

    // A right click on the picture puts the chosen sprite there, and the
    // window leaves the size of the source alone.
    auto* userSize = window.findChild<QRadioButton*>("radioUserSize");
    auto* sizeX = window.settingsPanel()->findChild<QSpinBox*>("numSizeX");
    CHECK(userSize && sizeX);
    if (userSize && sizeX) {
        userSize->click();
        const int width = sizeX->value();
        sendMouse(view, QEvent::MouseButtonPress, QPoint(200, 121), Qt::RightButton, Qt::RightButton);
        CHECK(window.mouseOwner() == panel);
        CHECK(spin("numPosX")->value() == 200 && spin("numPosY")->value() == 60 && places.x[2] == 200 && places.y[2] == 60);
        sendMouse(view, QEvent::MouseMove, QPoint(230, 141), Qt::NoButton, Qt::RightButton);
        CHECK(places.x[2] == 230 && places.y[2] == 70);
        sendMouse(view, QEvent::MouseButtonRelease, QPoint(230, 141), Qt::RightButton, Qt::NoButton);
        CHECK(window.mouseOwner() == nullptr);
        CHECK_EQ(sizeX->value(), width);
        CHECK(shown(230, 140) == red);
        // At twice the size, the place is the picture's all the same.
        window.findChild<QCheckBox*>("chkX2")->click();
        sendMouse(view, QEvent::MouseButtonPress, QPoint(301, 203), Qt::RightButton, Qt::RightButton);
        sendMouse(view, QEvent::MouseButtonRelease, QPoint(301, 203), Qt::RightButton, Qt::NoButton);
        CHECK(places.x[2] == 150 && places.y[2] == 50);
        window.findChild<QCheckBox*>("chkX2")->click();
        // With the sprites hidden the right button is the window's again.
        enable->click();
        CHECK(!document->showSprites && shown(150, 100) == behind);
        sendMouse(view, QEvent::MouseButtonPress, QPoint(300, 100), Qt::RightButton, Qt::RightButton);
        CHECK(window.mouseOwner() == nullptr);
        sendMouse(view, QEvent::MouseMove, QPoint(310, 100), Qt::NoButton, Qt::RightButton);
        sendMouse(view, QEvent::MouseButtonRelease, QPoint(310, 100), Qt::RightButton, Qt::NoButton);
        CHECK(places.x[2] == 150 && places.y[2] == 50);
        CHECK_EQ(sizeX->value(), width + 10);
        enable->click();
        window.findChild<QRadioButton*>("radioFit")->click();
    }
    // Another tool that has the picture's mouse keeps it.
    QObject other;
    window.setMouseOwner(&other);
    sendMouse(view, QEvent::MouseButtonPress, QPoint(20, 20), Qt::RightButton, Qt::RightButton);
    sendMouse(view, QEvent::MouseButtonRelease, QPoint(20, 20), Qt::RightButton, Qt::NoButton);
    CHECK(places.x[2] == 150 && window.mouseOwner() == &other);
    window.setMouseOwner(nullptr);

    // The places as a source to assemble: where each sprite is, and the
    // byte of its zoom as the ASIC wants it.
    spin("numSprite")->setValue(15);
    spin("numPosX")->setValue(1024);
    spin("numPosY")->setValue(272);
    spin("numZoomX")->setValue(3);
    copy->click();
    QString text;
    for (int i = 0; i < 16; ++i) {
        const int zoom = places.zoomX[i] * 4 + places.zoomY[i];
        text += QStringLiteral(";\tSprite %1\r\n\tDW\t%2,%3\t\t; position\r\n\tDB\t%4,%4,%4,%4\t\t; zoom\r\n").arg(i).arg(places.x[i]).arg(places.y[i]).arg(zoom);
    }
    CHECK(QApplication::clipboard()->text() == text);
    CHECK(text.contains(";\tSprite 2\r\n\tDW\t150,50\t\t; position\r\n\tDB\t5,5,5,5\t\t; zoom\r\n"));
    CHECK(text.contains(";\tSprite 15\r\n\tDW\t1024,272\t\t; position\r\n\tDB\t13,13,13,13\t\t; zoom\r\n"));
    CHECK(told.size() == 1 && told.value(0) == "Sprite positions copied to the clipboard");
    told.clear();

    // Kept in a file as C#'s serializer writes ConvImgCpc's: the places one
    // number each, the zooms as bytes in base 64.
    answer = scratch.filePath("places.xml");
    save->click();
    CHECK(askedFilters.value(0) == "XML file (*.xml)");
    QByteArray xml = "<?xml version=\"1.0\"?>\r\n"
                     "<PosSprite xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\">\r\n";
    xml += "  <posXSprite>\r\n";
    for (int i = 0; i < 16; ++i)
        xml += "    <int>" + QByteArray::number(places.x[i]) + "</int>\r\n";
    xml += "  </posXSprite>\r\n  <posYSprite>\r\n";
    for (int i = 0; i < 16; ++i)
        xml += "    <int>" + QByteArray::number(places.y[i]) + "</int>\r\n";
    xml += "  </posYSprite>\r\n";
    xml += "  <zoomXSprite>AAABAAACAgICAgICAgICAw==</zoomXSprite>\r\n";
    xml += "  <zoomYSprite>AAABAAABAQEBAQEBAQEBAQ==</zoomYSprite>\r\n";
    xml += "</PosSprite>";
    CHECK(fileText(answer) == xml);
    if (fileText(answer) != xml)
        std::printf("%s\n", fileText(answer).constData());
    // Read back.
    const Document::SpritePlaces kept = places;
    places = Document::SpritePlaces();
    spin("numSprite")->setValue(2);
    CHECK(spin("numPosX")->value() == 0 && shown(150, 100) == behind);
    read->click();
    for (int i = 0; i < 16; ++i)
        CHECK(places.x[i] == kept.x[i] && places.y[i] == kept.y[i] && places.zoomX[i] == kept.zoomX[i] && places.zoomY[i] == kept.zoomY[i]);
    CHECK(spin("numPosX")->value() == 150 && spin("numPosY")->value() == 50 && spin("numZoomX")->value() == 1);
    CHECK(shown(150, 100) == red);
    // A file of ConvImgCpc's own, as it comes from Windows.
    answer = scratch.filePath("convimgcpc.xml");
    {
        QFile file(answer);
        CHECK(file.open(QIODevice::WriteOnly));
        file.write("\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n<PosSprite xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
                   "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\r\n  <posXSprite>\r\n    <int>12</int>\r\n    <int>640</int>\r\n"
                   "  </posXSprite>\r\n  <posYSprite>\r\n    <int>7</int>\r\n  </posYSprite>\r\n"
                   "  <zoomXSprite>AwI=</zoomXSprite>\r\n  <zoomYSprite>AQ==</zoomYSprite>\r\n</PosSprite>");
    }
    read->click();
    CHECK(places.x[0] == 12 && places.x[1] == 640 && places.x[2] == 0 && places.y[0] == 7 && places.y[1] == 0);
    CHECK(places.zoomX[0] == 3 && places.zoomX[1] == 2 && places.zoomX[2] == 0 && places.zoomY[0] == 1 && places.zoomY[15] == 0);
    CHECK(told.isEmpty());
    // What is no such file is told of, and the places stay.
    answer = scratch.filePath("other.xml");
    {
        QFile file(answer);
        CHECK(file.open(QIODevice::WriteOnly));
        file.write("<?xml version=\"1.0\"?>\r\n<Param>\r\n  <posXSprite>\r\n    <int>99</int>\r\n  </posXSprite>\r\n</Param>");
    }
    read->click();
    CHECK(told.size() == 1 && told.value(0) == "The positions could not be read.");
    CHECK(places.x[0] == 12);
    told.clear();
    answer = scratch.filePath("no/such/folder/places.xml");
    save->click();
    CHECK(told.size() == 1 && told.value(0) == "The positions could not be saved.");
    told.clear();
    // Given up, nothing is asked further.
    answer.clear();
    save->click();
    read->click();
    CHECK(told.isEmpty());
    places = kept;
    document->touch();

    // A file that is read hides the sprites: the box follows.
    CHECK(enable->isChecked());
    document->showSprites = false;
    document->touch();
    CHECK(!enable->isChecked() && shown(150, 100) == behind);
    enable->click();
    CHECK(shown(150, 100) == red);

    // In French, in a window made while TuxSCR's translation is installed.
    {
        French french;
        CHECK(french.loaded());
        ConverterWindow other(nullptr);
        installSpriteTools(&other);
        other.show();
        other.findChild<QCheckBox*>("modePlus")->click();
        auto* otherPanel = other.findChild<SpritePlacesPanel*>("groupBoxPosSprite");
        CHECK(otherPanel != nullptr);
        if (otherPanel) {
            other.tools()->setCurrentIndex(other.tools()->indexOf(otherPanel));
            CHECK(other.tools()->tabText(other.tools()->indexOf(otherPanel)) == "Affichage sprites hard sur image");
            CHECK(otherPanel->findChild<QCheckBox*>("chkAfficheSH")->text() == "Activer");
            CHECK(otherPanel->findChild<QCheckBox*>("chkCopyZoom")->text() == "Recopier zoom aux autres sprites");
            CHECK(otherPanel->findChild<QPushButton*>("bpCopiePosSprites")->text() == "Copier positions dans presse-papier");
            CHECK(otherPanel->findChild<QLabel*>("label3")->text() == "N°Sprite" && otherPanel->findChild<QLabel*>("label7")->text() == "Pos X");
            CHECK(other.settingsPanel()->findChild<QLabel*>("label3")->text() == "Mode");
            otherPanel->findChild<QPushButton*>("bpCopiePosSprites")->click();
            CHECK(told.size() == 1 && told.value(0) == "Position des sprites copié(es) dans le presse-papier");
            told.clear();
            if (!prefix.isEmpty())
                CHECK(saveInBothLooks(&other, prefix + "sprites_places_fr"));
        }
    }

    if (!prefix.isEmpty()) {
        // Several sprites, of several sizes.
        for (int sprite = 0; sprite < 16; ++sprite)
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 16; ++x) {
                    const int dx = x - 8, dy = y - 8;
                    screen.sprites[0][sprite][x][y] = static_cast<uint8_t>(dx * dx + dy * dy < 56 ? 1 + ((x + y + sprite) % 15) : 0);
                }
        for (size_t pen = 1; pen < 16; ++pen)
            screen.spritePalette[pen] = plusColour(static_cast<int>((pen * 5) & 15), static_cast<int>((pen * 3 + 2) & 15), static_cast<int>(15 - pen));
        for (int sprite = 0; sprite < 16; ++sprite) {
            places.x[sprite] = 20 + (sprite % 8) * 76;
            places.y[sprite] = 20 + (sprite / 8) * 90;
            places.zoomX[sprite] = 1 + sprite % 3;
            places.zoomY[sprite] = 1 + (sprite / 3) % 3;
        }
        document->touch();
        CHECK(saveInBothLooks(&window, prefix + "sprites_places"));
    }

    // In another mode the sprites are not drawn, and their part goes.
    modes->setCurrentIndex(kModeEgx1);
    CHECK(!tools->isTabVisible(tab));
    CHECK(shown(places.x[2] + 2, places.y[2] * 2 + 2) != red);
    CHECK(told.isEmpty());
    return checkSummary("gui_sprite_places");
}
