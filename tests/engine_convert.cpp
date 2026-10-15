// The conversion, on pictures small enough to say what ConvImgCpc makes of
// them: which colours the pens get and in which order, what locking,
// leaving out and sorting do, the Plus's colours, and the palettes of the
// lines of a Plus's picture.

#include "formats/paramfile.h"
#include "check.h"
#include "engine/convert.h"

using namespace tuxscr;

namespace {

// A source for a screen: bands of colours, each so many pixels of mode 2
// wide, the same on every line.
Bitmap bands(const Screen& screen, std::initializer_list<std::pair<Rgb, int>> list)
{
    Bitmap source(screen.width(), screen.height());
    int x = 0;
    for (const auto& [colour, width] : list)
        for (int end = x + width; x < end && x < screen.width(); ++x)
            for (int y = 0; y < screen.height(); ++y)
                source.setPixel(x, y, colour);
    return source;
}

const Rgb kBlack(0, 0, 0), kBlue(0, 0, 255), kRed(255, 0, 0), kGreen(0, 255, 0), kWhite(255, 255, 255), kGrey(0x66, 0x66, 0x66);

}  // namespace

int main()
{
    // Mode 1: the four colours used most, the most used first.
    {
        Screen screen;
        Picture picture;
        Param param;
        Bitmap source = bands(screen, {{kRed, 320}, {kWhite, 160}, {kBlack, 96}, {kGreen, 64}});
        const Conversion done = convert(source, screen, picture, param);
        CHECK_EQ(done.colours, 4);
        CHECK(picture.computed);
        CHECK_EQ(screen.palette[0], 6);
        CHECK_EQ(screen.palette[1], 26);
        CHECK_EQ(screen.palette[2], 0);
        CHECK_EQ(screen.palette[3], 18);
        CHECK_EQ(picture.pen(0, 0), 0);
        CHECK_EQ(picture.pen(318, 398), 0);
        CHECK_EQ(picture.pen(320, 0), 1);
        CHECK_EQ(picture.pen(480, 200), 2);
        CHECK_EQ(picture.pen(638, 398), 3);
        // The bytes of the screen: four pixels of a pen each.
        const std::vector<uint8_t> memory = screenMemory(screen, picture);
        CHECK_EQ(memory[0], 0x00);
        CHECK_EQ(memory[40], 0xF0);
        CHECK_EQ(memory[60], 0x0F);
        CHECK_EQ(memory[79], 0xFF);
        CHECK_EQ(memory[0x800 + 79], 0xFF);
        CHECK_EQ(screen.memorySize(), 16336);

        // Sorted: by rising number, then by falling.
        param.newSortPal = 1;
        Bitmap again = bands(screen, {{kRed, 320}, {kWhite, 160}, {kBlack, 96}, {kGreen, 64}});
        convert(again, screen, picture, param);
        CHECK(screen.palette[0] == 0 && screen.palette[1] == 6 && screen.palette[2] == 18 && screen.palette[3] == 26);
        CHECK_EQ(picture.pen(0, 0), 1);
        param.newSortPal = 3;
        again = bands(screen, {{kRed, 320}, {kWhite, 160}, {kBlack, 96}, {kGreen, 64}});
        convert(again, screen, picture, param);
        CHECK(screen.palette[0] == 26 && screen.palette[1] == 18 && screen.palette[2] == 6 && screen.palette[3] == 0);
        param.newSortPal = 0;

        // A pen that is locked keeps its colour, and the others share what
        // is left; a pen left out gets none, and no pixel.
        screen.palette[0] = 2;
        param.lockState[0] = 1;
        param.disableState[1] = 1;
        again = bands(screen, {{kRed, 320}, {kWhite, 160}, {kBlack, 96}, {kGreen, 64}});
        convert(again, screen, picture, param);
        CHECK_EQ(screen.palette[0], 2);
        CHECK_EQ(screen.palette[1], kNoColour);
        CHECK_EQ(screen.palette[2], 6);
        CHECK_EQ(screen.palette[3], 26);
        CHECK_EQ(picture.pen(0, 0), 2);
        CHECK_EQ(picture.pen(320, 0), 3);
        // Black and green have no pen: each goes to the nearest there is.
        CHECK(picture.pen(480, 0) != 1 && picture.pen(600, 0) != 1);
    }

    // Colours between the CPC's are brought to the nearest of its three
    // levels: under 85, under 170, over.
    {
        Screen screen;
        Picture picture;
        Param param;
        Bitmap source = bands(screen, {{Rgb(84, 86, 200), 400}, {Rgb(171, 0, 90), 240}});
        convert(source, screen, picture, param);
        CHECK_EQ(screen.palette[0], 9 + 2);  // green at half, blue at full
        CHECK_EQ(screen.palette[1], 6 + 1);  // red at full, blue at half
    }

    // Mode 0, a picture of three colours: thirteen pens are left without.
    {
        Screen screen;
        screen.mode = kMode0;
        Picture picture;
        Param param;
        Bitmap source = bands(screen, {{kBlue, 320}, {kGrey, 200}, {kWhite, 120}});
        CHECK_EQ(convert(source, screen, picture, param).colours, 3);
        CHECK(screen.palette[0] == 2 && screen.palette[1] == 13 && screen.palette[2] == 26);
        CHECK_EQ(screen.palette[3], kNoColour);
        CHECK_EQ(picture.pen(316, 0), 0);
        CHECK_EQ(picture.pen(320, 0), 1);
        CHECK_EQ(picture.pen(636, 0), 2);
        const std::vector<uint8_t> memory = screenMemory(screen, picture);
        CHECK_EQ(memory[0], 0x00);
        CHECK_EQ(memory[40], 0xC0);  // two pixels of pen 1
        CHECK_EQ(memory[79], 0x0C);  // two pixels of pen 2
    }

    // A Plus: its colours are twelve bits, green first as ConvImgCpc keeps
    // them.
    {
        Screen screen;
        screen.plus = true;
        Picture picture;
        Param param;
        param.cpcPlus = true;
        Bitmap source = bands(screen, {{Rgb(0x12, 0xFE, 0x80), 400}, {Rgb(0xFF, 0x00, 0x10), 240}});
        convert(source, screen, picture, param);
        CHECK_EQ(screen.palette[0], plusColour(1, 15, 8));
        CHECK_EQ(screen.palette[1], plusColour(15, 0, 1));
        CHECK_EQ(asicWord(screen.palette[0]), 0xF18);
        CHECK_EQ(screen.rgb(screen.palette[0]), 0x11FF88);
    }

    // The EGX modes: lines of two modes in turn, the finer one first.
    {
        Screen screen;
        screen.mode = kModeEgx1;
        CHECK_EQ(screen.pixelWidth(0), 2);
        CHECK_EQ(screen.pixelWidth(2), 4);
        CHECK_EQ(screen.maxPen(0), 4);
        CHECK_EQ(screen.maxPen(2), 16);
        screen.yEgx = 2;
        CHECK_EQ(screen.pixelWidth(0), 4);
        CHECK_EQ(screen.pixelWidth(2), 2);
    }

    // A Plus's palette interrupts: from each one's line on, the pens it
    // defines have its colours, and the picture is converted with them.
    {
        Screen screen;
        screen.plus = true;
        Picture picture;
        Param param;
        param.cpcPlus = true;
        // The picture's own colours are red and black; white is given to
        // pen 1 from line 100 on.
        screen.palette[0] = plusColour(15, 0, 0);
        screen.palette[1] = plusColour(0, 0, 0);
        param.lockState[0] = param.lockState[1] = 1;
        param.disableState[2] = param.disableState[3] = 1;
        screen.interruptsOn = true;
        PaletteInterrupt interrupt;
        interrupt.line = 100;
        interrupt.colours[1] = plusColour(15, 15, 15);
        interrupt.defined[1] = true;
        screen.interrupts = {interrupt};
        screen.tidyInterrupts();
        CHECK(screen.interruptsActive());
        CHECK_EQ(screen.paletteAt(99)[1], plusColour(0, 0, 0));
        CHECK_EQ(screen.paletteAt(100)[1], plusColour(15, 15, 15));
        CHECK_EQ(screen.paletteAt(100)[0], plusColour(15, 0, 0));
        CHECK_EQ(screen.interrupts[0].colours[0], plusColour(15, 0, 0));
        // A band of black, a band of light grey (nearer to red than to
        // black: ConvImgCpc weighs green most), a band of red. The pens
        // are given by the picture's palette on every line: the white of
        // the interrupt colours again what pen 1 draws, and takes the grey
        // from no other pen.
        Bitmap source = bands(screen, {{Rgb(0, 0, 0), 200}, {Rgb(200, 200, 200), 200}, {kRed, 240}});
        convert(source, screen, picture, param);
        for (int line : {0, 99, 100, 199}) {
            CHECK_EQ(picture.pen(0, 2 * line), 1);
            CHECK_EQ(picture.pen(300, 2 * line), picture.pen(300, 0));
            CHECK_EQ(picture.pen(500, 2 * line), 0);
        }
        // Shown, each line has its own colours.
        const Bitmap shown = render(screen, picture);
        CHECK_EQ(shown.pixel(0, 2 * 50), 0x000000);
        CHECK_EQ(shown.pixel(0, 2 * 150), 0xFFFFFF);
        CHECK_EQ(shown.pixel(500, 2 * 150), 0xFF0000);
        CHECK_EQ(shown.pixel(300, 2 * 150), shown.pixel(300, 2 * 50));
        CHECK(!screen.interrupts[0].matched);

        // Its colours searched in its own lines: the picture's bottom half
        // is grey and red, whatever its top is.
        screen.interrupts[0].defined.fill(false);
        param.disableState.fill(0);
        Bitmap again = bands(screen, {{Rgb(200, 200, 200), 320}, {kRed, 320}});
        convert(again, screen, picture, param, 0);
        const PaletteInterrupt& found = screen.interrupts[0];
        CHECK(found.defined[0] && found.defined[1]);
        CHECK((found.colours[0] == plusColour(12, 12, 12) && found.colours[1] == plusColour(15, 0, 0))
              || (found.colours[1] == plusColour(12, 12, 12) && found.colours[0] == plusColour(15, 0, 0)));
        // Its lines are converted with what was found, the others are not.
        CHECK(found.matched && found.matchColours == found.colours);
        CHECK_EQ(found.colours[static_cast<size_t>(picture.pen(0, 2 * 150))], plusColour(12, 12, 12));
        CHECK_EQ(found.colours[static_cast<size_t>(picture.pen(400, 2 * 150))], plusColour(15, 0, 0));
        CHECK_EQ(picture.pen(400, 2 * 50), 0);
        // A colour chosen by hand after that moves no pixel either.
        const int greyPen = picture.pen(0, 2 * 150);
        screen.interrupts[0].colours[static_cast<size_t>(greyPen)] = plusColour(0, 0, 15);
        Bitmap third = bands(screen, {{Rgb(200, 200, 200), 320}, {kRed, 320}});
        convert(third, screen, picture, param);
        CHECK_EQ(picture.pen(0, 2 * 150), greyPen);
        CHECK_EQ(render(screen, picture).pixel(0, 2 * 150), 0x0000FF);

        // The settings file keeps the interrupts, the colours found for
        // one and those chosen since.
        {
            Param readParam;
            Screen readScreen;
            CHECK(readParamXml(paramXml(param, screen), readParam, readScreen));
            CHECK(readScreen.interruptsOn);
            CHECK_EQ(static_cast<int>(readScreen.interrupts.size()), 1);
            if (readScreen.interrupts.size() == 1) {
                const PaletteInterrupt& read = readScreen.interrupts[0];
                CHECK_EQ(read.line, 100);
                CHECK(read.matched && read.matchColours == screen.interrupts[0].matchColours);
                CHECK_EQ(read.colours[static_cast<size_t>(greyPen)], plusColour(0, 0, 15));
                CHECK(read.defined[0] && read.defined[1]);
            }
            Screen plain = screen;
            plain.interrupts[0].matched = false;
            CHECK(readParamXml(paramXml(param, plain), readParam, readScreen));
            CHECK(readScreen.interrupts.size() == 1 && !readScreen.interrupts[0].matched);
        }

        // Off a Plus, or out of modes 0 to 2, they do not apply.
        screen.plus = false;
        CHECK(!screen.interruptsActive());
        screen.plus = true;
        screen.mode = kModeEgx1;
        CHECK(!screen.interruptsActive());
        // Lines are kept inside the picture and in order.
        screen.mode = kMode1;
        PaletteInterrupt late, early;
        late.line = 500;
        early.line = -3;
        screen.interrupts = {late, early};
        screen.tidyInterrupts();
        CHECK(screen.interrupts[0].line == 1 && screen.interrupts[1].line == 199);
    }
    return checkSummary("engine_convert");
}
