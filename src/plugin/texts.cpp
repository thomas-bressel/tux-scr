#include "texts.h"

#include <QAbstractButton>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QWidget>

namespace {

struct WindowText {
    const char* name;
    const char* french;
    const char* english;
};

#include "strings.inc"

// What TuxSCR adds to ConvImgCpc's windows, and the texts ConvImgCpc has in
// one language only (those its designer gives its windows).
const WindowText kOwnTexts[] = {
    {"Main.chkDiffErr", nullptr, "Diffusion"},
    {"Main.bpImport", "Import", "Import"},
    {"Main.bpStandard", "Standard", "Standard"},
    {"Main.bpOverscan", "Overscan", "Fullscreen"},
    {"Main.label3", "Mode", "Mode"},
    {"Main.label4", "Type", "Type"},
    {"Main.label7", "Position :", "Position:"},
    {"Main.radioFit", "Fit", "Fit"},
    {"Main.radioKeepSmaller", "KeepSmaller", "KeepSmaller"},
    {"Main.radioKeepLarger", "KeepLarger", "KeepLarger"},
    {"Main.modePlus", "CPC+", "CPC+"},
    {"Main.chkInfo", "Informations", "Informations"},
    {"Main.chkGauss", "KMeans", "KMeans"},
    {"Main.chk17K", "<=17Ko", "<=17K"},
    {"Main.chkSwapEgx", "swapEGX", "swapEGX"},
    {"Main.bpRazAll", "Raz tout", "Reset All"},
    {"Main.bpEditSplit", "Éditeur de splits", "Split Editor"},
    {"Main.bpRasterPlus", "Rasters Plus", "Raster+"},
    {"Main.bpTest", "Matrice", "Matrix"},
    {"Main.bpTunnel", "Tunnel", "Tunnel"},
    {"Main.lblPack", "Méthode de compression", "Packing method"},
    {"Main.bpShow", "Montrer dans TuxAPE", "Show in TuxAPE"},
    {"Main.bpFromCpc", "Depuis l'écran du CPC", "From the CPC Screen"},
    // Palette interrupts.
    {"Interrupts.title", "Interruptions de palette (CPC+)", "Palette interrupts (CPC+)"},
    {"Interrupts.chkInterrupts", "Ajouter des interruptions de palette", "Add Palette Interrupt"},
    {"Interrupts.bpAdd", "Ajouter", "Add"},
    {"Interrupts.bpRemove", "Supprimer", "Remove"},
    {"Interrupts.bpAuto", "Auto", "Auto"},
    {"Interrupts.bpApply", "Appliquer à l'écran", "Apply to Screen"},
    {"Interrupts.bpExport", "Exporter en texte...", "Export as Text..."},
    {"Interrupts.lblLine", "Ligne", "Line"},
    {"Interrupts.picture", "Image", "Picture"},
    {"Interrupts.none", "Aucune interruption : « Ajouter » en place une.", "No interrupt: \"Add\" puts one."},
    {"Interrupts.tipLine",
     "La ligne de l'image à partir de laquelle les encres prennent ces couleurs. Un trait la montre sur l'image.",
     "The line of the picture from which the pens take these colours. A mark shows it on the picture."},
    {"Interrupts.tipPen",
     "Clic : choisir la couleur de l'encre à partir de cette ligne ; ce que l'encre dessine prend cette couleur. "
     "Clic droit ou Suppr : reprendre celle du dessus.",
     "Click: choose the pen's colour from this line on; what the pen draws takes that colour. "
     "Right click or Delete: take the one above again."},
    {"Interrupts.tipAuto",
     "Cherche dans l'image les meilleures couleurs pour les lignes de cette interruption, et convertit ces lignes avec elles. "
     "Relâché : ces couleurs sont retirées, les lignes sont de nouveau converties avec la palette du dessus.",
     "Searches the picture for the best colours for the lines of this interrupt, and converts those lines with them. "
     "Released: those colours are taken away, the lines are converted with the palette of above again."},
    {"Interrupts.onlyPlus", "Les interruptions de palette demandent un CPC+ et un des modes 0, 1 ou 2.",
     "Palette interrupts need a CPC+ and one of the modes 0, 1 or 2."},
    {"Interrupts.exported", "Interruptions de palette exportées : ", "Palette interrupts exported: "},
};

QString g_language = QStringLiteral("FR");

struct AddedText {
    QByteArray french;
    QByteArray english;
    bool hasFrench = false;
    bool hasEnglish = false;
};
QHash<QString, AddedText>& added()
{
    static QHash<QString, AddedText> texts;
    return texts;
}

const QHash<QString, const WindowText*>& table()
{
    static const QHash<QString, const WindowText*> names = [] {
        QHash<QString, const WindowText*> made;
        for (const WindowText& text : kWindowTexts)
            made.insert(QString::fromLatin1(text.name), &text);
        // TuxSCR's own go over ConvImgCpc's.
        for (const WindowText& text : kOwnTexts) {
            const auto there = made.constFind(QString::fromLatin1(text.name));
            if (there == made.constEnd() || (text.french && text.english))
                made.insert(QString::fromLatin1(text.name), &text);
        }
        return made;
    }();
    return names;
}

}  // namespace

namespace texts {

void setLanguage(const QString& language)
{
    g_language = language.compare(QLatin1String("EN"), Qt::CaseInsensitive) == 0 ? QStringLiteral("EN") : QStringLiteral("FR");
}

QString language()
{
    return g_language;
}

void add(const char* name, const char* french, const char* english)
{
    AddedText text;
    if (french) {
        text.french = french;
        text.hasFrench = true;
    }
    if (english) {
        text.english = english;
        text.hasEnglish = true;
    }
    added().insert(QString::fromLatin1(name), text);
}

QString text(const char* name)
{
    const QString key = QString::fromLatin1(name);
    if (const auto more = added().constFind(key); more != added().constEnd()) {
        const bool wantEnglish = g_language == QLatin1String("EN");
        if (wantEnglish ? more->hasEnglish : more->hasFrench)
            return QString::fromUtf8(wantEnglish ? more->english : more->french);
        if (more->hasEnglish || more->hasFrench)
            return QString::fromUtf8(more->hasEnglish ? more->english : more->french);
    }
    const WindowText* found = table().value(key);
    const auto pick = [](const WindowText* entry) -> const char* {
        if (!entry)
            return nullptr;
        const char* wanted = g_language == QLatin1String("EN") ? entry->english : entry->french;
        return wanted ? wanted : (entry->english ? entry->english : entry->french);
    };
    const char* picked = pick(found);
    if (!picked) {
        // One of ConvImgCpc's that TuxSCR's table has in the other language.
        for (const WindowText& own : kOwnTexts)
            if (key == QLatin1String(own.name) && pick(&own))
                return QString::fromUtf8(pick(&own));
        return {};
    }
    return QString::fromUtf8(picked);
}

QString text(const char* name, const QString& fallback)
{
    const QString found = text(name);
    return found.isEmpty() ? fallback : found;
}

void apply(QWidget* root, const char* prefix)
{
    const QString start = QString::fromLatin1(prefix) + QLatin1Char('.');
    const QList<QWidget*> all = root->findChildren<QWidget*>();
    for (QWidget* widget : all) {
        if (widget->objectName().isEmpty())
            continue;
        const QString found = text((start + widget->objectName()).toLatin1().constData());
        if (found.isEmpty())
            continue;
        if (auto* button = qobject_cast<QAbstractButton*>(widget))
            button->setText(found);
        else if (auto* label = qobject_cast<QLabel*>(widget))
            label->setText(found);
        else if (auto* box = qobject_cast<QGroupBox*>(widget))
            box->setTitle(found);
    }
}

}  // namespace texts
