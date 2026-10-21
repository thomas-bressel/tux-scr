#include "spritetexts.h"

#include <QRegularExpression>

#include "texts.h"

namespace tuxscr {

namespace {

const texts::WindowText kSpriteTexts[] = {
    // ---- The sprite editor ----
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "sprites", "EditSprites.bpRead"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "palette", "EditSprites.bpReadPal"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "palette", "EditSprites.bpSavePal"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Bank", "EditSprites.bpCopyBank"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Bank", "EditSprites.bpPasteBank"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Bank", "EditSprites.bpClearBank"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Make Palette", "EditSprites.bpGenPal"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Reverse Palette", "EditSprites.bpInversePalette"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Editing", "EditSprites.grpEdition"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Point", "EditSprites.rbPt"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Line", "EditSprites.rbLine"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Fill", "EditSprites.rbFill"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "1", "EditSprites.rb1Sprite"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "2x2", "EditSprites.rb2Sprite"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "4x4", "EditSprites.rb4Sprite"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "2x2x4", "EditSprites.rb42Sprite"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "2x8", "EditSprites.rb28sprite"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "SetPos", "EditSprites.rbPosSpr"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Zoom X", "EditSprites.label3"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Zoom Y", "EditSprites.label4"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Test!", "EditSprites.bpTest"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Sprite #: ", "EditSprites.selSprite"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Bank ", "EditSprites.bank"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "CPC+ palette (*.kit)", "EditSprites.paletteFilter"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Previous sprite (left arrow)", "EditSprites.tipPrev"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Next sprite (right arrow)", "EditSprites.tipSuiv"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Bank of 16 sprites being edited", "EditSprites.tipBank"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Copy the sprite", "EditSprites.tipCopySprite"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Paste the sprite that was copied", "EditSprites.tipPasteSprite"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Flip the sprite top to bottom", "EditSprites.tipHorizontalFlip"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Flip the sprite left to right", "EditSprites.tipVerticalFlip"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Turn the sprite a quarter turn", "EditSprites.tipRotateSprite"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Clear the sprite", "EditSprites.tipClearSprite"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Move the sprite up one pixel", "EditSprites.tipScrUp"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Move the sprite down one pixel", "EditSprites.tipScrDown"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Move the sprite left one pixel", "EditSprites.tipScrLeft"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Move the sprite right one pixel", "EditSprites.tipScrRight"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Copy the 16 sprites of the bank", "EditSprites.tipCopyBank"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Paste the 16 sprites that were copied", "EditSprites.tipPasteBank"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Clear the 16 sprites of the bank", "EditSprites.tipClearBank"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Read sprites (.spr), and their palette (.kit) if it is beside them",
                       "EditSprites.tipRead"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Save the sprites of the current bank", "EditSprites.tipSave"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Save the sprites of all the banks", "EditSprites.tipSaveAll"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "The sprites' palette is saved with them", "EditSprites.tipWithPal"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Read the sprites' palette (.kit)", "EditSprites.tipReadPal"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Save the sprites' palette (.kit)", "EditSprites.tipSavePal"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Shades between two colours of the sprites' palette",
                       "EditSprites.tipGenPal"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Reverse the order of colours 1 to 15", "EditSprites.tipInversePalette"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Click: edit this sprite. Right click on a sprite, then on another: they change places.",
                       "EditSprites.tipStrip"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Left button: draw with the chosen colour. Right button: rub out (transparent).",
                       "EditSprites.tipGrid"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Colour %1. Click: draw with it. Right click: change it.",
                       "EditSprites.tipPen"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Colour 0: transparent. Click: draw with it. Right click: choose how it is shown.",
                       "EditSprites.tipPen0"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", ", used by the sprite", "EditSprites.used"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Shows the sprites at the sizes the CPC+ gives them",
                       "EditSprites.tipTest"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "A click in the test area puts the current sprite there",
                       "EditSprites.tipPosSpr"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Sprites of the bank", "EditSprites.nameStrip"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Sprite being edited", "EditSprites.nameGrid"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Sprites test", "EditSprites.nameTest"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Colour of the left button", "EditSprites.namePenLeft"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Colour of the right button (transparent)", "EditSprites.namePenRight"),
    QT_TRANSLATE_NOOP3("tuxscr::EditSprites", "Color %1 is the same as color %2", "EditSprites.sameColour"),
    // ---- The capture of sprites ----
    QT_TRANSLATE_NOOP3("tuxscr::Capture", "Hard sprites capture", "Capture.title"),
    QT_TRANSLATE_NOOP3("tuxscr::Capture", "Sprite #", "Capture.label1"),
    QT_TRANSLATE_NOOP3("tuxscr::Capture", "Auto capture", "Capture.chkAutoCapture"),
    QT_TRANSLATE_NOOP3("tuxscr::Capture", "X:", "Capture.lblNbX"),
    QT_TRANSLATE_NOOP3("tuxscr::Capture", "Y:", "Capture.lblNbY"),
    QT_TRANSLATE_NOOP3("tuxscr::Capture", "Bank ", "Capture.bank"),
    QT_TRANSLATE_NOOP3("tuxscr::Capture", "Captured area", "Capture.namePreview"),
    QT_TRANSLATE_NOOP3("tuxscr::Capture", "Sprite capture x:%1, y:%2", "Capture.shown"),
    QT_TRANSLATE_NOOP3("tuxscr::Capture", "Sprite #%1 copied, bank %2", "Capture.stored"),
    QT_TRANSLATE_NOOP3("tuxscr::Capture", "There is no room after bank 8: the sprites that follow are not copied.",
                       "Capture.full"),
    // ---- On the picture's side ----
    QT_TRANSLATE_NOOP3("tuxscr::Sprites", "Hard sprites on the picture", "Sprites.tabPlaces"),
    QT_TRANSLATE_NOOP3("tuxscr::Sprites", "Sprites capture", "Sprites.tabCapture"),
    QT_TRANSLATE_NOOP3("tuxscr::Sprites", "Enable", "Sprites.chkAfficheSH"),
    QT_TRANSLATE_NOOP3("tuxscr::Sprites", "Sprite #", "Sprites.label3"),
    QT_TRANSLATE_NOOP3("tuxscr::Sprites", "Zoom X", "Sprites.label5"),
    QT_TRANSLATE_NOOP3("tuxscr::Sprites", "Zoom Y", "Sprites.label6"),
    QT_TRANSLATE_NOOP3("tuxscr::Sprites", "Pos X", "Sprites.label7"),
    QT_TRANSLATE_NOOP3("tuxscr::Sprites", "Pos Y", "Sprites.label8"),
    QT_TRANSLATE_NOOP3("tuxscr::Sprites", "Copy zoom to the other sprites", "Sprites.chkCopyZoom"),
    QT_TRANSLATE_NOOP3("tuxscr::Sprites", "Save", "Sprites.bpSaveCoord"),
    QT_TRANSLATE_NOOP3("tuxscr::Sprites", "Read", "Sprites.bpReadCoord"),
    QT_TRANSLATE_NOOP3("tuxscr::Sprites", "Copy positions to the clipboard", "Sprites.bpCopiePosSprites"),
    QT_TRANSLATE_NOOP3("tuxscr::Sprites", "Sprite positions copied to the clipboard", "Sprites.copied"),
    QT_TRANSLATE_NOOP3("tuxscr::Sprites", "The positions could not be read.", "Sprites.readError"),
    QT_TRANSLATE_NOOP3("tuxscr::Sprites", "The positions could not be saved.", "Sprites.saveError"),
    QT_TRANSLATE_NOOP3("tuxscr::Sprites", "XML file (*.xml)", "Sprites.xmlFilter"),
    QT_TRANSLATE_NOOP3("tuxscr::Sprites", "Zoom 0: the sprite is not shown. A right click on the picture puts the chosen sprite there.",
                       "Sprites.placesHint"),
    QT_TRANSLATE_NOOP3("tuxscr::Sprites", "Save the places and the zooms (.xml)", "Sprites.tipSaveCoord"),
    QT_TRANSLATE_NOOP3("tuxscr::Sprites", "Read the places and the zooms (.xml)", "Sprites.tipReadCoord"),
    QT_TRANSLATE_NOOP3("tuxscr::Sprites", "On the picture, the mouse moves a rectangle (or the arrow keys, once the picture was clicked); a click, or Space, shows what is in it in the capture window, where \"Capture\" makes sprites of it.",
                       "Sprites.captureHint"),
    QT_TRANSLATE_NOOP3("tuxscr::Sprites", "Another tool is using the picture's mouse.", "Sprites.mouseBusy"),
};

}  // namespace

void registerSpriteTexts()
{
    texts::add(kSpriteTexts);
}

QString spriteText(const char* name)
{
    QString text = texts::text(name);
    static const QRegularExpression breaks(QStringLiteral("[\\r\\n]+"));
    return text.replace(breaks, QStringLiteral(" "));
}

}  // namespace tuxscr
