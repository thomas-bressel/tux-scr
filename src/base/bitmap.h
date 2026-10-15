#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tuxscr {

// A colour: red, green and blue, each 0 to 255. It is ConvImgCpc's RvbColor.
struct Rgb {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    constexpr Rgb() = default;
    constexpr Rgb(uint8_t red, uint8_t green, uint8_t blue)
        : r(red)
        , g(green)
        , b(blue)
    {
    }

    // From and to one number, &RRGGBB.
    static constexpr Rgb fromValue(int value)
    {
        return Rgb(static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value));
    }
    constexpr int value() const { return b + (g << 8) + (r << 16); }

    bool operator==(const Rgb&) const = default;
};

// A picture of the PC's: its pixels row by row from the top, each &RRGGBB.
// It is ConvImgCpc's DirectBitmap, and answers as it does when asked about a
// pixel that is not there: nothing is written under the last row, and what
// is read there is the first pixel.
class Bitmap {
public:
    Bitmap() = default;
    Bitmap(int width, int height)
        : width_(width > 0 ? width : 0)
        , height_(height > 0 ? height : 0)
        , pixels_(static_cast<size_t>(width_) * static_cast<size_t>(height_), 0)
    {
    }

    int width() const { return width_; }
    int height() const { return height_; }
    bool empty() const { return pixels_.empty(); }

    int pixel(int x, int y) const
    {
        if (x < 0 || y < 0 || x >= width_ || y >= height_)
            return 0;
        return static_cast<int>(pixels_[index(x, y)]);
    }
    Rgb colour(int x, int y) const
    {
        const size_t at = y >= 0 && y < height_ ? index(x, y) : 0;
        return at < pixels_.size() ? Rgb::fromValue(static_cast<int>(pixels_[at])) : Rgb();
    }
    void setPixel(int x, int y, int value)
    {
        if (y < 0 || y >= height_ || x < 0)
            return;
        const size_t at = index(x, y);
        if (at < pixels_.size())
            pixels_[at] = static_cast<uint32_t>(value) & 0xFFFFFF;
    }
    void setPixel(int x, int y, Rgb colour) { setPixel(x, y, colour.value()); }

    // A run of pixels on a row and on the one under it: a CPC line is two
    // rows of the PC's.
    void setRunDouble(int x, int y, int length, int value)
    {
        if (x < 0 || y < 0)
            return;
        size_t at = index(x, y);
        const size_t row = static_cast<size_t>(width_);
        for (; length-- > 0; ++at) {
            if (at < pixels_.size()) {
                pixels_[at] = static_cast<uint32_t>(value) & 0xFFFFFF;
                if (at + row < pixels_.size())
                    pixels_[at + row] = static_cast<uint32_t>(value) & 0xFFFFFF;
            }
        }
    }

    const std::vector<uint32_t>& pixels() const { return pixels_; }
    std::vector<uint32_t>& pixels() { return pixels_; }

    bool operator==(const Bitmap&) const = default;

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<uint32_t> pixels_;

    size_t index(int x, int y) const { return static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x); }
};

}  // namespace tuxscr
