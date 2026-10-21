#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "base/bitmap.h"
#include "convert/picture.h"
#include "convert/screen.h"
#include "formats/asmtext.h"

// Split rasters: on a CPC, a pen given other colours along a line by a
// program that writes them to the Gate Array while the line is drawn. This
// is what ConvImgCpc's "Split Editor" works on: the lines and their splits
// (its LigneSplit.cs), and the overscan screen that shows what they give
// (the parts of its BitmapCpc.cs made for them).

namespace tuxscr {

// A colour the pen takes on a line, and how long it keeps it.
struct Split {
    // In pixels of mode 1, of which the program's NOP is eight: 32 at
    // least, the time it takes to write a colour.
    int length = 32;
    // A firmware number, 0 to 26.
    int colour = 0;
    bool enabled = false;

    bool operator==(const Split&) const = default;
};

inline constexpr int kSplitsPerLine = 7;

// A line of the screen: the pen whose colour changes, how far into the line
// the first change falls, and up to seven colours one after the other. A
// split counts only if those before it are enabled.
struct SplitLine {
    std::array<Split, kSplitsPerLine> splits;
    int pen = 0;
    int delay = 0;
    // The mode the line under this one is shown in, if it is to change.
    int newMode = 0;
    bool changeMode = false;

    bool operator==(const SplitLine&) const = default;
};

using SplitLines = std::array<SplitLine, kMaxLines>;

// The lines of an editor just opened: no split in use, and the earliest
// delay there is.
SplitLines freshSplitLines();

// The lines as ConvImgCpc saves them (.xml): what C#'s serializer writes of
// its SplitEcran, which ConvImgCpc reads back.
std::string splitXml(const SplitLines& lines);
// Reads such a file: false if it is not one. Lines and splits the file does
// not have are left as they are; a number the editor cannot hold is brought
// to the nearest it can.
bool readSplitXml(const std::string& xml, SplitLines& lines);

// The screen of the split editor: an overscan one of 96 bytes by 272 lines,
// its memory, and for each byte of each line the colours the pens have
// there once the splits are done.
class SplitScreen {
public:
    static constexpr int kColumns = 96;
    static constexpr int kLines = kMaxLines;
    // The earliest a line's first change can fall.
    static constexpr int kMinDelay = -4;
    // The lines the program changes colours on: those under are left to the
    // end of the frame.
    static constexpr int kProgramLines = 262;

    // Every line starts with its seven splits off and the earliest delay;
    // the pens have the colours given, the screen's own.
    explicit SplitScreen(const std::array<int, 17>& colours = Screen().palette);

    SplitLines lines;
    // The screen's memory: 64K, the picture's own from 0.
    std::vector<uint8_t> memory;
    // The palette the screen starts with, before any split: firmware
    // numbers. A colour that is none of the CPC's 27 is shown as the first.
    std::array<int, 17> startPalette;
    // The pens' colours at each byte of each line, the splits done:
    // [x][y][pen].
    asmgen::SplitPalette palette;
    // The CPC's mode at each line.
    std::array<int, kLines> lineModes{};

    void setStartColour(int pen, int colour) { startPalette[static_cast<size_t>(pen & 15)] = colour; }
    int startColour(int pen) const { return startPalette[static_cast<size_t>(pen & 15)]; }
    // A colour of the starting palette as a PC shows it.
    Rgb startRgb(int pen) const;

    // Takes a picture as the screen's memory: what it has outside the
    // screen it was made for is pen 0. Each line is in the mode the picture
    // has there, unless the line above it asks for another.
    void setPicture(const Screen& screen, const Picture& picture);

    // Works out the pens' colours along every line from the splits, and the
    // lines' modes.
    void calcPalette();
    // The colour of a pen at a byte of a line.
    Rgb paletteColour(int x, int y, int pen) const;
    // Draws the screen as a PC shows it, 768 by 544, from the byte
    // `offsetX >> 3` of each line on.
    void render(Bitmap& bitmap, int offsetX = 0) const;

    // Where the colour changes on a line, in bytes from the left: the first
    // change, then the end of each split in use.
    std::vector<int> changes(int line) const;

private:
    std::array<int, kLines> pictureModes_{};

    int applyPalette(int x, int y, int until, const std::array<int, 16>& colours);
};

// What reading a picture of splits found that the lines cannot hold.
struct SplitImportNote {
    enum class Kind {
        TooMany,   // more than seven colours on the line: seven are kept
        TooShort,  // a colour shorter than 32 pixels: it goes with the one before
    };
    Kind kind;
    int line;
};

// Makes the splits of a picture that shows them: 768 by 544, each line of
// the CPC two rows, each of its bytes eight pixels, in the CPC's colours or
// near them. `pen` is the pen the splits are for. ConvImgCpc's
// ImportSplit.ValideSplit.
std::vector<SplitImportNote> importSplits(const Bitmap& picture, int pen, SplitLines& lines);

// The CPC's colour nearest to a colour of a PC, each of red, green and blue
// being none, half or full.
int nearestCpcColour(Rgb colour);

}  // namespace tuxscr
