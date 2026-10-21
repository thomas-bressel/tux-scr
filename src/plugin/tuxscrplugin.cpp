#include "tuxscrplugin.h"

#include <QAction>
#include <QApplication>
#include <QEvent>

#include "converterwindow.h"
#include "icons.h"
#include "splittools.h"
#include "spritetools.h"

namespace tuxscr {

bool TuxScrPlugin::start(tuxape::PluginHost* host, QString*)
{
    host_ = host;
    // TuxSCR's texts in TuxAPE's language, before its first tr(): the
    // translations built into the library. None for English, the language
    // of the sources.
    if (translator_.load(QStringLiteral(":/i18n/tuxscr/tuxscr_%1.qm").arg(host->language())))
        QCoreApplication::installTranslator(&translator_);
    // One of the keys TuxAPE leaves to plugins (see PLUGINS.md).
    action_ = host->addAction(tr("TuxSCR – &Screen Converter..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F5));
    host->addButton(action_, screenIcon());
    connect(action_, &QAction::triggered, this, &TuxScrPlugin::showWindow);
    // Qt tells the application when the look changes: its events are
    // watched for that one, which costs a comparison an event.
    qApp->installEventFilter(this);
    return true;
}

void TuxScrPlugin::stop()
{
    // TuxAPE unloads the library next: nothing of it may be left running,
    // nor its translator installed. The window goes, its settings kept.
    qApp->removeEventFilter(this);
    if (window_)
        window_->close();
    delete window_;
    QCoreApplication::removeTranslator(&translator_);
    // The entry and the button are TuxAPE's: it takes them back.
    action_ = nullptr;
    host_ = nullptr;
}

bool TuxScrPlugin::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == qApp && event->type() == QEvent::ApplicationPaletteChange && host_ && action_)
        host_->addButton(action_, screenIcon());
    return false;
}

void TuxScrPlugin::showWindow()
{
    if (!window_) {
        window_ = new ConverterWindow(host_, host_->window());
        installSpriteTools(window_);
        installSplitTools(window_);
    }
    window_->show();
    window_->raise();
    window_->activateWindow();
}

}  // namespace tuxscr
