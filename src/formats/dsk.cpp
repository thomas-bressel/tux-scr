#include "formats/dsk.h"

#include <algorithm>
#include <cctype>

namespace tuxscr {

namespace {

constexpr uint8_t kDeleted = 0xE5;

std::array<uint8_t, 11> amsdosName(const std::string& file)
{
    std::string name, ext;
    const size_t p = file.find('.');
    const auto upper = [](std::string s) {
        for (char& c : s)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return s;
    };
    if (p != std::string::npos && p > 0) {
        name = file.substr(0, p);
        ext = file.substr(p + 1);
    } else {
        name = upper(file.size() > 8 ? file.substr(0, 8) : file);
        ext = file.size() > 8 ? upper(file.substr(8)) : "BIN";
    }
    std::array<uint8_t, 11> out;
    out.fill(' ');
    for (size_t i = 0; i < 8 && i < name.size(); ++i)
        out[i] = static_cast<uint8_t>(name[i]);
    for (size_t i = 0; i < 3 && i < ext.size(); ++i)
        out[8 + i] = static_cast<uint8_t>(ext[i]);
    return out;
}

}  // namespace

int Dsk::sectorBytes(const Sector& sector)
{
    int n = sector.size;
    if ((n & 0xFF) > 0) {
        n = (n + 0xFF) & 0x1F00;
    } else if (n == 0) {
        n = sector.n;
        n = n < 6 ? 128 << n : 0x1800;
    }
    return n > 0x100 ? n : 0x100;
}

void Dsk::format()
{
    // The name of what made the disc is TuxSCR's, where ConvImgCpc has its
    // own.
    id_ = "EXTENDED CPC DSK File\r\nDisk-Info\r\nTuxSCR        ";
    trackCount_ = 40;
    headCount_ = 1;
    trackSize_ = 0;
    tracks_.assign(static_cast<size_t>(kMaxTracks) * 2, {});
    trackSizes_.fill(0);
    for (int t = 0; t < kMaxTracks; ++t) {
        formatTrack(t, 0xC1, 9);
        trackSizes_[static_cast<size_t>(t)] = 0x13;
    }
}

void Dsk::formatTrack(int t, int minSect, int sectorCount)
{
    Track& tr = side0(t);
    tr = {};
    tr.data.assign(0x1800, kDeleted);
    tr.id = "Track-Info\r\n    ";
    tr.track = static_cast<uint8_t>(t);
    tr.head = 0;
    tr.sectorSize = 2;
    tr.sectorCount = static_cast<uint8_t>(sectorCount);
    tr.gap3 = 0x4E;
    tr.filler = kDeleted;
    // The sectors are interleaved.
    int readSect = 0;
    for (int s = 0; s < sectorCount;) {
        const auto set = [&](int sector, int r) {
            Sector& sect = tr.sectors[static_cast<size_t>(sector)];
            sect.c = static_cast<uint8_t>(t);
            sect.h = 0;
            sect.r = static_cast<uint8_t>(r);
            sect.n = 2;
            sect.size = kSectorSize;
        };
        set(s, readSect + minSect);
        ++readSect;
        if (++s < sectorCount) {
            set(s, readSect + minSect + 4);
            ++s;
        }
    }
}

int Dsk::dataPosition(int track, int sector) const
{
    int pos = 0;
    const Track& tr = side0(track);
    for (int s = 0; s < tr.sectorCount && s < kMaxSectors; ++s) {
        const Sector& sect = tr.sectors[static_cast<size_t>(s)];
        if (sect.r == sector)
            break;
        pos += sect.size != 0 ? sect.size : (128 << sect.n);
    }
    return pos;
}

int Dsk::minSector(bool standard) const
{
    int found = 0x100;
    const Track& tr = side0(0);
    for (int s = 0; s < tr.sectorCount && s < kMaxSectors; ++s) {
        const int current = tr.sectors[static_cast<size_t>(s)].r;
        if (standard && (current == 0x01 || current == 0x41 || current == 0xC1))
            return current;
        if (found > current)
            found = current;
    }
    return found;
}

int Dsk::directoryTrack() const
{
    const int min = minSector(true);
    return min == 0x41 ? 2 : min == 1 ? 1 : 0;
}

Dsk::DirEntry Dsk::dirEntry(int number) const
{
    DirEntry entry;
    const int t = directoryTrack();
    const size_t pos = static_cast<size_t>(dataPosition(t, (number >> 4) + minSector(true)) + ((number & 15) << 5));
    const std::vector<uint8_t>& data = side0(t).data;
    if (pos + 32 > data.size()) {
        entry.user = kDeleted;
        return entry;
    }
    entry.user = data[pos];
    std::copy_n(data.begin() + static_cast<long>(pos) + 1, 8, entry.name.begin());
    std::copy_n(data.begin() + static_cast<long>(pos) + 9, 3, entry.ext.begin());
    entry.extent = data[pos + 12];
    entry.records = data[pos + 15];
    std::copy_n(data.begin() + static_cast<long>(pos) + 16, 16, entry.blocks.begin());
    return entry;
}

void Dsk::setDirEntry(int number, const DirEntry& entry)
{
    const int t = directoryTrack();
    const size_t pos = static_cast<size_t>(dataPosition(t, (number >> 4) + minSector(true)) + ((number & 15) << 5));
    std::vector<uint8_t>& data = side0(t).data;
    if (pos + 32 > data.size())
        return;
    data[pos] = entry.user;
    std::copy(entry.name.begin(), entry.name.end(), data.begin() + static_cast<long>(pos) + 1);
    std::copy(entry.ext.begin(), entry.ext.end(), data.begin() + static_cast<long>(pos) + 9);
    data[pos + 12] = entry.extent;
    data[pos + 13] = 0;
    data[pos + 14] = 0;
    data[pos + 15] = entry.records;
    std::copy(entry.blocks.begin(), entry.blocks.end(), data.begin() + static_cast<long>(pos) + 16);
}

void Dsk::fillBitmap()
{
    bitmap_.fill(0);
    bitmap_[0] = 1;
    for (int i = 0; i < 64; ++i) {
        const DirEntry entry = dirEntry(i);
        if (entry.user == kDeleted)
            continue;
        for (const uint8_t block : entry.blocks)
            if (block > 1)
                bitmap_[block] = 1;
    }
}

int Dsk::fileExists(const std::array<uint8_t, 11>& name, int user) const
{
    for (int i = 0; i < 64; ++i) {
        const DirEntry entry = dirEntry(i);
        if (entry.user != user)
            continue;
        bool same = true;
        for (size_t c = 0; c < 11 && same; ++c)
            same = (name[c] & 0x7F) == ((c < 8 ? entry.name[c] : entry.ext[c - 8]) & 0x7F);
        if (same)
            return i;
    }
    return -1;
}

int Dsk::freeDirEntry() const
{
    for (int i = 0; i < 64; ++i)
        if (dirEntry(i).user == kDeleted)
            return i;
    return -1;
}

int Dsk::freeBlock(int maxBlock)
{
    for (int i = 2; i < maxBlock && i < 256; ++i)
        if (bitmap_[static_cast<size_t>(i)] == 0) {
            bitmap_[static_cast<size_t>(i)] = 1;
            return i;
        }
    return 0;
}

// A block of AMSDOS is two sectors.
void Dsk::writeBlock(int block, const std::vector<uint8_t>& file, int offset)
{
    int track = (block << 1) / 9;
    int sect = (block << 1) % 9;
    const int min = minSector(true);
    if (min == 0x41)
        track += 2;
    else if (min == 0x01)
        track++;
    if (track >= kMaxTracks - 1)
        return;
    // The disc is given the tracks the file needs.
    const auto grow = [&](int to) {
        if (to <= trackCount_ - 1)
            return;
        trackCount_ = to + 1;
        formatTrack(to, min, 9);
        // In an image whose tracks each say their size, the new one does.
        const size_t entry = static_cast<size_t>(to) * static_cast<size_t>(std::max(1, headCount_));
        if (id_.compare(0, 8, "EXTENDED") == 0 && entry < trackSizes_.size())
            trackSizes_[entry] = 0x13;
    };
    grow(track);
    const auto copy = [&](int t, int s, int from) {
        const int length = std::min(kSectorSize, static_cast<int>(file.size()) - from);
        std::vector<uint8_t>& data = side0(t).data;
        const int pos = dataPosition(t, s + min);
        if (length > 0 && static_cast<size_t>(pos + length) <= data.size())
            std::copy_n(file.begin() + from, length, data.begin() + pos);
    };
    copy(track, sect, offset);
    if (++sect > 8) {
        ++track;
        sect = 0;
        grow(track);
    }
    copy(track, sect, offset + kSectorSize);
}

Dsk::Error Dsk::addFile(const std::vector<uint8_t>& file, const std::string& fileName, int maxBlock, int user)
{
    fillBitmap();
    const std::array<uint8_t, 11> name = amsdosName(fileName);
    if (fileExists(name, user) > -1)
        return Error::FileExists;

    const int size = static_cast<int>(file.size());
    int extent = 0;
    for (int posFile = 0; posFile < size;) {
        const int posDir = freeDirEntry();
        if (posDir == -1)
            return Error::NoDirectoryEntry;
        DirEntry entry;
        entry.user = static_cast<uint8_t>(user);
        std::copy_n(name.begin(), 8, entry.name.begin());
        std::copy_n(name.begin() + 8, 3, entry.ext.begin());
        entry.extent = static_cast<uint8_t>(extent++);
        entry.records = static_cast<uint8_t>(std::min(128, ((size - posFile) + 127) >> 7));
        const int blocks = (entry.records + 7) >> 3;
        for (int j = 0; j < blocks; ++j) {
            const int block = freeBlock(maxBlock);
            if (block == 0)
                return Error::NoBlock;
            entry.blocks[static_cast<size_t>(j)] = static_cast<uint8_t>(block);
            writeBlock(block, file, posFile);
            posFile += 1024;
        }
        setDirEntry(posDir, entry);
    }
    return Error::None;
}

std::vector<std::string> Dsk::fileNames() const
{
    std::vector<std::string> names;
    for (int i = 0; i < 64; ++i) {
        const DirEntry entry = dirEntry(i);
        if (entry.user == kDeleted || entry.extent != 0)
            continue;
        std::string name;
        for (const uint8_t c : entry.name)
            name += static_cast<char>(c & 0x7F);
        name += '.';
        for (const uint8_t c : entry.ext)
            name += static_cast<char>(c & 0x7F);
        names.push_back(name);
    }
    return names;
}

std::vector<uint8_t> Dsk::file(const std::string& fileName, int user) const
{
    const std::array<uint8_t, 11> name = amsdosName(fileName);
    std::vector<uint8_t> out;
    const int min = minSector(true);
    for (int extent = 0; extent < 64; ++extent) {
        bool found = false;
        for (int i = 0; i < 64 && !found; ++i) {
            const DirEntry entry = dirEntry(i);
            if (entry.user != user || entry.extent != extent)
                continue;
            bool same = true;
            for (size_t c = 0; c < 11 && same; ++c)
                same = (name[c] & 0x7F) == ((c < 8 ? entry.name[c] : entry.ext[c - 8]) & 0x7F);
            if (!same)
                continue;
            found = true;
            int records = entry.records;
            for (const uint8_t block : entry.blocks) {
                if (block == 0 || records <= 0)
                    break;
                for (int half = 0; half < 2 && records > 0; ++half) {
                    int track = ((block << 1) + half) / 9 + (min == 0x41 ? 2 : min == 0x01 ? 1 : 0);
                    const int sect = ((block << 1) + half) % 9;
                    const std::vector<uint8_t>& data = side0(std::min(track, kMaxTracks - 1)).data;
                    const int pos = dataPosition(track, sect + min);
                    const int bytes = std::min(records, 4) * 128;
                    for (int b = 0; b < bytes; ++b)
                        out.push_back(static_cast<size_t>(pos + b) < data.size() ? data[static_cast<size_t>(pos + b)] : 0);
                    records -= 4;
                }
            }
        }
        if (!found)
            break;
    }
    return out;
}

std::vector<uint8_t> Dsk::save() const
{
    std::vector<uint8_t> out;
    const auto text = [&out](const std::string& s, size_t length) {
        for (size_t i = 0; i < length; ++i)
            out.push_back(i < s.size() ? static_cast<uint8_t>(s[i]) : 0);
    };
    const auto word = [&out](int value) {
        out.push_back(static_cast<uint8_t>(value & 0xFF));
        out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    };
    text(id_, 0x30);
    out.push_back(static_cast<uint8_t>(trackCount_));
    out.push_back(static_cast<uint8_t>(headCount_));
    word(trackSize_);
    out.insert(out.end(), trackSizes_.begin(), trackSizes_.end());
    for (int n = 0; n < trackCount_ * std::max(1, headCount_); ++n) {
        const int heads = std::max(1, headCount_);
        if (n / heads >= kMaxTracks)
            break;
        const Track& tr = tracks_[static_cast<size_t>(n / heads) * 2 + static_cast<size_t>(n % heads > 0 ? 1 : 0)];
        // A track the image did not have is not there to write.
        if (tr.id.empty())
            continue;
        text(tr.id, 0x10);
        out.push_back(tr.track);
        out.push_back(tr.head);
        word(tr.unused);
        out.push_back(tr.sectorSize);
        out.push_back(tr.sectorCount);
        out.push_back(tr.gap3);
        out.push_back(tr.filler);
        size_t dataSize = 0;
        for (int s = 0; s < kMaxSectors; ++s) {
            const Sector& sect = tr.sectors[static_cast<size_t>(s)];
            out.push_back(sect.c);
            out.push_back(sect.h);
            out.push_back(sect.r);
            out.push_back(sect.n);
            out.push_back(sect.st1);
            out.push_back(sect.st2);
            word(sect.size);
            if (s < tr.sectorCount)
                dataSize += static_cast<size_t>(sectorBytes(sect));
        }
        for (size_t i = 0; i < dataSize; ++i)
            out.push_back(i < tr.data.size() ? tr.data[i] : kDeleted);
    }
    return out;
}

bool Dsk::load(const std::vector<uint8_t>& file)
{
    if (file.size() < 0x100)
        return false;
    const std::string id(file.begin(), file.begin() + 0x30);
    if (id.compare(0, 8, "EXTENDED") != 0 && id.compare(0, 8, "MV - CPC") != 0)
        return false;
    format();
    id_ = id;
    size_t at = 0x30;
    const auto byte = [&]() -> uint8_t { return at < file.size() ? file[at++] : 0; };
    const auto word = [&]() -> int {
        const int low = byte();
        return low | (byte() << 8);
    };
    trackCount_ = std::min<int>(byte(), kMaxTracks);
    headCount_ = byte();
    trackSize_ = word();
    for (uint8_t& size : trackSizes_)
        size = byte();
    // Only the first side is written to; the other is kept as it is.
    for (int t = 0; t < trackCount_; ++t)
        for (int h = 0; h < std::max(1, headCount_); ++h) {
            if (at + 0x100 > file.size())
                return true;
            Track read;
            read.id.assign(file.begin() + static_cast<long>(at), file.begin() + static_cast<long>(at) + 0x10);
            at += 0x10;
            if (read.id.compare(0, 10, "Track-Info") != 0)
                return true;
            read.track = byte();
            read.head = byte();
            read.unused = word();
            read.sectorSize = byte();
            read.sectorCount = byte();
            read.gap3 = byte();
            read.filler = byte();
            size_t dataSize = 0;
            for (int s = 0; s < kMaxSectors; ++s) {
                Sector& sect = read.sectors[static_cast<size_t>(s)];
                sect.c = byte();
                sect.h = byte();
                sect.r = byte();
                sect.n = byte();
                sect.st1 = byte();
                sect.st2 = byte();
                sect.size = word();
                if (s < read.sectorCount)
                    dataSize += static_cast<size_t>(sectorBytes(sect));
            }
            read.data.resize(std::max<size_t>(dataSize, 0x1800), kDeleted);
            for (size_t i = 0; i < dataSize; ++i)
                read.data[i] = byte();
            if (h < 2)
                tracks_[static_cast<size_t>(t) * 2 + static_cast<size_t>(h)] = std::move(read);
        }
    return true;
}

}  // namespace tuxscr
