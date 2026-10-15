#include "formats/amsdos.h"

#include <cctype>

namespace tuxscr {

std::string baseName(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string extension(const std::string& path)
{
    const std::string name = baseName(path);
    const size_t dot = name.rfind('.');
    return dot == std::string::npos ? std::string() : name.substr(dot);
}

namespace {

int headerChecksum(const std::vector<uint8_t>& bytes)
{
    int sum = 0;
    for (size_t i = 0; i < 67; ++i)
        sum += bytes[i];
    return sum;
}

void poke16(std::vector<uint8_t>& bytes, size_t at, int value)
{
    bytes[at] = static_cast<uint8_t>(value & 0xFF);
    bytes[at + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

int peek16(const std::vector<uint8_t>& bytes, size_t at)
{
    return bytes[at] | (bytes[at + 1] << 8);
}

}  // namespace

std::vector<uint8_t> amsdosHeader(const std::string& fileName, int address, int length, int entry)
{
    std::string name = baseName(fileName);
    std::string ext = extension(fileName);
    if (!ext.empty()) {
        name = name.substr(0, name.size() - ext.size());
        ext = ext.substr(1);
        for (char& c : name)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        for (char& c : ext)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    std::vector<uint8_t> header(kAmsdosHeaderSize, 0);
    for (size_t i = 0; i < 8; ++i)
        header[1 + i] = i < name.size() ? static_cast<uint8_t>(name[i]) : ' ';
    for (size_t i = 0; i < 3; ++i)
        header[9 + i] = i < ext.size() ? static_cast<uint8_t>(ext[i]) : ' ';
    // A letter the CPC does not have is a question mark.
    for (size_t i = 1; i < 12; ++i)
        if (header[i] > 126)
            header[i] = '?';
    header[18] = 2;
    poke16(header, 19, length);
    poke16(header, 21, address);
    poke16(header, 24, length);
    poke16(header, 26, entry);
    poke16(header, 64, length);
    poke16(header, 67, headerChecksum(header));
    return header;
}

bool hasAmsdosHeader(const std::vector<uint8_t>& file)
{
    return file.size() >= kAmsdosHeaderSize && headerChecksum(file) == peek16(file, 67);
}

AmsdosHeader readAmsdosHeader(const std::vector<uint8_t>& file)
{
    AmsdosHeader header;
    if (file.size() < kAmsdosHeaderSize)
        return header;
    for (size_t i = 1; i < 12 && file[i] != 0; ++i)
        header.name += static_cast<char>(file[i]);
    header.type = file[18];
    // The file's length is at 64; ConvImgCpc writes it at 19 and 24 too,
    // which other programs leave empty.
    header.length = peek16(file, 64);
    header.address = peek16(file, 21);
    header.entry = peek16(file, 26);
    header.checksum = peek16(file, 67);
    return header;
}

std::vector<uint8_t> withAmsdosHeader(const std::string& fileName, int address, int entry, const std::vector<uint8_t>& data)
{
    std::vector<uint8_t> file = amsdosHeader(fileName, address, static_cast<int>(data.size()), entry);
    file.insert(file.end(), data.begin(), data.end());
    return file;
}

}  // namespace tuxscr
