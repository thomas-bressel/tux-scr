#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "base/bitmap.h"
#include "convert/param.h"
#include "convert/picture.h"
#include "convert/screen.h"
#include "formats/dsk.h"
#include "formats/pack.h"

// The files ConvImgCpc writes of a picture and reads back: screens (.SCR),
// packed or not, with or without the routine that shows them; assembler
// sources; palettes (.PAL, .KIT). Nothing here touches the disc: files are
// given and returned as bytes or text.

namespace tuxscr {

// ---- Screens ----

// A screen as a file of the CPC's, without its AMSDOS header.
struct ScreenData {
    std::vector<uint8_t> bytes;
    int address = 0;
    int entry = 0;
};

// The picture as a screen: the memory it takes, with the palette and the
// routine that shows it put in the bytes the picture leaves unused if the
// settings ask (param.withPalette, param.withCode); packed by `method`,
// then with the routine that unpacks it after it.
ScreenData screenData(const Screen& screen, const Picture& picture, const Param& param, PackMethod method,
                      const PackProgress& progress = {});
// The same from screen memory already made (64K), for an EGX picture's two
// files.
ScreenData screenData(const Screen& screen, std::vector<uint8_t> memory, const Param& param, PackMethod method,
                      const PackProgress& progress = {});

// The screen as a file with its header; `fileName` gives the header its
// name.
std::vector<uint8_t> screenFile(const std::string& fileName, const ScreenData& data);

// Puts the screen on a disc under the first of CNVIMG00 to CNVIMG99 that is
// free (.SCR, or .CMP when packed); `name` is told which.
Dsk::Error addScreenToDsk(Dsk& dsk, const ScreenData& data, bool packed, std::string* name);

// The labels a source gives its data and its palette; an empty one is not
// written.
struct AsmLabels {
    std::string media;
    std::string palette;
};

struct AsmSource {
    std::string text;
    // What is over 16K, when the source is to be in two files.
    std::string second;
};

// What a source's first line says wrote it, and when.
struct AsmStamp {
    std::string version;  // empty: the line is left out
    std::string time;     // empty: now
};

// The packed screen as an assembler source, with the routine that unpacks
// and shows it if the settings ask. On a Plus whose palette changes at
// lines of the picture, that routine has the interrupts that change it.
AsmSource screenAsm(const Screen& screen, const Picture& picture, const Param& param, PackMethod method,
                    const AsmLabels& labels, bool inTwoFiles, const AsmStamp& stamp, const PackProgress& progress = {});

// The picture as a sprite's bytes in a source, as they are or packed.
std::string spriteAsm(const Screen& screen, const Picture& picture, const Param& param, const AsmLabels& labels,
                      const AsmStamp& stamp, PackMethod method = PackMethod::None, const PackProgress& progress = {});

// The picture as a matrix of 64 by 64, one pen for each block of 8 by 4,
// packed, in a source ("Test" in ConvImgCpc's window).
std::string matrixAsm(const Screen& screen, const Picture& picture, const Param& param, PackMethod method,
                      const AsmStamp& stamp);
// The picture as the data of a bump effect.
std::string bumpAsm(const Screen& screen, const Picture& picture, const Param& param, const AsmStamp& stamp);
// The pictures of an animation as the data of a tunnel effect: each one's
// colours averaged over blocks of the screen, then the table of them.
std::string tunnelAsm(const Screen& screen, const std::vector<Picture>& pictures);
// What changes from one picture of an animation to the next, in a source:
// the runs of bytes of the screen's memory that differ, each with its
// address and its length.
std::string diffImagesAsm(const Screen& screen, const std::vector<Picture>& pictures, const AsmStamp& stamp);

// A block of the picture, `width` pixels by `height` lines at (x, y), given
// as its bytes: as an OCP window's data (its bytes, then its size), or in a
// source with `bytesPerLine` bytes a line of DB.
std::vector<uint8_t> blockWindow(const std::vector<uint8_t>& bytes, int width, int height);
std::string blockAsm(const Screen& screen, const std::vector<uint8_t>& bytes, int width, int height, int x, int y,
                     int bytesPerLine, const AsmStamp& stamp);

// The picture cut into tiles of `width` pixels by `height` lines, row after
// row or column after column, in a source: each tile's bytes, or all of
// them packed with `pack` (None: not packed); the palette after if the
// settings ask.
struct TilesetOptions {
    int width = 8;
    int height = 8;
    bool byRows = true;
    bool packed = false;
    PackMethod pack = PackMethod::None;
};
std::string tilesetAsm(const Screen& screen, const Picture& picture, const Param& param, const AsmLabels& labels,
                       const TilesetOptions& options);

// ---- Reading a screen back ----

struct LoadedScreen {
    bool ok = false;
    // The picture as a PC shows it, to be converted again.
    Bitmap shown;
};

// Reads a screen ConvImgCpc or OCP Art Studio saved: `data` is the file
// without its header. The screen is given the size, the mode and the
// palette the file has, when it says them, and `picture` its pixels.
LoadedScreen loadScreen(const std::vector<uint8_t>& data, Screen& screen, Picture& picture);
// A screen of Impdraw's (an overscan one saved by its viewer): the whole
// file, header included.
LoadedScreen loadImpdrawScreen(const std::vector<uint8_t>& file, Screen& screen, Picture& picture);
// Bytes that are a sprite, `columns` bytes wide and `lines` high, shown in
// the screen's mode; `realWidth`, if given, is its width in pixels.
Bitmap drawSprite(const std::vector<uint8_t>& bytes, int columns, int lines, const Screen& screen, Picture* picture,
                  int realWidth = 0);

// ---- Palettes ----

// The palette as a file of OCP Art Studio's (.PAL), with its header.
std::vector<uint8_t> paletteFile(const std::string& fileName, const Screen& screen, const Param& param);
// Reads one into the screen's palette; false if the file is not one.
bool readPaletteFile(const std::vector<uint8_t>& file, Screen& screen);

// A palette of the Plus (.KIT), with its header: the picture's sixteen
// colours, or the fifteen of the sprites.
std::vector<uint8_t> kitFile(const std::string& fileName, const std::array<int, 16>& colours, bool picture);
// Reads one: false if the file is not one. A file of fifteen colours leaves
// the first as it is.
bool readKitFile(const std::vector<uint8_t>& file, std::array<int, 16>& colours);

}  // namespace tuxscr
