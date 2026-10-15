// The split screen without its window: the lines' file as C#'s serializer
// writes it, the colours the splits give along the lines, the picture they
// show, the splits taken from a picture of them, and the program that shows
// the screen, compared line by line with what ConvImgCpc's GenSplitAsm
// writes for the same lines (worked out by hand from its source).

#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "formats/pack.h"
#include "splitasm.h"
#include "splitscreen.h"

using namespace tuxscr;

namespace {

std::vector<std::string> linesOf(const std::string& text)
{
    std::vector<std::string> lines;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

// A screen with six lines of splits that take the program through its
// cases: a first line after the frame's start, a line with nothing, a later
// first change, seven splits of which the last needs a register loaded
// again, a line so long that the next starts late, and that next line.
SplitScreen sixLines()
{
    SplitScreen screen;
    SplitLines& lines = screen.lines;
    lines[0].pen = 3;
    lines[0].splits[0] = {64, 6, true};
    lines[0].splits[1] = {32, 24, true};
    lines[2].pen = 3;
    lines[2].delay = 4;
    lines[2].splits[0] = {96, 2, true};
    lines[3].pen = 3;
    const int colours[7] = {0, 1, 2, 3, 4, 5, 26};
    for (size_t i = 0; i < 7; ++i)
        lines[3].splits[i] = {i == 5 ? 48 : 32, colours[i], true};
    lines[4].pen = 2;
    lines[4].splits[0] = {384, 26, true};
    lines[4].splits[1] = {32, 13, true};
    lines[5].pen = 2;
    lines[5].splits[0] = {32, 1, true};
    return screen;
}

void checkFile()
{
    const SplitScreen screen = sixLines();
    const std::string xml = splitXml(screen.lines);
    const std::string start =
        "<?xml version=\"1.0\"?>\r\n"
        "<SplitEcran xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\">\r\n"
        "  <LignesSplit>\r\n"
        "    <LigneSplit>\r\n"
        "      <numPen>3</numPen>\r\n"
        "      <retard>-4</retard>\r\n"
        "      <newMode>0</newMode>\r\n"
        "      <changeMode>false</changeMode>\r\n"
        "      <ListeSplit>\r\n"
        "        <Split>\r\n"
        "          <longueur>64</longueur>\r\n"
        "          <couleur>6</couleur>\r\n"
        "          <enable>true</enable>\r\n"
        "        </Split>\r\n"
        "        <Split>\r\n"
        "          <longueur>32</longueur>\r\n"
        "          <couleur>24</couleur>\r\n"
        "          <enable>true</enable>\r\n"
        "        </Split>\r\n"
        "        <Split>\r\n"
        "          <longueur>32</longueur>\r\n"
        "          <couleur>0</couleur>\r\n"
        "          <enable>false</enable>\r\n"
        "        </Split>\r\n";
    CHECK(xml.compare(0, start.size(), start) == 0);
    const std::string end = "      </ListeSplit>\r\n    </LigneSplit>\r\n  </LignesSplit>\r\n</SplitEcran>";
    CHECK(xml.size() > end.size() && xml.compare(xml.size() - end.size(), end.size(), end) == 0);
    // 272 lines of seven splits, each on its lines of the file.
    size_t count = 0;
    for (size_t at = 0; (at = xml.find("<LigneSplit>", at)) != std::string::npos; ++at)
        ++count;
    CHECK_EQ(count, 272);
    count = 0;
    for (size_t at = 0; (at = xml.find("<Split>", at)) != std::string::npos; ++at)
        ++count;
    CHECK_EQ(count, 272 * 7);

    // Read back, the lines are the same.
    SplitLines read = freshSplitLines();
    CHECK(readSplitXml(xml, read));
    CHECK(read == screen.lines);
    // The elements may come in another order, as C#'s reader takes them.
    SplitLines other = freshSplitLines();
    CHECK(readSplitXml("<?xml version=\"1.0\"?>\n<SplitEcran>\n<LignesSplit>\n<LigneSplit>\n<ListeSplit>\n<Split><enable>true</enable>"
                       "<couleur>11</couleur><longueur>70</longueur></Split>\n</ListeSplit>\n<changeMode>true</changeMode><newMode>2</newMode>"
                       "<retard>12</retard><numPen>1</numPen>\n</LigneSplit>\n<LigneSplit><numPen>99</numPen><retard>400</retard>"
                       "<ListeSplit /></LigneSplit>\n</LignesSplit>\n</SplitEcran>",
                       other));
    CHECK(other[0].splits[0].enabled && other[0].splits[0].colour == 11);
    // A length is a multiple of four; numbers the editor cannot hold are
    // brought to the nearest it can.
    CHECK_EQ(other[0].splits[0].length, 68);
    CHECK(other[0].changeMode && other[0].newMode == 2 && other[0].delay == 12 && other[0].pen == 1);
    CHECK(!other[0].splits[1].enabled);
    CHECK_EQ(other[1].pen, 15);
    CHECK_EQ(other[1].delay, 28);
    // What the file does not have is left.
    CHECK_EQ(other[2].delay, SplitScreen::kMinDelay);
    // Another file is not taken.
    CHECK(!readSplitXml("<?xml version=\"1.0\"?><Raster><line /></Raster>", other));
    CHECK(!readSplitXml("", other));
}

void checkColours()
{
    // Pen 3 everywhere, in mode 1: every byte is &FF.
    SplitScreen screen = sixLines();
    Screen cpc;
    cpc.columns = 96;
    cpc.lines = 272;
    Picture picture;
    picture.clear(3);
    screen.setPicture(cpc, picture);
    CHECK_EQ(screen.memory[0], 0xFF);
    // The second half of an overscan screen is 16K further.
    CHECK_EQ(screen.memory[0x3E00], 0xFF);
    CHECK_EQ(screen.memory[0x600], 0);
    screen.setStartColour(3, 9);
    screen.setStartColour(15, 18);
    screen.calcPalette();

    // Line 0: the pen is red at once (the earliest change is before the
    // picture), for 64 pixels of mode 1, 16 bytes less the one before the
    // picture; then yellow, to the end of the line and on the line under.
    CHECK(screen.paletteColour(0, 0, 3) == kCpcColours[6]);
    CHECK(screen.paletteColour(14, 0, 3) == kCpcColours[6]);
    CHECK(screen.paletteColour(15, 0, 3) == kCpcColours[24]);
    CHECK(screen.paletteColour(95, 0, 3) == kCpcColours[24]);
    CHECK(screen.paletteColour(0, 1, 3) == kCpcColours[24]);
    CHECK(screen.paletteColour(0, 0, 2) == kCpcColours[20]);
    CHECK(screen.changes(0) == (std::vector<int>{-1, 15, 23}));
    CHECK(screen.changes(1) == (std::vector<int>{-1}));
    // Line 2: its change comes a byte into the picture.
    CHECK(screen.paletteColour(0, 2, 3) == kCpcColours[24]);
    CHECK(screen.paletteColour(1, 2, 3) == kCpcColours[2]);
    CHECK(screen.changes(2) == (std::vector<int>{1, 25}));
    // Line 3: seven colours, eight bytes each but the sixth of twelve.
    CHECK(screen.paletteColour(6, 3, 3) == kCpcColours[0]);
    CHECK(screen.paletteColour(7, 3, 3) == kCpcColours[1]);
    CHECK(screen.paletteColour(39, 3, 3) == kCpcColours[5]);
    CHECK(screen.paletteColour(50, 3, 3) == kCpcColours[5]);
    CHECK(screen.paletteColour(51, 3, 3) == kCpcColours[26]);
    // Line 4 is for another pen: pen 3 keeps what it had.
    CHECK(screen.paletteColour(40, 4, 3) == kCpcColours[26]);
    CHECK(screen.paletteColour(40, 4, 2) == kCpcColours[26]);
    CHECK(screen.paletteColour(95, 4, 2) == kCpcColours[13]);
    // A split that would go past the line ends with it.
    CHECK(screen.changes(4) == (std::vector<int>{-1, 95, 96}));

    // The picture: 768 by 544, each byte eight pixels and each line two
    // rows.
    Bitmap shown(768, 544);
    screen.render(shown);
    CHECK(shown.colour(0, 0) == kCpcColours[6]);
    CHECK(shown.colour(119, 1) == kCpcColours[6]);
    CHECK(shown.colour(120, 0) == kCpcColours[24]);
    CHECK(shown.colour(767, 3) == kCpcColours[24]);
    CHECK(shown.colour(7, 4) == kCpcColours[24]);
    CHECK(shown.colour(8, 4) == kCpcColours[2]);
    CHECK(shown.colour(700, 543) == kCpcColours[26]);

    // A line that asks for another mode has the line under it shown in it:
    // in mode 0 the same bytes are pen 15.
    screen.lines[9].changeMode = true;
    screen.lines[9].newMode = 0;
    screen.calcPalette();
    CHECK_EQ(screen.lineModes[9], 1);
    CHECK_EQ(screen.lineModes[10], 0);
    CHECK_EQ(screen.lineModes[11], 1);
    screen.render(shown);
    CHECK(shown.colour(300, 20) == kCpcColours[18]);
    CHECK(shown.colour(300, 22) == kCpcColours[26]);
    // No longer asked, the line is back to the picture's mode.
    screen.lines[9].changeMode = false;
    screen.calcPalette();
    CHECK_EQ(screen.lineModes[10], 1);

    // A picture smaller than the screen: what it does not cover is pen 0.
    Screen small;
    SplitScreen partly;
    partly.setPicture(small, picture);
    CHECK_EQ(partly.memory[79], 0xFF);
    CHECK_EQ(partly.memory[80], 0);
    // Line 199 is the last of it: character line 24, row 7.
    CHECK_EQ(partly.memory[0x3E00 + 8 * 96 + 7 * 0x800], 0xFF);
    CHECK_EQ(partly.memory[0x3E00 + 9 * 96], 0);
    // In mode 0 the lines are mode 0's.
    small.mode = kMode0;
    partly.setPicture(small, picture);
    partly.calcPalette();
    CHECK_EQ(partly.lineModes[0], 0);
}

// The picture of splits ConvImgCpc would be given: rows of one colour,
// rows of two, a colour too short, and more colours than a line takes.
Bitmap splitPicture()
{
    const int red = 0xFF0000, blue = 0x0000FF, white = 0xFFFFFF;
    Bitmap picture(768, 544);
    const auto row = [&picture](int y, const auto& colourAt) {
        for (int x = 0; x < 384; ++x) {
            const int colour = colourAt(x);
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx)
                    picture.setPixel(x * 2 + dx, y * 2 + dy, colour);
        }
    };
    for (int y = 0; y < 10; ++y)
        row(y, [=](int) { return red; });
    for (int y = 10; y < 20; ++y)
        row(y, [=](int x) { return x < 100 ? red : blue; });
    row(30, [=](int x) { return x < 8 ? white : 0; });
    row(40, [=](int x) { return (x / 40) % 2 == 0 ? red : blue; });
    return picture;
}

void checkImport()
{
    SplitLines lines = freshSplitLines();
    lines[5].splits[3] = {200, 7, true};
    lines[270].splits[0] = {64, 9, true};
    const std::vector<SplitImportNote> notes = importSplits(splitPicture(), 2, lines);

    // One colour all along the first line: one split, as long as the line
    // from the earliest change. The lines under have nothing to change.
    CHECK(lines[0].splits[0] == (Split{380, 6, true}));
    CHECK(!lines[0].splits[1].enabled);
    CHECK_EQ(lines[0].pen, 2);
    CHECK(!lines[1].splits[0].enabled);
    CHECK(!lines[5].splits[0].enabled && !lines[5].splits[3].enabled);
    CHECK_EQ(lines[5].splits[3].length, 32);
    // Two colours: two splits, the first as long as its colour.
    CHECK(lines[10].splits[0] == (Split{100, 6, true}));
    CHECK(lines[10].splits[1] == (Split{280, 2, true}));
    CHECK(!lines[10].splits[2].enabled);
    CHECK(lines[19].splits[1] == (Split{280, 2, true}));
    // A line of one colour is compared with the lines above but the one
    // just above: the change to black is taken a line late.
    CHECK(!lines[20].splits[0].enabled);
    CHECK(lines[21].splits[0] == (Split{380, 0, true}));
    CHECK(!lines[22].splits[0].enabled);
    // A colour of eight pixels is too short: it lasts the 32 a change takes.
    CHECK(lines[30].splits[0] == (Split{32, 26, true}));
    CHECK(lines[30].splits[1] == (Split{348, 0, true}));
    CHECK(!lines[31].splits[0].enabled);
    CHECK(lines[32].splits[0] == (Split{380, 0, true}));
    // Ten colours on a line: the seven first are kept.
    for (size_t i = 0; i < 7; ++i)
        CHECK(lines[40].splits[i] == (Split{40, i % 2 == 0 ? 6 : 2, true}));
    // What was found is told: six times for the short colour, once for the
    // line that has too many.
    CHECK_EQ(notes.size(), 7);
    int tooShort = 0, tooMany = 0;
    for (const SplitImportNote& note : notes) {
        tooShort += note.kind == SplitImportNote::Kind::TooShort && note.line == 30;
        tooMany += note.kind == SplitImportNote::Kind::TooMany && note.line == 40;
    }
    CHECK_EQ(tooShort, 6);
    CHECK_EQ(tooMany, 1);
    // The lines the program does not reach are left as they were.
    CHECK(lines[270].splits[0] == (Split{64, 9, true}));
    CHECK_EQ(lines[270].pen, 0);
    CHECK_EQ(lines[261].pen, 2);

    CHECK_EQ(nearestCpcColour(Rgb(255, 255, 255)), 26);
    CHECK_EQ(nearestCpcColour(Rgb(86, 0, 171)), 5);
    CHECK_EQ(nearestCpcColour(Rgb(85, 170, 85)), 9);
}

void checkProgram()
{
    SplitScreen screen = sixLines();
    // A picture that is not all of one byte.
    for (size_t i = 0; i < 0x8000; ++i)
        screen.memory[i] = static_cast<uint8_t>((i / 96) * 7);
    // The pen the first line changes at once starts with a colour of its
    // own all the same.
    screen.setStartColour(3, 9);
    const std::vector<std::string> source = linesOf(splitProgram(screen, PackMethod::Standard));

    // The picture first, packed without what an overscan screen leaves
    // unused in memory, so that it ends where the program starts.
    std::vector<uint8_t> memory(screen.memory.begin(), screen.memory.begin() + 0x7CC0);
    for (size_t block = 0; block < 7; ++block)
        std::fill(memory.begin() + 0x600 + block * 0x800, memory.begin() + 0x800 + block * 0x800, 0);
    for (size_t block = 0; block < 8; ++block)
        std::fill(memory.begin() + 0x44C0 + block * 0x800, memory.begin() + std::min<size_t>(0x4600 + block * 0x800, 0x7CC0), 0);
    const std::vector<uint8_t> packed = pack(memory, PackMethod::Standard);
    CHECK(packed.size() > 16 && packed.size() < 0x7CC0);
    char text[64];
    std::snprintf(text, sizeof text, "\tORG\t#%04X", static_cast<unsigned>(0x8400 - packed.size()));
    CHECK(source.size() > 400);
    if (source.size() < 400)
        return;
    CHECK(source[0] == text);
    CHECK(source[1] == "\tNolist");
    CHECK(source[2] == "ImageCmp");
    std::string first = "\tDB\t";
    for (size_t i = 0; i < 16; ++i) {
        std::snprintf(text, sizeof text, "%s#%02X", i ? "," : "", packed[i]);
        first += text;
    }
    CHECK(source[3] == first);
    const size_t dataLines = (packed.size() + 15) / 16;
    std::snprintf(text, sizeof text, "; Taille totale %zu octets", packed.size());
    CHECK(source[3 + dataLines] == text);
    CHECK(source[4 + dataLines] == "\tRUN $");
    CHECK(source[5 + dataLines].empty());
    CHECK(source[6 + dataLines] == "\tnolist");
    CHECK(source[7 + dataLines] == "\tDI");
    CHECK(source[8 + dataLines] == "\tLD\tHL,ImageCmp");
    CHECK(source[9 + dataLines] == "\tLD\tDE,#0200");

    // The lines: from the frame's start (the DI after HALT) to the jump
    // back.
    size_t from = 0, to = 0;
    for (size_t i = 0; i + 1 < source.size(); ++i) {
        if (source[i] == "\tHALT" && source[i + 1] == "\tDI")
            from = i + 2;
        if (source[i] == "\tJP\tBoucle")
            to = i;
    }
    CHECK(from > 0 && to > from);
    const std::vector<std::string> wanted = {
        // 15132 pixels from the interrupt to the first line: 1891 NOPs.
        "\tLD\tBC,269\t\t\t; Attendre 1887 NOPs (7*269+4)",
        "\tDEC\tBC",
        "\tLD\tA,B",
        "\tOR\tC",
        "\tJR\tNZ,$-3",
        "\tLD\tB,#7F",
        "\tADD\tIX,BC\t\t\t; Attendre 4 NOPs",
        "; ---- Ligne 0 ----",
        "\tLD\tC,3\t\t\t; (2 NOPs)",
        "\tOUT\t(C),C\t\t\t; (4 NOPs)",
        "\tLD\tC,#4C\t\t\t; (2 NOPs)",
        "\tLD\tDE,#4A54\t\t; (3 NOPs)",
        // HL and A are not loaded: the five NOPs they would take.
        "\tADD\tIX,BC\t\t\t; Attendre 4 NOPs",
        "\tNOP\t\t\t\t; Attendre 1 NOP",
        "\tOUT\t(C),C\t\t\t; (4 NOPs)",
        "\tADD\tIX,BC\t\t\t; Attendre 4 NOPs",
        "\tOUT\t(C),D\t\t\t; (4 NOPs)",
        "; ---- Ligne 1 (vide) ----",
        // 36 NOPs left of line 0, the 64 of line 1 and one of delay.
        "\tLD\tB,24\t\t\t; Attendre 99 NOPs (4*24+3)",
        "\tDJNZ\t$-0",
        "\tLD\tB,#7F",
        "\tCP\t(HL)\t\t\t; Attendre 2 NOPs",
        "; ---- Ligne 2 ----",
        "\tLD\tC,3\t\t\t; (2 NOPs)",
        "\tOUT\t(C),C\t\t\t; (4 NOPs)",
        "\tLD\tC,#55\t\t\t; (2 NOPs)",
        "\tLD\tB,1\t\t\t; Attendre 7 NOPs (4*1+3)",
        "\tDJNZ\t$-0",
        "\tLD\tB,#7F",
        "\tNOP\t\t\t\t; Attendre 1 NOP",
        "\tOUT\t(C),C\t\t\t; (4 NOPs)",
        "\tLD\tB,10\t\t\t; Attendre 43 NOPs (4*10+3)",
        "\tDJNZ\t$-0",
        "\tLD\tB,#7F",
        "; ---- Ligne 3 ----",
        "\tLD\tC,3\t\t\t; (2 NOPs)",
        "\tOUT\t(C),C\t\t\t; (4 NOPs)",
        "\tLD\tC,#54\t\t\t; (2 NOPs)",
        "\tLD\tDE,#4455\t\t; (3 NOPs)",
        "\tLD\tHL,#5C58\t\t; (3 NOPs)",
        "\tLD\tA,#5D\t\t\t; (2 NOPs)",
        "\tOUT\t(C),C\t\t\t; (4 NOPs)",
        "\tOUT\t(C),D\t\t\t; (4 NOPs)",
        "\tOUT\t(C),E\t\t\t; (4 NOPs)",
        "\tOUT\t(C),H\t\t\t; (4 NOPs)",
        "\tOUT\t(C),L\t\t\t; (4 NOPs)",
        "\tOUT\t(C),A\t\t\t; (4 NOPs)",
        // The seventh colour is none of the six: A is loaded again.
        "\tLD\tA,#4B\t\t\t; (2 NOPs)",
        "\tOUT\t(C),A\t\t\t; (4 NOPs)",
        // 18 NOPs to line 4, the last three with HL: it is loaded again.
        "\tLD\tB,3\t\t\t; Attendre 15 NOPs (4*3+3)",
        "\tDJNZ\t$-0",
        "\tLD\tB,#7F",
        "\tADD\tHL,BC\t\t\t; Attendre 3 NOPs",
        "; ---- Ligne 4 ----",
        "\tLD\tC,2\t\t\t; (2 NOPs)",
        "\tOUT\t(C),C\t\t\t; (4 NOPs)",
        "\tLD\tC,#4B\t\t\t; (2 NOPs)",
        "\tLD\tDE,#4054\t\t; (3 NOPs)",
        "\tLD\tHL,#5454\t\t; (3 NOPs)",
        "\tCP\t(HL)\t\t\t; Attendre 2 NOPs",
        "\tOUT\t(C),C\t\t\t; (4 NOPs)",
        "\tLD\tB,10\t\t\t; Attendre 43 NOPs (4*10+3)",
        "\tDJNZ\t$-0",
        "\tLD\tB,#7F",
        "\tNOP\t\t\t\t; Attendre 1 NOP",
        "\tOUT\t(C),D\t\t\t; (4 NOPs)",
        // Line 4 ran four NOPs into line 5, whose pen is the same: it is
        // not selected again.
        "; ---- Ligne 5 ----",
        "\tCP\t(HL)\t\t\t; Attendre 2 NOPs",
        "\tLD\tC,#44\t\t\t; (2 NOPs)",
        "\tLD\tB,1\t\t\t; Attendre 7 NOPs (4*1+3)",
        "\tDJNZ\t$-0",
        "\tLD\tB,#7F",
        "\tNOP\t\t\t\t; Attendre 1 NOP",
        "\tOUT\t(C),C\t\t\t; (4 NOPs)",
        "; ---- Ligne 6 (vide) ----",
    };
    for (size_t i = 0; i < wanted.size() && from + i < to; ++i) {
        if (source[from + i] != wanted[i]) {
            std::printf("line %zu of the program: \"%s\", want \"%s\"\n", i, source[from + i].c_str(), wanted[i].c_str());
            ++g_failures;
        }
    }
    // Every line to the last the program does has its own, and no other.
    CHECK_EQ(to - from, wanted.size() + 255);
    CHECK(source[to - 1] == "; ---- Ligne 261 (vide) ----");
    // The end: the routine that unpacks, the overscan screen's registers
    // and the palette the screen starts with, as the Gate Array wants it.
    CHECK(source[to + 1].empty());
    CHECK(source[to + 2] == "; Decompactage");
    const size_t last = source.size() - 1;
    // Pen 3 is red at the first byte of the first line: that is what
    // ConvImgCpc writes for it, whatever it started with, which the editor
    // keeps all the same.
    CHECK(source[last] == "\tDB\t#44,#4A,#53,#4C,#4B,#54,#55,#45,#46,#5E,#5F,#47,#52,#59,#44,#5F");
    CHECK_EQ(screen.startColour(3), 9);
    CHECK(source[last - 1] == "Palette");
    CHECK(source[last - 2] == "\tDB\t1,48,2,50,3,#8E,6,34,7,35,12,13,13,0,0");
    CHECK(source[last - 3] == "Overscan");
    CHECK(source[last - 5] == "\tList");
}

}  // namespace

int main()
{
    checkFile();
    checkColours();
    checkImport();
    checkProgram();
    return checkSummary("gui_split_model");
}
