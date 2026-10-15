#include "splittools.h"

#include <QPointer>
#include <QPushButton>

#include "converterwindow.h"
#include "palettemaker.h"
#include "rastertable.h"
#include "spliteditor.h"
#include "splittexts.h"

using namespace tuxscr;

namespace {

// What the converter's window is given: the split editor and the raster
// table, of which one each at most is open, stay while the user works and
// follow the picture; the maker of palettes asks and goes.
class SplitTools : public QObject {
public:
    explicit SplitTools(ConverterWindow* window)
        : QObject(window)
        , window_(window)
    {
        if (auto* button = window->findChild<QPushButton*>(QStringLiteral("bpEditSplit")))
            connect(button, &QPushButton::clicked, this, [this] { openEditor(); });
        if (auto* button = window->findChild<QPushButton*>(QStringLiteral("bpRasterPlus")))
            connect(button, &QPushButton::clicked, this, [this] { openRasters(); });
        if (auto* button = window->findChild<QPushButton*>(QStringLiteral("bpGenPal")))
            connect(button, &QPushButton::clicked, this, [this] { makePalette(); });
        // The picture is drawn again: so are the windows that show it.
        connect(window->document(), &Document::changed, this, [this] {
            if (rasters_)
                rasters_->drawLines();
            if (editor_)
                editor_->updateScreen();
        });
        connect(window, &ConverterWindow::languageChanged, this, [this] {
            if (rasters_)
                rasters_->retranslate();
            if (editor_)
                editor_->retranslate();
        });
    }

private:
    ConverterWindow* window_;
    QPointer<SplitEditor> editor_;
    QPointer<RasterTable> rasters_;

    static void bringUp(QWidget* window)
    {
        window->show();
        window->raise();
        window->activateWindow();
    }

    void openEditor()
    {
        if (!editor_) {
            editor_ = new SplitEditor(window_->document(), window_);
            editor_->setAttribute(Qt::WA_DeleteOnClose);
        }
        bringUp(editor_);
    }

    void openRasters()
    {
        if (!rasters_) {
            // Which of the picture's colours the rasters are shown in place
            // of is asked first, once.
            const Screen& screen = window_->document()->screen;
            SelectColourDialog which(screen, window_);
            which.exec();
            const int rastered = which.chosen() < 0 ? -1 : screen.rgb(screen.palette[static_cast<size_t>(which.chosen())]);
            rasters_ = new RasterTable(window_->document(), rastered, window_);
            rasters_->setAttribute(Qt::WA_DeleteOnClose);
        }
        bringUp(rasters_);
    }

    void makePalette()
    {
        // The colours made are the picture's: every pen keeps its own, and
        // the picture is converted again with them.
        Document* document = window_->document();
        PaletteMaker maker(document->screen.palette.data(), 0,
                           [document] {
                               document->param.lockState.fill(1);
                               document->screen.tidyInterrupts();
                               document->convert(false);
                           },
                           window_);
        maker.exec();
    }
};

}  // namespace

void installSplitTools(ConverterWindow* window)
{
    if (!window)
        return;
    registerSplitTexts();
    new SplitTools(window);
}
