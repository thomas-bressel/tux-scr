#include "engine/picture.h"

#include <algorithm>
#include <array>

namespace tuxscr {

Picture::Picture()
    : pens_(static_cast<size_t>(kWidth) * kMaxLines, 0)
{
}

void Picture::setPixel(int x, int y, int pen, int width)
{
    const int line = y >> 1;
    if (line < 0 || line >= kMaxLines)
        return;
    for (int i = 0; i < width; ++i)
        if (x + i >= 0 && x + i < kWidth)
            pens_[static_cast<size_t>(line * kWidth + x + i)] = static_cast<uint8_t>(pen & 15);
}

void Picture::clear(int pen)
{
    std::fill(pens_.begin(), pens_.end(), static_cast<uint8_t>(pen & 15));
}

Bitmap render(const Screen& screen, const Picture& picture)
{
    Bitmap shown(screen.width(), screen.height());
    for (int y = 0; y < screen.height(); y += 2) {
        std::array<int, 16> colours;
        for (int pen = 0; pen < 16; ++pen)
            colours[static_cast<size_t>(pen)] = screen.rgb(screen.colourAt(pen, y));
        for (int x = 0; x < screen.width(); ++x) {
            const int value = colours[static_cast<size_t>(picture.pen(x, y))];
            shown.setPixel(x, y, value);
            shown.setPixel(x, y + 1, value);
        }
    }
    return shown;
}

namespace {

// The pen as the screen's memory holds it. Split rasters make more colours
// than mode 1 has pens: the ones over 3 are what rasters give, and are pen 3
// in memory.
int shownPen(const Screen& screen, int pen, int)
{
    return screen.mode == kModeSplit && pen > 3 ? 3 : pen;
}

}  // namespace

uint8_t screenByte(const Screen& screen, const Picture& picture, int x, int y, int width)
{
    int byte = 0, shift = 0;
    for (int p = 0; p < 8; p += width)
        byte |= kPenBits[shownPen(screen, picture.pen(x + p, y), y)] >> (shift++);
    return static_cast<uint8_t>(byte);
}

std::vector<uint8_t> screenMemory(const Screen& screen, const Picture& picture, bool alternate, int parity)
{
    std::vector<uint8_t> memory(0x10000, 0);
    for (int y = 0; y < screen.height(); y += 2) {
        const int address = screen.address(y);
        const int width = screen.pixelWidth(y);
        for (int x = 0; x < screen.width(); x += 8)
            if (!alternate || ((y >> 1) & 1) == parity)
                memory[static_cast<size_t>(address + (x >> 3)) & 0xFFFF] = screenByte(screen, picture, x, y, width);
    }
    return memory;
}

std::vector<uint8_t> spriteBytes(const Screen& screen, const Picture& picture, int posX, int posY, int width, int height)
{
    std::vector<uint8_t> bytes(static_cast<size_t>((width * height) >> 4), 0);
    for (int y = 0; y < height; y += 2) {
        const int tx = screen.pixelWidth(y);
        for (int x = 0; x < width; x += 8) {
            int byte = 0;
            for (int p = 0; p < 8; ++p)
                if ((p % tx) == 0)
                    byte |= kPenBits[shownPen(screen, picture.pen(x + p + posX, y + posY), y)] >> (p / tx);
            const size_t at = static_cast<size_t>((x >> 3) + (y >> 1) * (width >> 3));
            if (at < bytes.size())
                bytes[at] = static_cast<uint8_t>(byte);
        }
    }
    return bytes;
}

namespace {

// The pattern of mode ASC-UT that the block of 4 by 4 at (x, y) is.
int patternAt(const Screen& screen, const Picture& picture, int x, int y)
{
    for (int i = 0; i < 16; ++i) {
        bool found = true;
        for (int ym = 0; ym < 4 && found; ++ym)
            for (int xm = 0; xm < 4; ++xm)
                if (picture.pen(x + (xm << 1), y + (ym << 1)) != screen.patterns[i][xm][ym]) {
                    found = false;
                    break;
                }
        if (found)
            return i;
    }
    return 0;
}

}  // namespace

std::vector<uint8_t> asciiBytes(const Screen& screen, const Picture& picture)
{
    std::vector<uint8_t> bytes(0x1000, 0);
    size_t l = 0;
    const auto pen = [&](int x, int y) { return picture.pen(x, y); };
    for (int y = 0; y < screen.lines << 1; y += 16)
        for (int x = 0; x < screen.columns && l < bytes.size(); ++x) {
            const int px = x << 3;
            int byte = 0;
            switch (screen.mode) {
            case kModeAscUt:
                byte = patternAt(screen, picture, px, y) | (patternAt(screen, picture, px, y + 8) << 4);
                break;
            case kModeAsc0:
                byte = pen(px, y) | (pen(px, y + 8) << 4);
                break;
            case kModeAsc1:
                byte = pen(px, y) | (pen(px + 4, y) << 2) | (pen(px, y + 8) << 4) | (pen(px + 4, y + 8) << 6);
                break;
            case kModeAsc2:
                byte = pen(px, y) | (pen(px + 2, y) << 2) | (pen(px + 4, y) << 1) | (pen(px + 6, y) << 3)
                    | (pen(px, y + 8) << 4) | (pen(px + 2, y + 8) << 6) | (pen(px + 4, y + 8) << 5)
                    | (pen(px + 6, y + 8) << 7);
                break;
            default:
                break;
            }
            bytes[l++] = static_cast<uint8_t>(byte);
        }
    return bytes;
}

}  // namespace tuxscr
