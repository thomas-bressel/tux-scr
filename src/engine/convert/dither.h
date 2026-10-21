#pragma once

#include <string>
#include <vector>

#include "base/bitmap.h"
#include "convert/param.h"

namespace tuxscr {

// The matrices a picture is dithered with, by the names ConvImgCpc's window
// lists them under, in its order. "Aucun" (none) is not one of them.
const std::vector<std::string>& ditherNames();

// A matrix made ready for a conversion: its numbers brought to the strength
// asked for.
class Dither {
public:
    // The strength is Param's `pct`, eight times as much for a Plus as for a
    // CPC. strength() is 0 when nothing is to be dithered: no such matrix,
    // or none of it.
    explicit Dither(const Param& param);

    int strength() const { return strength_; }

    // Dithers the pixel at (x, y) of `source`, `width` wide: `colour` is what
    // it is, `chosen` the CPC's colour it is taken for. With `diffuse`, what
    // is lost between the two is spread over the pixels around; without, the
    // matrix is added to `colour`.
    void apply(Bitmap& source, int x, int y, int width, Rgb& colour, Rgb chosen, bool diffuse) const;

private:
    int strength_ = 0;
    int sizeX_ = 0;
    int sizeY_ = 0;
    std::vector<double> matrix_;  // [x * sizeY_ + y]

    double at(int x, int y) const { return matrix_[static_cast<size_t>(x * sizeY_ + y)]; }
};

}  // namespace tuxscr
