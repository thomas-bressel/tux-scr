#include "formats/asmgen.h"

#include <algorithm>
#include <cstdio>
#include <ctime>

namespace tuxscr::asmgen {

Text hex(int value, int digits)
{
    char text[24];
    std::snprintf(text, sizeof text, "%0*X", digits, static_cast<unsigned>(value));
    return Text(text);
}

Text dec(int value, int digits)
{
    char text[24];
    std::snprintf(text, sizeof text, "%0*d", digits, value);
    return Text(text);
}

// A switch of the generated functions leaves out what ConvImgCpc's does.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
#include "asmgen.inc"
#pragma GCC diagnostic pop

std::string timeStamp()
{
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char text[40];
    std::strftime(text, sizeof text, "%d/%m/%Y (%H %M %S)", &local);
    return text;
}

void WriteHeader(AsmText& sw, const Text& version, bool withSize, const std::string& stamp)
{
    if (!version.isNull()) {
        std::string named = version.str();
        std::replace(named.begin(), named.end(), '\n', ' ');
        sw.WriteLine(S("; Généré par ") + Text(named) + S(" - le ") + Text(stamp.empty() ? timeStamp() : stamp));
    }
    sw.WriteLine(S("; Mode écran - ") + Text(kModeNames[std::clamp(sw.Cpc.modeVirtuel, 0, kModeCount - 1)]));
    if (withSize)
        sw.WriteLine(S("; Taille (nbColsxNbLignes) ") + dec(sw.Cpc.NbCol) + S("x") + dec(sw.Cpc.NbLig));
}

// ---- Palette interrupts ----
//
// The ASIC raises an interrupt at the line its register PRI (#6800) names,
// as that line's HSYNC starts. A line is 64 microseconds, of which the
// picture takes 40 (48 in overscan): what is left before the next line
// starts is the time colours can be changed in without it being seen.
//
// So the interrupt is asked for two lines or more above the one that is to
// have the new colours, and the routine waits for the picture of the line
// just above to end. Up to three colours (two in overscan) are then written
// one by one, 8 microseconds each. More are written with the stack, the
// registers loaded beforehand: 4 or 5 microseconds each, five to a line's
// end (four in overscan). Past that they are spread over the ends of the
// lines above, so that no colour changes while a line is drawn: of a change
// of all sixteen colours, the pens from 5 up have their new colour one to
// three lines early.
//
// The program waits in HALT: the interrupt is taken at once, and the
// colours are written where they are meant to be to the microsecond.

namespace {

// The latest line the ASIC can raise an interrupt at: PRI is a byte.
constexpr int kLastPriLine = 255;
// Changes of palette less than this many lines apart are done by one
// interrupt, which waits from one to the next.
constexpr int kOwnInterruptGap = 8;

// The CRTC's registers 1 and 2 as the routine that shows the picture sets
// them: the width of the picture and where HSYNC starts, in characters.
int crtcWidth(const Screen& screen)
{
    return screen.columns != 80 ? (screen.columns + 1) >> 1 : 40;
}
int crtcHsync(const Screen& screen)
{
    return screen.columns != 80 ? 26 + (screen.columns >> 2) : 46;
}

// A HSYNC that lasts to the end of its line has the ASIC raise the
// interrupt twice, at the line's start and at its HSYNC: the firmware's 14
// characters are too many for a wide picture, and are cut to this.
int hsyncWidth(const Screen& screen)
{
    return crtcHsync(screen) + 14 >= 64 ? std::max(2, std::min(9, 63 - crtcHsync(screen))) : 14;
}

// The registers colours are pushed from, in the order they are pushed, and
// the microseconds each takes to load and to push.
struct PushRegister {
    const char* name;
    int loadTime;
    int loadBytes;
    int pushTime;
    int pushBytes;
};
constexpr PushRegister kPushRegisters[5] = {{"HL", 3, 3, 4, 1}, {"DE", 3, 3, 4, 1}, {"BC", 3, 3, 4, 1}, {"IX", 4, 4, 5, 2}, {"IY", 4, 4, 5, 2}};

// How many colours fit between the end of a line's picture and the start
// of the next: written one by one, and pushed.
int writtenAtOnce(const Screen& screen)
{
    return (64 - crtcWidth(screen) - 2) / 8 + 1 > 3 ? 3 : (64 - crtcWidth(screen) - 2) / 8 + 1;
}
int pushedAtOnce(const Screen& screen)
{
    const int room = 64 - crtcWidth(screen) + 1;
    int count = 0, time = 0;
    while (count < 5 && time + kPushRegisters[count].pushTime <= room) {
        time += kPushRegisters[count].pushTime;
        ++count;
    }
    return std::max(1, count);
}

// Colours written together at the end of the line above `line`.
struct Burst {
    int line = 0;
    bool pushed = false;
    std::vector<std::pair<int, int>> pens;  // pen and colour, in the order written
    bool restores = false;                  // under the picture: the colours of its top again
};

// What changes the palette in a frame, top to bottom: for each interrupt
// the pens it gives another colour to, and under the picture the colours of
// its top again.
std::vector<Burst> paletteBursts(const Screen& screen)
{
    std::vector<Burst> bursts;
    const int pens = screen.maxPen();
    const int atOnce = writtenAtOnce(screen);
    const int pushCount = pushedAtOnce(screen);
    std::array<int, 16> before = screen.paletteAt(0);
    const auto add = [&](int line, const std::array<int, 16>& now, bool restores) {
        std::vector<int> changed;
        for (int pen = 0; pen < pens; ++pen)
            if (now[static_cast<size_t>(pen)] != before[static_cast<size_t>(pen)])
                changed.push_back(pen);
        before = now;
        if (static_cast<int>(changed.size()) <= atOnce) {
            Burst burst;
            burst.line = line;
            burst.restores = restores;
            for (const int pen : changed)
                burst.pens.emplace_back(pen, now[static_cast<size_t>(pen)]);
            bursts.push_back(burst);
            return;
        }
        // Too many to write one by one: every pen from the first changed to
        // the last is pushed, the lowest ones last, at the line itself.
        std::vector<Burst> pushed;
        for (int low = changed.front(); low <= changed.back(); low += pushCount) {
            Burst burst;
            burst.pushed = true;
            burst.restores = restores;
            const int high = std::min(changed.back(), low + pushCount - 1);
            // The stack goes down: the highest pen is pushed first.
            for (int pen = high; pen >= low; --pen)
                burst.pens.emplace_back(pen, now[static_cast<size_t>(pen)]);
            pushed.push_back(burst);
        }
        // Above the line for a change in the picture; under the picture,
        // where nothing shows, for the colours of its top.
        for (size_t n = 0; n < pushed.size(); ++n) {
            pushed[n].line = restores ? line + static_cast<int>(n) : line - static_cast<int>(n);
            bursts.push_back(pushed[n]);
        }
    };
    for (const PaletteInterrupt& interrupt : screen.interrupts)
        add(interrupt.line, screen.paletteAt(interrupt.line), false);
    add(screen.lines, screen.paletteAt(0), true);
    std::stable_sort(bursts.begin(), bursts.end(), [](const Burst& a, const Burst& b) { return a.line < b.line; });
    return bursts;
}

// The routine, written line by line, with the bytes it takes counted.
class Routine {
public:
    explicit Routine(AsmText* sw)
        : sw_(sw)
    {
    }
    void label(const std::string& name)
    {
        if (sw_)
            sw_->WriteLine(Text(name + ":"));
    }
    void op(const std::string& text, int bytes, const std::string& comment = {})
    {
        size_ += bytes;
        if (sw_)
            sw_->WriteLine(Text("\t" + text + (comment.empty() ? "" : "\t\t; " + comment)));
    }
    void comment(const std::string& text)
    {
        if (sw_)
            sw_->WriteLine(Text("; " + text));
    }
    // Waits `time` microseconds, with A and BC.
    void wait(int time)
    {
        if (time >= 1024) {
            const int n = std::min((time - 2) / 7, 0xFFFF);
            op("LD\tBC," + std::to_string(n), 3, "Attendre " + std::to_string(7 * n + 2) + " NOPs");
            op("DEC\tBC", 1);
            op("LD\tA,B", 1);
            op("OR\tC", 1);
            op("JR\tNZ,$-3", 2);
            time -= 7 * n + 2;
        }
        if (time >= 7) {
            const int n = std::min((time - 1) / 4, 255);
            op("LD\tB," + std::to_string(n), 2, "Attendre " + std::to_string(4 * n + 1) + " NOPs");
            op("DJNZ\t$", 2);
            time -= 4 * n + 1;
        }
        for (; time > 0; --time)
            op("NOP", 1);
    }
    int size() const { return size_; }

private:
    AsmText* sw_;
    int size_ = 0;
};

std::string hexWord(int value)
{
    char text[8];
    std::snprintf(text, sizeof text, "#%04X", value & 0xFFFF);
    return text;
}

// The bursts each interrupt does: the first of a group has the interrupt
// asked for, the others are waited for.
std::vector<std::vector<Burst>> burstGroups(const Screen& screen)
{
    std::vector<std::vector<Burst>> groups;
    for (const Burst& burst : paletteBursts(screen)) {
        const bool own = groups.empty()
            || (burst.line - groups.back().back().line >= kOwnInterruptGap && burst.line - 3 <= kLastPriLine);
        if (own)
            groups.emplace_back();
        groups.back().push_back(burst);
    }
    return groups;
}

// The line a group's interrupt is asked for at: two above its first burst,
// three if that one is pushed, its registers being loaded first.
int priOf(const std::vector<Burst>& group)
{
    return std::clamp(group.front().line - (group.front().pushed ? 3 : 2), 1, kLastPriLine);
}

bool anyPushed(const std::vector<std::vector<Burst>>& groups)
{
    for (const std::vector<Burst>& group : groups)
        for (const Burst& burst : group)
            if (burst.pushed)
                return true;
    return false;
}

// The interrupt routine: what is at #38 while the picture is shown.
void writeInterruptRoutine(Routine& out, const Screen& screen)
{
    const std::vector<std::vector<Burst>> groups = burstGroups(screen);
    const bool pushes = anyPushed(groups);
    // The microseconds from the interrupt being asked for to a group's
    // routine being reached: the Z80 taking it out of HALT, the jump at
    // #38, the registers saved, the ASIC brought in, the jump to the group.
    const int entryTime = 30 + (pushes ? 14 : 0);
    // When the picture of the line under the one the interrupt is asked at
    // ends, counted from the interrupt.
    const int pictureEnd = (64 - crtcHsync(screen) - 1) + crtcWidth(screen);

    out.comment("Interruptions de palette : a chaque ligne choisie, les nouvelles couleurs");
    out.label("PalInt");
    out.op("PUSH\tAF", 1);
    out.op("PUSH\tBC", 1);
    out.op("PUSH\tHL", 1);
    if (pushes) {
        out.op("PUSH\tDE", 1);
        out.op("PUSH\tIX", 2);
        out.op("PUSH\tIY", 2);
    }
    out.op("LD\tBC,#7FB8", 3);
    out.op("OUT\t(C),C", 2, "Registres de l'ASIC en #4000");
    out.label("PalIntJump");
    out.op("JP\tPalInt0", 3);
    for (size_t g = 0; g < groups.size(); ++g) {
        const std::vector<Burst>& group = groups[g];
        out.label("PalInt" + std::to_string(g));
        int time = entryTime;
        const int first = priOf(group);
        bool stackMoved = false;
        for (const Burst& burst : group) {
            // The end of the picture of the line above the burst's.
            const int end = pictureEnd + 64 * (burst.line - 2 - first);
            out.comment(burst.restores ? "Sous l'image : la palette du haut de l'image" : "Ligne " + std::to_string(burst.line));
            if (!burst.pushed) {
                // The first colour is written just as the picture has ended
                // (measured on TuxAPE's Plus: a third of a microsecond after).
                const int start = end + 3 - 8;
                if (start > time) {
                    out.wait(start - time);
                    time = start;
                }
                for (const auto& [pen, colour] : burst.pens) {
                    out.op("LD\tHL," + hexWord(asicWord(colour)), 3);
                    out.op("LD\t(" + hexWord(0x6400 + 2 * pen) + "),HL", 3, "Encre " + std::to_string(pen));
                    time += 8;
                }
                continue;
            }
            // The registers are loaded while the line above is drawn, and
            // pushed as its picture ends: a push writes in its last two
            // microseconds, the first of them just after the end.
            int loadTime = stackMoved ? 3 : 9;
            for (size_t n = 0; n < burst.pens.size(); ++n)
                loadTime += kPushRegisters[n].loadTime;
            const int start = end - 1 - loadTime;
            if (start > time) {
                out.wait(start - time);
                time = start;
            }
            if (!stackMoved)
                out.op("LD\t(PalIntSp),SP", 4);
            stackMoved = true;
            out.op("LD\tSP," + hexWord(0x6400 + 2 * (burst.pens.front().first + 1)), 3);
            for (size_t n = 0; n < burst.pens.size(); ++n)
                out.op(std::string("LD\t") + kPushRegisters[n].name + "," + hexWord(asicWord(burst.pens[n].second)), kPushRegisters[n].loadBytes);
            time += loadTime;
            for (size_t n = 0; n < burst.pens.size(); ++n) {
                out.op(std::string("PUSH\t") + kPushRegisters[n].name, kPushRegisters[n].pushBytes, "Encre " + std::to_string(burst.pens[n].first));
                time += kPushRegisters[n].pushTime;
            }
        }
        const size_t next = (g + 1) % groups.size();
        if (stackMoved) {
            out.op("LD\tSP,(PalIntSp)", 4);
        }
        out.op("LD\tA," + std::to_string(priOf(groups[next])), 2, "Prochaine interruption");
        out.op("LD\tHL,PalInt" + std::to_string(next), 3);
        if (g + 1 < groups.size())
            out.op("JP\tPalIntEnd", 3);
    }
    out.label("PalIntEnd");
    out.op("LD\t(#6800),A", 3);
    out.op("LD\t(PalIntJump+1),HL", 3);
    out.op("LD\tBC,#7FA0", 3);
    out.op("OUT\t(C),C", 2);
    if (pushes) {
        out.op("POP\tIY", 2);
        out.op("POP\tIX", 2);
        out.op("POP\tDE", 1);
    }
    out.op("POP\tHL", 1);
    out.op("POP\tBC", 1);
    out.op("POP\tAF", 1);
    out.op("EI", 1);
    out.op("RET", 1);
    if (pushes) {
        out.label("PalIntSp");
        out.op("DW\t0", 2);
    }
}

// What comes before the wait for a key, and after it.
void writeInterruptsOn(Routine& out, const Screen& screen)
{
    out.op("LD\tHL,(#39)", 3);
    out.op("LD\t(PalIntOld+1),HL", 3);
    out.op("LD\tA,#C3", 2);
    out.op("LD\t(#38),A", 3);
    out.op("LD\tHL,PalInt", 3);
    out.op("LD\t(#39),HL", 3);
    out.op("IM\t1", 2);
    if (hsyncWidth(screen) != 14) {
        out.op("LD\tBC,#BC03", 3);
        out.op("OUT\t(C),C", 2);
        out.op("LD\tBC,#BD" + hexWord(0x80 | hsyncWidth(screen)).substr(3), 3, "HSYNC plus court : une seule interruption par ligne");
        out.op("OUT\t(C),C", 2);
    }
    out.op("LD\tBC,#7FB8", 3);
    out.op("OUT\t(C),C", 2);
    out.op("LD\tA," + std::to_string(priOf(burstGroups(screen).front())), 2);
    out.op("LD\t(#6800),A", 3, "Ligne de la premiere interruption");
    out.op("LD\tBC,#7FA0", 3);
    out.op("OUT\t(C),C", 2);
    out.op("EI", 1);
    // The Z80 waits for each interrupt in HALT, so that it is taken at
    // once; the keyboard is looked at after each.
    out.label("PalIntWait");
    out.op("HALT", 1);
}

void writeInterruptsOff(Routine& out, const Screen& screen)
{
    out.op("JR\tZ,PalIntWait", 2);
    out.op("DI", 1);
    out.op("LD\tBC,#7FB8", 3);
    out.op("OUT\t(C),C", 2);
    out.op("XOR\tA", 1);
    out.op("LD\t(#6800),A", 3, "Plus d'interruption de ligne");
    out.op("LD\tBC,#7FA0", 3);
    out.op("OUT\t(C),C", 2);
    out.label("PalIntOld");
    out.op("LD\tHL,0", 3);
    out.op("LD\t(#39),HL", 3);
    if (hsyncWidth(screen) != 14) {
        out.op("LD\tBC,#BC03", 3);
        out.op("OUT\t(C),C", 2);
        out.op("LD\tBC,#BD8E", 3);
        out.op("OUT\t(C),C", 2);
    }
}

}  // namespace

void GenereAfficheInterrupts(AsmText& sw, bool overscan, PackMethod pkMethode, const Text& labelMedia,
                             const Text& labelPalette)
{
    const Screen& screen = sw.Cpc.screen();
    Routine out(&sw);
    sw.WriteLine(S("	DI"));
    GenereInitPlus(sw, labelPalette);
    GenereFormatEcran(sw);
    if (pkMethode != PackMethod::None) {
        sw.WriteLine(S("	LD	HL,") + labelMedia);
        sw.WriteLine(S("	LD	DE,#") + (overscan ? S("0200") : S("C000")));
        sw.WriteLine(S("	CALL	Depack"));
    }
    if (pkMethode == PackMethod::ZX0Ovs) {
        sw.WriteLine(S("	EX	DE,HL"));
        sw.WriteLine(S("	DEC	HL"));
        sw.WriteLine(S("	LD	IX,Zones"));
        sw.WriteLine(S("LoopDepack:"));
        sw.WriteLine(S("	LD	A,(IX+1)"));
        sw.WriteLine(S("	AND	A"));
        sw.WriteLine(S("	JR	Z,PalIntStart"));
        sw.WriteLine(S("	LD	D,A"));
        sw.WriteLine(S("	LD	E,(IX+0)"));
        sw.WriteLine(S("	LD	B,(IX+3)"));
        sw.WriteLine(S("	LD	C,(IX+2)"));
        sw.WriteLine(S("	LDDR"));
        sw.WriteLine(S("	INC	IX"));
        sw.WriteLine(S("	INC	IX"));
        sw.WriteLine(S("	INC	IX"));
        sw.WriteLine(S("	INC	IX"));
        sw.WriteLine(S("	JR	LoopDepack"));
    }
    sw.WriteLine(S("PalIntStart:"));
    writeInterruptsOn(out, screen);
    GenereTspace(sw, false, true);
    writeInterruptsOff(out, screen);
    sw.WriteLine(S("	LD	BC,#BC0C"));
    sw.WriteLine(S("	LD	A,#30"));
    sw.WriteLine(S("	OUT	(C),C"));
    sw.WriteLine(S("	INC	B"));
    sw.WriteLine(S("	OUT	(C),A"));
    sw.WriteLine(S("	EI"));
    sw.WriteLine(S("	RET"));
    writeInterruptRoutine(out, screen);
    GenereDepack(sw, pkMethode);
    if (pkMethode == PackMethod::ZX0Ovs) {
        sw.WriteLine(S("Zones:"));
        sw.WriteLine(S("	DW	#7EBF,#6C0,#76BF,#6C0,#6EBF,#6C0,#66BF,#6C0,#5EBF,#6C0,#56BF,#6C0,#4EBF,#6C0,#46BF,#CC0"));
        sw.WriteLine(S("	DW	#37FF,#600,#2FFF,#600,#27FF,#600,#1FFF,#600,#17FF,#600,#0FFF,#600"));
        sw.WriteLine(S("	DW	#0"));
    }
}

int interruptsCodeSize(const Screen& screen)
{
    Routine out(nullptr);
    writeInterruptsOn(out, screen);
    writeInterruptsOff(out, screen);
    writeInterruptRoutine(out, screen);
    return out.size();
}

std::string paletteInterruptsText(const Screen& screen, const std::string& version, const std::string& stamp)
{
    AsmText sw(screen);
    const int pens = screen.maxPen();
    const auto palette = [&](int line) {
        const std::array<int, 16> colours = screen.paletteAt(line);
        Text text = S("	DW	");
        for (int pen = 0; pen < pens; ++pen)
            text += S("#") + hex(asicWord(colours[static_cast<size_t>(pen)]), 3) + (pen + 1 < pens ? S(",") : S(""));
        return text;
    };
    sw.WriteLine(S("; Interruptions de palette (CPC Plus) - ") + Text(version) + S(" - le ") + Text(stamp.empty() ? timeStamp() : stamp));
    sw.WriteLine(S("; Mode écran - ") + Text(kModeNames[std::clamp(screen.mode, 0, kModeCount - 1)]) + S(" - ") + dec(screen.columns)
                 + S("x") + dec(screen.lines));
    sw.WriteLine(S("; Chaque couleur est celle de l'ASIC (#GRB), encres 0 a ") + dec(pens - 1) + S(", a ecrire a partir de #6400."));
    sw.WriteLine(S("; Une encre garde sa couleur tant qu'une ligne ne lui en donne pas une autre."));
    sw.WriteLine(S("PalIntNb:"));
    sw.WriteLine(S("	DB	") + dec(static_cast<int>(screen.interrupts.size())) + S("			; Nombre d'interruptions"));
    sw.WriteLine(S("PalIntLigne0:				; Ligne 0 : palette de l'image"));
    sw.WriteLine(palette(0));
    int number = 1;
    for (const PaletteInterrupt& interrupt : screen.interrupts) {
        sw.WriteLine(S("PalIntLigne") + dec(number++) + S(":"));
        sw.WriteLine(S("	DB	") + dec(interrupt.line) + S("			; Ligne ") + dec(interrupt.line));
        sw.WriteLine(palette(interrupt.line));
    }
    return sw.text();
}

}  // namespace tuxscr::asmgen
