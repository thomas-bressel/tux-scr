#include "spritetools.h"

#include <QEvent>
#include <QPointer>
#include <QPushButton>
#include <QTabWidget>

#include "converterwindow.h"
#include "spritecapture.h"
#include "spriteeditor.h"
#include "spriteplaces.h"
#include "spritetexts.h"
#include "texts.h"

namespace tuxscr {

namespace {

// What the window was given of the sprites: it is the window's, and goes
// with it.
class SpriteTools : public QObject {
public:
    explicit SpriteTools(ConverterWindow* window)
        : QObject(window)
        , window_(window)
    {
        setObjectName(QStringLiteral("spriteTools"));
        QTabWidget* tools = window_->tools();
        places_ = new SpritePlacesPanel(window_);
        capture_ = new SpriteCapturePanel(window_);
        tools->addTab(places_, QString());
        tools->addTab(capture_, QString());

        if (auto* edit = window_->findChild<QPushButton*>(QStringLiteral("bpEditSprites")))
            connect(edit, &QPushButton::clicked, this, [this] { openEditor(); });
        Document* document = window_->document();
        connect(document, &Document::changed, this, [this] { refresh(); });
        connect(document, &Document::screenChanged, this, [this] { refresh(); });
        window_->installEventFilter(this);
        applyTexts();
        refresh();
    }

protected:
    // The sprites' own windows go when the converter's is closed, and the
    // capture ends: they are of no use without the picture. Not so when the
    // window is hidden as it is destroyed: it is no longer a converter's
    // window then, and what it had is gone.
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Hide && !event->spontaneous() && qobject_cast<ConverterWindow*>(watched)) {
            capture_->stop();
            if (editor_)
                editor_->hide();
        }
        return QObject::eventFilter(watched, event);
    }

private:
    ConverterWindow* window_;
    SpritePlacesPanel* places_;
    SpriteCapturePanel* capture_;
    QPointer<SpriteEditor> editor_;

    // The editor is one window: asked for again, it comes back to the front.
    void openEditor()
    {
        if (!editor_)
            editor_ = new SpriteEditor(window_->document(), window_);
        editor_->present();
    }

    // Each of the two parts under the picture is there when it has a use:
    // sprites are shown on a Plus's picture of mode 0, 1 or 2, and captured
    // in the mode made for it.
    void refresh()
    {
        QTabWidget* tools = window_->tools();
        tools->setTabVisible(tools->indexOf(places_), places_->applies());
        const int captureTab = tools->indexOf(capture_);
        const bool was = tools->isTabVisible(captureTab);
        tools->setTabVisible(captureTab, capture_->applies());
        // The capture's boxes come to the front with its mode.
        if (capture_->applies() && !was)
            tools->setCurrentIndex(captureTab);
        places_->refresh();
        capture_->refresh();
    }

    // The texts of the sprites' parts, in the language the window is in.
    void applyTexts()
    {
        QTabWidget* tools = window_->tools();
        tools->setTabText(tools->indexOf(places_), texts::text("Sprites.tabPlaces"));
        tools->setTabText(tools->indexOf(capture_), texts::text("Sprites.tabCapture"));
        places_->applyTexts();
        capture_->applyTexts();
        if (editor_)
            editor_->applyTexts();
    }
};

}  // namespace

void installSpriteTools(ConverterWindow* window)
{
    registerSpriteTexts();
    new SpriteTools(window);
}

}  // namespace tuxscr
