#pragma once

#include <array>
#include <vector>

#include "base/bitmap.h"
#include "convert/param.h"
#include "convert/picture.h"
#include "convert/screen.h"

namespace tuxscr {

struct Conversion {
    // The CPC's colours the picture was found to have, before it was brought
    // to the number of pens.
    int colours = 0;
    // In mode X and with split rasters: the colours the screen ends up
    // showing.
    int splitColours = 0;
};

// Converts `source`, a picture of the size the screen has on a PC
// (screen.width() by screen.height()), into `picture`. The source is changed
// on the way: its colours are brought to the CPC's, and dithered.
//
// The palette is searched for the pens that are not locked (param.lockState)
// and written to the screen; with palette interrupts, each line is drawn
// with the pens it has. `searchInterrupt`, when it is the number of one of
// them, has that interrupt's colours searched too, in the lines it covers.
Conversion convert(Bitmap& source, Screen& screen, Picture& picture, const Param& param, int searchInterrupt = -1);

// What the screen is, said to the settings too: the settings' files keep
// it there.
void screenIntoParam(const Screen& screen, Param& param);
// From a CPC's palette to a Plus's and back: the screen's colours brought to
// the nearest the other machine has.
void changeMachine(Screen& screen, bool plus);

// A pattern of 4 by 4 pens, for mode ASC-UT.
struct Pattern {
    uint8_t pens[4][4] = {};  // x, y
    int found = 0;            // how many blocks of the pictures are like it
};

// Converts `source` to mode 1 and lists the blocks of 4 by 4 it is made of,
// added to `patterns`: a block within four pixels of a pattern already
// listed counts as that one. The sixteen patterns of mode ASC-UT are then
// the ones found most.
void findPatterns(Bitmap& source, Screen& screen, const Param& param, std::vector<Pattern>& patterns);

}  // namespace tuxscr
