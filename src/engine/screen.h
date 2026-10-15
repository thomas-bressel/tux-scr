#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "base/bitmap.h"

namespace tuxscr {

// The modes a picture is converted for: the CPC's three, and the ones made
// of them. The numbers are ConvImgCpc's, kept in its files.
enum Mode {
    kMode0 = 0,
    kMode1 = 1,
    kMode2 = 2,
    kModeEgx1 = 3,        // lines of mode 0 and of mode 1 in turn
    kModeEgx2 = 4,        // lines of mode 1 and of mode 2 in turn
    kModeX = 5,           // mode 1, two of its colours changed at each line
    kModeSplit = 6,       // mode 1, colours changed along the line
    kModeAscUt = 7,       // mode 1 made of sixteen patterns of 4 by 4
    kModeAsc0 = 8,        // blocks of four lines, as wide as a pixel of mode 0
    kModeAsc1 = 9,
    kModeAsc2 = 10,
    kModeCaptureSprites = 11,
};
inline constexpr int kModeCount = 12;
extern const char* const kModeNames[kModeCount];

inline constexpr int kMaxColumns = 128;  // bytes across
inline constexpr int kMaxLines = 272;

// The CPC's 27 colours, by their firmware numbers, and the byte the Gate
// Array wants for each (as a letter: '@' + its number).
extern const Rgb kCpcColours[27];
extern const char kCpcVga[28];
// A pen's bits in a byte of mode 0, for the pixel on the left.
extern const int kPenBits[16];

// A colour of the Plus is twelve bits. ConvImgCpc keeps green in the top
// four, then blue, then red; the ASIC wants a word with green, red, blue.
constexpr int plusColour(int red, int green, int blue)
{
    return (green << 8) | (blue << 4) | red;
}
constexpr int plusRed(int colour) { return colour & 0x0F; }
constexpr int plusGreen(int colour) { return (colour >> 8) & 0x0F; }
constexpr int plusBlue(int colour) { return (colour >> 4) & 0x0F; }
constexpr int asicWord(int colour)
{
    return (colour & 0xF00) | ((colour & 0x0F) << 4) | ((colour & 0xF0) >> 4);
}

inline constexpr int kNoColour = 0xFFFF;  // a pen that was given no colour

// On a Plus, a change of palette at a line of the picture, done by an
// interrupt the ASIC raises there. The pens it gives a colour to are
// `defined`; the others keep what they had on the line above.
//
// Giving a pen another colour colours again what that pen draws: the
// picture's pixels keep their pens, which are those the conversion gave
// them with the picture's own palette. It is otherwise for an interrupt
// whose colours were searched in the picture (`matched`): its lines are
// converted with the colours found for them (`matchColours`), and a colour
// changed by hand after that colours again what the pen found draws.
struct PaletteInterrupt {
    int line = 1;
    std::array<int, 16> colours{};
    std::array<bool, 16> defined{};
    bool matched = false;
    std::array<int, 16> matchColours{};
};

// What ConvImgCpc knows of the CPC it converts for (its Cpc class): the
// mode, the size of the picture, the palette, and what the modes of its own
// need.
struct Screen {
    int mode = kMode1;
    bool plus = false;
    int columns = 80;  // bytes across
    int lines = 200;
    // In the EGX modes, which lines take the finer mode: 0 or 2.
    int yEgx = 0;
    // Pens 0 to 15 and the border: firmware colours, or the Plus's twelve
    // bits.
    std::array<int, 17> palette = {1, 24, 20, 6, 26, 0, 2, 7, 10, 12, 14, 16, 18, 22, 1, 14, 1};
    // Mode X and split rasters: the pens' colours at each line.
    std::array<std::array<int, 16>, kMaxLines> lineColours{};
    // The sixteen patterns of mode ASC-UT: pattern, x, y.
    uint8_t patterns[16][4][4] = {};
    // The Plus's hardware sprites: bank, sprite, x, y; and their palette.
    uint8_t sprites[8][16][16][16] = {};
    std::array<int, 16> spritePalette{};

    // The palette interrupts, by rising line, and whether they are in use.
    bool interruptsOn = false;
    std::vector<PaletteInterrupt> interrupts;

    // The picture's size as a PC shows it: a byte is eight pixels across, a
    // line two rows.
    int width() const { return columns << 3; }
    int height() const { return lines << 1; }
    void setWidth(int width) { columns = width >> 3; }
    void setHeight(int height) { lines = height >> 1; }
    // More than a screen of 16K holds.
    bool overscan() const { return lines * columns > 0x3F00; }

    // Where the row `y` of the picture starts in the screen's memory.
    int address(int y) const;
    // The bytes the picture takes in memory.
    int memorySize() const { return columns + address((height() & 0x3F8) - 2); }
    // The width of a pixel at row `y`, in pixels of mode 2 (8 for a byte).
    int pixelWidth(int y = 0) const;
    // The number of pens at row `y`.
    int maxPen(int y = 0) const;
    // The CPC's mode shown at row `y`.
    int lineMode(int y) const;

    // A colour of the palette as a PC shows it: &RRGGBB.
    int rgb(int colour) const;
    Rgb colour(int colour) const;
    Rgb penColour(int pen) const { return colour(palette[static_cast<size_t>(pen)]); }

    // Whether palette interrupts apply: a Plus, one of the CPC's own modes,
    // and at least one of them.
    bool interruptsActive() const { return plus && interruptsOn && mode <= kMode2 && !interrupts.empty(); }
    // The pens' colours at a line of the picture, the interrupts above it
    // done.
    std::array<int, 16> paletteAt(int line) const;
    // The colours a line's pixels are given their pens by: the picture's
    // palette, or the colours searched for the last interrupt above that
    // had its own.
    std::array<int, 16> matchPaletteAt(int line) const;
    // The interrupts by rising line, each at a line of its own inside the
    // picture.
    void tidyInterrupts();
    // The colour of pen `pen` at row `y`, whatever the mode.
    int colourAt(int pen, int y) const;

    // One of the sets of patterns ConvImgCpc comes with (0 to 7), for mode
    // ASC-UT.
    void copyPatterns(int set);
};

// The Plus's colour nearest to one of the CPC's 27, and the other way.
int plusFromCpc(int colour);
int cpcFromPlus(int colour);

}  // namespace tuxscr
