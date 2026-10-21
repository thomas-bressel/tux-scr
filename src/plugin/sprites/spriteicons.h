#pragma once

#include <QColor>
#include <QIcon>

namespace tuxscr {

// The pictograms of the sprite editor's buttons, after ConvImgCpc's: they
// are drawn in code in the colour of the buttons' text, for the look in
// use, and are to be drawn again when the look changes.

enum class SpriteIcon {
    Copy,
    Paste,
    // The top goes to the bottom; the left goes to the right.
    FlipDown,
    FlipAcross,
    Rotate,
    Clear,
    Load,
    Save,
    Previous,
    Next,
};

QIcon spriteIcon(SpriteIcon icon, const QColor& ink);

}  // namespace tuxscr
