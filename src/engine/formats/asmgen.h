#pragma once

#include <string>

#include "formats/asmtext.h"

// The assembler sources ConvImgCpc writes: data, the routines that unpack
// and show a picture, palettes. The functions are ConvImgCpc's (SaveAsm.cs,
// GenSplitAsm.cs), turned into C++ by tools/make_asm.py, and keep its names.

namespace tuxscr::asmgen {

#include "asmgen_decl.inc"

// The lines a source starts with: what wrote it and when, the mode, and
// with `withSize` the picture's size. `stamp` is the date and time, as
// "dd/MM/yyyy (HH mm ss)"; a source is given none when `version` is null.
void WriteHeader(AsmText& sw, const Text& version = Text(), bool withSize = false, const std::string& stamp = {});

// The date and time now, as a source's first line gives them.
std::string timeStamp();

// ---- Palette interrupts (TuxSCR's own) ----

// The routine that shows a picture of the Plus whose palette changes at
// lines of it: as GenereAfficheStd, with an interrupt at each of those
// lines that gives the pens their new colours.
void GenereAfficheInterrupts(AsmText& sw, bool overscan, PackMethod pkMethode, const Text& labelMedia,
                             const Text& labelPalette);
// The bytes that routine takes over what GenereAfficheStd's does.
int interruptsCodeSize(const Screen& screen);

// The lines of the picture where the palette changes and the palette from
// each, as a text to read or to assemble.
std::string paletteInterruptsText(const Screen& screen, const std::string& version, const std::string& stamp);

}  // namespace tuxscr::asmgen
