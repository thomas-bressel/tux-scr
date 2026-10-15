// The command-line program, run as a user runs it: pictures converted with
// its options, and the files it writes beside them.

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QTemporaryDir>

#include "check.h"
#include "formats/amsdos.h"
#include "formats/pack.h"

using namespace tuxscr;

namespace {

struct Ran {
    int status = -1;
    QByteArray out, err;
};

Ran run(const QStringList& arguments)
{
    QProcess process;
    process.start(QStringLiteral(TUXSCR_PROGRAM), arguments);
    Ran ran;
    if (!process.waitForFinished(120000))
        return ran;
    ran.status = process.exitCode();
    ran.out = process.readAllStandardOutput();
    ran.err = process.readAllStandardError();
    return ran;
}

std::vector<uint8_t> fileOf(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray all = file.readAll();
    return std::vector<uint8_t>(all.begin(), all.end());
}

}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir scratch;
    const QDir folder(scratch.path());
    // Two pictures: one red, one half red and half white.
    QImage red(320, 200, QImage::Format_RGB32);
    red.fill(QColor(255, 0, 0));
    CHECK(red.save(folder.filePath("one.png")));
    for (int y = 0; y < 200; ++y)
        for (int x = 160; x < 320; ++x)
            red.setPixelColor(x, y, QColor(255, 255, 255));
    CHECK(red.save(folder.filePath("two.png")));

    // With nothing, what it is and how it is used.
    Ran ran = run({});
    CHECK_EQ(ran.status, 2);
    CHECK(ran.out.startsWith("tuxscr ") && ran.out.contains("-F<files>"));
    CHECK_EQ(run({"-Q"}).status, 2);

    // A screen of mode 0 for each picture of a pattern.
    ran = run({"-M0", "-F" + folder.filePath("*.png")});
    CHECK_EQ(ran.status, 0);
    CHECK(ran.err.isEmpty());
    const std::vector<uint8_t> one = fileOf(folder.filePath("one.SCR")), two = fileOf(folder.filePath("two.SCR"));
    CHECK(one.size() == 128 + 16336 && two.size() == 128 + 16336);
    CHECK(hasAmsdosHeader(one) && readAmsdosHeader(one).name == "ONE     SCR");
    // Mode 0, and the routine that shows it.
    CHECK(one.size() > 128 + 0x17D0 && one[128 + 0x17D0] == 0 && one[128 + 0x7D0] == 0x3A);
    // The picture's one colour is red, whatever pen has it.
    if (one.size() > 128 + 0x17E0) {
        const int pen = (one[128] >> 7) | ((one[128] >> 2) & 2) | ((one[128] >> 3) & 4) | ((one[128] << 2) & 8);
        CHECK_EQ(one[128 + 0x17D1 + static_cast<size_t>(pen)], 6);
        CHECK_EQ(one[128], one[128 + 79]);
    }
    CHECK(two.size() > 200 && two[128] != two[128 + 79]);

    // Packed, on a Plus, in overscan: what is unpacked is a screen of 96 by
    // 272 with the Plus's palette in it.
    ran = run({"-M1", "-U1", "-Z1", "-K3", "-F" + folder.filePath("two.png")});
    CHECK_EQ(ran.status, 0);
    const std::vector<uint8_t> packed = fileOf(folder.filePath("two.CMP"));
    CHECK(hasAmsdosHeader(packed));
    if (packed.size() > 128) {
        const std::vector<uint8_t> unpacked = unpackZx1(std::vector<uint8_t>(packed.begin() + 128, packed.end()));
        CHECK_EQ(unpacked.size(), 0x7CC0);
        CHECK(unpacked.size() > 0x622 && unpacked[0x600] == (1 | 0x8C) && unpacked[0x621] == 0xF3);
    }

    // Sources, the palette of the first picture kept for the second.
    ran = run({"-A", "-L", "-M1", "-F" + folder.filePath("two.png"), folder.filePath("one.png")});
    CHECK_EQ(ran.status, 0);
    QFile source(folder.filePath("one.ASM"));
    CHECK(source.open(QIODevice::ReadOnly));
    const QByteArray text = source.readAll();
    CHECK(text.contains("\none\n\tDS\t16000,#") && text.contains("\nPaletteone\n\tDB\t#"));

    // A file that is not there is said, and the others are still converted.
    ran = run({"-F" + folder.filePath("none.png"), "-F" + folder.filePath("one.png")});
    CHECK_EQ(ran.status, 1);
    CHECK(ran.err.contains("none.png"));
    return checkSummary("cli_convert");
}
