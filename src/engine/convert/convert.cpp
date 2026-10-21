#include "convert/convert.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "convert/dither.h"

// ConvImgCpc's conversion (its Conversion class), kept to the letter: the
// order things are tried in decides which of two colours as good as each
// other is taken, and so what the picture is.

namespace tuxscr {

namespace {

constexpr int kColours = 4096;

// How many pixels of each line have each of the CPC's colours.
class Counts {
public:
    Counts()
        : cells_(static_cast<size_t>(kColours) * kMaxLines, 0)
    {
    }
    int& at(int colour, int line) { return cells_[static_cast<size_t>(colour) * kMaxLines + static_cast<size_t>(line)]; }
    void clear() { std::fill(cells_.begin(), cells_.end(), 0); }
    void clearColour(int colour)
    {
        if (colour >= 0 && colour < kColours)
            std::fill_n(cells_.begin() + static_cast<long>(colour) * kMaxLines, kMaxLines, 0);
    }
    int total(int colour, int from, int to)
    {
        int sum = 0;
        for (int y = from; y < to; ++y)
            sum += at(colour, y);
        return sum;
    }

private:
    std::vector<int> cells_;
};

// A pen's colour at a line, if it has one.
struct LineColour {
    Rgb colour;
    bool set = false;
};
using LineColours = std::vector<std::array<LineColour, kMaxLines>>;  // [pen][line]

struct Run {
    Bitmap& source;
    Screen& screen;
    Picture& picture;
    const Param& prm;
    Counts counts;
    uint8_t contrast[256] = {};
};

uint8_t toByte(double value)
{
    return value >= 0 ? value <= 255 ? static_cast<uint8_t>(value) : 255 : 0;
}

int distance(const Param& prm, Rgb a, Rgb b)
{
    return (a.r - b.r) * (a.r - b.r) * prm.coefR + (a.g - b.g) * (a.g - b.g) * prm.coefV
        + (a.b - b.b) * (a.b - b.b) * prm.coefB;
}

// Brightness and saturation.
void setLumiSat(float lumi, float satur, float& r, float& v, float& b)
{
    float min = std::min(r, std::min(v, b));
    float max = std::max(r, std::max(v, b));
    float dif = max - min;
    float hue = 0;
    if (max > min) {
        hue = v == max ? (b - r) / dif * 60.0f + 120.0f
            : b == max ? (r - v) / dif * 60.0f + 240.0f
            : (v - b) / dif * 60.0f + (b > v ? 360.0f : 0);
        if (hue < 0)
            hue += 360.0f;
    }
    hue *= 255.0f / 360.0f;
    const float sat = satur * (dif / max) * 255.0f;
    const float bri = lumi * max;
    r = v = b = bri;
    if (sat != 0) {
        max = bri;
        dif = bri * sat / 255.0f;
        min = bri - dif;
        const float h = hue * 360.0f / 255.0f;
        if (h < 60.0f) {
            r = max;
            v = h * dif / 60.0f + min;
            b = min;
        } else if (h < 120.0f) {
            r = -(h - 120.0f) * dif / 60.0f + min;
            v = max;
            b = min;
        } else if (h < 180.0f) {
            r = min;
            v = max;
            b = (h - 120.0f) * dif / 60.0f + min;
        } else if (h < 240.0f) {
            r = min;
            v = -(h - 240.0f) * dif / 60.0f + min;
            b = max;
        } else if (h < 300.0f) {
            r = (h - 240.0f) * dif / 60.0f + min;
            v = min;
            b = max;
        } else if (h <= 360.0f) {
            r = max;
            v = min;
            b = -(h - 360.0f) * dif / 60 + min;
        } else {
            r = v = b = 0;
        }
    }
}

// The CPC's colour (0 to 26) nearest to one of the PC's.
int cpcColourOf(const Param& prm, Rgb p)
{
    int chosen = 0;
    if (!prm.newMethode) {
        chosen = (p.r > prm.cstR2 ? 6 : p.r > prm.cstR1 ? 3 : 0) + (p.b > prm.cstB2 ? 2 : p.b > prm.cstB1 ? 1 : 0)
            + (p.g > prm.cstV2 ? 18 : p.g > prm.cstV1 ? 9 : 0);
    } else {
        int oldDist = 0x7FFFFFFF;
        for (int i = 0; i < 27; ++i) {
            const int dist = distance(prm, kCpcColours[i], p);
            if (dist < oldDist) {
                oldDist = dist;
                chosen = i;
                if (dist == 0)
                    break;
            }
        }
    }
    return chosen;
}

Rgb plusColourOf(Rgb p)
{
    return Rgb(static_cast<uint8_t>((p.r >> 4) * 17), static_cast<uint8_t>((p.g >> 4) * 17),
               static_cast<uint8_t>((p.b >> 4) * 17));
}

// A pixel of the source, with what the window's settings do to its colour.
Rgb sourcePixel(Run& run, const Dither& dither, int xPix, int yPix, int tx)
{
    const Param& prm = run.prm;
    Rgb p;
    if (prm.lissage) {
        float r = 0, v = 0, b = 0;
        for (int i = 0; i < tx; ++i) {
            p = run.source.colour(xPix + i, yPix);
            r += p.r;
            b += p.b;
            v += p.g;
        }
        p.r = static_cast<uint8_t>(r / static_cast<float>(tx));
        p.g = static_cast<uint8_t>(v / static_cast<float>(tx));
        p.b = static_cast<uint8_t>(b / static_cast<float>(tx));
    } else {
        p = run.source.colour(xPix, yPix);
    }

    switch (prm.bitsRVB) {
    case 12:
        p.r = static_cast<uint8_t>((p.r >> 4) * 17);
        p.g = static_cast<uint8_t>((p.g >> 4) * 17);
        p.b = static_cast<uint8_t>((p.b >> 4) * 17);
        break;
    case 9:
        p.r = static_cast<uint8_t>((p.r >> 5) * 36);
        p.g = static_cast<uint8_t>((p.g >> 5) * 36);
        p.b = static_cast<uint8_t>((p.b >> 5) * 36);
        break;
    case 6:
        p.r = static_cast<uint8_t>((p.r >> 6) * 85);
        p.g = static_cast<uint8_t>((p.g >> 6) * 85);
        p.b = static_cast<uint8_t>((p.b >> 6) * 85);
        break;
    default:
        break;
    }

    const int m1 = (prm.reductPal1 ? 0x11 : 0x00) | (prm.reductPal3 ? 0x22 : 0x00);
    const int m2 = (prm.reductPal2 ? 0xEE : 0xFF) & (prm.reductPal4 ? 0xDD : 0xFF);
    p.r = toByte(((p.r | m1) & m2) * prm.pctRed / 100.0);
    p.g = toByte(((p.g | m1) & m2) * prm.pctGreen / 100.0);
    p.b = toByte(((p.b | m1) & m2) * prm.pctBlue / 100.0);
    if (p.r != 0 || p.g != 0 || p.b != 0) {
        float r = run.contrast[p.r];
        float v = run.contrast[p.g];
        float b = run.contrast[p.b];
        if (prm.pctLumi != 100 || prm.pctSat != 100)
            setLumiSat(prm.pctLumi > 100 ? static_cast<float>(100 + (prm.pctLumi - 100) * 2) / 100.0f
                                         : static_cast<float>(prm.pctLumi) / 100.0f,
                       static_cast<float>(prm.pctSat) / 100.0f, r, v, b);
        p.r = toByte(r);
        p.g = toByte(v);
        p.b = toByte(b);
    }

    if (dither.strength() > 0) {
        const Rgb chosen = prm.cpcPlus ? plusColourOf(p) : kCpcColours[cpcColourOf(prm, p)];
        dither.apply(run.source, xPix, yPix, tx, p, chosen, prm.diffErr);
    }
    return p;
}

// First pass: brings the source to the CPC's colours and counts them, line
// by line; dithers on the way.
void firstPass(Run& run)
{
    const Param& prm = run.prm;
    const Screen& screen = run.screen;
    run.counts.clear();
    const Dither dither(prm);
    const int incY = prm.trameTc ? 4 : 2;
    for (int yPix = 0; yPix < screen.height(); yPix += incY) {
        const int tx = screen.pixelWidth(yPix);
        for (int xPix = 0; xPix < screen.width(); xPix += tx) {
            for (int yy = 0; yy < incY; yy += 2) {
                const Rgb p = sourcePixel(run, dither, xPix, yy + yPix, tx);
                Rgb chosen;
                int index;
                if (prm.cpcPlus) {
                    chosen = plusColourOf(p);
                    index = ((chosen.g << 4) & 0xF00) + (chosen.b & 0xF0) + (chosen.r >> 4);
                } else {
                    index = cpcColourOf(prm, p);
                    chosen = kCpcColours[index];
                }
                run.counts.at(index, yPix >> 1)++;
                if (!prm.trameTc)
                    run.source.setPixel(xPix, yy + yPix, prm.setPalCpc ? chosen : p);
            }
            if (prm.trameTc) {
                // The two pixels, one above the other, are given two colours
                // that make their mean between them.
                const Rgb p0 = sourcePixel(run, dither, xPix, yPix, tx);
                const Rgb p1 = sourcePixel(run, dither, xPix, yPix + 2, tx);
                const int r = p0.r + p1.r;
                const int v = p0.g + p1.g;
                const int b = p0.b + p1.b;
                Rgb n1, n2;
                if (prm.cpcPlus) {
                    n1 = Rgb(static_cast<uint8_t>(r >> 1), static_cast<uint8_t>(v >> 1), static_cast<uint8_t>(b >> 1));
                    n2 = Rgb(static_cast<uint8_t>(std::min(255, (r >> 3) * 8)), static_cast<uint8_t>(std::min(255, (v >> 3) * 8)),
                             static_cast<uint8_t>(std::min(255, (b >> 3) * 8)));
                } else {
                    n1 = kCpcColours[(r > prm.cstR3 ? 6 : r > prm.cstR1 ? 3 : 0) + (v > prm.cstV3 ? 18 : v > prm.cstV1 ? 9 : 0)
                                     + (b > prm.cstB3 ? 2 : b > prm.cstB1 ? 1 : 0)];
                    n2 = kCpcColours[(r > prm.cstR4 ? 6 : r > prm.cstR2 ? 3 : 0) + (v > prm.cstV4 ? 18 : v > prm.cstV2 ? 9 : 0)
                                     + (b > prm.cstB4 ? 2 : b > prm.cstB2 ? 1 : 0)];
                }
                run.source.setPixel(xPix, yPix, (xPix & tx) == 0 ? n1 : n2);
                run.source.setPixel(xPix, yPix + 2, (xPix & tx) == 0 ? n2 : n1);
            }
        }
    }
}

// What the palette is sorted by.
int sortValue(int c, bool plus, const Param& prm)
{
    if (plus) {
        const int v = c >> 8;
        const int r = (c >> 4) & 0x0F;
        const int b = c & 0x0F;
        return r * r * prm.coefR + v * v * prm.coefV + b * b * prm.coefB;
    }
    return c;
}

// Finds the best colours for the pens that are not locked, among the ones
// counted in the lines `from` to `to`, and writes them to `palette`.
void searchColours(Run& run, std::array<int, 17>& palette, int maxPen, const std::array<int, 16>& lockState,
                   const std::array<int, 16>& paramLock, int from = 0, int to = kMaxLines)
{
    const Param& prm = run.prm;
    const Screen& screen = run.screen;
    Counts& counts = run.counts;
    const auto pen = [&palette](int i) -> int& { return palette[static_cast<size_t>(i)]; };
    const auto locked = [&lockState](int i) { return i < 16 && lockState[static_cast<size_t>(i)] != 0; };
    const auto disabled = [&prm](int i) { return i < 16 && prm.disableState[static_cast<size_t>(i)] != 0; };

    for (int i = 0; i < 16; ++i)
        if (paramLock[static_cast<size_t>(i)] == 0 && lockState[static_cast<size_t>(i)] == 0)
            pen(i) = kNoColour;

    int cUtil, x, valMax = 0;
    const int findMax = screen.plus ? kColours : 27;
    for (x = 0; x < maxPen; ++x)
        if (lockState[static_cast<size_t>(x)] > 0)
            counts.clearColour(pen(x));

    // The colour most used.
    for (cUtil = 0; cUtil < maxPen; ++cUtil) {
        valMax = 0;
        if (!locked(cUtil) && !disabled(cUtil)) {
            for (int i = 0; i < findMax; ++i) {
                const int nbc = counts.total(i, from, to);
                if (valMax < nbc) {
                    valMax = nbc;
                    pen(cUtil) = i;
                }
            }
            counts.clearColour(pen(cUtil));
            if (prm.newReduc)
                break;  // the first colour is found: the others are another way
        }
    }
    if (prm.newReduc) {
        // The other way: in turn, the colour furthest from the last among
        // the ones used most, then the one used most.
        const Rgb colFirst = screen.colour(pen(cUtil));
        bool takeDist = false;
        for (x = 0; x < maxPen; ++x) {
            if (takeDist) {
                valMax = 0;
                if (!locked(x)) {
                    for (int i = 0; i < findMax; ++i) {
                        const int nbc = counts.total(i, from, to);
                        if (valMax < nbc) {
                            valMax = nbc;
                            pen(x) = i;
                        }
                    }
                }
            } else {
                if (!locked(x) && x != cUtil) {
                    int oldDist = 0;
                    for (int rech = 4; rech-- > 0;) {
                        for (int i = 0; i < findMax; ++i) {
                            const int nbc = counts.total(i, from, to);
                            if (nbc > valMax >> rech) {
                                const int dist = distance(prm, screen.colour(i), colFirst);
                                if (dist > oldDist) {
                                    oldDist = dist;
                                    pen(x) = i;
                                }
                            }
                        }
                    }
                    if (oldDist == 0)
                        --x;
                }
            }
            // ConvImgCpc stops here on an error when nothing was found for
            // the first pen: the palette stays as it is.
            if (x < 0)
                return;
            counts.clearColour(pen(x));
            takeDist = !takeDist;
            cUtil = x;
        }
    }
    if ((prm.newSortPal & 1) == 1) {
        const bool rising = (prm.newSortPal & 2) == 0;
        const auto sortable = [&](int i) { return !locked(i) && !disabled(i) && pen(i) != kNoColour; };
        for (x = 0; x < maxPen - 1; ++x) {
            if (!sortable(x))
                continue;
            for (int c = x + 1; c < maxPen; ++c) {
                if (!sortable(c))
                    continue;
                const int vx = sortValue(pen(x), screen.plus, prm);
                const int vc = sortValue(pen(c), screen.plus, prm);
                if ((vx > vc && rising) || (vx < vc && !rising))
                    std::swap(pen(x), pen(c));
            }
        }
    }
}

// Mode X: pens 0 and 1 for the whole picture, pens 2 and 3 for each line.
int searchColoursModeX(Run& run, const std::array<int, 16>& lockState, int yMax)
{
    Screen& screen = run.screen;
    Counts& counts = run.counts;
    const Param& prm = run.prm;
    const int findMax = screen.plus ? kColours : 27;
    const int fixed = screen.mode == kModeSplit ? 3 : 2;
    const int perLine = screen.mode == kModeSplit ? 9 : 4;

    for (int c = 0; c < fixed; ++c) {
        if (lockState[static_cast<size_t>(c)] == 0) {
            int valMax = 0;
            for (int i = 0; i < findMax; ++i) {
                const int found = counts.total(i, 0, yMax >> 1);
                if (valMax < found) {
                    valMax = found;
                    screen.palette[static_cast<size_t>(c)] = i;
                }
            }
        }
        int col = screen.palette[static_cast<size_t>(c)];
        if (col > kColours - 1)
            col = 0;
        counts.clearColour(col);
        for (int y = 0; y < kMaxLines; ++y)
            screen.lineColours[static_cast<size_t>(y)][static_cast<size_t>(c)] = col;
    }

    // The colours of each line.
    for (int c = fixed; c < perLine; ++c) {
        for (int y = 0; y < yMax >> 1; ++y) {
            int valMax = 0;
            int& colour = screen.lineColours[static_cast<size_t>(y)][static_cast<size_t>(c)];
            for (int i = 0; i < findMax; ++i) {
                for (int deltay = -(prm.trackModeX << 1); deltay <= (prm.trackModeX << 1); ++deltay) {
                    if (y + deltay >= 0 && y + deltay < (yMax >> 1) && valMax < counts.at(i, y + deltay)) {
                        valMax = counts.at(i, y + deltay);
                        colour = i;
                    }
                }
            }
            if (colour >= 0 && colour < kColours)
                counts.at(colour, y) = 0;
        }
    }

    std::vector<int> found(kColours, 0);
    int nbCol = 0;
    for (int i = 0; i < 16; ++i)
        for (int y = 0; y < kMaxLines; ++y) {
            const int colour = screen.lineColours[static_cast<size_t>(y)][static_cast<size_t>(i)];
            if (colour >= 0 && colour < kColours && found[static_cast<size_t>(colour)] == 0) {
                found[static_cast<size_t>(colour)] = 1;
                ++nbCol;
            }
        }
    return nbCol;
}

// The pen nearest to a colour among the first `maxPen` of a line.
int nearestPen(const Param& prm, const LineColours& tabCol, int line, int maxPen, Rgb pix, bool withDisabled)
{
    int oldDist = 0x7FFFFFFF;
    int chosen = 0;
    for (int i = 0; i < maxPen; ++i) {
        const LineColour& c = tabCol[static_cast<size_t>(i)][static_cast<size_t>(line)];
        if (c.set && (withDisabled || prm.disableState[static_cast<size_t>(i)] == 0)) {
            const int dist = distance(prm, c.colour, pix);
            if (dist < oldDist) {
                chosen = i;
                oldDist = dist;
                if (dist == 0)
                    break;
            }
        }
    }
    return chosen;
}

void convertStandard(Run& run, const LineColours& tabCol)
{
    const Screen& screen = run.screen;
    const int offsetY = run.prm.modeImpDraw && screen.height() == 544 ? 2 : 0;
    for (int y = 0; y < screen.height(); y += 2) {
        const int tx = screen.pixelWidth(y);
        const int maxPen = screen.maxPen(y);
        for (int x = 0; x < screen.width(); x += tx)
            run.picture.setPixel(x, y + offsetY, nearestPen(run.prm, tabCol, y >> 1, maxPen, run.source.colour(x, y), false), tx);
    }
}

void convertModeX(Run& run, const LineColours& tabCol)
{
    const Screen& screen = run.screen;
    const int offsetY = run.prm.modeImpDraw && screen.height() == 544 ? 2 : 0;
    for (int y = 0; y < screen.height(); y += 2)
        for (int x = 0; x < screen.width(); x += 2)
            run.picture.setPixel(x, y + offsetY, nearestPen(run.prm, tabCol, y >> 1, 4, run.source.colour(x, y), true), 2);
}

// Split rasters: a colour over pen 2 is one a raster gives, and stays for at
// least 32 pixels.
void convertSplit(Run& run, const LineColours& tabCol)
{
    const Screen& screen = run.screen;
    const Param& prm = run.prm;
    for (int y = 0; y < screen.height(); y += 2) {
        int tailleSplit = 0, colSplit = -1;
        for (int x = 0; x < screen.width(); x += 2) {
            int oldDist = 0x7FFFFFFF;
            const Rgb pix = run.source.colour(x, y);
            int choix = 0;
            for (int i = 0; i < 9; ++i) {
                int memoPen = i;
                if (colSplit != -1 && tailleSplit < 32 && i > 2)
                    memoPen = colSplit;
                const Rgb c = tabCol[static_cast<size_t>(memoPen)][static_cast<size_t>(y >> 1)].colour;
                const int dist = distance(prm, c, pix);
                if (dist < oldDist) {
                    choix = memoPen;
                    oldDist = dist;
                    if (dist == 0 || memoPen == colSplit)
                        i = 9;
                }
            }
            if (choix > 2 && colSplit != choix) {
                colSplit = choix;
                tailleSplit = 0;
            }
            ++tailleSplit;
            run.picture.setPixel(x, y, choix, 2);
        }
    }
}

// ASC0 to ASC2: blocks of four lines.
void convertAscii(Run& run, int maxPen, const LineColours& tabCol)
{
    const Screen& screen = run.screen;
    const int incY = 8;
    const int offsetY = run.prm.modeImpDraw && screen.height() == 544 ? 2 : 0;
    for (int y = 0; y < screen.height(); y += incY) {
        const int tx = screen.pixelWidth(y);
        for (int x = 0; x < screen.width(); x += tx) {
            int r = 0, v = 0, b = 0;
            for (int j = 0; j < incY; j += 2) {
                const Rgb pix = run.source.colour(x, y + j);
                r += pix.r;
                v += pix.g;
                b += pix.b;
            }
            const Rgb pix(static_cast<uint8_t>(r >> 2), static_cast<uint8_t>(v >> 2), static_cast<uint8_t>(b >> 2));
            const int choix = nearestPen(run.prm, tabCol, y >> 1, maxPen, pix, true);
            for (int j = 0; j < incY; j += 2)
                run.picture.setPixel(x, y + offsetY + j, choix, tx);
        }
    }
}

// ASC-UT: mode 1, each block of 4 by 4 one of sixteen patterns.
void convertAscUt(Run& run, const LineColours& tabCol)
{
    const Screen& screen = run.screen;
    const Param& prm = run.prm;
    for (int y = 0; y <= screen.height() - 8; y += 8) {
        for (int x = 0; x < screen.width(); x += 8) {
            int choix = 0, oldDist = 0x7FFFFFFF;
            for (int i = 0; i < 16; ++i) {
                int r1 = 0, v1 = 0, b1 = 0, r2 = 0, v2 = 0, b2 = 0;
                for (int ym = 0; ym < 4; ++ym)
                    for (int xm = 0; xm < 4; ++xm) {
                        const Rgb pix = run.source.colour(x + (xm << 1), y + (ym << 1));
                        const size_t line = static_cast<size_t>(std::min(kMaxLines - 1, ym + (y >> 1)));
                        const Rgb c = tabCol[screen.patterns[i][xm][ym] & 15][line].colour;
                        r1 += pix.r;
                        v1 += pix.g;
                        b1 += pix.b;
                        r2 += c.r;
                        v2 += c.g;
                        b2 += c.b;
                    }
                const int dist = std::abs(r1 - r2) * prm.coefR + std::abs(v1 - v2) * prm.coefV + std::abs(b1 - b2) * prm.coefB;
                if (dist < oldDist) {
                    choix = i;
                    oldDist = dist;
                    if (dist == 0)
                        break;
                }
            }
            for (int ym = 0; ym < 4; ++ym)
                for (int xm = 0; xm < 4; ++xm)
                    run.picture.setPixel(x + (xm << 1), y + (ym << 1), screen.patterns[choix][xm][ym], 2);
        }
    }
}

// Second pass: the palette is searched, and the picture brought to the pens
// each line has.
void secondPass(Run& run, int& splitColours, int searchInterrupt)
{
    Screen& screen = run.screen;
    const Param& prm = run.prm;
    LineColours tabCol(16);
    std::array<int, 16> memoLock = prm.lockState;
    int maxPen = screen.maxPen(screen.yEgx ^ 2);

    // The EGX modes: the pens of the lines that have the fewest come first.
    if (screen.mode == kModeEgx1 || screen.mode == kModeEgx2) {
        const int newMax = screen.maxPen(screen.yEgx);
        searchColours(run, screen.palette, newMax, memoLock, prm.lockState);
        for (int i = 0; i < newMax; ++i)
            memoLock[static_cast<size_t>(i)] = 1;
    }
    if (screen.mode == kModeX || screen.mode == kModeSplit) {
        splitColours = searchColoursModeX(run, memoLock, screen.height());
        if (screen.mode == kModeSplit)
            maxPen = 9;
        for (int y = 0; y < screen.height() >> 1; ++y)
            for (int i = 0; i < maxPen; ++i)
                tabCol[static_cast<size_t>(i)][static_cast<size_t>(y)]
                    = {screen.colour(screen.lineColours[static_cast<size_t>(y)][static_cast<size_t>(i)]), true};
    } else {
        // An interrupt whose colours are asked for is searched in what its
        // own lines hold, before the picture's palette takes its share.
        const bool zones = screen.plus && screen.interruptsOn && screen.mode <= kMode2;
        if (zones && searchInterrupt >= 0 && searchInterrupt < static_cast<int>(screen.interrupts.size())) {
            screen.tidyInterrupts();
            PaletteInterrupt& interrupt = screen.interrupts[static_cast<size_t>(searchInterrupt)];
            const size_t next = static_cast<size_t>(searchInterrupt) + 1;
            const int to = next < screen.interrupts.size() ? screen.interrupts[next].line : screen.lines;
            const Counts kept = run.counts;
            std::array<int, 17> found{};
            found.fill(kNoColour);
            searchColours(run, found, maxPen, {}, {}, interrupt.line, to);
            run.counts = kept;
            // Its lines are from now on converted with what was found; a
            // pen the lines have no colour for is black.
            interrupt.matched = true;
            interrupt.matchColours = screen.matchPaletteAt(interrupt.line - 1);
            for (int i = 0; i < maxPen; ++i)
                if (prm.disableState[static_cast<size_t>(i)] == 0) {
                    const int colour = found[static_cast<size_t>(i)] != kNoColour ? found[static_cast<size_t>(i)] : 0;
                    interrupt.matchColours[static_cast<size_t>(i)] = colour;
                    interrupt.colours[static_cast<size_t>(i)] = colour;
                    interrupt.defined[static_cast<size_t>(i)] = true;
                }
        }
        searchColours(run, screen.palette, maxPen, memoLock, prm.lockState);
        if (zones)
            screen.tidyInterrupts();
        for (int y = 0; y < screen.height(); y += 2) {
            maxPen = screen.maxPen(y);
            // The pens are given by the picture's palette: a colour an
            // interrupt gives a pen colours again what the pen draws, and
            // moves no pixel to another pen.
            const std::array<int, 16> pens = screen.matchPaletteAt(y >> 1);
            for (int i = 0; i < maxPen; ++i)
                tabCol[static_cast<size_t>(i)][static_cast<size_t>(y >> 1)] = {screen.colour(pens[static_cast<size_t>(i)]), true};
        }
    }
    switch (screen.mode) {
    case kModeX:
        convertModeX(run, tabCol);
        break;
    case kModeSplit:
        convertSplit(run, tabCol);
        break;
    case kModeAscUt:
        convertAscUt(run, tabCol);
        break;
    case kModeAsc0:
    case kModeAsc1:
    case kModeAsc2:
        convertAscii(run, maxPen, tabCol);
        break;
    default:
        convertStandard(run, tabCol);
        break;
    }
}

// A level of the K-means filter: a colour, and what is gathered round it.
struct Level {
    Rgb colour;
    int nbR = 0, nbV = 0, nbB = 0, nbPixels = 0;

    void reset() { nbR = nbV = nbB = nbPixels = 0; }
    void toMean()
    {
        colour.r = nbPixels > 0 ? static_cast<uint8_t>(nbR / nbPixels) : 0;
        colour.g = nbPixels > 0 ? static_cast<uint8_t>(nbV / nbPixels) : 0;
        colour.b = nbPixels > 0 ? static_cast<uint8_t>(nbB / nbPixels) : 0;
    }
};

Level& nearestLevel(const Param& prm, std::vector<Level>& levels, Rgb coul)
{
    int minDist = 0x7FFFFFFF;
    size_t ret = 0;
    const int r = coul.r, v = coul.g, b = coul.b;
    for (size_t n = 0; n < levels.size(); ++n) {
        const Rgb c = levels[n].colour;
        int dist;
        switch (prm.kMeansDist) {
        case 1:  // Euclid's
            dist = (r - c.r) * (r - c.r) * prm.coefR + (v - c.g) * (v - c.g) * prm.coefV + (b - c.b) * (b - c.b) * prm.coefB;
            break;
        case 0:  // the largest of the three
            dist = std::max(std::abs(r - c.r), std::max(std::abs(v - c.g), std::abs(b - c.b)));
            break;
        default:  // Manhattan's
            dist = std::abs(r - c.r) * prm.coefR + std::abs(v - c.g) * prm.coefV + std::abs(b - c.b) * prm.coefB;
            break;
        }
        if (dist < minDist) {
            ret = n;
            minDist = dist;
        }
    }
    levels[ret].nbR += r;
    levels[ret].nbV += v;
    levels[ret].nbB += b;
    levels[ret].nbPixels++;
    return levels[ret];
}

// The K-means filter: the source brought to a few colours of its own.
void reduceColours(Run& run)
{
    const Param& prm = run.prm;
    const Screen& screen = run.screen;
    if (prm.kMeansColor < 2)
        return;
    std::vector<Level> levels(static_cast<size_t>(prm.kMeansColor));
    for (int n = 0; n < prm.kMeansColor; ++n) {
        const uint8_t grey = static_cast<uint8_t>(255 * n / (prm.kMeansColor - 1));
        levels[static_cast<size_t>(n)].colour = Rgb(grey, grey, grey);
    }
    const int incY = prm.trameTc ? 4 : 2;
    for (int p = 0; p < prm.kMeansPass; ++p) {
        for (Level& level : levels)
            level.reset();
        for (int y = 0; y < screen.height(); y += incY) {
            const int tx = screen.pixelWidth(y);
            for (int x = 0; x < screen.width(); x += tx)
                nearestLevel(prm, levels, run.source.colour(x, y));
        }
        for (Level& level : levels)
            level.toMean();
    }
    for (int y = 0; y < screen.height(); y += incY)
        for (int x = 0; x < screen.width(); ++x)
            run.source.setPixel(x, y, nearestLevel(prm, levels, run.source.colour(x, y)).colour);
}

void setContrast(Run& run)
{
    const double c = run.prm.pctContrast / 100.0;
    for (int i = 0; i < 256; ++i)
        run.contrast[i] = toByte(((((i / 255.0) - 0.5) * c) + 0.5) * 255.0);
}

}  // namespace

Conversion convert(Bitmap& source, Screen& screen, Picture& picture, const Param& param, int searchInterrupt)
{
    Run run{source, screen, picture, param, {}, {}};
    setContrast(run);
    if (param.filtre)
        reduceColours(run);
    firstPass(run);

    Conversion result;
    for (int i = 0; i < kColours; ++i)
        if (run.counts.total(i, 0, kMaxLines) > 0)
            ++result.colours;

    secondPass(run, result.splitColours, searchInterrupt);
    picture.computed = true;
    return result;
}

void findPatterns(Bitmap& source, Screen& screen, const Param& param, std::vector<Pattern>& patterns)
{
    Picture unused;
    Run run{source, screen, unused, param, {}, {}};
    setContrast(run);
    firstPass(run);
    searchColours(run, screen.palette, 4, param.lockState, param.lockState);
    for (int y = 0; y < screen.height(); y += 8) {
        for (int x = 0; x < screen.width(); x += 8) {
            Pattern local;
            for (int maty = 0; maty < 4; ++maty)
                for (int matx = 0; matx < 4; ++matx) {
                    const Rgb pix = source.colour(x + (matx << 1), y + (maty << 1));
                    int oldDist = 0x7FFFFFFF;
                    int choix = 0;
                    for (int i = 0; i < 4; ++i) {
                        const int dist = distance(param, screen.penColour(i), pix);
                        if (dist < oldDist) {
                            choix = i;
                            oldDist = dist;
                            if (dist == 0)
                                break;
                        }
                    }
                    local.pens[matx][maty] = static_cast<uint8_t>(choix);
                }
            bool known = false;
            for (Pattern& listed : patterns) {
                int different = 0;
                for (int py = 0; py < 4 && different <= 4; ++py)
                    for (int px = 0; px < 4; ++px)
                        if (listed.pens[px][py] != local.pens[px][py] && ++different > 4)
                            break;
                if (different <= 4) {
                    ++listed.found;
                    known = true;
                    break;
                }
            }
            if (!known)
                patterns.push_back(local);
        }
    }
}

void screenIntoParam(const Screen& screen, Param& param)
{
    param.cpcPlus = screen.plus;
    param.modeVirtuel = screen.mode;
    param.nbCols = screen.columns;
    param.nbLignes = screen.lines;
}

void changeMachine(Screen& screen, bool plus)
{
    screen.plus = plus;
    for (size_t i = 0; i < 16; ++i)
        screen.palette[i] = plus ? (screen.palette[i] < 27 ? plusFromCpc(screen.palette[i]) : 0) : cpcFromPlus(screen.palette[i]);
    if (plus)
        screen.tidyInterrupts();
}

}  // namespace tuxscr
