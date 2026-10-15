// The packers: what each packs is unpacked to what it was, whatever it is.
// That the CPC's own routines unpack it too is checked on TuxAPE's machine
// (tuxape_interrupts).

#include <cstdint>
#include <vector>

#include "check.h"
#include "formats/pack.h"

using namespace tuxscr;

namespace {

std::vector<uint8_t> unpacked(const std::vector<uint8_t>& packed, PackMethod method)
{
    switch (method) {
    case PackMethod::Standard:
        return unpackStandard(packed);
    case PackMethod::ZX0:
        return unpackZx0(packed, false);
    case PackMethod::ZX0_V2:
        return unpackZx0(packed, true);
    case PackMethod::ZX1:
        return unpackZx1(packed);
    default:
        return {};
    }
}

}  // namespace

int main()
{
    std::vector<std::vector<uint8_t>> inputs;
    inputs.push_back(std::vector<uint8_t>(1, 0x42));
    inputs.push_back(std::vector<uint8_t>(2, 0x00));
    inputs.push_back(std::vector<uint8_t>(5000, 0x00));
    inputs.push_back({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13});
    // Bytes that follow no rule.
    std::vector<uint8_t> noise;
    uint32_t seed = 12345;
    for (int i = 0; i < 3000; ++i) {
        seed = seed * 1664525u + 1013904223u;
        noise.push_back(static_cast<uint8_t>(seed >> 24));
    }
    inputs.push_back(noise);
    // A screen: runs, patterns that come back from far, and a little noise.
    std::vector<uint8_t> screen;
    for (int i = 0; i < 16384; ++i) {
        seed = seed * 1664525u + 1013904223u;
        const int line = i / 80;
        screen.push_back(static_cast<uint8_t>(line % 7 == 0 ? (seed >> 28) * 17 : (i % 80 < 40 ? 0xF0 : 0x0F) ^ (line & 0x18)));
    }
    inputs.push_back(screen);
    // The same three bytes again and again, and runs of 256 and more.
    std::vector<uint8_t> runs;
    for (int i = 0; i < 1200; ++i)
        runs.push_back(static_cast<uint8_t>("abc"[i % 3]));
    runs.insert(runs.end(), 256, 0x55);
    runs.insert(runs.end(), 257, 0xAA);
    runs.insert(runs.end(), 255, 0x55);
    inputs.push_back(runs);

    const PackMethod methods[] = {PackMethod::Standard, PackMethod::ZX0, PackMethod::ZX0_V2, PackMethod::ZX1};
    for (const PackMethod method : methods)
        for (size_t n = 0; n < inputs.size(); ++n) {
            const std::vector<uint8_t> packed = pack(inputs[n], method);
            const std::vector<uint8_t> back = unpacked(packed, method);
            if (back != inputs[n]) {
                std::printf("method %d, input %zu: %zu bytes packed to %zu, unpacked to %zu\n", static_cast<int>(method), n,
                            inputs[n].size(), packed.size(), back.size());
                ++g_failures;
            }
            CHECK(!packed.empty());
        }
    // What is worth packing gets smaller, and the best parsers do best.
    const size_t standard = pack(screen, PackMethod::Standard).size();
    const size_t zx0 = pack(screen, PackMethod::ZX0).size();
    const size_t zx1 = pack(screen, PackMethod::ZX1).size();
    CHECK(standard < screen.size() / 2);
    CHECK(zx0 <= standard && zx1 <= standard);
    CHECK_EQ(pack(screen, PackMethod::ZX0_V2).size(), zx0);
    CHECK(pack(std::vector<uint8_t>(5000, 0), PackMethod::ZX0).size() < 12);
    CHECK(pack({}, PackMethod::ZX0).empty() && pack({}, PackMethod::Standard).empty());

    // ZX0Ovs packs an overscan screen without the bytes its lines leave
    // unused: 15 parts of what is used.
    std::vector<uint8_t> overscan(0x7CC0, 0);
    for (size_t i = 0; i < overscan.size(); ++i)
        overscan[i] = static_cast<uint8_t>(i >> 5);
    const std::vector<uint8_t> used = unpackZx0(pack(overscan, PackMethod::ZX0Ovs), true);
    CHECK_EQ(used.size(), 7 * 0x600 + 0xCC0 + 7 * 0x6C0);
    CHECK(used.size() > 0x600 && used[0] == overscan[0] && used[0x5FF] == overscan[0x5FF] && used[0x600] == overscan[0x800]);

    // The progress told goes up to 100.
    int last = 0, calls = 0;
    pack(screen, PackMethod::ZX0, [&](int percent) {
        CHECK(percent >= last && percent <= 100);
        last = percent;
        ++calls;
    });
    CHECK(calls > 10 && last >= 98);
    return checkSummary("formats_pack");
}
