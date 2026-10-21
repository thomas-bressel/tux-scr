#pragma once

#include <cstdint>
#include <vector>

#include "base/bitmap.h"
#include "convert/screen.h"

namespace tuxscr {

// A picture of the CPC's: the pen of each of its pixels. Whatever the mode,
// a line is kept as pixels of mode 2, a pixel of mode 1 being two of them
// and one of mode 0 four: lines of different modes go in the same picture,
// as the EGX modes need.
//
// Places are given as ConvImgCpc gives them, on the picture as a PC shows
// it: x in pixels of mode 2, y in rows of which a CPC line is two.
class Picture {
public:
    static constexpr int kWidth = kMaxColumns * 8;

    Picture();

    int pen(int x, int y) const
    {
        const int line = y >> 1;
        if (x < 0 || line < 0 || x >= kWidth || line >= kMaxLines)
            return 0;
        return pens_[static_cast<size_t>(line * kWidth + x)];
    }
    // A pixel `width` wide.
    void setPixel(int x, int y, int pen, int width);
    void clear(int pen = 0);

    // Whether it holds a picture that was converted, read or drawn.
    bool computed = false;

    bool operator==(const Picture&) const = default;

private:
    std::vector<uint8_t> pens_;
};

// The picture as the CPC shows it, for a PC to show: the screen's width and
// height, each pen in the colour it has on its line.
Bitmap render(const Screen& screen, const Picture& picture);

// The picture as the bytes of the CPC's screen memory (64K of them, the
// screen's own from 0). With `alternate`, only the lines of one parity are
// written (`parity`, 0 or 1): what a file of an EGX picture holds.
std::vector<uint8_t> screenMemory(const Screen& screen, const Picture& picture, bool alternate = false, int parity = 0);

// The byte of the screen for the eight pixels at (x, y), whose pixels are
// `width` wide.
uint8_t screenByte(const Screen& screen, const Picture& picture, int x, int y, int width);

// A part of the picture as a sprite: its bytes, line after line.
std::vector<uint8_t> spriteBytes(const Screen& screen, const Picture& picture, int x, int y, int width, int height);
inline std::vector<uint8_t> spriteBytes(const Screen& screen, const Picture& picture)
{
    return spriteBytes(screen, picture, 0, 0, screen.width(), screen.height());
}

// The picture of an ASCII mode as its characters: one byte for two blocks,
// one above the other.
std::vector<uint8_t> asciiBytes(const Screen& screen, const Picture& picture);

}  // namespace tuxscr
