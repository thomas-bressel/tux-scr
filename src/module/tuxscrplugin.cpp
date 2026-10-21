#include "tuxscrplugin.h"

#include <QAction>
#include <QApplication>
#include <QEvent>

#include "converterwindow.h"
#include "icons.h"
#include "splittools.h"
#include "spritetools.h"

bool TuxScrPlugin::start(tuxape::PluginHost* host, QString*)
{
    host_ = host;
    action_ = host->addAction(tr("TuxSCR – &Screen Converter..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F5));
    host->addButton(action_, screenIcon());
    connect(action_, &QAction::triggered, this, &TuxScrPlugin::showWindow);
    qApp->installEventFilter(this);
    return true;
}

void TuxScrPlugin::stop()
{
    qApp->removeEventFilter(this);
    // TuxAPE is unloading the library: the window goes, its settings kept.
    if (window_)
        window_->close();
    delete window_;
    // The entry and the button are TuxAPE's: it takes them back.
    action_ = nullptr;
    host_ = nullptr;
}

bool TuxScrPlugin::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == qApp && event->type() == QEvent::ApplicationPaletteChange && host_)
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
