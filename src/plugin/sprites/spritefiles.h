#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <QColor>

#include "document.h"
#include "convert/screen.h"
#include "formats/pack.h"
#include "formats/screenfile.h"

// The files of the Plus's hardware sprites, as ConvImgCpc's sprite editor
// writes them and reads them back: the sprites themselves (.SPR, the format
// of Impdraw V2: a sprite is 256 bytes, a pen for each pixel, line after
// line), assembler sources of three kinds, and the places of the sprites on
// the picture (.XML). Nothing here touches the disc: files are given and
// returned as bytes or text.

namespace tuxscr {

inline constexpr int kSpriteBanks = 8;
inline constexpr int kSpritesPerBank = 16;
inline constexpr int kSpriteSize = 16;                        // pixels across and down
inline constexpr int kSpriteBytes = kSpriteSize * kSpriteSize;

// A colour of the sprites' palette as a PC shows it. The palette is the
// Plus's whatever the picture is converted for.
inline QColor spriteColour(int colour)
{
    if (colour < 0 || colour >= kNoColour)
        return QColor(Qt::black);
    return QColor(plusRed(colour) * 17, plusGreen(colour) * 17, plusBlue(colour) * 17);
}

// How many times its size the ASIC shows a sprite, across or down, for the
// two bits of zoom its register has: 0 hides it, 1 and 2 show it once and
// twice its size, and 3 four times.
inline constexpr int spriteMagnification(int zoom)
{
    return zoom <= 0 ? 0 : zoom >= 3 ? 4 : zoom;
}

// The sprites a file takes: those of one bank, or of all the banks up to the
// last that has something drawn.
struct SpriteSpan {
    int firstBank = 0;
    // The banks a .SPR holds whole.
    int banks = 1;
    // The sprites from the first of these banks to the last that is not
    // empty: what a source holds.
    int sprites = 1;
};
SpriteSpan spriteSpan(const Screen& screen, int bank, bool allBanks);

// The pens of the span's banks, 256 bytes a sprite.
std::vector<uint8_t> hardSpriteBytes(const Screen& screen, const SpriteSpan& span);
// The same as a file of the CPC's, with its header; `fileName` gives the
// header its name.
std::vector<uint8_t> hardSpriteFile(const std::string& fileName, const Screen& screen, const SpriteSpan& span);
// Reads such a file: one bank goes to the bank `bank`, more go to the first
// and those after it. False if the file is not one.
bool readHardSpriteFile(const std::vector<uint8_t>& file, Screen& screen, int bank);

// The labels a source gives the sprites, their palette and the table of
// their addresses; an empty one is not written.
struct SpriteLabels {
    std::string media;
    std::string palette;
    std::string pointers;
    // The table of addresses ends with a zero.
    bool zeroPointer = false;
};

// The sprites as they are: sixteen lines of sixteen bytes each, a label
// before each sprite.
std::string hardSpritesAsm(const Screen& screen, const SpriteSpan& span, const SpriteLabels& labels, bool withPalette,
                           const AsmStamp& stamp);
// Each sprite packed by itself under its label, then the table of their
// addresses.
std::string packedSpritesAsm(const Screen& screen, const SpriteSpan& span, const SpriteLabels& labels, bool withPalette,
                             PackMethod method, const AsmStamp& stamp);
// The first `count` sprites of the span packed as one block.
std::string packedSpriteSetAsm(const Screen& screen, const SpriteSpan& span, int count, const SpriteLabels& labels,
                               bool withPalette, PackMethod method, const AsmStamp& stamp);

// ---- The sprites' places on the picture ----

// What "Copy positions to the clipboard" copies: for each sprite, its place
// and the byte of its zoom, as a source to assemble.
std::string spritePlacesText(const Document::SpritePlaces& places);
// The places as C#'s serializer writes ConvImgCpc's PosSprite.
std::string spritePlacesXml(const Document::SpritePlaces& places);
// Reads such a file: false, the places left as they were, if it is not one.
bool readSpritePlacesXml(const std::string& xml, Document::SpritePlaces& places);

}  // namespace tuxscr
