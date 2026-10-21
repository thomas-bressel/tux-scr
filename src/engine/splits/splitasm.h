#pragma once

#include <string>

#include "formats/pack.h"
#include "splits/splitscreen.h"

namespace tuxscr {

// The program that shows a split screen, as an assembler source: the
// picture packed, the routine that unpacks it and sets an overscan screen,
// and for each line the colours written to the Gate Array at their places,
// timed to the microsecond. It is ConvImgCpc's GenSplitAsm.CreeAsm, text
// for text. The colours along the lines are worked out again first: the
// palette the program starts with is read from them.
std::string splitProgram(SplitScreen& screen, PackMethod method, const PackProgress& progress = {});

}  // namespace tuxscr
