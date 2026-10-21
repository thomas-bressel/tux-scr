#include "formats/screenfile.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "formats/amsdos.h"
#include "formats/asmgen.h"

namespace tuxscr {

namespace {

#include "codes.inc"

void poke16(std::vector<uint8_t>& bytes, size_t at, int value)
{
    bytes[at] = static_cast<uint8_t>(value & 0xFF);
    bytes[at + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

template <size_t N>
void put(std::vector<uint8_t>& bytes, size_t at, const uint8_t (&code)[N])
{
    std::copy(code, code + N, bytes.begin() + static_cast<long>(at));
}

template <size_t N>
void append(std::vector<uint8_t>& bytes, const uint8_t (&code)[N])
{
    bytes.insert(bytes.end(), code, code + N);
}

// The mode and the palette as a screen holds them: 48 bytes.
std::vector<uint8_t> modeAndPalette(const Screen& screen, const Param& param)
{
    std::vector<uint8_t> bytes(48, 0);
    if (param.cpcPlus) {
        bytes[0] = static_cast<uint8_t>(screen.mode | 0x8C);
        size_t k = 1;
        for (size_t i = 0; i < 16; ++i) {
            const int colour = screen.palette[i];
            bytes[k++] = static_cast<uint8_t>(((colour >> 4) & 0x0F) | (colour << 4));
            bytes[k++] = static_cast<uint8_t>(colour >> 8);
        }
    } else {
        bytes[0] = static_cast<uint8_t>(screen.mode);
        for (size_t i = 0; i < 16; ++i)
            bytes[1 + i] = static_cast<uint8_t>(screen.palette[i]);
    }
    return bytes;
}

asmgen::Text label(const std::string& text)
{
    return asmgen::Text(text);
}

void writeHeader(asmgen::AsmText& sw, const AsmStamp& stamp, bool withSize)
{
    asmgen::WriteHeader(sw, stamp.version.empty() ? asmgen::Text() : asmgen::Text(stamp.version), withSize, stamp.time);
}

const uint8_t* dataOf(const std::vector<uint8_t>& bytes)
{
    static const uint8_t none = 0;
    return bytes.empty() ? &none : bytes.data();
}

}  // namespace

ScreenData screenData(const Screen& screen, const Picture& picture, const Param& param, PackMethod method,
                      const PackProgress& progress)
{
    return screenData(screen, screenMemory(screen, picture), param, method, progress);
}

ScreenData screenData(const Screen& screen, std::vector<uint8_t> memory, const Param& param, PackMethod method,
                      const PackProgress& progress)
{
    const bool overscan = screen.overscan();
    const bool egx = screen.mode == kModeEgx1 || screen.mode == kModeEgx2;
    const std::vector<uint8_t> modePal = param.withPalette ? modeAndPalette(screen, param) : std::vector<uint8_t>(48, 0);
    memory.resize(0x10000, 0);

    // The routine of the EGX modes is told which mode its first line has.
    uint8_t egx0[sizeof kCodeEgx0];
    std::memcpy(egx0, kCodeEgx0, sizeof egx0);
    if (egx) {
        int mode = screen.mode == kModeEgx2 ? 0x8D03 : 0x8C01;
        if (screen.yEgx == 2)
            mode += 0x100;
        egx0[20] = static_cast<uint8_t>(mode & 0xFF);
        egx0[21] = static_cast<uint8_t>(mode >> 8);
    }

    if (!overscan) {
        // A screen of 16K leaves 48 bytes free at the end of each 2K.
        if (param.withPalette)
            std::copy(modePal.begin(), modePal.end(), memory.begin() + 0x17D0);
        if (param.withCode) {
            if (param.cpcPlus) {
                put(memory, 0x07D0, kCodeP0);
                put(memory, 0x0FD0, kCodeP1);
                put(memory, 0x1FD0, kCodeP3);
            } else {
                put(memory, 0x07D0, kCodeStd);
            }
            if (egx) {
                put(memory, 0x37D0, egx0);
                put(memory, 0x2FD0, kCodeEgx1);
                memory[0x07F2] = 0xD0;
                memory[0x07F3] = 0xF7;  // CALL #F7D0
                memory[0x37FA] = 0xEF;  // CALL #EFD0
            }
        }
    } else if (screen.lines * screen.columns > 0x4000) {
        std::copy(modePal.begin(), modePal.end(), memory.begin() + 0x600);
        if (param.withCode) {
            if (param.cpcPlus)
                put(memory, 0x621, kCodeOvP);
            else
                put(memory, 0x611, kCodeOv);
            if (egx) {
                put(memory, 0x1600, egx0);
                put(memory, 0x1640, kCodeEgx1);
                if (param.cpcPlus) {
                    memory[0x669] = 0xCD;
                    poke16(memory, 0x66A, 0x1800);  // CALL #1800
                } else {
                    poke16(memory, 0x631, 0x1800);
                }
                poke16(memory, 0x1629, 0x1840);  // CALL #1840
            }
        }
    }

    ScreenData out;
    out.address = overscan ? 0x200 : 0xC000;
    out.entry = overscan ? (param.cpcPlus ? 0x821 : 0x811) : 0xC7D0;
    const size_t size = static_cast<size_t>(std::clamp(screen.memorySize(), 0, 0x10000));
    memory.resize(size);
    if (method == PackMethod::ZX0Ovs && (screen.lines != 272 || screen.columns != 96))
        method = PackMethod::ZX0;
    if (method == PackMethod::None) {
        out.bytes = std::move(memory);
        return out;
    }

    out.bytes = pack(memory, method, progress);
    const int lg = static_cast<int>(out.bytes.size());
    const size_t at = out.bytes.size();
    const int screenAddress = out.address;
    const int screenEntry = out.entry;
    // The routine that unpacks goes after the packed screen, the whole
    // ending at #A657.
    const auto finish = [&](size_t codeSize) {
        out.address = 0xA657 - (lg + static_cast<int>(codeSize));
        poke16(out.bytes, at + 0x01, out.address);
        poke16(out.bytes, at + 0x04, screenAddress);
        out.entry = 0xA657 - static_cast<int>(codeSize);
    };
    if (!param.withCode) {
        out.address = 0xA657 - lg;
        out.entry = 0;
        return out;
    }
    switch (method) {
    case PackMethod::Standard:
        append(out.bytes, kCodeDepack);
        finish(sizeof kCodeDepack);
        poke16(out.bytes, at + 0x20, screenEntry);
        break;
    case PackMethod::ZX0: {
        append(out.bytes, kCodeDZX0);
        finish(sizeof kCodeDZX0);
        const int code = out.entry;
        poke16(out.bytes, at + 0x0E, code + 0x3F);
        poke16(out.bytes, at + 0x16, code + 0x3F);
        poke16(out.bytes, at + 0x23, code + 0x3F);
        poke16(out.bytes, at + 0x3A, code + 0x47);
        poke16(out.bytes, at + 0x2A, screenEntry);
        break;
    }
    case PackMethod::ZX0_V2: {
        append(out.bytes, kCodeDZX0_V2);
        finish(sizeof kCodeDZX0_V2);
        const int code = out.entry;
        poke16(out.bytes, at + 0x0E, code + 0x3D);
        poke16(out.bytes, at + 0x16, code + 0x3D);
        poke16(out.bytes, at + 0x26, code + 0x3E);
        poke16(out.bytes, at + 0x38, code + 0x45);
        poke16(out.bytes, at + 0x2A, screenEntry);
        break;
    }
    case PackMethod::ZX1: {
        append(out.bytes, kCodeDZX1);
        finish(sizeof kCodeDZX1);
        const int code = out.entry;
        poke16(out.bytes, at + 0x0D, code + 0x3B);
        poke16(out.bytes, at + 0x15, code + 0x3B);
        poke16(out.bytes, at + 0x36, code + 0x3B);
        poke16(out.bytes, at + 0x30, screenEntry);
        break;
    }
    default:
        // ZX0Ovs has no routine of its own to go with the file.
        out.address = 0xA657 - lg;
        out.entry = 0;
        break;
    }
    return out;
}

std::vector<uint8_t> screenFile(const std::string& fileName, const ScreenData& data)
{
    return withAmsdosHeader(fileName, data.address, data.entry, data.bytes);
}

Dsk::Error addScreenToDsk(Dsk& dsk, const ScreenData& data, bool packed, std::string* name)
{
    Dsk::Error error = Dsk::Error::None;
    for (int i = 0; i < 100; ++i) {
        char tried[16];
        std::snprintf(tried, sizeof tried, "CNVIMG%02d%s", i, packed ? ".CMP" : ".SCR");
        error = dsk.addFile(withAmsdosHeader(tried, data.address, data.entry, data.bytes), tried);
        if (error == Dsk::Error::None) {
            if (name)
                *name = tried;
            break;
        }
        // A full disc is no fuller under another name.
        if (error != Dsk::Error::FileExists)
            break;
    }
    return error;
}

AsmSource screenAsm(const Screen& screen, const Picture& picture, const Param& param, PackMethod method,
                    const AsmLabels& labels, bool inTwoFiles, const AsmStamp& stamp, const PackProgress& progress)
{
    using namespace asmgen;
    // The source has the picture alone: the routine that shows it is written
    // as text after it, not in the picture's own bytes.
    Param plain = param;
    plain.withCode = false;
    plain.withPalette = false;
    if (method == PackMethod::ZX0Ovs && (screen.lines != 272 || screen.columns != 96))
        method = PackMethod::ZX0;
    const ScreenData data = screenData(screen, picture, plain, method, progress);
    const std::vector<uint8_t>& bytes = data.bytes;
    const int lg = static_cast<int>(bytes.size());

    const bool overscan = screen.overscan();
    const bool interrupts = param.withCode && screen.interruptsActive();
    const bool twoFiles = inTwoFiles && method != PackMethod::None && !param.withCode && lg > 16384;
    const std::string labelMedia = labels.media.empty() && param.withCode ? "ImageCmp" : labels.media;
    const std::string labelPalette = labels.palette.empty() ? "Palette" : labels.palette;

    AsmSource out;
    AsmText sw(screen);
    writeHeader(sw, stamp, true);
    if (param.withCode) {
        int org = 0xA500 - lg - (screen.mode == kModeX ? 600 : 0);
        if (interrupts)
            org -= interruptsCodeSize(screen);
        sw.WriteLine(S("	ORG	#") + hex(org, 4));
        sw.WriteLine(S("	Nolist"));
    }
    if (twoFiles) {
        GenereDatas(sw, bytes.data(), 16384, 16);
        out.text = sw.text();
        AsmText rest(screen);
        writeHeader(rest, stamp, true);
        GenereDatas(rest, bytes.data() + 16384, lg - 16384, 16);
        out.second = rest.text();
        return out;
    }
    GenereDatas(sw, dataOf(bytes), lg, 16, 0, label(labelMedia));
    if (param.withCode) {
        sw.WriteLine(S("	List"));
        sw.WriteLine(S("	RUN	$"));
        sw.WriteLine(S("_StartDepack:"));
        if (screen.mode == kModeEgx1 || screen.mode == kModeEgx2)
            GenereAfficheModeEgx(sw, screen.palette.data(), overscan, method, label(labelMedia), label(labelPalette));
        else if (screen.mode == kModeX)
            GenereAfficheModeX(sw, screen.lineColours, overscan, method, label(labelMedia));
        else if (interrupts)
            GenereAfficheInterrupts(sw, overscan, method, label(labelMedia), label(labelPalette));
        else
            GenereAfficheStd(sw, overscan, method, label(labelMedia), label(labelPalette));
    }
    if (param.withPalette && (screen.mode < kModeEgx1 || screen.mode > kModeX))
        // The routine names the palette: it is given its label even if none
        // was asked for.
        GenerePalette(sw, param, param.withCode, param.withCode, label(param.withCode ? labelPalette : labels.palette));
    out.text = sw.text();
    return out;
}

std::string spriteAsm(const Screen& screen, const Picture& picture, const Param& param, const AsmLabels& labels,
                      const AsmStamp& stamp, PackMethod method, const PackProgress& progress)
{
    using namespace asmgen;
    const std::vector<uint8_t> sprite = spriteBytes(screen, picture);
    AsmText sw(screen);
    writeHeader(sw, stamp, true);
    if (method == PackMethod::None) {
        const int width = screen.width();
        GenereDatas(sw, dataOf(sprite), static_cast<int>(sprite.size()), width >> (width <= 320 ? 3 : width <= 640 ? 4 : 5), 0,
                    label(labels.media));
    } else {
        const std::vector<uint8_t> packed = pack(sprite, method == PackMethod::ZX0Ovs ? PackMethod::ZX0 : method, progress);
        GenereDatas(sw, dataOf(packed), static_cast<int>(packed.size()), 16, 0, label(labels.media));
    }
    if (param.withPalette)
        GenerePalette(sw, param, false, false, label(labels.palette));
    return sw.text();
}

std::string matrixAsm(const Screen& screen, const Picture& picture, const Param& param, PackMethod method,
                      const AsmStamp& stamp)
{
    using namespace asmgen;
    std::vector<uint8_t> matrix;
    for (int y = 0; y < screen.height(); y += 8)
        for (int x = 0; x < screen.width(); x += 8) {
            const int pen = picture.pen(x, y);
            matrix.push_back(static_cast<uint8_t>(kPenBits[pen] + (kPenBits[pen] >> 1)));
        }
    const std::vector<uint8_t> packed = method == PackMethod::None ? matrix : pack(matrix, method == PackMethod::ZX0Ovs ? PackMethod::ZX0 : method);
    AsmText sw(screen);
    writeHeader(sw, stamp, false);
    GenerePalette(sw, param, false, false, S("Palette"));
    sw.WriteLine(S("Mat64x64Cmp"));
    GenereDatas(sw, dataOf(packed), static_cast<int>(packed.size()), 16);
    return sw.text();
}

std::string bumpAsm(const Screen& screen, const Picture& picture, const Param& param, const AsmStamp& stamp)
{
    using namespace asmgen;
    std::vector<uint8_t> bump;
    for (int y = 0; y < screen.height(); y += 16) {
        const int tx = std::max(1, screen.pixelWidth(y) >> 1);
        for (int x = 0; x < screen.width(); x += 8)
            for (int yy = 0; yy < 2; ++yy)
                bump.push_back(screenByte(screen, picture, x, y + (yy << 3), tx));
    }
    AsmText sw(screen);
    writeHeader(sw, stamp, true);
    GenerePalette(sw, param, false, false, S("Palette"));
    GenereDatas(sw, dataOf(bump), static_cast<int>(bump.size()), 16);
    return sw.text();
}

// ---- Reading a screen back ----

namespace {

void setPalette(Screen& screen, const std::vector<uint8_t>& from, size_t start, bool plus)
{
    if (start + 33 > from.size())
        return;
    screen.mode = from[start] & 0x03;
    for (size_t i = 0; i < 16; ++i)
        screen.palette[i] = plus ? ((from[start + 1 + (i << 1)] << 4) & 0xF0) + (from[start + 1 + (i << 1)] >> 4)
                                       + (from[start + 2 + (i << 1)] << 8)
                                 : from[start + i + 1];
}

bool matches(const std::vector<uint8_t>& memory, size_t at, std::initializer_list<uint8_t> bytes)
{
    for (const uint8_t byte : bytes)
        if (memory[at++] != byte)
            return false;
    return true;
}

// A screen ConvImgCpc saved has its routine in it, which says what it is
// and where its palette is.
bool readOwnScreen(Screen& screen, const std::vector<uint8_t>& memory)
{
    if (matches(memory, 0x7D0, {0x3A, 0xD0, 0xD7, 0xCD})) {  // a CPC's, 16K
        screen.plus = false;
        screen.columns = 80;
        screen.lines = 200;
        setPalette(screen, memory, 0x17D0, false);
        return true;
    }
    if (matches(memory, 0x7D0, {0xF3, 0x01, 0x11, 0xBC})) {  // a Plus's, 16K
        screen.plus = true;
        screen.columns = 80;
        screen.lines = 200;
        setPalette(screen, memory, 0x17D0, true);
        return true;
    }
    if (matches(memory, 0x611, {0x21, 0x47, 0x08, 0xCD})) {  // a CPC's, overscan
        screen.plus = false;
        screen.columns = 96;
        screen.lines = 272;
        setPalette(screen, memory, 0x600, false);
        return true;
    }
    if (matches(memory, 0x621, {0xF3, 0x01, 0x11, 0xBC})) {  // a Plus's, overscan
        screen.plus = true;
        screen.columns = 96;
        screen.lines = 272;
        setPalette(screen, memory, 0x600, true);
        return true;
    }
    return false;
}

// A screen packed by OCP Art Studio: blocks that start with "MJH".
void unpackOcp(std::vector<uint8_t>& memory)
{
    const std::vector<uint8_t> in = memory;
    size_t posIn = 0, posOut = 0;
    const auto marker = [&in](size_t at) { return at + 2 < in.size() && in[at] == 'M' && in[at + 1] == 'J' && in[at + 2] == 'H'; };
    while (posOut < 0x4000 && marker(posIn)) {
        posIn += 3;
        int length = in[posIn] | (in[posIn + 1] << 8);
        posIn += 2;
        int count = 0;
        while (count < length && posIn < in.size() && !marker(posIn)) {
            uint8_t a = in[posIn++];
            if (a == 1) {
                int c = in[posIn++];
                a = in[posIn++];
                if (c == 0)
                    c = 0x100;
                for (int i = 0; i < c && count < length && posOut < memory.size(); ++i, ++count)
                    memory[posOut++] = a;
            } else if (posOut < memory.size()) {
                memory[posOut++] = a;
                ++count;
            }
        }
    }
}

// A screen packed with a header of four letters that says what it is: PKSL
// (16K), PKS3, PKSP (16K, Plus), PKVL (overscan), PKVP (overscan, Plus).
void unpackPk(Screen& screen, std::vector<uint8_t>& memory)
{
    screen.columns = 80;
    screen.lines = 200;
    screen.plus = memory[3] == 'P' || memory[2] == 'O';
    const bool overscan = memory[2] == 'V' || memory[3] == 'V';
    const bool standard = memory[2] == 'S' && memory[3] == 'L';
    const std::vector<uint8_t> palette(memory.begin() + 4, memory.begin() + 4 + 34);
    const std::vector<uint8_t> unpacked = unpackStandard(memory, standard ? 21 : 4);
    if (standard) {
        // Column after column.
        size_t i = 0;
        for (int x = 0; x < 80; ++x)
            for (int y = 0; y < 200; ++y, ++i)
                memory[static_cast<size_t>(x + screen.address(y << 1))] = i < unpacked.size() ? unpacked[i] : 0;
    } else {
        std::copy_n(unpacked.begin(), std::min(unpacked.size(), memory.size()), memory.begin());
    }
    if (overscan) {
        screen.columns = 96;
        screen.lines = 272;
        setPalette(screen, memory, 0x600, screen.plus);
    } else if (standard) {
        setPalette(screen, palette, 0, screen.plus);
    } else {
        setPalette(screen, memory, 0x17D0, screen.plus);
    }
}

// The screen's memory as a picture, of the PC's and of the CPC's.
Bitmap draw(const std::vector<uint8_t>& memory, int columns, int lines, const Screen& screen, Picture* picture, bool sprite,
            int realWidth)
{
    const int width = realWidth != 0 ? realWidth : columns << 3;
    Bitmap shown(width, lines << 1);
    const auto byteAt = [&memory](int at) -> int { return at >= 0 && static_cast<size_t>(at) < memory.size() ? memory[static_cast<size_t>(at)] : 0; };
    for (int y = 0; y < lines << 1; y += 2) {
        const int mode = screen.lineMode(y);
        const int address = sprite ? columns * (y >> 1) : screen.address(y);
        const int pixels = 2 << mode;        // in a byte
        const int pixelWidth = 8 / pixels;   // in pixels of mode 2
        for (int x = 0; x < columns; ++x) {
            const int byte = byteAt(address + x);
            for (int p = 0; p < pixels; ++p) {
                int pen;
                switch (mode) {
                case 0:
                    pen = p == 0 ? (byte >> 7) + ((byte & 0x20) >> 3) + ((byte & 0x08) >> 2) + ((byte & 0x02) << 2)
                                 : ((byte & 0x40) >> 6) + ((byte & 0x10) >> 2) + ((byte & 0x04) >> 1) + ((byte & 0x01) << 3);
                    break;
                case 1:
                    pen = ((byte >> (7 - p)) & 1) + (((byte >> (3 - p)) & 1) << 1);
                    break;
                default:
                    pen = (byte >> (7 - p)) & 1;
                    break;
                }
                const int px = (x << 3) + p * pixelWidth;
                if (px < width)
                    shown.setRunDouble(px, y, std::min(pixelWidth, width - px), screen.rgb(screen.palette[static_cast<size_t>(pen)]));
                if (picture)
                    picture->setPixel(px, y, pen, pixelWidth);
            }
        }
    }
    if (picture)
        picture->computed = true;
    return shown;
}

}  // namespace

LoadedScreen loadScreen(const std::vector<uint8_t>& data, Screen& screen, Picture& picture)
{
    LoadedScreen loaded;
    if (data.empty())
        return loaded;
    const int length = static_cast<int>(data.size());
    std::vector<uint8_t> memory(0x10000, 0);
    std::copy_n(data.begin(), std::min(data.size(), memory.size()), memory.begin());
    if (memory[0] == 'M' && memory[1] == 'J' && memory[2] == 'H') {
        unpackOcp(memory);
    } else if (memory[0] == 'P' && memory[1] == 'K' && (memory[2] == 'O' || memory[2] == 'V' || memory[2] == 'S')) {
        unpackPk(screen, memory);
    } else if (!readOwnScreen(screen, memory)) {
        if (length == 16384 || length == 16336) {
            screen.columns = 80;
            screen.lines = 200;
        } else if (length < 31000) {
            // A file this short of a screen is taken for a packed one.
            const std::vector<uint8_t> unpacked = unpackStandard(memory, 0);
            if (unpacked.empty())
                return loaded;
            std::fill(memory.begin(), memory.end(), 0);
            std::copy_n(unpacked.begin(), std::min(unpacked.size(), memory.size()), memory.begin());
            if (!readOwnScreen(screen, memory)) {
                screen.plus = false;
                screen.columns = 96;
                screen.lines = 272;
                setPalette(screen, memory, 0x600, false);
            }
        } else if (length > 0x4000) {
            screen.columns = 96;
            screen.lines = 272;
        }
    }
    picture.clear();
    loaded.shown = draw(memory, screen.columns, screen.lines, screen, &picture, false, 0);
    loaded.ok = true;
    return loaded;
}

LoadedScreen loadImpdrawScreen(const std::vector<uint8_t>& file, Screen& screen, Picture& picture)
{
    LoadedScreen loaded;
    if (file.size() < 0x7E20)
        return loaded;
    screen.mode = std::clamp(file[0x94] - 0x0E, 0, 2);
    screen.columns = 96;
    screen.lines = 272;
    screen.plus = file[0xBC] != 0;
    if (screen.plus) {
        for (size_t i = 0; i < 16; ++i)
            screen.palette[i] = ((file[0x0711 + (i << 1)] << 4) & 0xF0) + (file[0x0711 + (i << 1)] >> 4) + (file[0x0712 + (i << 1)] << 8);
    } else {
        for (size_t i = 0; i < 16; ++i) {
            const char* found = std::strchr(kCpcVga, static_cast<char>(file[0x7E10 + i]));
            screen.palette[i] = found && *found ? static_cast<int>(found - kCpcVga) : 0;
        }
    }
    std::vector<uint8_t> memory(0x10000, 0);
    std::copy_n(file.begin() + 0x110, std::min(file.size() - 0x110, memory.size()), memory.begin());
    picture.clear();
    loaded.shown = draw(memory, screen.columns, screen.lines, screen, &picture, false, 0);
    loaded.ok = true;
    return loaded;
}

Bitmap drawSprite(const std::vector<uint8_t>& bytes, int columns, int lines, const Screen& screen, Picture* picture, int realWidth)
{
    return draw(bytes, columns, lines, screen, picture, true, realWidth);
}

// ---- Palettes ----

std::vector<uint8_t> paletteFile(const std::string& fileName, const Screen& screen, const Param& param)
{
    std::vector<uint8_t> pal(239, 0);
    pal[0] = static_cast<uint8_t>(param.modeVirtuel);
    size_t at = 3;
    if (param.cpcPlus) {
        // Each colour is the three letters of its red, blue and green,
        // four times over.
        for (size_t i = 0; i < 16; ++i)
            for (int j = 0; j < 4; ++j) {
                const int colour = screen.palette[i];
                pal[at++] = static_cast<uint8_t>(kCpcVga[26 - ((colour >> 4) & 0x0F)]);
                pal[at++] = static_cast<uint8_t>(kCpcVga[26 - (colour & 0x0F)]);
                pal[at++] = static_cast<uint8_t>(kCpcVga[26 - ((colour >> 8) & 0x0F)]);
            }
        pal[195] = pal[3];
        pal[196] = pal[4];
        pal[197] = pal[5];
    } else {
        for (size_t i = 0; i < 16; ++i)
            for (int j = 0; j < 12; ++j)
                pal[at++] = static_cast<uint8_t>(kCpcVga[screen.palette[i] >= 0 && screen.palette[i] < 27 ? screen.palette[i] : 0]);
        for (size_t i = 0; i < 12; ++i)
            pal[at++] = pal[i + 3];
    }
    return withAmsdosHeader(fileName, 0x8809, 0x8809, pal);
}

bool readPaletteFile(const std::vector<uint8_t>& file, Screen& screen)
{
    if (!hasAmsdosHeader(file) || file.size() < kAmsdosHeaderSize + 3 + 16 * 12)
        return false;
    const uint8_t* pal = file.data() + kAmsdosHeaderSize;
    if (pal[0] >= 11)
        return false;
    for (size_t i = 0; i < 16; ++i)
        for (int j = 0; j < 27; ++j)
            if (pal[3 + i * 12] == static_cast<uint8_t>(kCpcVga[j]))
                screen.palette[i] = screen.plus ? plusFromCpc(j) : j;
    return true;
}

std::vector<uint8_t> kitFile(const std::string& fileName, const std::array<int, 16>& colours, bool picture)
{
    std::vector<uint8_t> kit;
    for (size_t i = picture ? 0 : 1; i < 16; ++i) {
        const int colour = colours[i];
        kit.push_back(static_cast<uint8_t>(((colour & 0x0F) << 4) + ((colour & 0xF0) >> 4)));
        kit.push_back(static_cast<uint8_t>(colour >> 8));
    }
    // ConvImgCpc's header says thirty bytes whichever it is.
    std::vector<uint8_t> file = amsdosHeader(baseName(fileName), 0x8000, 30, 0);
    file.insert(file.end(), kit.begin(), kit.end());
    return file;
}

bool readKitFile(const std::vector<uint8_t>& file, std::array<int, 16>& colours)
{
    if (!hasAmsdosHeader(file) || (file.size() != 158 && file.size() != 160))
        return false;
    size_t at = kAmsdosHeaderSize;
    for (size_t i = file.size() == 160 ? 0 : 1; i < 16; ++i) {
        const int kit = file[at] + (file[at + 1] << 8);
        colours[i] = (kit & 0xF00) + ((kit & 0x0F) << 4) + ((kit & 0xF0) >> 4);
        at += 2;
    }
    return true;
}

}  // namespace tuxscr
