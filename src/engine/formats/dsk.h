#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace tuxscr {

// A disc image (.DSK) a file is put on: ConvImgCpc's GestDSK. A new one is
// a DATA disc of 40 tracks; one read from a file keeps what it had.
class Dsk {
public:
    enum class Error { None, NoDirectoryEntry, NoBlock, FileExists };

    Dsk() { format(); }

    void format();
    // False if `file` is not a disc image.
    bool load(const std::vector<uint8_t>& file);
    std::vector<uint8_t> save() const;

    // Puts a file on the disc under an AMSDOS name ("NAME.EXT").
    Error addFile(const std::vector<uint8_t>& file, const std::string& name, int maxBlock = 178, int user = 0);

    // The names of the files on the disc, as "NAME    .EXT".
    std::vector<std::string> fileNames() const;
    // A file of the disc, as it was put there; empty if it is not on it.
    std::vector<uint8_t> file(const std::string& name, int user = 0) const;

private:
    static constexpr int kMaxTracks = 99;
    static constexpr int kMaxSectors = 29;
    static constexpr int kSectorSize = 512;

    struct Sector {
        uint8_t c = 0, h = 0, r = 0, n = 0, st1 = 0, st2 = 0;
        int size = 0;
    };
    struct Track {
        std::string id;
        uint8_t track = 0, head = 0;
        int unused = 0;
        uint8_t sectorSize = 0, sectorCount = 0, gap3 = 0, filler = 0;
        std::array<Sector, kMaxSectors> sectors{};
        std::vector<uint8_t> data;
    };
    struct DirEntry {
        uint8_t user = 0;
        std::array<uint8_t, 8> name{};
        std::array<uint8_t, 3> ext{};
        uint8_t extent = 0;
        uint8_t records = 0;
        std::array<uint8_t, 16> blocks{};
    };

    std::string id_;
    int trackCount_ = 40;
    int headCount_ = 1;
    int trackSize_ = 0;
    std::array<uint8_t, 204> trackSizes_{};
    std::vector<Track> tracks_;  // two sides to a track
    Track& side0(int track) { return tracks_[static_cast<size_t>(track) * 2]; }
    const Track& side0(int track) const { return tracks_[static_cast<size_t>(track) * 2]; }
    std::array<uint8_t, 256> bitmap_{};

    void formatTrack(int t, int minSector, int sectorCount);
    int dataPosition(int track, int sector) const;
    int minSector(bool standard) const;
    int directoryTrack() const;
    DirEntry dirEntry(int number) const;
    void setDirEntry(int number, const DirEntry& entry);
    void fillBitmap();
    int fileExists(const std::array<uint8_t, 11>& name, int user) const;
    int freeDirEntry() const;
    int freeBlock(int maxBlock);
    void writeBlock(int block, const std::vector<uint8_t>& file, int offset);
    static int sectorBytes(const Sector& sector);
};

}  // namespace tuxscr
