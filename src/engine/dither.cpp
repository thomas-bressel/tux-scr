#include "engine/dither.h"

namespace tuxscr {

namespace {

// A matrix as ConvImgCpc writes it: rows of numbers. It reads them [x, y]
// with x the row, which is how they are read here.
struct Matrix {
    const char* name;
    int rows;
    int columns;
    double values[64];
};

const Matrix kMatrices[] = {
    {"Floyd-Steinberg (2x2)", 2, 2, {7, 3, 5, 1}},
    {"Bayer 1 (2X2)", 2, 2, {1, 3, 4, 2}},
    {"Bayer 2 (4x4)", 4, 4, {0, 12, 3, 15, 8, 4, 11, 7, 2, 14, 1, 13, 10, 6, 9, 5}},
    {"Bayer 3 (4X4)", 4, 4, {1, 9, 3, 11, 13, 5, 15, 7, 4, 12, 2, 10, 16, 8, 14, 6}},
    {"Ordered 1 (2x2)", 2, 2, {1, 3, 2, 4}},
    // ConvImgCpc has a matrix of 3 by 3 that no name gives: "Ordered 2" is
    // its 4 by 4 and "Ordered 3" its 8 by 8.
    {"Ordered 2 (3x3)", 4, 4, {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5}},
    {"Ordered 3 (4x4)", 8, 8,
     {0,  48, 12, 60, 3,  51, 15, 63, 32, 16, 44, 28, 35, 19, 47, 31, 8,  56, 4,  52, 11, 59,
      7,  55, 40, 24, 36, 20, 43, 27, 39, 23, 2,  50, 14, 62, 1,  49, 13, 61, 34, 18, 46, 30,
      33, 17, 45, 29, 10, 58, 6,  54, 9,  57, 5,  53, 42, 26, 38, 22, 41, 25, 37, 21}},
    {"ZigZag1 (3x3)", 3, 3, {0, 4, 0, 3, 0, 1, 0, 2, 0}},
    {"ZigZag2 (4x3)", 3, 4, {0, 4, 2, 0, 6, 0, 5, 3, 0, 7, 1, 0}},
    {"ZigZag3 (5x4)", 4, 5, {0, 0, 0, 7, 0, 0, 2, 6, 9, 8, 3, 0, 1, 5, 0, 0, 4, 0, 0, 0}},
    {"Test0", 4, 4, {1, 16, 1, 16, 16, 1, 16, 1, 1, 16, 1, 16, 16, 1, 16, 1}},
    {"Test1", 4, 4, {1, 4, 1, 4, 4, 1, 4, 1, 1, 4, 1, 4, 4, 1, 4, 1}},
    {"Test2", 4, 4, {8, 1, 8, 1, 1, 8, 1, 8, 8, 1, 8, 1, 1, 8, 1, 8}},
    {"Test3", 2, 4, {8, 16, 16, 8, 16, 8, 8, 16}},
    {"Test4", 3, 2, {0, 3, 0, 5, 7, 1}},
    {"Test5", 2, 3, {0, 0, 7, 3, 5, 1}},
    {"Test6", 4, 4, {1, 9, 4, 7, 4, 7, 1, 9, 1, 9, 4, 7, 4, 7, 1, 9}},
    {"Test7", 3, 3, {12, 11, 0, 13, 10, 19, 11, 13, 0}},
    {"Test8", 2, 4, {3, 7, 6, 2, 5, 4, 1, 0}},
    {"Test9", 4, 4, {1, 5, 10, 14, 3, 7, 8, 12, 13, 9, 6, 2, 15, 11, 4, 0}},
};

uint8_t toByte(double value)
{
    return value >= 0 ? value <= 255 ? static_cast<uint8_t>(value) : 255 : 0;
}

}  // namespace

const std::vector<std::string>& ditherNames()
{
    static const std::vector<std::string> names = [] {
        std::vector<std::string> list;
        for (const Matrix& matrix : kMatrices)
            list.emplace_back(matrix.name);
        return list;
    }();
    return names;
}

Dither::Dither(const Param& param)
{
    const int strength = param.cpcPlus ? param.pct << 3 : param.pct << 1;
    if (strength <= 0)
        return;
    for (const Matrix& matrix : kMatrices) {
        if (param.method != matrix.name)
            continue;
        sizeX_ = matrix.rows;
        sizeY_ = matrix.columns;
        const int count = sizeX_ * sizeY_;
        double sum = 0;
        for (int i = 0; i < count; ++i)
            sum += matrix.values[i];
        matrix_.resize(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i)
            matrix_[static_cast<size_t>(i)] = (matrix.values[i] * strength) / sum;
        strength_ = strength;
        return;
    }
}

void Dither::apply(Bitmap& source, int xPix, int yPix, int width, Rgb& p, Rgb chosen, bool diffuse) const
{
    if (!strength_)
        return;
    if (diffuse) {
        for (int y = 0; y < sizeY_; ++y)
            for (int x = 0; x < sizeX_; ++x)
                if (xPix + width * x < source.width() && yPix + 2 * y < source.height()) {
                    Rgb pix = source.colour(xPix + width * x, yPix + (y << 1));
                    pix.r = toByte(static_cast<double>(pix.r) + (p.r - chosen.r) * at(x, y) / 256.0);
                    pix.g = toByte(static_cast<double>(pix.g) + (p.g - chosen.g) * at(x, y) / 256.0);
                    pix.b = toByte(static_cast<double>(pix.b) + (p.b - chosen.b) * at(x, y) / 256.0);
                    source.setPixel(xPix + width * x, yPix + (y << 1), pix);
                }
    } else {
        const int xm = (xPix / width) % sizeX_;
        const int ym = (yPix >> 1) % sizeY_;
        p.r = toByte(static_cast<double>(p.r) + at(xm, ym));
        p.g = toByte(static_cast<double>(p.g) + at(xm, ym));
        p.b = toByte(static_cast<double>(p.b) + at(xm, ym));
    }
}

}  // namespace tuxscr
