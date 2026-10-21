#include "formats/pack.h"

#include <cstring>
#include <deque>

namespace tuxscr {

namespace {

// ---- ConvImgCpc's own method ----

constexpr int kSeekBack = 0x1000;
constexpr int kMaxString = 256;

std::vector<uint8_t> packStandard(const std::vector<uint8_t>& in)
{
    const int lengthIn = static_cast<int>(in.size());
    std::vector<uint8_t> out;
    if (lengthIn == 0)
        return out;
    out.reserve(in.size() + in.size() / 8 + 32);
    uint8_t codebuffer[24];
    uint8_t bits = 0;
    int count = 0, bitcount = 0, codecount = 0;
    int matchtablestart = 0, matchtableend = 0;
    std::vector<int> matches(kMaxString, 0);
    // The places each byte was met at, among the last 4096.
    std::vector<int> matchtable(static_cast<size_t>(kMaxString) * kSeekBack, 0);
    const auto table = [&matchtable](int b, int n) -> int& {
        return matchtable[static_cast<size_t>(b) * kSeekBack + static_cast<size_t>(n)];
    };
    const auto byteAt = [&in](int at) { return in[static_cast<size_t>(at)]; };

    while (true) {
        int c, d, start, end, max;
        for (c = matchtableend; c < count; ++c) {
            const int b = byteAt(c);
            table(b, matches[static_cast<size_t>(b)]) = c;
            matches[static_cast<size_t>(b)]++;
        }
        matchtableend = count;

        if (count >= 2) {
            int stlen = 0, stpos = 0, stlen2 = 0;
            int bb = byteAt(count);
            for (c = matches[static_cast<size_t>(bb)] - 1; c >= 0; --c) {
                start = table(bb, c);
                end = start + kMaxString;
                if (end > count)
                    end = count;
                max = end - start;
                if (max >= stlen) {
                    for (d = 1; d < max; ++d)
                        if (start + d >= lengthIn || count + d >= lengthIn || byteAt(start + d) != byteAt(count + d))
                            break;
                    if (d >= 2 && d > stlen) {
                        stlen = d;
                        stpos = count - start;
                    }
                    if (d == stlen && count - start < stpos)
                        stpos = count - start;
                }
                if (stlen == kMaxString && stpos == stlen)
                    break;
            }
            if (count + 1 < lengthIn) {
                bb = byteAt(count + 1);
                for (c = matches[static_cast<size_t>(bb)] - 1; c >= 0; --c) {
                    start = table(bb, c);
                    end = start + kMaxString;
                    if (end > count + 1)
                        end = count + 1;
                    max = end - start;
                    if (max >= stlen2) {
                        for (d = 1; d < max; ++d)
                            if (start + d >= lengthIn || count + d + 1 >= lengthIn || byteAt(start + d) != byteAt(count + d + 1))
                                break;
                        if (d >= 2 && d >= stlen2)
                            stlen2 = d;
                    }
                    if (stlen2 == kMaxString)
                        break;
                }
                if (stlen2 - 1 > stlen)
                    stlen = 0;
            }

            if (stlen > 1) {
                if (stlen == 2 && stpos >= kMaxString) {
                    codebuffer[codecount++] = byteAt(count++);
                    bitcount++;
                } else {
                    if (stpos == stlen) {
                        if (stlen == kMaxString)
                            codebuffer[codecount++] = 0x1;
                        else if (stlen <= 14)
                            codebuffer[codecount++] = static_cast<uint8_t>(stlen);
                        else {
                            codebuffer[codecount++] = 0x0F;
                            codebuffer[codecount++] = static_cast<uint8_t>(stlen - 1);
                        }
                    } else if (stlen == 2 && stpos < 65) {
                        codebuffer[codecount++] = static_cast<uint8_t>(0x40 + stpos - 1);
                    } else if (stlen <= 33 && stpos < 257) {
                        codebuffer[codecount++] = static_cast<uint8_t>(0x20 + stlen - 2);
                        codebuffer[codecount++] = static_cast<uint8_t>(stpos - 1);
                    } else if (stlen >= 3 && stlen <= 10) {
                        codebuffer[codecount++] = static_cast<uint8_t>(0x80 + ((stlen - 3) << 4) + ((stpos - 1) >> 8));
                        codebuffer[codecount++] = static_cast<uint8_t>(stpos - 1);
                    } else {
                        codebuffer[codecount++] = static_cast<uint8_t>(0x10 + ((stpos - 1) >> 8));
                        codebuffer[codecount++] = static_cast<uint8_t>(stpos - 1);
                        codebuffer[codecount++] = static_cast<uint8_t>(stlen - 1);
                    }
                    bits = static_cast<uint8_t>(bits | (1 << bitcount));
                    bitcount++;
                    count += stlen;
                }
            } else {
                codebuffer[codecount++] = byteAt(count++);
                bitcount++;
            }
        } else {
            codebuffer[codecount++] = byteAt(count++);
            bitcount++;
        }
        if (bitcount == 8) {
            out.push_back(bits);
            out.insert(out.end(), codebuffer, codebuffer + codecount);
            bitcount = codecount = 0;
            bits = 0;
        }
        if (count >= lengthIn)
            break;

        const int oldmatchtablestart = matchtablestart;
        matchtablestart = count - kSeekBack;
        if (matchtablestart < 0)
            matchtablestart = 0;
        for (c = oldmatchtablestart; c < matchtablestart; ++c) {
            const int b = byteAt(c);
            int& known = matches[static_cast<size_t>(b)];
            for (d = 0; d < known; ++d)
                if (table(b, d) >= matchtablestart) {
                    std::memmove(&table(b, 0), &table(b, d), static_cast<size_t>(known - d) * sizeof(int));
                    break;
                }
            known -= d;
        }
    }
    codebuffer[codecount++] = 0;
    out.push_back(static_cast<uint8_t>(bits | (1 << bitcount)));
    out.insert(out.end(), codebuffer, codebuffer + codecount);
    return out;
}

// ---- ZX0 and ZX1: Einar Saukas's optimal parsers, as ConvImgCpc has them ----

constexpr int kMaxOffsetZx0 = 32640;
constexpr int kMaxOffsetZx1 = 32512;
constexpr int kMaxScale = 50;

struct Block {
    Block* chain = nullptr;
    Block* ghostChain = nullptr;
    int bits = 0, index = 0, offset = 0, references = 0, length = 0;
};

class Packer {
public:
    explicit Packer(const PackProgress& progress)
        : progress_(progress)
    {
    }

    std::vector<uint8_t> zx0(const std::vector<uint8_t>& input, bool v2);
    std::vector<uint8_t> zx1(const std::vector<uint8_t>& input);

private:
    const PackProgress& progress_;
    std::deque<Block> blocks_;
    Block* ghostRoot_ = nullptr;
    std::vector<uint8_t> out_;
    size_t bitIndex_ = 0;
    int bitMask_ = 0;
    bool backTrack_ = false;

    Block* allocate(int bits, int index, int offset, int length, Block* chain)
    {
        Block* ptr;
        if (!ghostRoot_) {
            ptr = &blocks_.emplace_back();
        } else {
            ptr = ghostRoot_;
            ghostRoot_ = ptr->ghostChain;
            if (ptr->chain && --ptr->chain->references == 0) {
                ptr->chain->ghostChain = ghostRoot_;
                ghostRoot_ = ptr->chain;
            }
        }
        ptr->bits = bits;
        ptr->index = index;
        ptr->offset = offset;
        ptr->length = length;
        if (chain)
            chain->references++;
        ptr->chain = chain;
        ptr->references = 0;
        return ptr;
    }

    void assign(Block*& ptr, Block* chain)
    {
        chain->references++;
        if (ptr && --ptr->references == 0) {
            ptr->ghostChain = ghostRoot_;
            ghostRoot_ = ptr;
        }
        ptr = chain;
    }

    static int eliasGammaBits(int value)
    {
        int bits = 1;
        while ((value >>= 1) != 0)
            bits += 2;
        return bits;
    }

    void writeByte(int value) { out_.push_back(static_cast<uint8_t>(value)); }

    void writeBit(int value)
    {
        if (backTrack_) {
            if (value)
                out_.back() |= 1;
            backTrack_ = false;
        } else {
            if (bitMask_ == 0) {
                bitMask_ = 128;
                bitIndex_ = out_.size();
                out_.push_back(0);
            }
            if (value)
                out_[bitIndex_] |= static_cast<uint8_t>(bitMask_);
            bitMask_ >>= 1;
        }
    }

    void writeEliasZx0(int value, bool inverted)
    {
        int i;
        for (i = 2; i <= value; i <<= 1) {
        }
        i >>= 1;
        while ((i >>= 1) != 0) {
            writeBit(0);
            writeBit(inverted ? ((value & i) == 0 ? 1 : 0) : value & i);
        }
        writeBit(1);
    }

    void writeEliasZx1(int value)
    {
        int i;
        for (i = 2; i <= value; i <<= 1) {
        }
        i >>= 1;
        while ((i >>= 1) > 0) {
            writeBit(1);
            writeBit(value & i);
        }
        writeBit(0);
    }

    // The cheapest way to the end of `input`: the last block of its chain.
    Block* optimize(const std::vector<uint8_t>& input, bool zx1);
};

Block* Packer::optimize(const std::vector<uint8_t>& input, bool zx1)
{
    const int inputSize = static_cast<int>(input.size());
    const int limit = zx1 ? kMaxOffsetZx1 : kMaxOffsetZx0;
    int maxOffset = inputSize - 1 > limit ? limit : inputSize - 1 < 1 ? 1 : inputSize - 1;
    int dots = 0;
    std::vector<Block*> lastLiteral(static_cast<size_t>(maxOffset) + 1, nullptr);
    std::vector<Block*> lastMatch(static_cast<size_t>(maxOffset) + 1, nullptr);
    std::vector<Block*> optimal(static_cast<size_t>(inputSize) + 1, nullptr);
    std::vector<int> matchLength(static_cast<size_t>(maxOffset) + 1, 0);
    std::vector<int> bestLength(static_cast<size_t>(inputSize) + 2, 0);
    bestLength[2] = 2;

    assign(lastMatch[1], allocate(-1, -1, 1, 0, nullptr));
    for (int index = 0; index < inputSize; ++index) {
        int bestLengthSize = 2;
        maxOffset = index > limit ? limit : index < 1 ? 1 : index;
        Block*& best = optimal[static_cast<size_t>(index)];
        for (int offset = 1; offset <= maxOffset; ++offset) {
            const size_t o = static_cast<size_t>(offset);
            if (index != 0 && index >= offset && input[static_cast<size_t>(index)] == input[static_cast<size_t>(index - offset)]) {
                // A copy from the last offset.
                if (lastLiteral[o]) {
                    const int length = index - lastLiteral[o]->index;
                    const int bits = lastLiteral[o]->bits + 1 + eliasGammaBits(length);
                    assign(lastMatch[o], allocate(bits, index, offset, length, lastLiteral[o]));
                    if (!best || best->bits > bits)
                        assign(best, lastMatch[o]);
                }
                // A copy from a new offset.
                if (++matchLength[o] > 1) {
                    if (bestLengthSize < matchLength[o]) {
                        int bits = optimal[static_cast<size_t>(index - bestLength[static_cast<size_t>(bestLengthSize)])]->bits
                            + eliasGammaBits(bestLength[static_cast<size_t>(bestLengthSize)] - 1);
                        do {
                            ++bestLengthSize;
                            const int bits2 = optimal[static_cast<size_t>(index - bestLengthSize)]->bits
                                + eliasGammaBits(bestLengthSize - 1);
                            if (bits2 <= bits) {
                                bestLength[static_cast<size_t>(bestLengthSize)] = bestLengthSize;
                                bits = bits2;
                            } else {
                                bestLength[static_cast<size_t>(bestLengthSize)] = bestLength[static_cast<size_t>(bestLengthSize) - 1];
                            }
                        } while (bestLengthSize < matchLength[o]);
                    }
                    const int length = bestLength[static_cast<size_t>(matchLength[o])];
                    Block* from = optimal[static_cast<size_t>(index - length)];
                    const int bits = zx1 ? from->bits + 1 + (offset > 128 ? 16 : 8) + eliasGammaBits(length - 1)
                                         : from->bits + 8 + eliasGammaBits((offset - 1) / 128 + 1) + eliasGammaBits(length - 1);
                    if (!lastMatch[o] || lastMatch[o]->index != index || lastMatch[o]->bits > bits) {
                        assign(lastMatch[o], allocate(bits, index, offset, length, from));
                        if (!best || best->bits > bits)
                            assign(best, lastMatch[o]);
                    }
                }
            } else {
                // Literals.
                matchLength[o] = 0;
                if (lastMatch[o]) {
                    const int length = index - lastMatch[o]->index;
                    const int bits = lastMatch[o]->bits + 1 + eliasGammaBits(length) + length * 8;
                    assign(lastLiteral[o], allocate(bits, index, 0, length, lastMatch[o]));
                    if (!best || best->bits > bits)
                        assign(best, lastLiteral[o]);
                }
            }
        }
        if (index * kMaxScale / inputSize > dots) {
            ++dots;
            if (progress_)
                progress_(100 * dots / kMaxScale);
        }
    }
    return optimal[static_cast<size_t>(inputSize) - 1];
}

std::vector<uint8_t> Packer::zx0(const std::vector<uint8_t>& input, bool v2)
{
    if (input.empty())
        return {};
    Block* optimal = optimize(input, false);
    // The chain is from the end: it is turned round.
    Block* prev = nullptr;
    while (optimal) {
        Block* next = optimal->chain;
        optimal->chain = prev;
        prev = optimal;
        optimal = next;
    }
    out_.clear();
    bitMask_ = 0;
    int lastOffset = 1;
    size_t inputIndex = 0;
    backTrack_ = true;
    for (optimal = prev->chain; optimal; prev = optimal, optimal = optimal->chain) {
        const int length = optimal->index - prev->index;
        if (optimal->offset == 0) {
            writeBit(0);
            writeEliasZx0(length, false);
            for (int i = 0; i < length; ++i)
                writeByte(input[inputIndex++]);
        } else if (optimal->offset == lastOffset) {
            writeBit(0);
            writeEliasZx0(length, false);
            inputIndex += static_cast<size_t>(length);
        } else {
            writeBit(1);
            writeEliasZx0((optimal->offset - 1) / 128 + 1, v2);
            writeByte((127 - (optimal->offset - 1) % 128) << 1);
            backTrack_ = true;
            writeEliasZx0(length - 1, false);
            inputIndex += static_cast<size_t>(length);
            lastOffset = optimal->offset;
        }
    }
    writeBit(1);
    writeEliasZx0(256, v2);
    return out_;
}

std::vector<uint8_t> Packer::zx1(const std::vector<uint8_t>& input)
{
    if (input.empty())
        return {};
    Block* optimal = optimize(input, true);
    Block* next = nullptr;
    while (optimal) {
        Block* prev = optimal->chain;
        optimal->chain = next;
        next = optimal;
        optimal = prev;
    }
    out_.clear();
    bitMask_ = 0;
    backTrack_ = false;
    int lastOffset = 1;
    size_t inputIndex = 0;
    bool first = true;
    for (optimal = next->chain; optimal; optimal = optimal->chain) {
        if (optimal->offset == 0) {
            if (first)
                first = false;
            else
                writeBit(0);
            writeEliasZx1(optimal->length);
            for (int i = 0; i < optimal->length; ++i)
                writeByte(input[inputIndex++]);
        } else if (optimal->offset == lastOffset) {
            writeBit(0);
            writeEliasZx1(optimal->length);
            inputIndex += static_cast<size_t>(optimal->length);
        } else {
            writeBit(1);
            if (optimal->offset > 128) {
                writeByte(255 - ((optimal->offset - 1) & 254));
                writeByte(252 - (optimal->offset - 1) / 256 * 2 + optimal->offset % 2);
            } else {
                writeByte(256 - optimal->offset * 2);
            }
            writeEliasZx1(optimal->length - 1);
            inputIndex += static_cast<size_t>(optimal->length);
            lastOffset = optimal->offset;
        }
    }
    writeBit(1);
    writeByte(255);
    writeByte(255);
    return out_;
}

// An overscan screen of 96 by 272 without the bytes its lines leave unused
// at the end of each 2K: what ZX0Ovs packs.
std::vector<uint8_t> overscanUsed(const std::vector<uint8_t>& in)
{
    std::vector<uint8_t> used;
    size_t posIn = 0;
    for (int i = 0; i < 15; ++i) {
        const size_t lIn = i < 7 ? 0x600 : i == 7 ? 0xCC0 : 0x6C0;
        for (size_t n = 0; n < lIn; ++n)
            used.push_back(posIn + n < in.size() ? in[posIn + n] : 0);
        posIn += i != 7 ? 0x800 : 0xE00;
    }
    return used;
}

// What the CPC's routines read their bits with.
class BitReader {
public:
    explicit BitReader(const std::vector<uint8_t>& in)
        : in_(in)
    {
    }
    // Whether more was asked for than the file has: it is not one that
    // was packed this way, or it is cut.
    bool ended() const { return overrun_; }
    int byte()
    {
        if (at_ >= in_.size())
            overrun_ = true;
        last_ = at_ < in_.size() ? in_[at_] : 0;
        ++at_;
        return last_;
    }
    int bit()
    {
        if (backtrack) {
            backtrack = false;
            return last_ & 1;
        }
        mask_ >>= 1;
        if (mask_ == 0) {
            mask_ = 128;
            value_ = byte();
        }
        return value_ & mask_ ? 1 : 0;
    }
    bool backtrack = false;

private:
    const std::vector<uint8_t>& in_;
    size_t at_ = 0;
    int last_ = 0, mask_ = 0, value_ = 0;
    bool overrun_ = false;
};

void copyBack(std::vector<uint8_t>& out, int offset, int length)
{
    for (int i = 0; i < length; ++i) {
        if (offset <= 0 || static_cast<size_t>(offset) > out.size())
            return;
        out.push_back(out[out.size() - static_cast<size_t>(offset)]);
    }
}

}  // namespace

std::vector<uint8_t> pack(const std::vector<uint8_t>& data, PackMethod method, const PackProgress& progress)
{
    switch (method) {
    case PackMethod::Standard:
        return packStandard(data);
    case PackMethod::ZX0:
        return Packer(progress).zx0(data, false);
    case PackMethod::ZX0_V2:
        return Packer(progress).zx0(data, true);
    case PackMethod::ZX1:
        return Packer(progress).zx1(data);
    case PackMethod::ZX0Ovs:
        // In the second format: it is the one the routine ConvImgCpc writes
        // for this packing unpacks (ConvImgCpc itself packs in the first,
        // which that routine cannot read).
        return Packer(progress).zx0(overscanUsed(data), true);
    case PackMethod::None:
        break;
    }
    return {};
}

std::vector<uint8_t> unpackStandard(const std::vector<uint8_t>& in, size_t inBytes)
{
    std::vector<uint8_t> out;
    int depackBits = 0;
    const auto byte = [&in](size_t at) -> int { return at < in.size() ? in[at] : 0; };
    while (inBytes < in.size()) {
        int bit = depackBits & 1;
        depackBits >>= 1;
        if (depackBits == 0) {
            depackBits = byte(inBytes++);
            bit = depackBits & 1;
            depackBits >>= 1;
            depackBits |= 0x80;
        }
        if (bit == 0) {
            out.push_back(static_cast<uint8_t>(byte(inBytes++)));
            continue;
        }
        const int a = byte(inBytes);
        if (a == 0)
            break;  // the end
        int length, delta;
        if (a & 0x80) {
            length = 3 + ((a >> 4) & 7);
            delta = (byte(inBytes++) & 15) << 8;
            delta |= byte(inBytes++);
            delta++;
        } else if (a & 0x40) {
            length = 2;
            delta = byte(inBytes++) & 0x3F;
            delta++;
        } else if (a & 0x20) {
            length = 2 + (byte(inBytes++) & 31);
            delta = byte(inBytes++);
            delta++;
        } else if (a & 0x10) {
            delta = (byte(inBytes++) & 15) << 8;
            delta |= byte(inBytes++);
            length = byte(inBytes++) + 1;
            delta++;
        } else if (a == 15) {
            length = delta = byte(inBytes + 1) + 1;
            inBytes += 2;
        } else {
            length = delta = a > 1 ? a : 256;
            inBytes++;
        }
        if (static_cast<size_t>(delta) > out.size())
            break;  // not a file this method packed
        copyBack(out, delta, length);
        if (out.size() > 0x20000)
            break;
    }
    return out;
}

std::vector<uint8_t> unpackZx0(const std::vector<uint8_t>& packed, bool v2)
{
    std::vector<uint8_t> out;
    BitReader in(packed);
    const auto elias = [&in](bool inverted) {
        int value = 1;
        while (!in.bit() && !in.ended())
            value = (value << 1) | (in.bit() ^ (inverted ? 1 : 0));
        return value;
    };
    int lastOffset = 1;
    enum { Literals, LastOffset, NewOffset } state = Literals;
    while (!in.ended() && out.size() < 0x20000) {
        if (state == Literals) {
            const int length = elias(false);
            for (int i = 0; i < length; ++i)
                out.push_back(static_cast<uint8_t>(in.byte()));
            state = in.bit() ? NewOffset : LastOffset;
        } else if (state == LastOffset) {
            copyBack(out, lastOffset, elias(false));
            state = in.bit() ? NewOffset : Literals;
        } else {
            lastOffset = elias(v2);
            if (lastOffset == 256)
                break;
            lastOffset = lastOffset * 128 - (in.byte() >> 1);
            in.backtrack = true;
            copyBack(out, lastOffset, elias(false) + 1);
            state = in.bit() ? NewOffset : Literals;
        }
    }
    return out;
}

std::vector<uint8_t> unpackZx1(const std::vector<uint8_t>& packed)
{
    std::vector<uint8_t> out;
    BitReader in(packed);
    const auto elias = [&in] {
        int value = 1;
        while (in.bit() && !in.ended())
            value = (value << 1) | in.bit();
        return value;
    };
    int lastOffset = 1;
    enum { Literals, LastOffset, NewOffset } state = Literals;
    while (!in.ended() && out.size() < 0x20000) {
        if (state == Literals) {
            const int length = elias();
            for (int i = 0; i < length; ++i)
                out.push_back(static_cast<uint8_t>(in.byte()));
            state = in.bit() ? NewOffset : LastOffset;
        } else if (state == LastOffset) {
            copyBack(out, lastOffset, elias());
            state = in.bit() ? NewOffset : Literals;
        } else {
            const int b1 = in.byte();
            if ((b1 & 1) == 0) {
                lastOffset = 128 - (b1 >> 1);
            } else {
                const int b2 = in.byte();
                const int high = ((0x80 | (b2 >> 1)) + 1) & 0xFF;
                if (high == 0)
                    break;
                lastOffset = 0x10000 - ((high << 8) | (b1 & 0xFE) | (b2 & 1));
            }
            copyBack(out, lastOffset, elias() + 1);
            state = in.bit() ? NewOffset : Literals;
        }
    }
    return out;
}

}  // namespace tuxscr
