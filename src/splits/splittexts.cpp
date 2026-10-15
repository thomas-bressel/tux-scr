#include "splittexts.h"

#include "texts.h"

namespace {

struct OwnText {
    const char* name;
    const char* french;
    const char* english;
};

const OwnText kTexts[] = {
    // The split editor.
    {"EditSplit.title", "Éditeur de splits", "Split Editor"},
    {"EditSplit.bpLoad", "Lire Splits", "Load Splits"},
    {"EditSplit.bpSave", "Sauver Splits", "Save Splits"},
    {"EditSplit.label1", "Ligne", "Line"},
    {"EditSplit.label15", "Numéro de stylo à changer", "Pen to change"},
    {"EditSplit.label14", "Retard", "Delay"},
    {"EditSplit.chkChgt", "Afficher les lignes de changements", "Show where the colours change"},
    {"EditSplit.grpSplit", "Splits", "Splits"},
    {"EditSplit.chkSplit0", "Split 1", "Split 1"},
    {"EditSplit.chkSplit1", "Split 2", "Split 2"},
    {"EditSplit.chkSplit2", "Split 3", "Split 3"},
    {"EditSplit.chkSplit3", "Split 4", "Split 4"},
    {"EditSplit.chkSplit4", "Split 5", "Split 5"},
    {"EditSplit.chkSplit5", "Split 6", "Split 6"},
    {"EditSplit.chkSplit6", "Split 7", "Split 7"},
    {"EditSplit.label2", "Largeur", "Width"},
    {"EditSplit.label3", "Largeur", "Width"},
    {"EditSplit.label4", "Largeur", "Width"},
    {"EditSplit.label5", "Largeur", "Width"},
    {"EditSplit.label6", "Largeur", "Width"},
    {"EditSplit.label7", "Largeur", "Width"},
    {"EditSplit.label16", "Largeur", "Width"},
    {"EditSplit.label13", "Couleur", "Colour"},
    {"EditSplit.label12", "Couleur", "Colour"},
    {"EditSplit.label11", "Couleur", "Colour"},
    {"EditSplit.label10", "Couleur", "Colour"},
    {"EditSplit.label9", "Couleur", "Colour"},
    {"EditSplit.label8", "Couleur", "Colour"},
    {"EditSplit.label18", "Couleur", "Colour"},
    {"EditSplit.chkChangeMode", "Changement de mode ligne suivante", "Change the mode at the next line"},
    {"EditSplit.bpCopieLigne", "Copier ligne précédente", "Copy the line above"},
    {"EditSplit.groupPal", "Palette initiale", "Starting palette"},
    {"EditSplit.bpImportSplit", "Importer image split", "Import a split picture"},
    {"EditSplit.bpGenAsm", "Générer assembleur", "Generate assembler"},
    {"EditSplit.xmlFiles", "Fichiers SplitEditor (*.xml)", "SplitEditor files (*.xml)"},
    {"EditSplit.asmFiles", "Source assembleur (*.asm)", "Assembler source (*.asm)"},
    {"EditSplit.readError", "Impossible de lire ces splits.", "These splits could not be read."},
    {"EditSplit.saveError", "Impossible d'écrire le fichier.", "The file could not be written."},
    {"EditSplit.saved", "Programme des splits écrit : ", "Split program written: "},
    {"EditSplit.position", "Position X:%1, Y:%2", "Position X:%1, Y:%2"},
    {"EditSplit.mode", "Nouveau mode", "New mode"},
    {"EditSplit.picture", "Écran avec ses splits", "Screen with its splits"},
    {"EditSplit.zoom", "Ligne agrandie", "Line enlarged"},
    {"EditSplit.zoomPlace", "Position de l'agrandissement", "Place of the enlargement"},
    // The picture splits are taken from.
    {"ImportSplit.title", "Importer image split", "Import a Split Picture"},
    {"ImportSplit.bpLire", "Lire image", "Load picture"},
    {"ImportSplit.label1", "N° Stylo", "Pen"},
    {"ImportSplit.imageFiles", "Images (*.bmp *.gif *.png *.jpg)", "Pictures (*.bmp *.gif *.png *.jpg)"},
    {"ImportSplit.allFiles", "Tous fichiers (*)", "All files (*)"},
    {"ImportSplit.badSize", "L'image doit avoir une dimension de 384 pixels par 272", "The picture must be 384 pixels by 272"},
    {"ImportSplit.unreadable", "Impossible de lire l'image (format inconnu ???)", "The picture could not be read (unknown format?)"},
    {"ImportSplit.tooMany", "Plus de 7 splits trouvés sur la ligne %1, seulement 7 seront pris en compte.",
     "More than 7 splits found on line %1: only 7 will be kept."},
    {"ImportSplit.tooShort", "Split de longueur inférieure à 32 trouvé sur la ligne %1, la longueur sera ramenée à 32 pixels.",
     "Split shorter than 32 found on line %1: its length will be brought to 32 pixels."},
    {"ImportSplit.listErr", "Ce que l'image a que les lignes ne peuvent pas tenir", "What the picture has that the lines cannot hold"},
    {"ImportSplit.pictureSplit", "Image des splits", "Picture of the splits"},
    // The Plus's raster table.
    {"RasterTablePlus.title", "Rasters Plus", "Raster+"},
    {"RasterTablePlus.bpLoad", "Lire rasters", "Load rasters"},
    {"RasterTablePlus.bpSave", "Sauver rasters", "Save rasters"},
    {"RasterTablePlus.bpClearAll", "Effacer les lignes", "Clear all lines"},
    {"RasterTablePlus.bpAddLine", "Ajouter un dégradé", "Add Lines color"},
    {"RasterTablePlus.chkSprites", "Avec les sprites", "With sprites"},
    {"RasterTablePlus.lblLine", "ligne %1", "line %1"},
    {"RasterTablePlus.groupBox2", "Couleur constante", "Constant color"},
    {"RasterTablePlus.bpAddConstant", "Ajouter sur", "Add for"},
    {"RasterTablePlus.label6", "lignes", "lines"},
    {"RasterTablePlus.label7", "Début :", "Start:"},
    {"RasterTablePlus.bpUndo", "Annuler", "Undo"},
    {"RasterTablePlus.label3", "V", "V"},
    {"RasterTablePlus.label4", "R", "R"},
    {"RasterTablePlus.label5", "B", "B"},
    {"RasterTablePlus.bpScrollUp", "Monter", "Scroll Up"},
    {"RasterTablePlus.bpScrollDown", "Descendre", "Scroll Down"},
    {"RasterTablePlus.label8", "lignes", "lines"},
    {"RasterTablePlus.groupBox1", "Sortie", "Output"},
    {"RasterTablePlus.label1", "De la ligne :", "From line :"},
    {"RasterTablePlus.label2", "À la ligne :", "To line :"},
    {"RasterTablePlus.bpGenerate", "Générer assembleur", "Generate asm output"},
    {"RasterTablePlus.bpGenFade", "Générer fondu asm", "Generate asm fade"},
    {"RasterTablePlus.bpCopyToClipboard", "Copier (presse-papier)", "Copy to clipboard"},
    {"RasterTablePlus.bpImportKit", "Importer KIT", "Import KIT"},
    {"RasterTablePlus.xmlFiles", "Fichiers xml (*.xml)", "XML files (*.xml)"},
    {"RasterTablePlus.kitFiles", "Fichiers palette (*.kit)", "Palette files (*.kit)"},
    {"RasterTablePlus.asmFiles", "Fichier assembleur (*.asm)", "Assembler file (*.asm)"},
    {"RasterTablePlus.readError", "Erreur lecture rasters ...", "The rasters could not be read."},
    {"RasterTablePlus.saveError", "Erreur sauvegarde rasters...", "The rasters could not be saved."},
    {"RasterTablePlus.badRange", "Les lignes à écrire vont de 0 à 271, la première avant la dernière.",
     "The lines to write are from 0 to 271, the first before the last."},
    {"RasterTablePlus.badLines", "Ces lignes ne sont pas toutes entre 0 et 271.", "These lines are not all between 0 and 271."},
    {"RasterTablePlus.badKit", "Ce fichier n'est pas une palette de CPC+ (.KIT).", "This file is not a palette of the Plus (.KIT)."},
    // The colour the rasters are shown in place of.
    {"SelectColor.title", "Couleur des rasters", "Raster Colour"},
    {"SelectColor.label1", "Choisissez la couleur qui sera rastérisée :", "Choose the color that will be rasterized :"},
    {"SelectColor.lblHint", "Fenêtre fermée sans choix : toutes les couleurs le seront.",
     "Closed without a choice, every colour will be."},
    {"SelectColor.pen", "Encre %1", "Pen %1"},
    // The maker of palettes.
    {"GenPalette.title", "Générer palette", "Make Palette"},
    {"GenPalette.lblFrom", "De la couleur", "From color"},
    {"GenPalette.lblTo", "À la couleur", "To color"},
    {"GenPalette.lblFromLine", "De la ligne", "From line"},
    {"GenPalette.lblToLine", "À la ligne", "To line"},
    {"GenPalette.lblStartText", "Couleur de début", "Start color"},
    {"GenPalette.lblEndText", "Couleur de fin", "End color"},
    {"GenPalette.bpGetCol", "Lire couleur", "Get color"},
    {"GenPalette.bpGenerate", "Générer", "Generate"},
    {"GenPalette.label1", "R", "R"},
    {"GenPalette.label2", "R", "R"},
    {"GenPalette.label3", "V", "V"},
    {"GenPalette.label4", "V", "V"},
    {"GenPalette.label5", "B", "B"},
    {"GenPalette.label6", "B", "B"},
    {"GenPalette.same", "La couleur %1 est la même que la couleur %2", "Color %1 is the same as color %2"},
    {"GenPalette.invalid", "Ces valeurs ne conviennent pas.", "These values cannot be used."},
};

}  // namespace

void registerSplitTexts()
{
    static bool given = false;
    if (given)
        return;
    given = true;
    for (const OwnText& text : kTexts)
        texts::add(text.name, text.french, text.english);
}
