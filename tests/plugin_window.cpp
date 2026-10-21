// The plugin as TuxAPE finds it: the library loaded with QPluginLoader and
// taken only if it is a tuxape::Plugin, started with a host that only notes
// what it is asked, its entry chosen, its window shown, a picture given to
// TuxAPE to assemble and run, and the plugin stopped, then unloaded. Then
// once more in French, as TuxAPE starts it when its windows are in French,
// and in a language TuxSCR has no translation for. Runs without a display
// (QT_QPA_PLATFORM=offscreen).
//
//   plugin_window [prefix]   also saves a picture of the window as
//                            <prefix>tuxscr_plugin.png

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QPluginLoader>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>

#include "check.h"
#include "host.h"

namespace {

QDialog* converterWindow()
{
    for (QWidget* widget : QApplication::topLevelWidgets())
        if (widget->objectName() == "TuxScrWindow")
            return qobject_cast<QDialog*>(widget);
    return nullptr;
}

// One load of the plugin, in the language the host speaks: the texts of its
// entry and its window, as that language has them. In English, a picture is
// also given to TuxAPE to show, and the window saved as `picture` if that
// names a file.
struct Run {
    QString entry;
    QString title;
    QString read;
    QString show;
};

Run runOnce(const QString& language, const QString& folder, const QString& picture)
{
    Run run;
    QPluginLoader loader(QStringLiteral(TUXSCR_PLUGIN));
    QObject* instance = loader.instance();
    CHECK(instance != nullptr);
    if (!instance) {
        std::printf("  %s\n", qPrintable(loader.errorString()));
        return run;
    }
    // The interface TuxAPE asks of a plugin.
    CHECK(loader.metaData().value("IID").toString() == QStringLiteral("org.tuxape.Plugin/1"));
    auto* plugin = qobject_cast<tuxape::Plugin*>(instance);
    CHECK(plugin != nullptr);
    if (!plugin)
        return run;

    {
        Host host(folder);
        host.speaks = language;
        QString error;
        CHECK(plugin->start(&host, &error) && error.isEmpty());
        // One entry, with its key and its button; no window until it is chosen.
        CHECK_EQ(host.actions.size(), 1);
        CHECK(host.actions.size() == 1 && host.actions[0]->shortcut() == QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F5));
        CHECK(host.buttons == 1 && host.pictures == 1);
        CHECK(converterWindow() == nullptr);
        // A change of look has the button's picture drawn again.
        QEvent look(QEvent::ApplicationPaletteChange);
        QApplication::sendEvent(qApp, &look);
        CHECK_EQ(host.pictures, 2);
        if (host.actions.isEmpty())
            return run;
        run.entry = host.actions[0]->text();

        host.actions[0]->trigger();
        QDialog* window = converterWindow();
        CHECK(window && window->isVisible() && window->parentWidget() == &host.frame);
        // Chosen again, it is the same window.
        host.actions[0]->trigger();
        CHECK(converterWindow() == window);
        if (!window)
            return run;
        run.title = window->windowTitle();
        auto* read = window->findChild<QPushButton*>("bpLoad");
        auto* show = window->findChild<QPushButton*>("bpShow");
        CHECK(read && show && show->isVisibleTo(window));
        run.read = read ? read->text() : QString();
        run.show = show ? show->text() : QString();
        // The language is TuxAPE's: the window has no button to change it.
        CHECK(window->findChild<QPushButton*>("bpFr") == nullptr && window->findChild<QPushButton*>("bpEn") == nullptr);

        if (language == "en" && show) {
            // With TuxAPE there, the picture can be shown on the CPC: a
            // source is written in the plugin's folder and given to
            // assemble and run.
            show->click();
            CHECK_EQ(host.assembled.size(), 1);
            CHECK(host.assembled.size() == 1 && host.assembled[0] == QDir(folder).filePath("tuxscr_show.asm"));
            QFile source(host.assembled.value(0));
            CHECK(source.open(QIODevice::ReadOnly));
            const QByteArray text = source.readAll();
            CHECK(text.contains("\tORG\t#") && text.contains("\tRUN\t$") && text.contains("Depack"));
            if (!picture.isEmpty()) {
                QTest::qWait(50);
                window->grab().save(picture);
            }
            // Closed, the window leaves its settings in the folder TuxAPE
            // gives the plugin.
            window->close();
            CHECK(QFile::exists(QDir(folder).filePath("TuxSCR.xml")));
        }

        // Stopped, it leaves nothing of its own behind.
        plugin->stop();
        CHECK(converterWindow() == nullptr);
    }
    // Its translation is gone with it.
    CHECK(QCoreApplication::translate("tuxscr::Main", "Read", "Main.bpLoad") == "Read");
    // What the plugin left to be deleted goes before its library does, as
    // TuxAPE sees to it.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(loader.unload());
    return run;
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    const QString prefix = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    QTemporaryDir folder;

    // In English, the language of the sources.
    const Run english = runOnce("en", folder.path(), prefix.isEmpty() ? QString() : prefix + "tuxscr_plugin.png");
    CHECK(english.entry == QString::fromUtf8("TuxSCR – &Screen Converter..."));
    CHECK(english.title == QString::fromUtf8("TuxSCR – Screen Converter"));
    CHECK(english.read == "Read" && english.show == "Show in TuxAPE");

    // In French: TuxAPE's language, TuxSCR's own translation, ConvImgCpc's
    // words for ConvImgCpc's controls.
    const Run french = runOnce("fr", folder.path(), QString());
    CHECK(french.entry == QString::fromUtf8("TuxSCR – &Convertisseur d'écrans..."));
    CHECK(french.title == QString::fromUtf8("TuxSCR – Convertisseur d'écrans"));
    CHECK(french.read == "Lecture" && french.show == "Montrer dans TuxAPE");

    // A language TuxSCR has no translation for: English.
    const Run other = runOnce("xx", folder.path(), QString());
    CHECK(other.entry == english.entry && other.read == english.read);
    return checkSummary("plugin_window");
}
