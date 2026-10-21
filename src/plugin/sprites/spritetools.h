#pragma once

namespace tuxscr {

class ConverterWindow;

// Gives the converter's window the Plus's hardware sprites: their editor, their capture from the picture, their places on it.
// Called once the window is made, by those who make it.
void installSpriteTools(ConverterWindow* window);

}  // namespace tuxscr
