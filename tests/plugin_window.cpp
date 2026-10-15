// The plugin as TuxAPE finds it: the library loaded, started with a host
// that only notes what it is asked, its entry chosen, its window shown, a
// picture given to TuxAPE to assemble and run, and the plugin stopped. Runs
// without a display (QT_QPA_PLATFORM=offscreen).
//
//   plugin_window [prefix]   also saves a picture of the window as
//                            <prefix>tuxscr_plugin.png

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QMainWindow>
#include <QPluginLoader>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>

#include "check.h"
#include "plugin.h"

namespace {

// A TuxAPE of the test's own: a window, and a note of what was asked.
class Host : public tuxape::PluginHost {
public:
    explicit Host(const QString& folder)
        : folder_(folder)
    {
    }

    QMainWindow frame;
    QList<QAction*> actions;
    int buttons = 0;
    int pictures = 0;
    QStringList assembled;

    QWidget* window() override { return &frame; }
    QAction* addAction(const QString& text, const QKeySequence& shortcut) override
    {
        auto* action = new QAction(text, &frame);
        action->setShortcut(shortcut);
        actions << action;
        return action;
    }
    void addButton(QAction* action, const QIcon& icon) override
    {
        if (actions.contains(action) && !icon.isNull())
            ++pictures;
        buttons = 1;
    }
    bool insertDisc(int, const QString&) override { return false; }
    bool loadSnapshot(const QString&) override { return false; }
    void autoType(const QString&) override {}
    void reset(bool) override {}
    QByteArray readMemory(int, int size) override { return QByteArray(size, '\0'); }
    tuxape::PluginScreen screen() override { return {}; }
    void openInAssembler(const QString&) override {}
    bool assembleAndRun(const QString& path) override
    {
        assembled << path;
        return QFile::exists(path);
    }
    QString dataFolder() override { return folder_; }

private:
    QString folder_;
};

QDialog* converterWindow()
{
    for (QWidget* widget : QApplication::topLevelWidgets())
        if (widget->objectName() == "TuxScrWindow")
            return qobject_cast<QDialog*>(widget);
    return nullptr;
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    const QString prefix = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    QTemporaryDir scratch;

    QPluginLoader loader(QStringLiteral(TUXSCR_PLUGIN));
    QObject* instance = loader.instance();
    CHECK(instance != nullptr);
    if (!instance) {
        std::printf("  %s\n", qPrintable(loader.errorString()));
        return checkSummary("plugin_window");
    }
    auto* plugin = qobject_cast<tuxape::Plugin*>(instance);
    CHECK(plugin != nullptr);
    if (!plugin)
        return checkSummary("plugin_window");

    {
        Host host(scratch.path());
        QString error;
        CHECK(plugin->start(&host, &error) && error.isEmpty());
        // One entry, with its key and its button; no window until it is chosen.
        CHECK_EQ(host.actions.size(), 1);
        CHECK(host.actions.size() == 1 && host.actions[0]->text() == "TuxSCR – &Screen Converter...");
        CHECK(host.actions.size() == 1 && host.actions[0]->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_I));
        CHECK(host.buttons == 1 && host.pictures == 1);
        CHECK(converterWindow() == nullptr);
        // A change of look has the button's picture drawn again.
        QEvent look(QEvent::ApplicationPaletteChange);
        QApplication::sendEvent(&app, &look);
        CHECK_EQ(host.pictures, 2);

        host.actions[0]->trigger();
        QDialog* window = converterWindow();
        CHECK(window && window->isVisible() && window->parentWidget() == &host.frame);
        CHECK(window && window->windowTitle() == "TuxSCR – Screen Converter");
        // Chosen again, it is the same window.
        host.actions[0]->trigger();
        CHECK(converterWindow() == window);

        // With TuxAPE there, the picture can be shown on the CPC: a source
        // is written in the plugin's folder and given to assemble and run.
        auto* show = window ? window->findChild<QPushButton*>("bpShow") : nullptr;
        CHECK(show && show->isVisibleTo(window));
        if (show) {
            show->click();
            CHECK_EQ(host.assembled.size(), 1);
            CHECK(host.assembled.size() == 1 && host.assembled[0] == QDir(scratch.path()).filePath("tuxscr_show.asm"));
            QFile source(host.assembled.value(0));
            CHECK(source.open(QIODevice::ReadOnly));
            const QByteArray text = source.readAll();
            CHECK(text.contains("\tORG\t#") && text.contains("\tRUN\t$") && text.contains("Depack"));
        }
        if (window && !prefix.isEmpty()) {
            QTest::qWait(50);
            window->grab().save(prefix + "tuxscr_plugin.png");
        }

        // Closed, the window leaves its settings in the folder TuxAPE gives
        // the plugin.
        if (window)
            window->close();
        CHECK(QFile::exists(QDir(scratch.path()).filePath("TuxSCR.xml")));

        // Stopped, it leaves nothing of its own behind.
        plugin->stop();
        CHECK(converterWindow() == nullptr);
    }
    CHECK(loader.unload());
    return checkSummary("plugin_window");
}
