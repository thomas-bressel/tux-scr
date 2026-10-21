#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "convert/param.h"
#include "convert/screen.h"
#include "formats/pack.h"

// What ConvImgCpc's assembler sources are written with here. Its functions
// that write them are turned into C++ by tools/make_asm.py, text for text:
// this gives them the strings, the file and the CPC they were written for,
// under the names C# has for them.

namespace tuxscr::asmgen {

// A string as C# has them: it may be none at all, which is not the same as
// an empty one.
class Text {
public:
    Text() = default;
    Text(const char* text)
        : text_(text)
        , null_(false)
    {
    }
    Text(const std::string& text)
        : text_(text)
        , null_(false)
    {
    }

    bool isNull() const { return null_; }
    bool empty() const { return text_.empty(); }
    int Length() const { return static_cast<int>(text_.size()); }
    Text Substring(int start, int length) const
    {
        return Text(text_.substr(static_cast<size_t>(start), static_cast<size_t>(length)));
    }
    const std::string& str() const { return text_; }

    Text& operator+=(const Text& other)
    {
        text_ += other.text_;
        null_ = false;
        return *this;
    }

private:
    std::string text_;
    bool null_ = true;
};

inline Text operator+(const Text& a, const Text& b)
{
    return Text(a.str() + b.str());
}
inline Text operator+(const Text& a, int number)
{
    return Text(a.str() + std::to_string(number));
}

inline Text S(const char* text)
{
    return Text(text);
}

// The end of a line: the system's, as C#'s is.
#ifdef _WIN32
inline const Text NL("\r\n");
#else
inline const Text NL("\n");
#endif

// A number in hexadecimal capitals, of at least `digits` digits; in decimal,
// with zeros before it to `digits` digits; a character.
Text hex(int value, int digits);
Text dec(int value, int digits = 1);
inline Text chr(char c)
{
    return Text(std::string(1, c));
}

// To the nearest whole number, a half going to the even one: C#'s
// Convert.ToInt32.
inline int roundToInt(double value)
{
    return static_cast<int>(std::nearbyint(value));
}

// The pens' colours at each line (mode X), and at each byte of each line
// (split rasters): [x][y][pen].
using LineTable = std::array<std::array<int, 16>, kMaxLines>;
using SplitPalette = std::vector<std::vector<std::array<int, 17>>>;

// The letters the Gate Array's colours are written as. One asked for a
// number that is no colour of the CPC is the first.
struct VgaLetters {
    char operator[](int colour) const { return kCpcVga[colour >= 0 && colour < 27 ? colour : 0]; }
};

// The CPC a source is written for: ConvImgCpc's Cpc, as its functions name
// it.
struct CpcView {
    explicit CpcView(const Screen& screen)
        : NbCol(screen.columns)
        , NbLig(screen.lines)
        , modeVirtuel(screen.mode)
        , yEgx(screen.yEgx)
        , cpcPlus(screen.plus)
        , Palette(screen.palette.data())
        , tabOctetMode(kPenBits)
        , trameM1(screen.patterns)
        , screen_(&screen)
    {
    }

    int NbCol;
    int NbLig;
    int modeVirtuel;
    int yEgx;
    bool cpcPlus;
    const int* Palette;
    VgaLetters CpcVGA;
    const int* tabOctetMode;
    const uint8_t (*trameM1)[4][4];

    int MaxPen(int y = 0) const { return screen_->maxPen(y); }
    const Screen& screen() const { return *screen_; }

private:
    const Screen* screen_;
};

// A source being written: C#'s StreamWriter, the text kept in memory.
class AsmText {
public:
    explicit AsmText(const Screen& screen)
        : Cpc(screen)
    {
    }

    CpcView Cpc;

    void WriteLine(const Text& line) { text_ += line.str() + NL.str(); }
    void Write(const Text& text) { text_ += text.str(); }

    const std::string& text() const { return text_; }

private:
    std::string text_;
};

}  // namespace tuxscr::asmgen
