// The programs TuxSCR writes, run on TuxAPE's own machine: the source of a
// picture and of the routine that shows it is assembled with TuxAPE's
// assembler, put in the machine's memory and run; the monitor's picture is
// then read line by line.
//
// The palette interrupts of the Plus: each pen must have, from each
// interrupt's line on, the colour that line gives it, and the one from
// above on the line before, from the first pixel of a line to its last.
//
// ConvImgCpc's own programs: each way of packing is unpacked by its own
// routine on the Z80, and the picture shown must be the one converted.
//
// Built when TuxAPE's core library is there (its build folder).
//
//   tuxape_interrupts [-v]    -v prints the routine written, and where each
//                             colour was found to change

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "check.h"
#include "core/assembler.h"
#include "core/cartridge.h"
#include "core/cpc.h"
#include "engine/picture.h"
#include "engine/screen.h"
#include "formats/animfile.h"
#include "formats/screenfile.h"

using namespace tuxscr;

namespace {

bool g_verbose = false;

// The monitor shows an overscan picture from this line of it on.
constexpr int kOverscanFirstLine = 5;

uint32_t shownColour(int colour)
{
    return 0xFF000000u | static_cast<uint32_t>((plusRed(colour) * 17) << 16 | (plusGreen(colour) * 17) << 8 | plusBlue(colour) * 17);
}

struct Machine {
    tuxape::Cpc cpc;

    // A Plus with a cartridge that does nothing, its screen as the firmware
    // sets it (or an overscan one), and the program in memory, run.
    bool run(const std::string& source, bool overscan)
    {
        const tuxape::AsmResult result = tuxape::assemble(source, "show.asm");
        for (const tuxape::AsmError& error : result.errors)
            std::printf("  show.asm:%d: %s\n", error.line, error.message.c_str());
        if (!result.ok() || !result.run)
            return false;
        std::vector<std::pair<int, std::vector<uint8_t>>> blocks;
        for (const tuxape::AsmBlock& block : result.memory)
            blocks.emplace_back(block.address, block.data);
        start(blocks, *result.run, overscan);
        return true;
    }

    // Bytes put in memory and entered at an address.
    void start(const std::vector<std::pair<int, std::vector<uint8_t>>>& blocks, int entry, bool overscan)
    {
        tuxape::Cartridge cartridge;
        cartridge.data.assign(8 * tuxape::Cartridge::kPageSize, 0);
        cartridge.data[0] = 0xF3;
        cartridge.data[1] = 0x18;
        cartridge.data[2] = 0xFE;
        cpc.setCartridge(&cartridge);
        cpc.crtc().setType(tuxape::CrtcType::AsicPlus);
        cpc.reset();
        static const uint8_t standard[] = {63, 40, 46, 0x8E, 38, 0, 25, 30, 0, 7, 0, 0, 0x30, 0};
        static const uint8_t full[] = {63, 48, 50, 0x8E, 38, 0, 34, 35, 0, 7, 0, 0, 0x0C, 0};
        for (int r = 0; r < 14; ++r) {
            cpc.out(0xBC00, static_cast<uint8_t>(r));
            cpc.out(0xBD00, overscan ? full[r] : standard[r]);
        }
        for (int frame = 0; frame < 4; ++frame)
            cpc.runFrame();
        for (const auto& [address, data] : blocks)
            for (size_t i = 0; i < data.size(); ++i)
                cpc.memory().write(static_cast<uint16_t>(static_cast<size_t>(address) + i), data[i]);
        // As a program started from BASIC finds the machine: no ROM in the
        // way of what is under #4000.
        cpc.out(0x7F00, 0x8D);
        // What the firmware has in RAM that programs count on: a jump at
        // #38 for the interrupts (here to "EI, RET"), and somewhere to
        // return to (here a loop that does nothing).
        const uint8_t stubs[] = {0xFB, 0xC9, 0x18, 0xFE};
        for (size_t i = 0; i < sizeof stubs; ++i)
            cpc.memory().write(static_cast<uint16_t>(0xBF00 + i), stubs[i]);
        cpc.memory().write(0x0038, 0xC3);
        cpc.memory().write(0x0039, 0x00);
        cpc.memory().write(0x003A, 0xBF);
        cpc.memory().write(0xBFF0, 0x02);
        cpc.memory().write(0xBFF1, 0xBF);
        cpc.cpu().sp = 0xBFF0;
        cpc.cpu().pc = static_cast<uint16_t>(entry);
        // As the firmware leaves it.
        cpc.cpu().im = 1;
        // Unpacked, shown, and a few pictures more for it to settle.
        for (int frame = 0; frame < 40; ++frame)
            cpc.runFrame();
    }
};

// Bands of the pens across the picture, the same on every line.
Picture bands(const Screen& screen)
{
    Picture picture;
    const int pens = screen.maxPen();
    const int tx = screen.pixelWidth();
    for (int y = 0; y < screen.height(); y += 2)
        for (int x = 0; x < screen.width(); x += tx)
            picture.setPixel(x, y, x * pens / screen.width(), tx);
    return picture;
}

void check(const char* what, Screen screen, PackMethod method)
{
    std::printf("%s\n", what);
    screen.tidyInterrupts();
    const Picture picture = bands(screen);
    Param param;
    param.cpcPlus = screen.plus;
    param.modeVirtuel = screen.mode;
    param.withCode = true;
    param.withPalette = true;
    const AsmSource source = screenAsm(screen, picture, param, method, {}, false, {"TuxSCR", "01/01/2026 (00 00 00)"});
    if (g_verbose) {
        // The routine, from its label on.
        const size_t from = source.text.find("PalIntStart:");
        const size_t to = source.text.find("; Decompactage");
        if (from != std::string::npos)
            std::printf("%s\n", source.text.substr(from, to == std::string::npos ? std::string::npos : to - from).c_str());
    }
    Machine machine;
    const bool overscan = screen.overscan();
    CHECK(machine.run(source.text, overscan));
    const uint32_t* frame = machine.cpc.monitor().frame();
    const int width = tuxape::Monitor::kWidth;
    const int height = tuxape::Monitor::kHeight;

    // Where the picture is on the monitor: pen 1's band is looked for, its
    // colour being no other's, and the picture's corner is one band to its
    // left, on the first row that has it.
    const int pens = screen.maxPen();
    const int bandWidth = screen.width() / pens;
    const auto shown = [&screen](int colour) { return 0xFF000000u | static_cast<uint32_t>(screen.rgb(colour)); };
    const uint32_t first = shown(screen.paletteAt(overscan ? kOverscanFirstLine : 0)[1]);
    int top = -1, left = -1;
    for (int y = 0; y < height && top < 0; ++y)
        for (int x = 0; x < width; ++x)
            if (frame[y * width + x] == first) {
                top = y;
                left = x - bandWidth;
                break;
            }
    if (g_verbose)
        std::printf("  monitor %dx%d, picture at %d,%d\n", width, height, left, top);
    CHECK(top >= 0 && left >= 0);
    if (top < 0 || left < 0)
        return;
    // An overscan picture has its first lines above the monitor's top: the
    // first row it shows is not line 0.
    const int firstLine = overscan ? kOverscanFirstLine : 0;
    // More colours than fit at the end of one line (five) are spread over
    // the ends of the lines above: on the three lines before such a change
    // a pen may have its new colour already. On the line itself and after,
    // every pen has it.
    const auto early = [&screen](int line) -> const PaletteInterrupt* {
        for (const PaletteInterrupt& interrupt : screen.interrupts) {
            if (line < interrupt.line - 3 || line >= interrupt.line)
                continue;
            int changed = 0;
            const std::array<int, 16> now = screen.paletteAt(interrupt.line), before = screen.paletteAt(interrupt.line - 1);
            for (size_t pen = 0; pen < 16; ++pen)
                changed += now[pen] != before[pen];
            if (changed > 5)
                return &interrupt;
        }
        return nullptr;
    };
    int wrong = 0;
    for (int row = 0; firstLine + row < screen.lines - (overscan ? 6 : 0) && top + row < height; ++row) {
        const int line = firstLine + row;
        const std::array<int, 16> palette = screen.paletteAt(line);
        const PaletteInterrupt* soon = early(line);
        for (int pen = 0; pen < pens; ++pen) {
            // The middle of the band, and its two ends: a colour that
            // changes while the line is drawn shows at one and not the other.
            for (const int at : {0, bandWidth / 2, bandWidth - 1}) {
                const int x = left + pen * bandWidth + at;
                const uint32_t got = frame[(top + row) * width + x];
                const uint32_t want = shown(palette[static_cast<size_t>(pen)]);
                if (got != want && !(soon && got == shown(screen.paletteAt(soon->line)[static_cast<size_t>(pen)]))) {
                    if (wrong < 12 || g_verbose)
                        std::printf("  line %d pen %d at %d: %06X, want %06X\n", line, pen, at, got & 0xFFFFFF, want & 0xFFFFFF);
                    ++wrong;
                }
            }
        }
    }
    CHECK_EQ(wrong, 0);
    if (g_verbose) {
        // The line each pen changes colour at, as the monitor has it.
        for (int pen = 0; pen < pens; ++pen) {
            std::printf("  pen %d changes at:", pen);
            const int x = left + pen * bandWidth + bandWidth / 2;
            for (int row = 1; firstLine + row < screen.lines && top + row < height; ++row)
                if (frame[(top + row) * width + x] != frame[(top + row - 1) * width + x])
                    std::printf(" %d", firstLine + row);
            std::printf("\n");
        }
    }
}

// Where on the monitor a pen is first seen in the colour an interrupt gives
// it: the picture is all of that pen, and the place is told as a line and
// the microseconds from the start of its picture (negative: before it, in
// the line above's border and blanking).
void landing(const char* what, Screen screen, int pen, int line)
{
    screen.tidyInterrupts();
    Picture picture;
    for (int y = 0; y < screen.height(); y += 2)
        picture.setPixel(0, y, pen, screen.width());
    Param param;
    param.cpcPlus = true;
    param.modeVirtuel = screen.mode;
    const AsmSource source = screenAsm(screen, picture, param, PackMethod::Standard, {}, false, {"TuxSCR", "01/01/2026 (00 00 00)"});
    Machine machine;
    if (!machine.run(source.text, screen.overscan()))
        return;
    const uint32_t* frame = machine.cpc.monitor().frame();
    const int width = tuxape::Monitor::kWidth;
    const uint32_t before = shownColour(screen.paletteAt(line - 4)[static_cast<size_t>(pen)]);
    const uint32_t after = shownColour(screen.paletteAt(line)[static_cast<size_t>(pen)]);
    const int left = (width - screen.width()) / 2;
    const int top = screen.overscan() ? -kOverscanFirstLine : 35;
    for (int l = line - 4; l <= line + 2; ++l) {
        int firstNew = -1, lastOld = -1, other = 0;
        for (int x = 0; x < screen.width(); ++x) {
            const uint32_t got = frame[(top + l) * width + left + x];
            if (got == after && firstNew < 0)
                firstNew = x;
            if (got == before)
                lastOld = x;
            if (got != after && got != before)
                ++other;
        }
        if (firstNew >= 0 && lastOld >= 0)
            std::printf("  %s: pen %d turns on line %d, %.2f microseconds into its picture (%d pixels neither colour)\n", what, pen, l,
                        firstNew / 16.0, other);
        else if (firstNew == 0 && lastOld < 0 && l <= line)
            std::printf("  %s: pen %d is new on all of line %d\n", what, pen, l);
        else if (other)
            std::printf("  %s: pen %d has %d pixels of neither colour on line %d\n", what, pen, other, l);
    }
}

// A Plus's screen as a file of its own, with the routine that shows it in
// the bytes its picture leaves unused: loaded where its header says and
// entered where it says, it shows the picture in its colours.
void checkFile(const char* what, Screen screen, PackMethod method)
{
    std::printf("%s\n", what);
    const Picture picture = bands(screen);
    Param param;
    param.cpcPlus = true;
    param.modeVirtuel = screen.mode;
    const ScreenData data = screenData(screen, picture, param, method);
    Machine machine;
    machine.start({{data.address, data.bytes}}, data.entry, screen.overscan());
    const uint32_t* monitor = machine.cpc.monitor().frame();
    const int width = tuxape::Monitor::kWidth;
    const bool overscan = screen.overscan();
    const Bitmap want = render(screen, picture);
    const int left = (width - screen.width()) / 2, top = overscan ? -kOverscanFirstLine : 35;
    int wrong = 0;
    for (int line = overscan ? kOverscanFirstLine : 0; line < screen.lines - (overscan ? 6 : 0); ++line)
        for (int x = 0; x < screen.width(); x += 3)
            if ((monitor[(top + line) * width + left + x] & 0xFFFFFF) != static_cast<uint32_t>(want.pixel(x, line * 2))) {
                if (wrong < 4 || g_verbose)
                    std::printf("  line %d x %d: %06X, want %06X\n", line, x, monitor[(top + line) * width + left + x] & 0xFFFFFF,
                                static_cast<unsigned>(want.pixel(x, line * 2)));
                ++wrong;
            }
    CHECK_EQ(wrong, 0);
}

// An animation of three frames, a block that moves over a ground of bands,
// as the source of the program that plays it: assembled and run, what the
// monitor shows when it has played is the last frame.
void checkAnimation(const char* what, int mode, const AnimOptions& options, PackMethod method)
{
    std::printf("%s\n", what);
    Screen screen;
    screen.mode = mode;
    screen.palette = {1, 24, 20, 6, 26, 0, 2, 8, 10, 12, 14, 16, 18, 22, 3, 5, 13};
    std::vector<Picture> frames(3, bands(screen));
    const int tx = screen.pixelWidth();
    for (int frame = 0; frame < 3; ++frame)
        for (int y = 40 + 30 * frame; y < 100 + 30 * frame; y += 2)
            for (int x = 64 + 96 * frame; x < 200 + 96 * frame; x += tx)
                frames[static_cast<size_t>(frame)].setPixel(x, y, (frame + 1) % screen.maxPen(), tx);
    Param param;
    param.modeVirtuel = mode;
    const AnimSource source = animationAsm(screen, {&frames[0], &frames[1], &frames[2]}, {60, 60, 60}, param, method, options,
                                           {"TuxSCR", "01/01/2026 (00 00 00)"});
    CHECK(!source.overflow);
    Machine machine;
    CHECK(machine.run(source.text, false));
    // A second and a half more: three frames, each waited for.
    for (int frame = 0; frame < 75; ++frame)
        machine.cpc.runFrame();
    const uint32_t* monitor = machine.cpc.monitor().frame();
    const int width = tuxape::Monitor::kWidth;
    const Bitmap want = render(screen, frames[2]);
    const int left = (width - 640) / 2, top = 35;
    int wrong = 0;
    for (int line = 0; line < 200; ++line)
        for (int x = 0; x < 640; x += 3)
            if ((monitor[(top + line) * width + left + x] & 0xFFFFFF) != static_cast<uint32_t>(want.pixel(x, line * 2))) {
                if (wrong < 4 || g_verbose)
                    std::printf("  line %d x %d: %06X, want %06X\n", line, x, monitor[(top + line) * width + left + x] & 0xFFFFFF,
                                static_cast<unsigned>(want.pixel(x, line * 2)));
                ++wrong;
            }
    CHECK_EQ(wrong, 0);
}

PaletteInterrupt at(int line, std::initializer_list<std::pair<int, int>> pens)
{
    PaletteInterrupt interrupt;
    interrupt.line = line;
    for (const auto& [pen, colour] : pens) {
        interrupt.colours[static_cast<size_t>(pen)] = colour;
        interrupt.defined[static_cast<size_t>(pen)] = true;
    }
    return interrupt;
}

}  // namespace

int main(int argc, char* argv[])
{
    g_verbose = argc > 1 && std::strcmp(argv[1], "-v") == 0;

    Screen mode1;
    mode1.plus = true;
    mode1.mode = kMode1;
    mode1.palette = {plusColour(0, 0, 0), plusColour(15, 0, 0), plusColour(0, 15, 0), plusColour(0, 0, 15), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, plusColour(3, 3, 3)};
    mode1.interruptsOn = true;
    mode1.interrupts = {at(50, {{1, plusColour(15, 15, 0)}}),
                        at(100, {{2, plusColour(0, 15, 15)}, {3, plusColour(15, 15, 15)}}),
                        at(150, {{0, plusColour(5, 5, 5)}, {1, plusColour(15, 0, 15)}})};
    check("mode 1, three interrupts", mode1, PackMethod::Standard);

    // Three colours at once are all in place before the line starts, at
    // its very first pixel.
    Screen three = mode1;
    three.interrupts = {at(60, {{0, plusColour(4, 4, 4)}, {1, plusColour(15, 15, 0)}, {3, plusColour(15, 15, 15)}}),
                        at(120, {{3, plusColour(9, 0, 9)}, {2, plusColour(0, 9, 9)}, {0, plusColour(9, 9, 0)}})};
    check("mode 1, three colours at once", three, PackMethod::Standard);

    // The four of mode 1 at once, and back: pushed, and in place as well.
    Screen four = mode1;
    four.interrupts = {at(40, {{0, plusColour(4, 4, 4)}, {1, plusColour(15, 15, 0)}, {2, plusColour(0, 9, 9)}, {3, plusColour(15, 15, 15)}}),
                       at(41, {{0, plusColour(0, 0, 9)}, {1, plusColour(9, 0, 0)}, {2, plusColour(0, 9, 0)}, {3, plusColour(9, 9, 9)}}),
                       at(130, {{0, plusColour(15, 15, 15)}, {1, plusColour(0, 0, 0)}, {2, plusColour(15, 0, 0)}, {3, plusColour(0, 0, 15)}})};
    check("mode 1, four colours at once", four, PackMethod::Standard);

    // Interrupts close together are done by one, which waits between them;
    // one line apart is as close as they get.
    Screen close = mode1;
    close.interrupts = {at(20, {{1, plusColour(15, 15, 0)}}), at(21, {{1, plusColour(0, 15, 15)}}), at(24, {{2, plusColour(8, 8, 8)}}),
                        at(197, {{3, plusColour(15, 15, 15)}})};
    check("mode 1, interrupts close together", close, PackMethod::ZX0_V2);

    // Mode 0: sixteen pens, all of them changed at once.
    Screen mode0;
    mode0.plus = true;
    mode0.mode = kMode0;
    for (int pen = 0; pen < 16; ++pen)
        mode0.palette[static_cast<size_t>(pen)] = plusColour(pen, 15 - pen, pen / 2);
    mode0.palette[16] = plusColour(3, 3, 3);
    mode0.interruptsOn = true;
    PaletteInterrupt all;
    all.line = 80;
    for (int pen = 0; pen < 16; ++pen) {
        all.colours[static_cast<size_t>(pen)] = plusColour(15 - pen, pen, 15);
        all.defined[static_cast<size_t>(pen)] = true;
    }
    mode0.interrupts = {all};
    check("mode 0, sixteen colours at once", mode0, PackMethod::ZX1);

    // Overscan: lines past 255 are out of the ASIC's reach, and are waited
    // for.
    Screen overscan = mode1;
    overscan.columns = 96;
    overscan.lines = 272;
    overscan.interrupts = {at(100, {{1, plusColour(15, 15, 0)}}), at(260, {{2, plusColour(0, 15, 15)}})};
    check("overscan, an interrupt past line 255", overscan, PackMethod::ZX0);
    // Overscan in mode 1 with its four colours changed at once: all that
    // fits in the sixteen microseconds between two lines.
    Screen wide = overscan;
    wide.interrupts = {at(90, {{0, plusColour(4, 4, 4)}, {1, plusColour(15, 15, 0)}, {2, plusColour(0, 9, 9)}, {3, plusColour(15, 15, 15)}})};
    check("overscan, four colours at once", wide, PackMethod::Standard);

    // The programs ConvImgCpc itself writes, with no interrupt: each way of
    // packing is unpacked by its own routine on the Z80, for a CPC and for
    // a Plus, a screen of 16K and an overscan one.
    Screen old;
    old.mode = kMode1;
    old.palette = {1, 24, 20, 6, 26, 0, 2, 8, 10, 12, 14, 16, 18, 22, 3, 5, 13};
    for (const PackMethod method : {PackMethod::Standard, PackMethod::ZX0, PackMethod::ZX0_V2, PackMethod::ZX1})
        check("a CPC's screen, each packing", old, method);
    Screen old0 = old;
    old0.mode = kMode0;
    check("a CPC's screen, mode 0", old0, PackMethod::ZX0_V2);
    Screen old2 = old;
    old2.mode = kMode2;
    check("a CPC's screen, mode 2", old2, PackMethod::ZX1);
    Screen oldWide = old;
    oldWide.columns = 96;
    oldWide.lines = 272;
    for (const PackMethod method : {PackMethod::Standard, PackMethod::ZX0Ovs, PackMethod::ZX1})
        check("a CPC's overscan screen", oldWide, method);
    Screen plusPlain = mode1;
    plusPlain.interrupts.clear();
    check("a Plus's screen, no interrupt", plusPlain, PackMethod::ZX0_V2);

    // A Plus's screens as files, with their routine in them: as they are,
    // and packed, the routine that unpacks them after them.
    checkFile("a Plus's screen file", plusPlain, PackMethod::None);
    Screen plusWide = plusPlain;
    plusWide.columns = 96;
    plusWide.lines = 272;
    checkFile("a Plus's overscan screen file", plusWide, PackMethod::None);
    for (const PackMethod method : {PackMethod::Standard, PackMethod::ZX0, PackMethod::ZX0_V2, PackMethod::ZX1})
        checkFile("a Plus's packed screen file", plusPlain, method);
    checkFile("a Plus's packed overscan screen file", plusWide, PackMethod::ZX0_V2);

    // Animations: each of ConvImgCpc's three ways of telling what changes
    // from a frame to the next, played by its own routine.
    AnimOptions window;
    checkAnimation("an animation, windows of what changed", kMode1, window, PackMethod::Standard);
    window.withDelay = true;
    checkAnimation("an animation, each frame waited for", kMode0, window, PackMethod::ZX0_V2);
    AnimOptions columns;
    columns.byColumns = true;
    checkAnimation("an animation, by columns", kMode1, columns, PackMethod::ZX1);
    AnimOptions halves;
    halves.twoZones = true;
    checkAnimation("an animation, the screen's two halves apart", kMode1, halves, PackMethod::ZX0);
    AnimOptions direct;
    direct.method = 1;
    checkAnimation("an animation, the bytes that changed", kMode1, direct, PackMethod::Standard);
    AnimOptions blocks;
    blocks.method = 2;
    checkAnimation("an animation, blocks", kMode2, blocks, PackMethod::ZX0_V2);

    if (g_verbose) {
        // Where the colours land, to the microsecond.
        Screen one = mode1;
        one.interrupts = {at(100, {{1, plusColour(15, 15, 0)}})};
        landing("one colour", one, 1, 100);
        Screen threeOf = mode1;
        threeOf.interrupts = {at(100, {{0, plusColour(4, 4, 4)}, {1, plusColour(15, 15, 0)}, {3, plusColour(15, 15, 15)}})};
        landing("first of three", threeOf, 0, 100);
        landing("last of three", threeOf, 3, 100);
        Screen fourOf = mode1;
        fourOf.interrupts = {at(100, {{0, plusColour(4, 4, 4)}, {1, plusColour(15, 15, 0)}, {2, plusColour(0, 9, 9)}, {3, plusColour(15, 15, 15)}})};
        landing("first pushed of four", fourOf, 3, 100);
        landing("last pushed of four", fourOf, 0, 100);
        Screen near = mode1;
        near.interrupts = {at(197, {{3, plusColour(15, 15, 15)}})};
        landing("near the bottom", near, 3, 197);
    }
    return checkSummary("tuxape_interrupts");
}
