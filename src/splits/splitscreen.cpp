#include "splitscreen.h"

#include <algorithm>
#include <sstream>

#include "xmltext.h"

namespace tuxscr {

namespace {

// What the editor's controls can hold.
constexpr int kMaxDelay = SplitScreen::kMinDelay + 32;
constexpr int kMinLength = 32;
constexpr int kMaxLength = 384;

// A colour of the CPC as a PC shows it; one that is none of its 27 is the
// first.
int cpcRgb(int colour)
{
    return kCpcColours[colour >= 0 && colour < 27 ? colour : 0].value();
}

}  // namespace

SplitLines freshSplitLines()
{
    SplitLines lines;
    for (SplitLine& line : lines)
        line.delay = SplitScreen::kMinDelay;
    return lines;
}

// ---- The file ----

std::string splitXml(const SplitLines& lines)
{
    std::ostringstream out;
    out << xmltext::header("SplitEcran");
    out << "  <LignesSplit>\r\n";
    for (const SplitLine& line : lines) {
        out << "    <LigneSplit>\r\n";
        out << "      <numPen>" << line.pen << "</numPen>\r\n";
        out << "      <retard>" << line.delay << "</retard>\r\n";
        out << "      <newMode>" << line.newMode << "</newMode>\r\n";
        out << "      <changeMode>" << (line.changeMode ? "true" : "false") << "</changeMode>\r\n";
        out << "      <ListeSplit>\r\n";
        for (const Split& split : line.splits) {
            out << "        <Split>\r\n";
            out << "          <longueur>" << split.length << "</longueur>\r\n";
            out << "          <couleur>" << split.colour << "</couleur>\r\n";
            out << "          <enable>" << (split.enabled ? "true" : "false") << "</enable>\r\n";
            out << "        </Split>\r\n";
        }
        out << "      </ListeSplit>\r\n";
        out << "    </LigneSplit>\r\n";
    }
    out << "  </LignesSplit>\r\n";
    out << "</SplitEcran>";
    return out.str();
}

bool readSplitXml(const std::string& xml, SplitLines& lines)
{
    size_t at = 0;
    std::string root;
    if (!xmltext::nextElement(xml, "SplitEcran", at, root))
        return false;
    std::string list;
    at = 0;
    if (!xmltext::nextElement(root, "LignesSplit", at, list))
        return true;  // a screen without a line: nothing to take
    at = 0;
    std::string text;
    for (size_t y = 0; y < lines.size() && xmltext::nextElement(list, "LigneSplit", at, text); ++y) {
        SplitLine line = lines[y];
        std::string splits;
        size_t place = 0;
        if (xmltext::nextElement(text, "ListeSplit", place, splits)) {
            size_t inside = 0;
            std::string one;
            for (size_t s = 0; s < line.splits.size() && xmltext::nextElement(splits, "Split", inside, one); ++s) {
                Split& split = line.splits[s];
                xmltext::read(one, "longueur", split.length);
                xmltext::read(one, "couleur", split.colour);
                xmltext::read(one, "enable", split.enabled);
                split.length = std::clamp(split.length, kMinLength, kMaxLength) & 0xFFC;
                if (split.colour < 0 || split.colour > 26)
                    split.colour = 0;
            }
        }
        xmltext::read(text, "numPen", line.pen);
        xmltext::read(text, "retard", line.delay);
        xmltext::read(text, "newMode", line.newMode);
        xmltext::read(text, "changeMode", line.changeMode);
        line.pen = std::clamp(line.pen, 0, 15);
        line.delay = std::clamp(line.delay, SplitScreen::kMinDelay, kMaxDelay);
        line.newMode = std::clamp(line.newMode, 0, 2);
        lines[y] = line;
    }
    return true;
}

// ---- The screen ----

SplitScreen::SplitScreen(const std::array<int, 17>& colours)
    : lines(freshSplitLines())
    , memory(0x10000, 0)
    , startPalette(colours)
    , palette(kColumns, std::vector<std::array<int, 17>>(kLines, colours))
{
    pictureModes_.fill(kMode1);
    lineModes = pictureModes_;
}

void SplitScreen::setPicture(const Screen& screen, const Picture& picture)
{
    // The screen's own size, whatever the picture's: where its lines are in
    // memory depends on it.
    Screen wide = screen;
    wide.columns = kColumns;
    wide.lines = kLines;
    std::fill(memory.begin(), memory.end(), 0);
    for (int y = 0; y < wide.height(); y += 2) {
        const int address = wide.address(y);
        const int width = screen.pixelWidth(y);
        pictureModes_[static_cast<size_t>(y >> 1)] = screen.lineMode(y);
        if (y >= screen.height())
            continue;
        for (int x = 0; x < wide.width() && x < screen.width(); x += 8)
            memory[static_cast<size_t>(address + (x >> 3))] = screenByte(screen, picture, x, y, width);
    }
}

int SplitScreen::applyPalette(int x, int y, int until, const std::array<int, 16>& colours)
{
    for (; x < until && x < kColumns; ++x)
        std::copy(colours.begin(), colours.end(), palette[static_cast<size_t>(x)][static_cast<size_t>(y)].begin());
    return x;
}

void SplitScreen::calcPalette()
{
    std::array<int, 16> current;
    std::copy_n(startPalette.begin(), 16, current.begin());

    // Every byte of the screen back to the palette it starts with.
    for (int y = 0; y < kLines; ++y)
        applyPalette(0, y, kColumns, current);
    lineModes = pictureModes_;

    // A colour given to a pen stays until it is given another: on the rest
    // of the line and on the lines under it.
    for (int y = 0; y < kLines; ++y) {
        const SplitLine& line = lines[static_cast<size_t>(y)];
        const size_t pen = static_cast<size_t>(line.pen & 15);
        int until = line.delay >> 2;
        int x = applyPalette(0, y, until, current);
        for (const Split& split : line.splits) {
            if (!split.enabled)
                break;
            until += split.length >> 2;
            current[pen] = split.colour;
            if (until > kColumns)
                until = kColumns;
            x = applyPalette(x, y, until, current);
        }
        applyPalette(x, y, kColumns, current);
        if (y < kLines - 1 && line.changeMode)
            lineModes[static_cast<size_t>(y) + 1] = line.newMode;
    }
}

Rgb SplitScreen::startRgb(int pen) const
{
    const int colour = startColour(pen);
    return kCpcColours[colour >= 0 && colour < 27 ? colour : 0];
}

Rgb SplitScreen::paletteColour(int x, int y, int pen) const
{
    const int colour = palette[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(pen & 15)];
    return kCpcColours[colour >= 0 && colour < 27 ? colour : 0];
}

void SplitScreen::render(Bitmap& bitmap, int offsetX) const
{
    for (int y = 0; y < (kLines << 1); y += 2) {
        int address = (y >> 4) * kColumns + (y & 14) * 0x400;
        if (y > 255)
            address += 0x3800;
        address += offsetX >> 3;
        const std::vector<std::array<int, 17>>* column = palette.data();
        const size_t line = static_cast<size_t>(y >> 1);
        int xBitmap = 0;
        for (int x = 0; x < kColumns; ++x) {
            const int byte = memory[static_cast<size_t>(address + x) & 0xFFFF];
            const std::array<int, 17>& pens = column[x][line];
            switch (lineModes[line]) {
            case 0:
                bitmap.setRunDouble(xBitmap, y, 4,
                                    cpcRgb(pens[static_cast<size_t>((byte >> 7) + ((byte & 0x20) >> 3) + ((byte & 0x08) >> 2) + ((byte & 0x02) << 2))]));
                bitmap.setRunDouble(xBitmap + 4, y, 4,
                                    cpcRgb(pens[static_cast<size_t>(((byte & 0x40) >> 6) + ((byte & 0x10) >> 2) + ((byte & 0x04) >> 1) + ((byte & 0x01) << 3))]));
                xBitmap += 8;
                break;

            case 1:
            case 3:
                bitmap.setRunDouble(xBitmap, y, 2, cpcRgb(pens[static_cast<size_t>(((byte >> 7) & 1) + ((byte >> 2) & 2))]));
                bitmap.setRunDouble(xBitmap + 2, y, 2, cpcRgb(pens[static_cast<size_t>(((byte >> 6) & 1) + ((byte >> 1) & 2))]));
                bitmap.setRunDouble(xBitmap + 4, y, 2, cpcRgb(pens[static_cast<size_t>(((byte >> 5) & 1) + ((byte >> 0) & 2))]));
                bitmap.setRunDouble(xBitmap + 6, y, 2, cpcRgb(pens[static_cast<size_t>(((byte >> 4) & 1) + ((byte << 1) & 2))]));
                xBitmap += 8;
                break;

            case 2:
                for (int i = 8; i-- > 0;) {
                    const int value = cpcRgb(pens[static_cast<size_t>((byte >> i) & 1)]);
                    bitmap.setPixel(xBitmap, y, value);
                    bitmap.setPixel(xBitmap++, y + 1, value);
                }
                break;

            default:
                xBitmap += 8;
            }
        }
    }
}

std::vector<int> SplitScreen::changes(int y) const
{
    const SplitLine& line = lines[static_cast<size_t>(std::clamp(y, 0, kLines - 1))];
    std::vector<int> places;
    int until = line.delay >> 2;
    places.push_back(until);
    for (const Split& split : line.splits) {
        if (!split.enabled)
            break;
        until = std::min(until + (split.length >> 2), static_cast<int>(kColumns));
        places.push_back(until);
    }
    return places;
}

// ---- A picture of splits ----

int nearestCpcColour(Rgb colour)
{
    // Under a third of the way a component is none, over two thirds it is
    // full.
    constexpr int kHalf = 85, kFull = 170;
    return (colour.r > kFull ? 6 : colour.r > kHalf ? 3 : 0) + (colour.b > kFull ? 2 : colour.b > kHalf ? 1 : 0)
        + (colour.g > kFull ? 18 : colour.g > kHalf ? 9 : 0);
}

std::vector<SplitImportNote> importSplits(const Bitmap& picture, int pen, SplitLines& lines)
{
    std::vector<SplitImportNote> notes;
    int lastLineWritten = 0;
    for (int y = 0; y < SplitScreen::kProgramLines; ++y) {
        int count = 0, length = 0;
        int current = nearestCpcColour(picture.colour(0, y << 1));
        SplitLine& line = lines[static_cast<size_t>(y)];
        for (Split& split : line.splits) {
            split.enabled = false;
            split.length = kMinLength;
        }
        line.pen = pen;
        for (int x = 0; x < SplitScreen::kColumns; ++x) {
            // The colour at a byte is looked at where the earliest change
            // of the program would show.
            const int posX = (x << 3) + (SplitScreen::kMinDelay << 1);
            if (posX >= picture.width() || posX < 0)
                continue;
            const int found = nearestCpcColour(picture.colour(posX, y << 1));
            if (found != current && length >= kMinLength) {
                Split& split = line.splits[static_cast<size_t>(count)];
                split.colour = current;
                split.length = length;
                split.enabled = true;
                current = found;
                length = 0;
                if (count++ >= kSplitsPerLine - 1) {
                    notes.push_back({SplitImportNote::Kind::TooMany, y});
                    break;
                }
            } else if (found != current && length > 0) {
                notes.push_back({SplitImportNote::Kind::TooShort, y});
            }
            length += 4;
        }
        if (count < kSplitsPerLine) {
            // A line of one colour that the lines above already left the
            // pen in has nothing to change.
            bool sameAsAbove = y > 0;
            for (int k = y - 1; k-- > lastLineWritten;) {
                for (int j = kSplitsPerLine - 1; --j >= 0;) {
                    const Split& above = lines[static_cast<size_t>(k)].splits[static_cast<size_t>(j)];
                    if (above.enabled && above.colour != current) {
                        sameAsAbove = false;
                        break;
                    }
                }
                if (!sameAsAbove)
                    break;
            }
            if (length >= kMinLength && (y == 0 || count > 0 || !sameAsAbove)) {
                Split& split = line.splits[static_cast<size_t>(count)];
                split.colour = current;
                split.length = length;
                split.enabled = true;
                lastLineWritten = y;
            }
        }
    }
    return notes;
}

}  // namespace tuxscr
