#include "engine/screen.h"

#include <algorithm>
#include <cstdlib>

namespace tuxscr {

const char* const kModeNames[kModeCount] = {
    "Mode 0", "Mode 1", "Mode 2", "Mode EGX1", "Mode EGX2", "Mode X",
    "Mode <Split>", "Mode ASC-UT", "Mode ASC0", "Mode ASC1", "Mode ASC2", "Capture Sprites",
};

namespace {
constexpr uint8_t kLum0 = 0x00;
constexpr uint8_t kLum1 = 0x66;
constexpr uint8_t kLum2 = 0xFF;

#include "patterns.inc"
}  // namespace

const Rgb kCpcColours[27] = {
    {kLum0, kLum0, kLum0}, {kLum0, kLum0, kLum1}, {kLum0, kLum0, kLum2}, {kLum1, kLum0, kLum0}, {kLum1, kLum0, kLum1},
    {kLum1, kLum0, kLum2}, {kLum2, kLum0, kLum0}, {kLum2, kLum0, kLum1}, {kLum2, kLum0, kLum2}, {kLum0, kLum1, kLum0},
    {kLum0, kLum1, kLum1}, {kLum0, kLum1, kLum2}, {kLum1, kLum1, kLum0}, {kLum1, kLum1, kLum1}, {kLum1, kLum1, kLum2},
    {kLum2, kLum1, kLum0}, {kLum2, kLum1, kLum1}, {kLum2, kLum1, kLum2}, {kLum0, kLum2, kLum0}, {kLum0, kLum2, kLum1},
    {kLum0, kLum2, kLum2}, {kLum1, kLum2, kLum0}, {kLum1, kLum2, kLum1}, {kLum1, kLum2, kLum2}, {kLum2, kLum2, kLum0},
    {kLum2, kLum2, kLum1}, {kLum2, kLum2, kLum2},
};

const char kCpcVga[28] = "TDU\\X]LEMVFW^@_NGORBSZY[JCK";

const int kPenBits[16] = {0x00, 0x80, 0x08, 0x88, 0x20, 0xA0, 0x28, 0xA8, 0x02, 0x82, 0x0A, 0x8A, 0x22, 0xA2, 0x2A, 0xAA};

int Screen::address(int y) const
{
    int address = (y >> 4) * columns + (y & 14) * 0x400;
    if (y > 255 && columns * lines > 0x4000)
        address += 0x3800;
    return address;
}

int Screen::pixelWidth(int y) const
{
    const int shift = mode == kModeCaptureSprites ? 2
        : mode >= kModeAsc0 ? mode - kModeAsc0
        : mode >= kModeX ? 2
        : mode > kMode2 ? ((y & 2) == yEgx ? mode - 1 : mode - 2)
        : mode + 1;
    return 8 >> shift;
}

int Screen::maxPen(int y) const
{
    switch (mode) {
    case kMode0:
    case kMode1:
    case kMode2:
        return 1 << (4 >> mode);
    case kModeEgx1:
    case kModeEgx2:
        return 1 << (4 >> ((y & 2) == yEgx ? mode - 2 : mode - 3));
    case kModeX:
        return 4;
    case kModeSplit:
        return 16;
    case kModeAscUt:
        return 4;
    case kModeAsc0:
        return 16;
    case kModeAsc1:
        return 4;
    case kModeAsc2:
        return 2;
    }
    return 16;
}

int Screen::lineMode(int y) const
{
    if (mode >= kModeX)
        return 1;
    if (mode >= kModeEgx1)
        return (y & 2) == yEgx ? mode - 2 : mode - 3;
    return mode;
}

int Screen::rgb(int colour) const
{
    // A pen that was given no colour is black.
    if (colour >= kNoColour)
        return 0;
    if (plus)
        return (plusBlue(colour) * 17) + ((plusGreen(colour) * 17) << 8) + ((plusRed(colour) * 17) << 16);
    return kCpcColours[colour >= 0 && colour < 27 ? colour : 0].value();
}

Rgb Screen::colour(int colour) const
{
    if (colour < kNoColour) {
        if (plus)
            return Rgb(static_cast<uint8_t>(plusRed(colour) * 17), static_cast<uint8_t>(plusGreen(colour) * 17),
                       static_cast<uint8_t>(plusBlue(colour) * 17));
        return kCpcColours[colour >= 0 && colour < 27 ? colour : 0];
    }
    return Rgb();
}

std::array<int, 16> Screen::paletteAt(int line) const
{
    std::array<int, 16> pens;
    std::copy_n(palette.begin(), 16, pens.begin());
    if (!interruptsActive())
        return pens;
    for (const PaletteInterrupt& interrupt : interrupts) {
        if (interrupt.line > line)
            break;
        for (size_t pen = 0; pen < 16; ++pen)
            if (interrupt.defined[pen])
                pens[pen] = interrupt.colours[pen];
    }
    return pens;
}

std::array<int, 16> Screen::matchPaletteAt(int line) const
{
    std::array<int, 16> pens;
    std::copy_n(palette.begin(), 16, pens.begin());
    if (!interruptsActive())
        return pens;
    for (const PaletteInterrupt& interrupt : interrupts) {
        if (interrupt.line > line)
            break;
        if (interrupt.matched)
            pens = interrupt.matchColours;
    }
    return pens;
}

void Screen::tidyInterrupts()
{
    for (PaletteInterrupt& interrupt : interrupts)
        interrupt.line = std::clamp(interrupt.line, 1, std::max(1, lines - 1));
    std::stable_sort(interrupts.begin(), interrupts.end(),
                     [](const PaletteInterrupt& a, const PaletteInterrupt& b) { return a.line < b.line; });
    // What a pen that is not defined shows is what it has above: the colours
    // kept for it follow.
    std::array<int, 16> above;
    std::copy_n(palette.begin(), 16, above.begin());
    for (PaletteInterrupt& interrupt : interrupts) {
        for (size_t pen = 0; pen < 16; ++pen) {
            if (interrupt.defined[pen])
                above[pen] = interrupt.colours[pen];
            else
                interrupt.colours[pen] = above[pen];
        }
    }
}

int Screen::colourAt(int pen, int y) const
{
    const size_t line = static_cast<size_t>(std::clamp(y >> 1, 0, kMaxLines - 1));
    const size_t index = static_cast<size_t>(pen & 15);
    if (mode == kModeX || mode == kModeSplit)
        return lineColours[line][index];
    if (interruptsActive())
        return paletteAt(static_cast<int>(line))[index];
    return palette[index];
}

void Screen::copyPatterns(int set)
{
    if (set < 0 || set >= 8)
        return;
    for (int i = 0; i < 16; ++i)
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x)
                patterns[i][x][y] = kPatternSets[set][i][x][y];
}

int plusFromCpc(int colour)
{
    if (colour < 0 || colour >= 27)
        return 0;
    const Rgb rgb = kCpcColours[colour];
    return ((rgb.b >> 4) << 4) + ((rgb.g >> 4) << 8) + (rgb.r >> 4);
}

int cpcFromPlus(int colour)
{
    const Rgb rgb(static_cast<uint8_t>((colour & 0x0F) * 17), static_cast<uint8_t>((colour >> 8) * 17),
                  static_cast<uint8_t>(((colour >> 4) & 0x0F) * 17));
    int best = 0x7FFFFFF, chosen = 0;
    for (int p = 0; p < 27; ++p) {
        const int delta = std::abs(rgb.r - kCpcColours[p].r) + std::abs(rgb.g - kCpcColours[p].g)
            + std::abs(rgb.b - kCpcColours[p].b);
        if (delta < best) {
            best = delta;
            chosen = p;
        }
    }
    return chosen;
}

}  // namespace tuxscr
