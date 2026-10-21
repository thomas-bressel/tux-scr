#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace tuxscr {

// The ways ConvImgCpc packs what it saves, by its own numbers.
enum class PackMethod {
    None = 0,
    Standard,  // its own
    ZX0,       // Einar Saukas's ZX0, first format
    ZX0_V2,    // ZX0, second format
    ZX1,       // Einar Saukas's ZX1
    ZX0Ovs,    // ZX0 of an overscan screen without what the picture leaves unused
};

// Told how far packing has got, 0 to 100: ZX0 and ZX1 take their time.
using PackProgress = std::function<void(int)>;

// Packs `data` and returns what it packs to.
std::vector<uint8_t> pack(const std::vector<uint8_t>& data, PackMethod method, const PackProgress& progress = {});

// Unpacks what ConvImgCpc's own method packed, from `start` on.
std::vector<uint8_t> unpackStandard(const std::vector<uint8_t>& packed, size_t start = 0);

// Unpacks ZX0 (either format) and ZX1: what the CPC's routines do, for the
// tests to check with.
std::vector<uint8_t> unpackZx0(const std::vector<uint8_t>& packed, bool v2);
std::vector<uint8_t> unpackZx1(const std::vector<uint8_t>& packed);

}  // namespace tuxscr
