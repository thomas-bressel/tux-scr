#pragma once

#include <QString>

namespace tuxscr {

// The texts of the sprites' windows that ConvImgCpc's table does not have:
// the captions its designer gives the controls, which it shows in French and
// in English alike, and what TuxSCR adds to them (what a button with a
// pictogram does, said to those who point at it or do not see it).

// Puts them in the table of texts. Done once, before a window is made.
void registerSpriteTexts();

// A text of the table in the language in use, its line ends taken away:
// ConvImgCpc breaks some captions in two for its narrow buttons.
QString spriteText(const char* name);

}  // namespace tuxscr
