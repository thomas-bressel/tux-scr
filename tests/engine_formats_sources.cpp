// The sources and files of effects the engine writes from pictures: an
// animation's differences and its tunnel, a block of the picture, its
// tiles; on pictures made by the test.

#include <string>
#include <vector>

#include "check.h"
#include "formats/screenfile.h"

using namespace tuxscr;

namespace {

bool has(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

}  // namespace

int main()
{
    Screen screen;  // mode 1, 80 bytes by 200 lines
    std::vector<Picture> pictures(2);
    pictures[1].setPixel(0, 0, 3, 2);  // the first byte changes, no other

    // ---- What changes from one picture to the next ----
    {
        const std::string text = diffImagesAsm(screen, pictures, AsmStamp{"TuxSCR 0.1", "now"});
        CHECK(has(text, "\tnolist") && has(text, "DiffImage_1:"));
        CHECK(has(text, "\tDW\t#0000\t;adresse\n") && has(text, "\tDB\t#01\t;nbre octets\n"));
        CHECK(has(text, "\tDW\t#0000\t;fin de table") && has(text, "\tDW\t#0000;\tFin anim"));
        CHECK(!has(text, "DiffImage_0") && !has(text, "DiffImage_2"));
    }
    // ---- The tunnel: a table of colours each, then the table of them ----
    {
        const std::string text = tunnelAsm(screen, pictures);
        CHECK(has(text, "DataImage000\n") && has(text, "DataImage001\n") && has(text, "\nAnim\n"));
        CHECK(has(text, "\tDW\tDataImage000\n\tDW\tDataImage001\n\tDW\t0"));
    }
    // ---- A block of the picture ----
    {
        const std::vector<uint8_t> bytes = {1, 2, 3, 4};
        CHECK(blockWindow(bytes, 16, 4) == (std::vector<uint8_t>{1, 2, 3, 4, 16, 0, 2, 0}));
        const std::string text = blockAsm(screen, bytes, 16, 4, 8, 6, 2, AsmStamp{"TuxSCR 0.1", "now"});
        CHECK(has(text, ";Width:16, Height:4\n;Posx:8, Posy:6\n"));
        CHECK(has(text, "#01,#02") && has(text, "#03,#04"));
    }
    // ---- Its tiles ----
    {
        Param param;
        param.withPalette = false;
        const Picture blank;
        TilesetOptions options;
        options.width = 64;
        options.height = 40;
        const std::string text = tilesetAsm(screen, blank, param, AsmLabels{"Tiles", ""}, options);
        // 640 by 400 cut into 10 by 10 tiles of 160 bytes, each after a
        // line of its own.
        CHECK(has(text, "Tiles\n\tDS\t160,#00\n;\n") && has(text, "; Taille totale 16000 octets"));
        options.packed = true;
        options.pack = PackMethod::None;
        const std::string together = tilesetAsm(screen, blank, param, AsmLabels{"Tiles", ""}, options);
        // Packed (here as they are), they are all of a piece.
        CHECK(has(together, "Tiles\n\tDS\t16000,#00\n") && !has(together, "\n;\n"));
    }
    return checkSummary("engine_formats_sources");
}
