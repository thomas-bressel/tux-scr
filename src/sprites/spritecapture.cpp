#include "spritecapture.h"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "controls.h"
#include "converterwindow.h"
#include "document.h"
#include "pictureview.h"
#include "spritefiles.h"
#include "spritetexts.h"
#include "spriteviews.h"
#include "texts.h"

using namespace tuxscr;

namespace {

// A sprite on the picture: 16 pixels as wide as mode 1's by 16 lines, which
// a PC shows as 32 by 32.
constexpr int kSpriteOnPicture = 32;

}  // namespace

// ---- The capture window ----

SpriteCaptureWindow::SpriteCaptureWindow(Document* document, QWidget* parent)
    : QDialog(parent)
    , document_(document)
{
    setObjectName(QStringLiteral("Capture"));
    auto* layout = new QHBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    auto* column = new QVBoxLayout;
    rbCapt1_ = namedRadio("rbCapt1");
    rbCapt2_ = namedRadio("rbCapt2");
    rbCapt4_ = namedRadio("rbCapt4");
    rbCaptUser_ = namedRadio("rbCaptUser");
    rbCapt1_->setChecked(true);
    column->addWidget(rbCapt1_);
    column->addWidget(rbCapt2_);
    column->addWidget(rbCapt4_);
    auto* user = new QHBoxLayout;
    lblNbX_ = namedLabel("lblNbX");
    txbNbX_ = namedSpin("txbNbX", 1, 16, 1);
    lblNbX_->setBuddy(txbNbX_);
    lblNbY_ = namedLabel("lblNbY");
    txbNbY_ = namedSpin("txbNbY", 1, 16, 1);
    lblNbY_->setBuddy(txbNbY_);
    user->addWidget(rbCaptUser_);
    user->addSpacing(8);
    user->addWidget(lblNbX_);
    user->addWidget(txbNbX_);
    user->addWidget(lblNbY_);
    user->addWidget(txbNbY_);
    user->addStretch();
    column->addLayout(user);
    for (QWidget* widget : {static_cast<QWidget*>(lblNbX_), static_cast<QWidget*>(txbNbX_), static_cast<QWidget*>(lblNbY_),
                            static_cast<QWidget*>(txbNbY_)}) {
        // They keep their place: the window does not change size with them.
        QSizePolicy policy = widget->sizePolicy();
        policy.setRetainSizeWhenHidden(true);
        widget->setSizePolicy(policy);
        widget->setVisible(false);
    }
    column->addStretch(1);

    // Where the sprites go.
    auto* place = new QHBoxLayout;
    label1_ = namedLabel("label1");
    numSprite_ = namedSpin("numSprite", 0, 15, 0);
    label1_->setBuddy(numSprite_);
    place->addWidget(label1_);
    place->addWidget(numSprite_);
    place->addStretch();
    column->addLayout(place);
    auto* bank = new QHBoxLayout;
    comboBanque_ = new QComboBox;
    comboBanque_->setObjectName(QStringLiteral("comboBanque"));
    for (int i = 0; i < kSpriteBanks; ++i)
        comboBanque_->addItem(QString());
    bpCapture_ = namedButton("bpCapture");
    bank->addWidget(comboBanque_);
    bank->addSpacing(12);
    bank->addWidget(bpCapture_);
    bank->addStretch();
    column->addLayout(bank);
    column->addSpacing(16);
    chkAutoCapture_ = namedCheck("chkAutoCapture");
    column->addWidget(chkAutoCapture_);
    column->addStretch(2);
    layout->addLayout(column);

    view_ = new SpriteCaptureView(&document_->screen);
    view_->setObjectName(QStringLiteral("pictCapture"));
    layout->addWidget(view_);

    const auto fixed = [this](QRadioButton* radio, int size, int lastStart) {
        connect(radio, &QRadioButton::toggled, this, [this, size, lastStart](bool on) {
            if (on)
                setSize(size, size, lastStart);
        });
    };
    // One sprite goes anywhere; four start a row of four at the latest; a
    // bank's sixteen start the bank.
    fixed(rbCapt1_, 1, 15);
    fixed(rbCapt2_, 2, 12);
    fixed(rbCapt4_, 4, 0);
    connect(rbCaptUser_, &QRadioButton::toggled, this, [this](bool on) {
        for (QWidget* widget : {static_cast<QWidget*>(lblNbX_), static_cast<QWidget*>(txbNbX_), static_cast<QWidget*>(lblNbY_),
                                static_cast<QWidget*>(txbNbY_)})
            widget->setVisible(on);
        if (on)
            userSize();
    });
    connect(txbNbX_, &QSpinBox::valueChanged, this, &SpriteCaptureWindow::userSize);
    connect(txbNbY_, &QSpinBox::valueChanged, this, &SpriteCaptureWindow::userSize);
    connect(bpCapture_, &QPushButton::clicked, this, &SpriteCaptureWindow::capture);
    retranslate();
}

void SpriteCaptureWindow::retranslate()
{
    setWindowTitle(texts::text("Capture.title"));
    texts::apply(this, "Capture");
    for (int bank = 0; bank < kSpriteBanks; ++bank)
        comboBanque_->setItemText(bank, texts::text("Capture.bank") + QString::number(bank + 1));
    comboBanque_->setAccessibleName(texts::text("EditSprites.tipBank"));
    txbNbX_->setAccessibleName(rbCaptUser_->text() + QStringLiteral(" X"));
    txbNbY_->setAccessibleName(rbCaptUser_->text() + QStringLiteral(" Y"));
    view_->setAccessibleName(texts::text("Capture.namePreview"));
}

void SpriteCaptureWindow::reject()
{
    QDialog::reject();
    emit closed();
}

void SpriteCaptureWindow::setSize(int across, int down, int lastStart)
{
    across_ = across;
    down_ = down;
    numSprite_->setMaximum(lastStart);
    view_->clear(across_, down_);
    emit sizeChanged();
}

// As many sprites across and down as the two boxes say, if the banks from
// the chosen one on have room for them; a box that asks for more is put
// back.
void SpriteCaptureWindow::userSize()
{
    if (!rbCaptUser_->isChecked())
        return;
    const int room = kSpritesPerBank * (kSpriteBanks - comboBanque_->currentIndex());
    int across = across_, down = down_;
    if (txbNbX_->value() * down <= room)
        across = txbNbX_->value();
    if (txbNbY_->value() * across <= room)
        down = txbNbY_->value();
    setQuietly<QSpinBox, int>(txbNbX_, &QSpinBox::setValue, across);
    setQuietly<QSpinBox, int>(txbNbY_, &QSpinBox::setValue, down);
    setSize(across, down, (room - across * down) % kSpritesPerBank);
}

void SpriteCaptureWindow::setCapture(int x, int y, bool withClick)
{
    const Screen& screen = document_->screen;
    const Picture& picture = document_->picture();
    // A pixel of a sprite is one of mode 1's on the picture: two across
    // and two rows down as a PC shows it. Past the picture there is pen 0.
    for (int sy = 0; sy < down_ * kSpriteSize; ++sy)
        for (int sx = 0; sx < across_ * kSpriteSize; ++sx) {
            const int px = x + sx * 2;
            const int py = y + sy * 2;
            const bool inside = px >= 0 && py >= 0 && px < screen.width() && py < screen.height();
            view_->setPen(sx, sy, inside ? picture.pen(px, py) & 0x0F : 0);
        }
    document_->say(texts::text("Capture.shown").arg(x).arg(y));
    if (withClick && chkAutoCapture_->isChecked())
        capture();
}

void SpriteCaptureWindow::capture()
{
    Screen& screen = document_->screen;
    int sprite = numSprite_->value();
    int bank = comboBanque_->currentIndex();
    document_->say(texts::text("Capture.stored").arg(sprite).arg(bank + 1));
    // Of two pens of the same colour, ConvImgCpc knows the first only.
    const auto firstOfColour = [&screen](int pen) {
        for (int other = 0; other < pen; ++other)
            if ((screen.palette[static_cast<size_t>(other)] & 0xFFF) == (screen.palette[static_cast<size_t>(pen)] & 0xFFF))
                return other;
        return pen;
    };
    bool full = false;
    for (int row = 0; row < down_; ++row)
        for (int column = 0; column < across_; ++column) {
            if (bank >= kSpriteBanks) {
                full = true;
                continue;
            }
            for (int y = 0; y < kSpriteSize; ++y)
                for (int x = 0; x < kSpriteSize; ++x) {
                    const int pen = view_->pen(x + column * kSpriteSize, y + row * kSpriteSize);
                    screen.sprites[bank][sprite][x][y]
                        = static_cast<uint8_t>(pen == SpriteCaptureView::kNothing ? 0 : firstOfColour(pen));
                }
            if (++sprite >= kSpritesPerBank) {
                sprite = 0;
                ++bank;
            }
        }
    // The sprites have the picture's colours.
    for (size_t pen = 0; pen < 16; ++pen)
        screen.spritePalette[pen] = screen.palette[pen] >= kNoColour ? 0 : screen.palette[pen] & 0xFFF;

    // The next sprites go after these; after the last sprite of the last
    // bank there is no room left.
    if (bank >= kSpriteBanks) {
        bank = kSpriteBanks - 1;
        sprite = kSpritesPerBank - 1;
    }
    if (numSprite_->maximum() < sprite)
        numSprite_->setMaximum(sprite);
    numSprite_->setValue(sprite);
    comboBanque_->setCurrentIndex(bank);
    if (full)
        document_->say(texts::text("Capture.full"));
    // The sprite editor and the picture show them.
    document_->touch();
}

// ---- The capture's side of the converter's window ----

SpriteCapturePanel::SpriteCapturePanel(ConverterWindow* window)
    : window_(window)
    , document_(window->document())
    , view_(window->pictureView())
{
    setObjectName(QStringLiteral("captureSprites"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    auto* boxes = new QHBoxLayout;
    modeCapture_ = namedCheck("modeCaptureSprites");
    grid_ = namedCheck("chkGrilleSprite");
    boxes->addWidget(modeCapture_);
    boxes->addSpacing(12);
    boxes->addWidget(grid_);
    boxes->addStretch();
    layout->addLayout(boxes);
    hint_ = new QLabel;
    hint_->setObjectName(QStringLiteral("lblCaptureHint"));
    hint_->setWordWrap(true);
    layout->addWidget(hint_);
    layout->addStretch();

    connect(modeCapture_, &QCheckBox::toggled, this, &SpriteCapturePanel::captureToggled);
    connect(grid_, &QCheckBox::toggled, this, [this] {
        settingsReach();
        redraw();
    });
    connect(view_, &PictureView::mouse, this, &SpriteCapturePanel::mouse);
    connect(view_, &PictureView::mouseLeft, this, [this] {
        if (corner_.x() < 0)
            return;
        corner_ = QPoint(-1, -1);
        redraw();
    });
    connect(view_, &PictureView::arrow, this, &SpriteCapturePanel::arrow);
    overlay_ = view_->addOverlay([this](QPainter& painter) { paint(painter); });
    retranslate();
}

SpriteCapturePanel::~SpriteCapturePanel()
{
    // The picture may be gone before its tools.
    if (view_)
        view_->removeOverlay(overlay_);
}

void SpriteCapturePanel::redraw()
{
    if (view_)
        view_->update();
}

bool SpriteCapturePanel::applies() const
{
    return document_->screen.mode == kModeCaptureSprites;
}

void SpriteCapturePanel::retranslate()
{
    modeCapture_->setText(texts::text("ImageCpc.modeCaptureSprites"));
    grid_->setText(texts::text("ImageCpc.chkGrilleSprite"));
    hint_->setText(texts::text("Sprites.captureHint"));
    if (capture_)
        capture_->retranslate();
}

void SpriteCapturePanel::stop()
{
    modeCapture_->setChecked(false);
}

void SpriteCapturePanel::refresh()
{
    // In another mode there is nothing to capture, nor a grid to show.
    if (!applies()) {
        modeCapture_->setChecked(false);
        grid_->setChecked(false);
    }
    if (capture_)
        capture_->update();
}

// While sprites are captured or the grid is shown, the settings are out of
// reach: the picture stays as it is.
void SpriteCapturePanel::settingsReach()
{
    const bool hold = modeCapture_->isChecked() || grid_->isChecked();
    if (hold == holdsSettings_)
        return;
    holdsSettings_ = hold;
    window_->holdSettings(this, hold);
}

void SpriteCapturePanel::captureToggled(bool on)
{
    if (on) {
        if (window_->mouseOwner() && window_->mouseOwner() != this) {
            // Another tool has the picture's mouse.
            document_->say(texts::text("Sprites.mouseBusy"));
            setQuietly<QAbstractButton, bool>(modeCapture_, &QAbstractButton::setChecked, false);
            return;
        }
        window_->setMouseOwner(this);
        capture_ = new SpriteCaptureWindow(document_, window_);
        connect(capture_, &SpriteCaptureWindow::closed, this, [this] { modeCapture_->setChecked(false); });
        connect(capture_, &SpriteCaptureWindow::sizeChanged, this, [this] { redraw(); });
        capture_->show();
        // The arrow keys and Space are the picture's.
        if (view_)
            view_->setFocus();
    } else {
        if (capture_) {
            capture_->hide();
            capture_->deleteLater();
            capture_ = nullptr;
        }
        if (window_->mouseOwner() == this)
            window_->setMouseOwner(nullptr);
        corner_ = QPoint(-1, -1);
        clicked_ = false;
    }
    settingsReach();
    redraw();
}

// Whether the picture's mouse and keys are the capture's now. A tool that
// had taken them and gave them back leaves them to the capture, which is
// still on.
bool SpriteCapturePanel::hasMouse()
{
    if (!capture_)
        return false;
    if (!window_->mouseOwner())
        window_->setMouseOwner(this);
    return window_->mouseOwner() == this;
}

void SpriteCapturePanel::mouse(QPoint at, Qt::MouseButtons buttons)
{
    if (!hasMouse())
        return;
    // The rectangle's corner is on a pixel of mode 1 and on a line.
    const QPoint here(std::max(0, at.x()) & 0xFFE, std::max(0, at.y()) & 0xFFE);
    if (corner_.x() < 0)
        corner_ = here;
    if (buttons == Qt::LeftButton) {
        // What the rectangle held when the button went down, and as it is
        // dragged; only the first counts as a click.
        capture_->setCapture(corner_.x(), corner_.y(), !clicked_);
        clicked_ = true;
    } else {
        clicked_ = false;
    }
    corner_ = here;
    redraw();
}

void SpriteCapturePanel::arrow(int dx, int dy, bool choose)
{
    if (!hasMouse())
        return;
    const Screen& screen = document_->screen;
    if (corner_.x() < 0) {
        corner_ = QPoint(0, 0);
    } else {
        if (choose)
            capture_->setCapture(corner_.x(), corner_.y());
        // A pixel of mode 1, or a line, at a time.
        corner_ = QPoint(std::clamp(corner_.x() + dx * 2, 0, std::max(0, screen.width() - 2)),
                         std::clamp(corner_.y() + dy * 2, 0, std::max(0, screen.height() - 2)));
    }
    redraw();
}

void SpriteCapturePanel::paint(QPainter& painter)
{
    if (!applies())
        return;
    const Screen& screen = document_->screen;
    if (grid_->isChecked()) {
        for (int x = 0; x < screen.width(); x += kSpriteOnPicture)
            spriteMarkLine(painter, QPointF(x, 0), screen.height(), false);
        for (int y = 0; y < screen.height(); y += kSpriteOnPicture)
            spriteMarkLine(painter, QPointF(0, y), screen.width(), true);
    }
    // The rectangle is heavier than the grid, which it may lie on.
    if (capture_ && corner_.x() >= 0)
        spriteMarkFrame(painter, QRectF(corner_.x(), corner_.y(), capture_->across() * kSpriteOnPicture, capture_->down() * kSpriteOnPicture), 2);
}
