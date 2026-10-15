#include "spritetexts.h"

#include <QRegularExpression>

#include "texts.h"

namespace {

struct SpriteText {
    const char* name;
    const char* french;
    const char* english;
};

const SpriteText kSpriteTexts[] = {
    // ---- The sprite editor ----
    {"EditSprites.bpRead", "sprites", "sprites"},
    {"EditSprites.bpReadPal", "palette", "palette"},
    {"EditSprites.bpSavePal", "palette", "palette"},
    {"EditSprites.bpCopyBank", "Bank", "Bank"},
    {"EditSprites.bpPasteBank", "Bank", "Bank"},
    {"EditSprites.bpClearBank", "Bank", "Bank"},
    {"EditSprites.bpGenPal", "Générer palette", "Make Palette"},
    {"EditSprites.bpInversePalette", "Inverser palette", "Reverse Palette"},
    {"EditSprites.grpEdition", "Edition", "Editing"},
    {"EditSprites.rbPt", "Point", "Point"},
    {"EditSprites.rbLine", "Ligne", "Line"},
    {"EditSprites.rbFill", "Fill", "Fill"},
    {"EditSprites.rb1Sprite", "1", "1"},
    {"EditSprites.rb2Sprite", "2x2", "2x2"},
    {"EditSprites.rb4Sprite", "4x4", "4x4"},
    {"EditSprites.rb42Sprite", "2x2x4", "2x2x4"},
    {"EditSprites.rb28sprite", "2x8", "2x8"},
    {"EditSprites.rbPosSpr", "SetPos", "SetPos"},
    {"EditSprites.label3", "Zoom X", "Zoom X"},
    {"EditSprites.label4", "Zoom Y", "Zoom Y"},
    {"EditSprites.bpTest", "Test !", "Test!"},
    {"EditSprites.selSprite", "Sprite n° : ", "Sprite #: "},
    {"EditSprites.bank", "Bank ", "Bank "},
    {"EditSprites.paletteFilter", "Palette CPC+ (*.kit)", "CPC+ palette (*.kit)"},
    {"EditSprites.tipPrev", "Sprite précédent (flèche gauche)", "Previous sprite (left arrow)"},
    {"EditSprites.tipSuiv", "Sprite suivant (flèche droite)", "Next sprite (right arrow)"},
    {"EditSprites.tipBank", "Bank de 16 sprites en cours d'édition", "Bank of 16 sprites being edited"},
    {"EditSprites.tipCopySprite", "Copier le sprite", "Copy the sprite"},
    {"EditSprites.tipPasteSprite", "Coller le sprite copié", "Paste the sprite that was copied"},
    {"EditSprites.tipHorizontalFlip", "Retourner le sprite de haut en bas", "Flip the sprite top to bottom"},
    {"EditSprites.tipVerticalFlip", "Retourner le sprite de gauche à droite", "Flip the sprite left to right"},
    {"EditSprites.tipRotateSprite", "Tourner le sprite d'un quart de tour", "Turn the sprite a quarter turn"},
    {"EditSprites.tipClearSprite", "Effacer le sprite", "Clear the sprite"},
    {"EditSprites.tipScrUp", "Décaler le sprite d'un pixel vers le haut", "Move the sprite up one pixel"},
    {"EditSprites.tipScrDown", "Décaler le sprite d'un pixel vers le bas", "Move the sprite down one pixel"},
    {"EditSprites.tipScrLeft", "Décaler le sprite d'un pixel vers la gauche", "Move the sprite left one pixel"},
    {"EditSprites.tipScrRight", "Décaler le sprite d'un pixel vers la droite", "Move the sprite right one pixel"},
    {"EditSprites.tipCopyBank", "Copier les 16 sprites de la bank", "Copy the 16 sprites of the bank"},
    {"EditSprites.tipPasteBank", "Coller les 16 sprites copiés", "Paste the 16 sprites that were copied"},
    {"EditSprites.tipClearBank", "Effacer les 16 sprites de la bank", "Clear the 16 sprites of the bank"},
    {"EditSprites.tipRead", "Lire des sprites (.spr), et leur palette (.kit) si elle est à côté",
     "Read sprites (.spr), and their palette (.kit) if it is beside them"},
    {"EditSprites.tipSave", "Sauver les sprites de la bank courante", "Save the sprites of the current bank"},
    {"EditSprites.tipSaveAll", "Sauver les sprites de toutes les banks", "Save the sprites of all the banks"},
    {"EditSprites.tipWithPal", "La palette des sprites est sauvée avec eux", "The sprites' palette is saved with them"},
    {"EditSprites.tipReadPal", "Lire la palette des sprites (.kit)", "Read the sprites' palette (.kit)"},
    {"EditSprites.tipSavePal", "Sauver la palette des sprites (.kit)", "Save the sprites' palette (.kit)"},
    {"EditSprites.tipGenPal", "Dégradé entre deux couleurs de la palette des sprites",
     "Shades between two colours of the sprites' palette"},
    {"EditSprites.tipInversePalette", "Inverser l'ordre des couleurs 1 à 15", "Reverse the order of colours 1 to 15"},
    {"EditSprites.tipStrip", "Clic : éditer ce sprite. Clic droit sur un sprite puis sur un autre : les échanger.",
     "Click: edit this sprite. Right click on a sprite, then on another: they change places."},
    {"EditSprites.tipGrid", "Bouton gauche : dessiner avec la couleur choisie. Bouton droit : effacer (transparent).",
     "Left button: draw with the chosen colour. Right button: rub out (transparent)."},
    {"EditSprites.tipPen", "Couleur %1. Clic : dessiner avec. Clic droit : la modifier.",
     "Colour %1. Click: draw with it. Right click: change it."},
    {"EditSprites.tipPen0", "Couleur 0 : transparente. Clic : dessiner avec. Clic droit : choisir comment elle est montrée.",
     "Colour 0: transparent. Click: draw with it. Right click: choose how it is shown."},
    {"EditSprites.used", ", utilisée par le sprite", ", used by the sprite"},
    {"EditSprites.tipTest", "Montre les sprites aux tailles que le CPC+ leur donne", "Shows the sprites at the sizes the CPC+ gives them"},
    {"EditSprites.tipPosSpr", "Un clic dans la zone d'essai y place le sprite courant",
     "A click in the test area puts the current sprite there"},
    {"EditSprites.nameStrip", "Sprites de la bank", "Sprites of the bank"},
    {"EditSprites.nameGrid", "Sprite en cours d'édition", "Sprite being edited"},
    {"EditSprites.nameTest", "Essai des sprites", "Sprites test"},
    {"EditSprites.namePenLeft", "Couleur du bouton gauche", "Colour of the left button"},
    {"EditSprites.namePenRight", "Couleur du bouton droit (transparent)", "Colour of the right button (transparent)"},
    {"EditSprites.sameColour", "La couleur %1 est la même que la couleur %2", "Color %1 is the same as color %2"},
    // ---- The capture of sprites ----
    {"Capture.title", "Capture sprites hard", "Hard sprites capture"},
    {"Capture.label1", "Sprite n°", "Sprite #"},
    {"Capture.chkAutoCapture", "Capture Auto", "Auto capture"},
    {"Capture.lblNbX", "X:", "X:"},
    {"Capture.lblNbY", "Y:", "Y:"},
    {"Capture.bank", "Bank ", "Bank "},
    {"Capture.namePreview", "Zone capturée", "Captured area"},
    {"Capture.shown", "Capture sprite x:%1, y:%2", "Sprite capture x:%1, y:%2"},
    {"Capture.stored", "Copie sprite n°%1, bank %2", "Sprite #%1 copied, bank %2"},
    {"Capture.full", "Il n'y a plus de place après la bank 8 : les sprites suivants ne sont pas copiés.",
     "There is no room after bank 8: the sprites that follow are not copied."},
    // ---- On the picture's side ----
    {"Sprites.tabPlaces", "Affichage sprites hard sur image", "Hard sprites on the picture"},
    {"Sprites.tabCapture", "Capture de sprites", "Sprites capture"},
    {"Sprites.chkAfficheSH", "Activer", "Enable"},
    {"Sprites.label3", "N°Sprite", "Sprite #"},
    {"Sprites.label5", "Zoom X", "Zoom X"},
    {"Sprites.label6", "Zoom Y", "Zoom Y"},
    {"Sprites.label7", "Pos X", "Pos X"},
    {"Sprites.label8", "Pos Y", "Pos Y"},
    {"Sprites.chkCopyZoom", "Recopier zoom aux autres sprites", "Copy zoom to the other sprites"},
    {"Sprites.bpSaveCoord", "Save", "Save"},
    {"Sprites.bpReadCoord", "Read", "Read"},
    {"Sprites.bpCopiePosSprites", "Copier positions dans presse-papier", "Copy positions to the clipboard"},
    {"Sprites.copied", "Position des sprites copié(es) dans le presse-papier", "Sprite positions copied to the clipboard"},
    {"Sprites.readError", "Erreur lors de la lecture des positions...", "The positions could not be read."},
    {"Sprites.saveError", "Erreur lors de la sauvegarde des positions...", "The positions could not be saved."},
    {"Sprites.xmlFilter", "Fichier XML (*.xml)", "XML file (*.xml)"},
    {"Sprites.placesHint",
     "Zoom 0 : le sprite n'est pas montré. Un clic droit sur l'image y place le sprite choisi.",
     "Zoom 0: the sprite is not shown. A right click on the picture puts the chosen sprite there."},
    {"Sprites.tipSaveCoord", "Sauver les positions et les zooms (.xml)", "Save the places and the zooms (.xml)"},
    {"Sprites.tipReadCoord", "Lire les positions et les zooms (.xml)", "Read the places and the zooms (.xml)"},
    {"Sprites.captureHint",
     "Sur l'image, la souris déplace un rectangle (ou les flèches, une fois l'image cliquée) ; un clic, ou Espace, "
     "montre ce qu'il contient dans la fenêtre de capture, où « Capturer » en fait des sprites.",
     "On the picture, the mouse moves a rectangle (or the arrow keys, once the picture was clicked); a click, or Space, "
     "shows what is in it in the capture window, where \"Capture\" makes sprites of it."},
    {"Sprites.mouseBusy", "La souris de l'image est utilisée par un autre outil.", "Another tool is using the picture's mouse."},
};

}  // namespace

void registerSpriteTexts()
{
    for (const SpriteText& text : kSpriteTexts)
        texts::add(text.name, text.french, text.english);
}

QString spriteText(const char* name)
{
    QString text = texts::text(name);
    static const QRegularExpression breaks(QStringLiteral("[\\r\\n]+"));
    return text.replace(breaks, QStringLiteral(" "));
}
