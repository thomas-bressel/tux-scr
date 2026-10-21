// Disc images: a new one is a DATA disc of 40 tracks; files put on it are
// found in its directory and read back; a disc saved and loaded is the
// same; what does not fit is refused.

#include <cstdint>
#include <vector>

#include "check.h"
#include "formats/amsdos.h"
#include "formats/dsk.h"

using namespace tuxscr;

int main()
{
    Dsk disc;
    const std::vector<uint8_t> fresh = disc.save();
    CHECK_EQ(fresh.size(), 256 + 40 * (256 + 9 * 512));
    CHECK(std::equal(fresh.begin(), fresh.begin() + 34, "EXTENDED CPC DSK File\r\nDisk-Info\r\n"));
    CHECK_EQ(fresh[0x30], 40);
    CHECK_EQ(fresh[0x31], 1);
    CHECK_EQ(fresh[0x34], 0x13);
    // A track: its header, nine sectors of 512 bytes from &C1, interleaved.
    CHECK(std::equal(fresh.begin() + 256, fresh.begin() + 256 + 10, "Track-Info"));
    CHECK_EQ(fresh[256 + 0x15], 9);
    CHECK_EQ(fresh[256 + 0x18 + 2], 0xC1);
    CHECK_EQ(fresh[256 + 0x18 + 8 + 2], 0xC6);
    CHECK_EQ(fresh[256 + 0x18 + 16 + 2], 0xC2);
    CHECK_EQ(fresh[256 + 0x100], 0xE5);
    CHECK(disc.fileNames().empty());

    // A file of 3000 bytes with its header: three blocks, in one entry.
    std::vector<uint8_t> data;
    for (int i = 0; i < 3000; ++i)
        data.push_back(static_cast<uint8_t>(i * 7));
    const std::vector<uint8_t> file = withAmsdosHeader("picture.scr", 0xC000, 0xC7D0, data);
    CHECK(disc.addFile(file, "PICTURE.SCR") == Dsk::Error::None);
    CHECK(disc.addFile(file, "PICTURE.SCR") == Dsk::Error::FileExists);
    CHECK(disc.addFile(file, "OTHER.BIN") == Dsk::Error::None);
    const std::vector<std::string> names = disc.fileNames();
    CHECK(names.size() == 2 && names[0] == "PICTURE .SCR" && names[1] == "OTHER   .BIN");
    std::vector<uint8_t> back = disc.file("PICTURE.SCR");
    // Whole records of 128 bytes.
    CHECK_EQ(back.size(), ((file.size() + 127) / 128) * 128);
    back.resize(file.size());
    CHECK(back == file);
    CHECK(disc.file("NOTHERE.SCR").empty());
    // The directory entry: user 0, the name, three blocks from 2.
    const std::vector<uint8_t> saved = disc.save();
    const size_t dir = 256 + 256;
    CHECK_EQ(saved[dir], 0);
    CHECK(std::equal(saved.begin() + static_cast<long>(dir) + 1, saved.begin() + static_cast<long>(dir) + 12, "PICTURE SCR"));
    CHECK_EQ(saved[dir + 15], (file.size() + 127) / 128);
    CHECK(saved[dir + 16] == 2 && saved[dir + 17] == 3 && saved[dir + 18] == 4 && saved[dir + 19] == 5 && saved[dir + 20] == 0);

    // Saved and loaded, it is the same disc; saved again, the same file.
    Dsk loaded;
    CHECK(loaded.load(saved));
    CHECK(loaded.save() == saved);
    CHECK(loaded.fileNames() == names);
    back = loaded.file("OTHER.BIN");
    back.resize(file.size());
    CHECK(back == file);
    CHECK(!loaded.load(std::vector<uint8_t>(300, 0)));
    CHECK(!loaded.load({}));

    // A file of more than 16K takes two entries of the directory.
    Dsk large;
    const std::vector<uint8_t> big(40000, 0x5A);
    CHECK(large.addFile(big, "BIG.BIN") == Dsk::Error::None);
    back = large.file("BIG.BIN");
    CHECK(back.size() >= big.size() && std::equal(big.begin(), big.end(), back.begin()));
    CHECK_EQ(large.fileNames().size(), 1);
    // The disc holds 178 blocks: a fifth file of 40000 bytes does not fit.
    CHECK(large.addFile(big, "BIG2.BIN") == Dsk::Error::None);
    CHECK(large.addFile(big, "BIG3.BIN") == Dsk::Error::None);
    CHECK(large.addFile(big, "BIG4.BIN") == Dsk::Error::None);
    CHECK(large.addFile(big, "BIG5.BIN") == Dsk::Error::NoBlock);

    // 64 entries is all the directory has.
    Dsk many;
    Dsk::Error error = Dsk::Error::None;
    int added = 0;
    for (int i = 0; i < 70 && error == Dsk::Error::None; ++i) {
        char name[16];
        std::snprintf(name, sizeof name, "F%02d.BIN", i);
        error = many.addFile(std::vector<uint8_t>(100, 1), name);
        added += error == Dsk::Error::None;
    }
    CHECK_EQ(added, 64);
    CHECK(error == Dsk::Error::NoDirectoryEntry);
    return checkSummary("engine_formats_dsk");
}
