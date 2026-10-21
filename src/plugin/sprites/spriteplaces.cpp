#include "spriteplaces.h"

#include <algorithm>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "controls.h"
#include "converterwindow.h"
#include "document.h"
#include "prompts.h"
#include "spritefiles.h"
#include "texts.h"

namespace tuxscr {

SpritePlacesPanel::SpritePlacesPanel(ConverterWindow* window)
    : window_(window)
    , document_(window->document())
    , view_(window->pictureView())
{
    setObjectName(QStringLiteral("groupBoxPosSprite"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(4);
    layout->addLayout(grid);

    chkAfficheSH_ = namedCheck("chkAfficheSH");
    const char* const labelNames[5] = {"label3", "label5", "label6", "label7", "label8"};
    for (int i = 0; i < 5; ++i)
        labels_[i] = namedLabel(labelNames[i]);
    numSprite_ = namedSpin("numSprite", 0, 15, 0);
    numZoomX_ = namedSpin("numZoomX", 0, 3, 0);
    numZoomY_ = namedSpin("numZoomY", 0, 3, 0);
    numPosX_ = namedSpin("numPosX", 0, 1024, 0);
    numPosY_ = namedSpin("numPosY", 0, 272, 0);
    QSpinBox* const spins[5] = {numSprite_, numZoomX_, numZoomY_, numPosX_, numPosY_};
    for (int i = 0; i < 5; ++i)
        labels_[i]->setBuddy(spins[i]);
    chkCopyZoom_ = namedCheck("chkCopyZoom");
    bpSaveCoord_ = namedButton("bpSaveCoord");
    bpReadCoord_ = namedButton("bpReadCoord");
    bpCopiePosSprites_ = namedButton("bpCopiePosSprites");

    grid->addWidget(chkAfficheSH_, 0, 0);
    grid->setColumnMinimumWidth(1, 12);
    grid->addWidget(labels_[0], 0, 2);
    grid->addWidget(numSprite_, 0, 3);
    grid->setColumnMinimumWidth(4, 12);
    grid->addWidget(labels_[1], 0, 5);
    grid->addWidget(numZoomX_, 0, 6);
    grid->addWidget(labels_[2], 1, 5);
    grid->addWidget(numZoomY_, 1, 6);
    grid->addWidget(chkCopyZoom_, 0, 7, 2, 1);
    grid->setColumnMinimumWidth(8, 12);
    grid->addWidget(labels_[3], 0, 9);
    grid->addWidget(numPosX_, 0, 10);
    grid->addWidget(labels_[4], 1, 9);
    grid->addWidget(numPosY_, 1, 10);
    grid->setColumnMinimumWidth(11, 12);
    grid->addWidget(bpSaveCoord_, 0, 12);
    grid->addWidget(bpReadCoord_, 1, 12);
    grid->addWidget(bpCopiePosSprites_, 0, 13);
    grid->setColumnStretch(14, 1);
    hint_ = new QLabel;
    hint_->setObjectName(QStringLiteral("lblPlacesHint"));
    hint_->setWordWrap(true);
    layout->addWidget(hint_);
    layout->addStretch();

    connect(chkAfficheSH_, &QCheckBox::toggled, this, [this](bool on) {
        document_->showSprites = on;
        redraw();
    });
    connect(numSprite_, &QSpinBox::valueChanged, this, &SpritePlacesPanel::showChosen);
    // A zoom may be given to the sprites that follow too.
    const auto zoomChanged = [this](int* zooms, int value) {
        const int chosen = numSprite_->value();
        zooms[chosen] = value;
        if (chkCopyZoom_->isChecked())
            for (int i = chosen + 1; i < 16; ++i)
                zooms[i] = value;
        redraw();
    };
    connect(numZoomX_, &QSpinBox::valueChanged, this, [this, zoomChanged](int value) { zoomChanged(document_->spritePlaces.zoomX, value); });
    connect(numZoomY_, &QSpinBox::valueChanged, this, [this, zoomChanged](int value) { zoomChanged(document_->spritePlaces.zoomY, value); });
    connect(numPosX_, &QSpinBox::valueChanged, this, [this](int value) {
        document_->spritePlaces.x[numSprite_->value()] = value;
        redraw();
    });
    connect(numPosY_, &QSpinBox::valueChanged, this, [this](int value) {
        document_->spritePlaces.y[numSprite_->value()] = value;
        redraw();
    });
    connect(bpCopiePosSprites_, &QPushButton::clicked, this, &SpritePlacesPanel::copyPlaces);
    connect(bpReadCoord_, &QPushButton::clicked, this, &SpritePlacesPanel::readPlaces);
    connect(bpSaveCoord_, &QPushButton::clicked, this, &SpritePlacesPanel::savePlaces);

    view_->installEventFilter(this);
    overlay_ = view_->addOverlay([this](QPainter& painter) { paint(painter); });
    applyTexts();
    refresh();
}

SpritePlacesPanel::~SpritePlacesPanel()
{
    // The picture may be gone before its tools.
    if (view_)
        view_->removeOverlay(overlay_);
}

bool SpritePlacesPanel::applies() const
{
    return document_->screen.plus && document_->screen.mode <= kMode2;
}

void SpritePlacesPanel::applyTexts()
{
    chkAfficheSH_->setText(texts::text("Sprites.chkAfficheSH"));
    const char* const labelTexts[5] = {"Sprites.label3", "Sprites.label5", "Sprites.label6", "Sprites.label7", "Sprites.label8"};
    for (int i = 0; i < 5; ++i)
        labels_[i]->setText(texts::text(labelTexts[i]));
    chkCopyZoom_->setText(texts::text("Sprites.chkCopyZoom"));
    bpSaveCoord_->setText(texts::text("Sprites.bpSaveCoord"));
    bpReadCoord_->setText(texts::text("Sprites.bpReadCoord"));
    bpCopiePosSprites_->setText(texts::text("Sprites.bpCopiePosSprites"));
    bpSaveCoord_->setToolTip(texts::text("Sprites.tipSaveCoord"));
    bpReadCoord_->setToolTip(texts::text("Sprites.tipReadCoord"));
    bpSaveCoord_->setAccessibleName(bpSaveCoord_->toolTip());
    bpReadCoord_->setAccessibleName(bpReadCoord_->toolTip());
    hint_->setText(texts::text("Sprites.placesHint"));
}

void SpritePlacesPanel::redraw()
{
    if (view_)
        view_->update();
}

void SpritePlacesPanel::refresh()
{
    // A file that was read may have hidden the sprites.
    setQuietly<QAbstractButton, bool>(chkAfficheSH_, &QAbstractButton::setChecked, document_->showSprites);
    showChosen();
    redraw();
}

// The chosen sprite's zoom and place, in their boxes.
void SpritePlacesPanel::showChosen()
{
    const Document::SpritePlaces& places = document_->spritePlaces;
    const int chosen = numSprite_->value();
    setQuietly<QSpinBox, int>(numZoomX_, &QSpinBox::setValue, places.zoomX[chosen]);
    setQuietly<QSpinBox, int>(numZoomY_, &QSpinBox::setValue, places.zoomY[chosen]);
    setQuietly<QSpinBox, int>(numPosX_, &QSpinBox::setValue, places.x[chosen]);
    setQuietly<QSpinBox, int>(numPosY_, &QSpinBox::setValue, places.y[chosen]);
}

void SpritePlacesPanel::paint(QPainter& painter)
{
    if (!document_->showSprites || !applies())
        return;
    const Screen& screen = document_->screen;
    const Document::SpritePlaces& places = document_->spritePlaces;
    painter.setClipRect(QRect(0, 0, screen.width(), screen.height()));
    for (int sprite = 0; sprite < kSpritesPerBank; ++sprite) {
        // A pixel of a sprite is as wide as one of mode 2 and a line high,
        // once, twice or four times: on the picture a line is two rows.
        const int width = spriteMagnification(places.zoomX[sprite]);
        const int height = spriteMagnification(places.zoomY[sprite]) * 2;
        if (width == 0 || height == 0)
            continue;
        const int left = places.x[sprite];
        const int top = places.y[sprite] * 2;
        for (int y = 0; y < kSpriteSize; ++y)
            for (int x = 0; x < kSpriteSize; ++x) {
                // Pen 0 lets the picture show.
                const int pen = screen.sprites[0][sprite][x][y];
                if (pen != 0)
                    painter.fillRect(left + x * width, top + y * height, width, height,
                                     spriteColour(screen.spritePalette[static_cast<size_t>(pen & 0x0F)]));
            }
    }
}

bool SpritePlacesPanel::eventFilter(QObject* watched, QEvent* event)
{
    // The mouse's events only: the picture is not asked about while it is
    // made or goes.
    const QEvent::Type type = event->type();
    if ((type == QEvent::MouseButtonPress || type == QEvent::MouseMove || type == QEvent::MouseButtonRelease) && watched == view_.data()) {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        const QObject* owner = window_->mouseOwner();
        if (mouse->buttons() == Qt::RightButton && document_->showSprites && applies() && (!owner || owner == this)) {
            // The window is told before it sees the button: it leaves the
            // source's size alone.
            window_->setMouseOwner(this);
            // The pixel of the picture under the mouse, whatever the size
            // the picture is shown at.
            const QPoint where = mouse->position().toPoint();
            const QPoint at(where.x() / view_->scale(), where.y() / view_->scale());
            if (where.x() >= 0 && where.y() >= 0 && at.x() < document_->screen.width() && at.y() < document_->screen.height()) {
                numPosX_->setValue(at.x());
                numPosY_->setValue(at.y() / 2);
            }
        } else if (owner == this) {
            window_->setMouseOwner(nullptr);
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ---- The places as a text and as a file ----

void SpritePlacesPanel::copyPlaces()
{
    QApplication::clipboard()->setText(QString::fromStdString(spritePlacesText(document_->spritePlaces)));
    prompts::inform(window_, texts::text("Sprites.copied"));
}

void SpritePlacesPanel::readPlaces()
{
    const QString path = prompts::openName(window_, texts::text("Sprites.tipReadCoord"), QString::fromStdString(document_->param.lastReadPath),
                                           {texts::text("Sprites.xmlFilter")});
    if (path.isEmpty())
        return;
    const std::optional<std::vector<uint8_t>> file = readHostFile(path);
    if (!file || !readSpritePlacesXml(std::string(file->begin(), file->end()), document_->spritePlaces)) {
        document_->fail(texts::text("Sprites.readError"));
        return;
    }
    document_->param.lastReadPath = QFileInfo(path).absolutePath().toStdString();
    showChosen();
    redraw();
}

void SpritePlacesPanel::savePlaces()
{
    const QString path = prompts::saveName(window_, texts::text("Sprites.tipSaveCoord"), QString::fromStdString(document_->param.lastSavePath),
                                           {texts::text("Sprites.xmlFilter")});
    if (path.isEmpty())
        return;
    if (!writeHostText(path, spritePlacesXml(document_->spritePlaces))) {
        document_->fail(texts::text("Sprites.saveError"));
        return;
    }
    document_->param.lastSavePath = QFileInfo(path).absolutePath().toStdString();
}

}  // namespace tuxscr
