#include "texts.h"

#include <cstring>

#include <QAbstractButton>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QWidget>

#include "windowtexts.h"

namespace tuxscr::texts {

namespace {

// What TuxSCR adds to ConvImgCpc's windows, and the texts ConvImgCpc has in
// French only (those its designer gives its windows), with an English of
// TuxSCR's: they go over ConvImgCpc's of the same name.
const WindowText kOwnTexts[] = {
    QT_TRANSLATE_NOOP3("tuxscr::Main", "Diffusion", "Main.chkDiffErr"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "Import", "Main.bpImport"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "Standard", "Main.bpStandard"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "Fullscreen", "Main.bpOverscan"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "Mode", "Main.label3"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "Type", "Main.label4"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "Position:", "Main.label7"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "Fit", "Main.radioFit"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "KeepSmaller", "Main.radioKeepSmaller"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "KeepLarger", "Main.radioKeepLarger"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "CPC+", "Main.modePlus"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "Informations", "Main.chkInfo"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "KMeans", "Main.chkGauss"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "<=17K", "Main.chk17K"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "swapEGX", "Main.chkSwapEgx"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "Reset All", "Main.bpRazAll"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "Split Editor", "Main.bpEditSplit"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "Raster+", "Main.bpRasterPlus"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "Matrix", "Main.bpTest"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "Tunnel", "Main.bpTunnel"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "Packing method", "Main.lblPack"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "Show in TuxAPE", "Main.bpShow"),
    QT_TRANSLATE_NOOP3("tuxscr::Main", "From the CPC Screen", "Main.bpFromCpc"),
    // Palette interrupts.
    QT_TRANSLATE_NOOP3("tuxscr::Interrupts", "Palette interrupts (CPC+)", "Interrupts.title"),
    QT_TRANSLATE_NOOP3("tuxscr::Interrupts", "Add Palette Interrupt", "Interrupts.chkInterrupts"),
    QT_TRANSLATE_NOOP3("tuxscr::Interrupts", "Add", "Interrupts.bpAdd"),
    QT_TRANSLATE_NOOP3("tuxscr::Interrupts", "Remove", "Interrupts.bpRemove"),
    QT_TRANSLATE_NOOP3("tuxscr::Interrupts", "Auto", "Interrupts.bpAuto"),
    QT_TRANSLATE_NOOP3("tuxscr::Interrupts", "Apply to Screen", "Interrupts.bpApply"),
    QT_TRANSLATE_NOOP3("tuxscr::Interrupts", "Export as Text...", "Interrupts.bpExport"),
    QT_TRANSLATE_NOOP3("tuxscr::Interrupts", "Line", "Interrupts.lblLine"),
    QT_TRANSLATE_NOOP3("tuxscr::Interrupts", "Picture", "Interrupts.picture"),
    QT_TRANSLATE_NOOP3("tuxscr::Interrupts", "No interrupt: \"Add\" puts one.", "Interrupts.none"),
    QT_TRANSLATE_NOOP3("tuxscr::Interrupts", "The line of the picture from which the pens take these colours. A mark shows it on the picture.",
                       "Interrupts.tipLine"),
    QT_TRANSLATE_NOOP3("tuxscr::Interrupts", "Click: choose the pen's colour from this line on; what the pen draws takes that colour. Right click or Delete: take the one above again.",
                       "Interrupts.tipPen"),
    QT_TRANSLATE_NOOP3("tuxscr::Interrupts", "Searches the picture for the best colours for the lines of this interrupt, and converts those lines with them. Released: those colours are taken away, the lines are converted with the palette of above again.",
                       "Interrupts.tipAuto"),
    QT_TRANSLATE_NOOP3("tuxscr::Interrupts", "Palette interrupts need a CPC+ and one of the modes 0, 1 or 2.",
                       "Interrupts.onlyPlus"),
    QT_TRANSLATE_NOOP3("tuxscr::Interrupts", "Palette interrupts exported: ", "Interrupts.exported"),
};

QHash<QString, const WindowText*>& table()
{
    static QHash<QString, const WindowText*> names = [] {
        QHash<QString, const WindowText*> made;
        for (const WindowText& text : kWindowTexts)
            made.insert(QString::fromLatin1(text.name), &text);
        for (const WindowText& text : kOwnTexts)
            made.insert(QString::fromLatin1(text.name), &text);
        return made;
    }();
    return names;
}

// The context a text is translated in: its window's, "tuxscr::Main" for
// "Main.bpLoad", as its QT_TRANSLATE_NOOP3 says.
QByteArray contextOf(const char* name)
{
    const char* dot = std::strchr(name, '.');
    return QByteArrayLiteral("tuxscr::") + QByteArray(name, dot ? static_cast<qsizetype>(dot - name) : -1);
}

}  // namespace

void add(std::span<const WindowText> more)
{
    for (const WindowText& text : more)
        table().insert(QString::fromLatin1(text.name), &text);
}

QString text(const char* name)
{
    const WindowText* found = table().value(QString::fromLatin1(name));
    if (!found)
        return {};
    return QCoreApplication::translate(contextOf(found->name).constData(), found->english, found->name);
}

QString text(const char* name, const QString& fallback)
{
    const QString found = text(name);
    return found.isEmpty() ? fallback : found;
}

QString windowTitle(const QString& title)
{
    return QStringLiteral("TuxSCR – ") + title;
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

}  // namespace tuxscr::texts
