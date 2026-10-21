#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tuxscr {

inline constexpr size_t kAmsdosHeaderSize = 128;

// What the header AMSDOS puts before a file says of it.
struct AmsdosHeader {
    std::string name;  // eight characters of name and three of extension
    int type = 2;      // 0 BASIC, 2 binary
    int address = 0;
    int length = 0;
    int entry = 0;
    int checksum = 0;
};

// The header of a binary file, as ConvImgCpc writes it: the name is the
// file's on the PC, in capitals, cut to 8.3.
std::vector<uint8_t> amsdosHeader(const std::string& fileName, int address, int length, int entry);

// Whether a file starts with a header: 128 bytes whose checksum is right.
bool hasAmsdosHeader(const std::vector<uint8_t>& file);
AmsdosHeader readAmsdosHeader(const std::vector<uint8_t>& file);

// A file with its header before it.
std::vector<uint8_t> withAmsdosHeader(const std::string& fileName, int address, int entry, const std::vector<uint8_t>& data);

// The name of a file without its folders, and its extension with its dot.
std::string baseName(const std::string& path);
std::string extension(const std::string& path);

}  // namespace tuxscr
