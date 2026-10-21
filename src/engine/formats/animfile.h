#pragma once

#include <string>
#include <vector>

#include "convert/param.h"
#include "convert/picture.h"
#include "convert/screen.h"
#include "formats/pack.h"
#include "formats/screenfile.h"

namespace tuxscr {

// What ConvImgCpc's "save animation" window is set to (its SaveAnim).
struct AnimOptions {
    int startAddress = 0x200;
    int maxAddress = 0;         // 0: none set
    bool loop = false;          // back to the first picture after the last
    bool memory128K = false;    // the frames go on into the second 64K
    bool withDelay = false;     // each frame waits its time
    int lineStep = 1;           // every line, or one in 2, 4 or 8
    bool twoZones = false;      // the screen's two halves apart
    bool verticalZones = false; // left and right rather than top and bottom
    bool byColumns = false;     // column after column
    int method = 0;             // 0 a window of what changed, 1 the bytes that changed, 2 blocks
    // The ASCII modes: each frame whole or as what changed, whichever is
    // shorter; always what changed; always whole.
    enum class Ascii { Shortest, Changes, Whole };
    Ascii ascii = Ascii::Shortest;
    bool rawData = false;       // the frames as they are, no program
    bool singlePicture = false; // one picture of an ASCII mode, not an animation
};

// What ConvImgCpc says while it saves an animation: the windows word it.
struct AnimNote {
    enum Kind {
        Started,      // the saving begins
        BytesLost,    // a frame packed takes `bytes` more than unpacked
        TotalLength,  // the frames take `bytes` in all
        Overflow,     // more than the memory holds
        Saved,
    };
    Kind kind = Started;
    int bytes = 0;
};

struct AnimSource {
    std::string text;
    int length = 0;               // the frames' bytes in all
    bool overflow = false;        // more than the memory holds
    std::vector<AnimNote> notes;  // in the order they came
};

// The frames as an assembler source: each as what changes from the one
// before, packed, with the program that plays them if the settings ask.
// `times` is how long each frame stays, in milliseconds.
AnimSource animationAsm(const Screen& screen, const std::vector<const Picture*>& frames, const std::vector<int>& times,
                        const Param& param, PackMethod method, const AnimOptions& options, const AsmStamp& stamp,
                        const std::string& labelPalette = "Palette", const PackProgress& progress = {});

// Pictures of at most 256 colours as an animated GIF that plays for ever.
// Each is `width` by `height` pixels, &RRGGBB, and stays its time (in
// milliseconds).
std::vector<uint8_t> animatedGif(const std::vector<Bitmap>& frames, const std::vector<int>& times);

}  // namespace tuxscr
