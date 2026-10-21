#include "formats/animfile.h"

#include <algorithm>
#include <cstring>
#include <map>

#include "formats/asmgen.h"

namespace tuxscr {

namespace {

// ConvImgCpc's SaveAnim: what it keeps from one frame to the next, and its
// ways of packing a frame.
class Animation {
public:
    Animation(const Screen& screen, const Param& param, PackMethod method, AnimOptions options, std::vector<AnimNote>& notes)
        : screen_(screen)
        , param_(param)
        , method_(method)
        , options_(options)
        , notes_(notes)
        , previous_(0x10000, 0)
        , oldAscii_(0x1000, 0)
    {
    }

    const AnimOptions& options() const { return options_; }
    char lastAscii() const { return lastAscii_; }

    // A frame packed; empty if nothing of it changed. `sizeUnpacked` is
    // told the room it takes once unpacked.
    std::vector<uint8_t> packFrame(const Picture& picture, int& sizeUnpacked, bool clearPrevious, bool firstFrame, int zone)
    {
        if (options_.rawData)
            return spriteBytes(screen_, picture);
        if (screen_.mode >= kModeAscUt)
            return packAscii(asciiBytes(screen_, picture), sizeUnpacked, clearPrevious, firstFrame);
        memory_ = screenMemory(screen_, picture);
        if (clearPrevious)
            std::fill(previous_.begin(), previous_.end(), 0);
        switch (options_.method) {
        case 1:
            return packDirectMemory(sizeUnpacked);
        case 2:
            return packBlocks(8, sizeUnpacked);
        default:
            return packWindow(sizeUnpacked, zone);
        }
    }

private:
    const Screen& screen_;
    const Param& param_;
    PackMethod method_;
    AnimOptions options_;
    std::vector<AnimNote>& notes_;
    std::vector<uint8_t> memory_;
    std::vector<uint8_t> previous_;
    std::vector<uint8_t> oldAscii_;
    char lastAscii_ = '\0';

    std::vector<uint8_t> packed(const std::vector<uint8_t>& data) const
    {
        return method_ == PackMethod::None ? data : pack(data, method_ == PackMethod::ZX0Ovs ? PackMethod::ZX0 : method_);
    }

    // The window of what changed: its size, where it goes, its bytes.
    std::vector<uint8_t> packWindow(int& sizeUnpacked, int zone)
    {
        const int columns = screen_.columns, lines = screen_.lines;
        int xFin = 0, xDeb = columns, yDeb = lines, yFin = 0;
        int lStart = 0, lEnd = lines, xStart = 0, xEnd = columns;
        if (options_.verticalZones) {
            xStart = zone < 1 ? 0 : columns >> 1;
            xEnd = zone == 0 ? columns >> 1 : columns;
        } else {
            lStart = zone < 1 ? 0 : lines >> 1;
            lEnd = zone == 0 ? lines >> 1 : lines;
        }
        const int step = options_.lineStep;
        for (int l = lStart; l < lEnd; l += step) {
            const size_t adr = static_cast<size_t>(screen_.address(l << 1));
            for (int oct = xStart; oct < xEnd; ++oct)
                if (memory_[adr + static_cast<size_t>(oct)] != previous_[adr + static_cast<size_t>(oct)]) {
                    xDeb = std::min(xDeb, oct);
                    xFin = std::max(xFin, oct);
                    yDeb = std::min(yDeb, l);
                    yFin = std::max(yFin, l);
                    previous_[adr + static_cast<size_t>(oct)] = memory_[adr + static_cast<size_t>(oct)];
                }
        }
        const int tailleX = xFin > xDeb ? xFin - xDeb + 1 : 0;
        const int tailleY = (yFin + 1 - yDeb) / step;
        const int length = tailleX * tailleY;
        if (length <= 0)
            return {};
        std::vector<uint8_t> line;
        line.push_back(static_cast<uint8_t>(tailleX));
        line.push_back(static_cast<uint8_t>(tailleY));
        const int screenAddress = 0xC000 + xDeb + (yDeb >> 3) * columns + (yDeb & 7) * 0x800;
        line.push_back(static_cast<uint8_t>(screenAddress & 0xFF));
        line.push_back(static_cast<uint8_t>(screenAddress >> 8));
        const auto offsetOf = [&](int l, int x) { return static_cast<size_t>((l >> 3) * columns + (l & 7) * 0x800 + x); };
        if (options_.byColumns) {
            for (int x = xDeb; x <= xFin; ++x)
                for (int l = 0; l < tailleY * step; l += step)
                    line.push_back(previous_[offsetOf(l + yDeb, x)]);
        } else {
            for (int l = yDeb; l <= yFin; l += step)
                for (int x = 0; x < tailleX; ++x)
                    line.push_back(previous_[offsetOf(l, xDeb + x)]);
        }
        sizeUnpacked = length + 4;
        return packed(line);
    }

    // The bytes that changed, each with how far it is from the one before;
    // a run of the same byte is told once.
    std::vector<uint8_t> packDirectMemory(int& sizeUnpacked)
    {
        int maxSize = screen_.columns + ((screen_.lines - 1) >> 3) * screen_.columns + ((screen_.lines - 1) & 7) * 0x800;
        if (maxSize >= 0x4000)
            maxSize += 0x3800;
        maxSize = std::min(maxSize, 0xFE00);
        std::vector<uint8_t> diff;
        int count = 0;
        int delta = 0;
        for (int adr = 0; adr < maxSize; ++adr) {
            const size_t a = static_cast<size_t>(adr);
            const uint8_t o = previous_[a];
            const uint8_t n = memory_[a];
            if (delta == 127 || o != n) {
                if (adr < maxSize - 256 && previous_[a + 1] != memory_[a + 1] && previous_[a + 2] != memory_[a + 2] && memory_[a + 1] == n
                    && memory_[a + 2] == n) {
                    diff.push_back(static_cast<uint8_t>(delta | 0x80));
                    ++count;
                    int d = 0;
                    while (d < 255 && memory_[a + static_cast<size_t>(d)] == n)
                        ++d;
                    diff.push_back(static_cast<uint8_t>(d));
                    diff.push_back(n);
                    delta = 0;
                    adr += d - 1;
                } else {
                    diff.push_back(static_cast<uint8_t>(delta));
                    diff.push_back(n);
                    ++count;
                    delta = 0;
                    if (diff.size() >= 0x8000)
                        break;
                }
            } else {
                ++delta;
            }
        }
        std::vector<uint8_t> data{static_cast<uint8_t>(count), static_cast<uint8_t>(count >> 8)};
        data.insert(data.end(), diff.begin(), diff.end());
        previous_ = memory_;
        sizeUnpacked = static_cast<int>(diff.size()) + 2;
        return packed(data);
    }

    // The blocks that changed, `width` bytes by `height` lines each, with
    // where each goes.
    std::vector<uint8_t> tryBlocks(int width, int height, int& sizeUnpacked)
    {
        std::vector<uint8_t> line;
        std::vector<uint8_t> block(static_cast<size_t>(width * height));
        line.push_back(static_cast<uint8_t>(width));
        for (int row = 0; row < screen_.lines / height; ++row)
            for (int col = 0; col < screen_.columns / width; ++col) {
                bool changed = false;
                for (int y = 0; y < height; ++y)
                    for (int x = 0; x < width; ++x) {
                        const size_t adr = static_cast<size_t>(screen_.address((row * height + y) << 1) + x + col * width);
                        block[static_cast<size_t>(x + y * width)] = memory_[adr];
                        changed = changed || memory_[adr] != previous_[adr];
                    }
                if (changed) {
                    const int adr = screen_.address((row * height) << 1) + col * width;
                    // The address with its top bits set: a zero ends the list.
                    line.push_back(static_cast<uint8_t>((adr >> 8) | 0xC0));
                    line.push_back(static_cast<uint8_t>(adr & 0xFF));
                    line.insert(line.end(), block.begin(), block.end());
                }
            }
        line.push_back(0);
        const std::vector<uint8_t> out = packed(line);
        sizeUnpacked = static_cast<int>(out.size()) + 2;
        return out;
    }

    std::vector<uint8_t> packBlocks(int height, int& sizeUnpacked)
    {
        // Blocks of one, two or four bytes across: whichever packs best.
        size_t best = 0xFFFF;
        int bestPass = 0;
        for (int pass = 0; pass < 3; ++pass) {
            const size_t length = tryBlocks(1 << pass, height, sizeUnpacked).size();
            if (length < best) {
                best = length;
                bestPass = pass;
            }
        }
        std::vector<uint8_t> out = tryBlocks(1 << bestPass, height, sizeUnpacked);
        previous_ = memory_;
        return out;
    }

    // An ASCII mode: the screen's characters, whole ('O') or as those that
    // changed ('D'); a frame like the one before is 'I'.
    std::vector<uint8_t> packAscii(std::vector<uint8_t> ascii, int& sizeUnpacked, bool clearPrevious, bool firstFrame)
    {
        using Ascii = AnimOptions::Ascii;
        const bool single = options_.singlePicture;
        const size_t tailleMax = std::min<size_t>(static_cast<size_t>((screen_.lines * screen_.columns) >> 3), ascii.size());
        if (clearPrevious)
            std::fill(oldAscii_.begin(), oldAscii_.end(), 0);
        std::vector<uint8_t> diff;
        size_t lastDiff = 0;
        int different = 0;
        int unchanged = 0;
        for (size_t i = 0; i < tailleMax; ++i) {
            const uint8_t oldAsc = oldAscii_[i];
            const uint8_t newAsc = ascii[i];
            if (unchanged == 255 || oldAsc != newAsc || firstFrame) {
                if (oldAsc != newAsc) {
                    ++different;
                    lastDiff = diff.size() + 2;
                }
                diff.push_back(static_cast<uint8_t>(unchanged));
                diff.push_back(newAsc);
                unchanged = 0;
                if (diff.size() >= 0x8000)
                    break;
            } else {
                ++unchanged;
            }
        }
        oldAscii_ = ascii;
        sizeUnpacked = static_cast<int>(ascii.size()) + 4;
        if (different == 0 && options_.ascii == Ascii::Shortest) {
            lastAscii_ = 'I';
            return packed({static_cast<uint8_t>('I')});
        }
        const std::vector<uint8_t> characters(ascii.begin(), ascii.begin() + static_cast<long>(tailleMax));
        std::vector<uint8_t> whole;
        if (!(options_.ascii == Ascii::Whole || single))
            whole.push_back('O');
        whole.insert(whole.end(), characters.begin(), characters.end());
        const std::vector<uint8_t> packedWhole = packed(whole);

        const size_t posDiff = std::min(lastDiff, diff.size());
        std::vector<uint8_t> changes;
        if (!(options_.ascii == Ascii::Changes || single))
            changes.push_back('D');
        changes.push_back(static_cast<uint8_t>(posDiff >> 1));
        changes.push_back(static_cast<uint8_t>(posDiff >> 9));
        changes.insert(changes.end(), diff.begin(), diff.begin() + static_cast<long>(posDiff));
        const std::vector<uint8_t> packedChanges = packed(changes);
        if (packedChanges.size() > changes.size())
            notes_.push_back({AnimNote::BytesLost, static_cast<int>(packedChanges.size() - changes.size())});
        if ((packedWhole.size() > packedChanges.size() && options_.ascii != Ascii::Whole) || options_.ascii == Ascii::Changes) {
            if (single)
                options_.ascii = Ascii::Changes;
            lastAscii_ = 'D';
            return packedChanges;
        }
        if (single)
            options_.ascii = Ascii::Whole;
        lastAscii_ = 'O';
        return packedWhole;
    }
};

}  // namespace

AnimSource animationAsm(const Screen& screen, const std::vector<const Picture*>& frames, const std::vector<int>& times,
                        const Param& param, PackMethod method, const AnimOptions& asked, const AsmStamp& stamp,
                        const std::string& labelPalette, const PackProgress& progress)
{
    using namespace asmgen;
    AnimSource out;
    if (frames.empty())
        return out;
    Animation animation(screen, param, method, asked, out.notes);
    const AnimOptions& options = asked;
    const int count = static_cast<int>(frames.size());
    const auto timeOf = [&times](int frame) { return frame < static_cast<int>(times.size()) ? times[static_cast<size_t>(frame)] : 0; };
    const int adrDeb = options.startAddress;
    const int adrMax = options.maxAddress == 0 ? 0xBE00 : options.maxAddress;

    std::vector<std::vector<uint8_t>> packedFrames;
    std::vector<int> speed;
    int sizeUnpacked = 0, maxUnpacked = 0, total = 0;
    const auto keep = [&](std::vector<uint8_t> frame, int time) {
        if (frame.empty())
            return;
        total += static_cast<int>(frame.size());
        packedFrames.push_back(std::move(frame));
        speed.push_back(time);
    };
    if (options.loop)
        // The last picture first: the first is then what changes from it.
        keep(animation.packFrame(*frames[static_cast<size_t>(count - 1)], sizeUnpacked, true, false, -1), timeOf(count - 1));
    out.notes.push_back({AnimNote::Started, 0});
    for (int i = 0; i < (options.singlePicture ? 1 : count); ++i) {
        const bool first = i == 0 && !options.loop;
        if (options.twoZones) {
            // What is on the screen is forgotten once for the frame, not
            // again between its two halves.
            keep(animation.packFrame(*frames[static_cast<size_t>(i)], sizeUnpacked, first, first, 0), timeOf(i));
            keep(animation.packFrame(*frames[static_cast<size_t>(i)], sizeUnpacked, false, first, 1), 0);
        } else {
            keep(animation.packFrame(*frames[static_cast<size_t>(i)], sizeUnpacked, first, first, -1), timeOf(i));
        }
        maxUnpacked = std::max(maxUnpacked, sizeUnpacked);
        if (progress)
            progress(100 * (i + 1) / count);
    }

    // What packing chose for a single picture of an ASCII mode decides the
    // routine that shows it.
    const AnimOptions& chosen = animation.options();
    const bool withProgram = param.withCode && !options.rawData;
    AsmText sw(screen);
    WriteHeader(sw, stamp.version.empty() ? Text() : Text(stamp.version), true, stamp.time);
    if (withProgram) {
        GenereEntete(sw, adrDeb);
        if (screen.plus)
            GenereInitPlus(sw, Text(labelPalette));
        else
            GenereInitOld(sw, Text(labelPalette));
    }
    bool gest128K = options.memory128K;
    if (total + adrDeb < adrMax && total + adrDeb < 0xBE00 - maxUnpacked)
        gest128K = false;
    if (withProgram) {
        GenereAffichage(sw, options.withDelay, options.loop, gest128K, options.singlePicture, method);
        if (screen.mode >= kModeAscUt) {
            GenereDrawAscii(sw, chosen.ascii == AnimOptions::Ascii::Shortest, chosen.ascii == AnimOptions::Ascii::Whole,
                            chosen.ascii == AnimOptions::Ascii::Changes, gest128K, options.singlePicture, options.withDelay);
        } else {
            switch (options.method) {
            case 1: {
                // ConvImgCpc's routine goes three bytes on in the table of
                // the frames whether each has its delay there or not: one
                // step too many is taken out when there is none.
                AsmText draw(screen);
                GenereDrawDirect(draw, gest128K);
                std::string text = draw.text();
                const std::string step = "\tINC\tIX" + NL.str();
                if (!options.withDelay)
                    if (const size_t at = text.find(step); at != std::string::npos)
                        text.erase(at, step.size());
                sw.Write(Text(text));
                break;
            }
            case 2:
                GenereDrawBlock(sw, 8, options.withDelay, gest128K);
                break;
            default:
                GenereDrawDC(sw, options.withDelay, options.byColumns, gest128K,
                             options.lineStep == 8 ? 0x3F : options.lineStep == 4 ? 0x1F : options.lineStep == 2 ? 0xF : 0x7, false);
                break;
            }
        }
    }
    if ((param.withPalette || param.withCode) && !options.rawData)
        GenerePalette(sw, param, true, param.withCode, S("Palette"));

    const int frameCount = static_cast<int>(packedFrames.size());
    std::vector<int> bank(static_cast<size_t>(frameCount) + 1, 0xC0);
    int endBank0 = 0, inBank = 0, numBank = 0xC0;
    for (int i = 0; i < frameCount; ++i) {
        const int length = static_cast<int>(packedFrames[static_cast<size_t>(i)].size());
        inBank += length;
        if (!options.rawData && gest128K && inBank > (numBank == 0xC0 ? std::min(0xBE00 - maxUnpacked - adrDeb, adrMax - adrDeb) : 0x4000)
            && (numBank > 0xC0 || inBank + adrDeb - length > 0x4000)) {
            if (numBank == 0xC0) {
                endBank0 = inBank + adrDeb - length;
                sw.WriteLine(S("EndBank0:"));
                numBank = 0xC4;
            } else {
                ++numBank;
                if ((numBank & 15) == 8)
                    numBank += 4;
                if ((numBank & 15) == 15)
                    numBank += 5;
            }
            inBank = length;
            sw.WriteLine(S("	ORG	#4000"));
            sw.WriteLine(S("	Write Direct -1,-1,#") + hex(numBank, 2));
        }
        bank[static_cast<size_t>(i)] = numBank;
        if (options.singlePicture && animation.lastAscii() != '\0')
            sw.WriteLine(S("; Type Frame ='") + chr(animation.lastAscii()) + S("'"));
        sw.WriteLine(S("Delta") + dec(i) + S(":\t\t; Taille #") + hex(length, 4));
        GenereDatas(sw, packedFrames[static_cast<size_t>(i)].data(), length, 16);
    }
    speed.push_back(0);
    GenerePointeurs(sw, frameCount, bank.data(), options.withDelay ? speed.data() : nullptr, gest128K && numBank > 0xC0);
    GenereFin(sw, total, gest128K && endBank0 < 0x8000, withProgram);
    out.text = sw.text();
    out.length = total;
    out.notes.push_back({AnimNote::TotalLength, total});
    out.overflow = !options.rawData && (numBank > 0xC7 || (!options.memory128K && total + adrDeb >= 0xBE00 - maxUnpacked));
    out.notes.push_back({out.overflow ? AnimNote::Overflow : AnimNote::Saved, 0});
    return out;
}

// ---- Animated GIF ----

namespace {

// GIF's LZW: codes of a growing width, least significant bit first, in
// blocks of at most 255 bytes.
class GifLzw {
public:
    explicit GifLzw(std::vector<uint8_t>& out, int codeSize)
        : out_(out)
        , minCode_(codeSize)
    {
    }

    void encode(const std::vector<uint8_t>& pixels)
    {
        const int clear = 1 << minCode_;
        const int end = clear + 1;
        int codeBits = minCode_ + 1;
        int next = end + 1;
        std::map<std::pair<int, uint8_t>, int> table;
        out_.push_back(static_cast<uint8_t>(minCode_));
        write(clear, codeBits);
        int current = -1;
        for (const uint8_t pixel : pixels) {
            if (current < 0) {
                current = pixel;
                continue;
            }
            const auto found = table.find({current, pixel});
            if (found != table.end()) {
                current = found->second;
                continue;
            }
            write(current, codeBits);
            table[{current, pixel}] = next++;
            if (next > (1 << codeBits) && codeBits < 12)
                ++codeBits;
            if (next >= 4095) {
                write(clear, codeBits);
                table.clear();
                codeBits = minCode_ + 1;
                next = end + 1;
            }
            current = pixel;
        }
        if (current >= 0)
            write(current, codeBits);
        write(end, codeBits);
        if (bitCount_ > 0)
            block_.push_back(static_cast<uint8_t>(bits_));
        flush();
        out_.push_back(0);
    }

private:
    std::vector<uint8_t>& out_;
    int minCode_;
    uint32_t bits_ = 0;
    int bitCount_ = 0;
    std::vector<uint8_t> block_;

    void write(int code, int width)
    {
        bits_ |= static_cast<uint32_t>(code) << bitCount_;
        bitCount_ += width;
        while (bitCount_ >= 8) {
            block_.push_back(static_cast<uint8_t>(bits_));
            bits_ >>= 8;
            bitCount_ -= 8;
            if (block_.size() == 255)
                flush();
        }
    }

    void flush()
    {
        if (block_.empty())
            return;
        out_.push_back(static_cast<uint8_t>(block_.size()));
        out_.insert(out_.end(), block_.begin(), block_.end());
        block_.clear();
    }
};

}  // namespace

std::vector<uint8_t> animatedGif(const std::vector<Bitmap>& frames, const std::vector<int>& times)
{
    std::vector<uint8_t> out;
    if (frames.empty() || frames[0].empty())
        return out;
    const int width = frames[0].width(), height = frames[0].height();
    // One table of colours for all the frames: a CPC's pictures have few.
    std::map<uint32_t, int> index;
    std::vector<uint32_t> colours;
    for (const Bitmap& frame : frames)
        for (const uint32_t pixel : frame.pixels())
            if (colours.size() < 256 && index.emplace(pixel, static_cast<int>(colours.size())).second)
                colours.push_back(pixel);
    int tableBits = 1;
    while ((1 << tableBits) < static_cast<int>(colours.size()))
        ++tableBits;
    const auto word = [&out](int value) {
        out.push_back(static_cast<uint8_t>(value & 0xFF));
        out.push_back(static_cast<uint8_t>(value >> 8));
    };
    out.insert(out.end(), {'G', 'I', 'F', '8', '9', 'a'});
    word(width);
    word(height);
    out.push_back(static_cast<uint8_t>(0xF0 | (tableBits - 1)));
    out.push_back(0);
    out.push_back(0);
    for (int i = 0; i < (1 << tableBits); ++i) {
        const uint32_t colour = i < static_cast<int>(colours.size()) ? colours[static_cast<size_t>(i)] : 0;
        out.push_back(static_cast<uint8_t>(colour >> 16));
        out.push_back(static_cast<uint8_t>(colour >> 8));
        out.push_back(static_cast<uint8_t>(colour));
    }
    // Played for ever.
    const uint8_t loop[] = {0x21, 0xFF, 0x0B, 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0', 0x03, 0x01, 0x00, 0x00, 0x00};
    out.insert(out.end(), loop, loop + sizeof loop);
    for (size_t n = 0; n < frames.size(); ++n) {
        const Bitmap& frame = frames[n];
        if (frame.width() != width || frame.height() != height)
            continue;
        const int time = n < times.size() ? times[n] : 100;
        out.insert(out.end(), {0x21, 0xF9, 0x04, 0x00});
        word(std::max(1, time / 10));
        out.push_back(0);
        out.push_back(0);
        out.push_back(0x2C);
        word(0);
        word(0);
        word(width);
        word(height);
        out.push_back(0);
        std::vector<uint8_t> pixels;
        pixels.reserve(frame.pixels().size());
        for (const uint32_t pixel : frame.pixels()) {
            const auto found = index.find(pixel);
            pixels.push_back(found == index.end() ? 0 : static_cast<uint8_t>(found->second));
        }
        GifLzw(out, std::max(2, tableBits)).encode(pixels);
    }
    out.push_back(0x3B);
    return out;
}

}  // namespace tuxscr
