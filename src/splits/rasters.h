#pragma once

#include <array>
#include <string>

#include "engine/screen.h"

// The Plus's raster table: a colour for each line of an overscan picture,
// which a program gives to a pen as the lines go by. It is ConvImgCpc's
// Raster (RasterTablePlus.cs), with what its windows work out of colours
// that go from one to another (GenPalette.cs).

namespace tuxscr {

// A line's colour is the word the ASIC wants: green, red, blue, four bits
// each. (A pen's colour in a palette has red and blue the other way round.)
constexpr int rasterWord(int red, int green, int blue)
{
    return blue + (red << 4) + (green << 8);
}
constexpr int rasterRed(int word) { return (word & 0xF0) >> 4; }
constexpr int rasterGreen(int word) { return word >> 8; }
constexpr int rasterBlue(int word) { return word & 0x0F; }
// As a PC shows it: &RRGGBB.
constexpr int rasterRgb(int word)
{
    return (rasterBlue(word) * 17) + ((rasterGreen(word) * 17) << 8) + ((rasterRed(word) * 17) << 16);
}

struct Raster {
    std::array<int, kMaxLines> line{};
    // The lines that are written out.
    int minLine = 0;
    int maxLine = 0;

    bool operator==(const Raster&) const = default;
};

// The table as ConvImgCpc saves it (.xml): what C#'s serializer writes of
// its Raster, which ConvImgCpc reads back.
std::string rasterXml(const Raster& raster);
// Reads such a file: false if it is not one. Lines the file does not have
// are black.
bool readRasterXml(const std::string& xml, Raster& raster);

// The lines moved up or down by `count` (1 to 271), those that come in
// being black.
void scrollRaster(Raster& raster, int count, bool up);

// The lines from `start` to `end` as the words of a source, sixteen to a
// line of it: what "Copy to clipboard" takes.
std::string rasterData(const Raster& raster, int start, int end);
// The same as a source of its own.
std::string rasterAsm(const Screen& screen, const Raster& raster, int start, int end);
// Sixteen tables that take the lines from black to their colours: Fade_0,
// all black, to Fade_15, the lines themselves.
std::string rasterFadeAsm(const Screen& screen, const Raster& raster, int start, int end);

// Red, green and blue of a colour of the Plus, each from 0 to under 16.
struct Shade {
    double red = 0;
    double green = 0;
    double blue = 0;

    bool valid() const { return red >= 0 && green >= 0 && blue >= 0 && red < 16 && green < 16 && blue < 16; }
};

// Gives the pens from `start` to `end` of a palette the colours that go
// evenly from one shade to the other (start < end).
void gradientPens(int* palette, int start, int end, Shade from, Shade to);
// The same for the lines of a raster table, `start` being over or under
// `end`.
void gradientLines(int* lines, int start, int end, Shade from, Shade to);

}  // namespace tuxscr
