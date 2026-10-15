// tuxscr: ConvImgCpc's command line. Its options are read in turn, each
// setting something of the conversion; -F converts the files it names with
// what was set before it, and writes each beside its picture.

#include <cstdio>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

#include "base/version.h"
#include "document.h"
#include "engine/dither.h"
#include "texts.h"

using namespace tuxscr;

namespace {

const char* const kUsage =
    "tuxscr %s - converts pictures into Amstrad CPC screens (ConvImgCpc's command line)\n"
    "\n"
    "  tuxscr [options] -F<files> [more files]\n"
    "\n"
    "Options are read in turn; -F converts with what was set before it, and writes each\n"
    "file beside its picture: .SCR, .CMP when packed, .ASM with -A.\n"
    "\n"
    "  -F<files>   the pictures to convert: a file, or a pattern such as 'pics/*.png'\n"
    "  -M<mode>    0 to 10: modes 0, 1, 2, EGX1, EGX2, X, split, ASC-UT, ASC0, ASC1, ASC2\n"
    "  -Z<0|1>     0: a screen of 80 by 200 bytes, 1: overscan, 96 by 272\n"
    "  -U<0|1>     0: a CPC's palette, 1: a Plus's\n"
    "  -I          keep the picture's proportions, all of it shown (keep smaller)\n"
    "  -S          keep the picture's proportions, the screen filled (keep larger)\n"
    "  -T<n>       dithering: 0 none, 1 Floyd-Steinberg, 2 Bayer 1... (%d of them)\n"
    "  -P<pct>     how much of the dithering, 0 to 400\n"
    "  -X          the dithering spreads its error\n"
    "  -R<pct> -V<pct> -B<pct>   red, green and blue, 0 to 800 (100: as they are)\n"
    "  -C<file>    a palette (.PAL) to convert with\n"
    "  -L          the palette found for the first picture is kept for the others\n"
    "  -K<n>       packing: 0 none, 1 standard, 2 ZX0, 3 ZX1, 4 ZX0 second format\n"
    "  -A          write assembler sources\n"
    "  -help       this text\n";

bool number(const QString& text, int low, int high, int& value)
{
    bool ok = false;
    const int read = text.toInt(&ok);
    if (!ok || read < low || read > high)
        return false;
    value = read;
    return true;
}

}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList arguments = app.arguments().mid(1);
    const int ditherCount = static_cast<int>(ditherNames().size());
    if (arguments.isEmpty() || arguments.contains(QLatin1String("-help"), Qt::CaseInsensitive)
        || arguments.contains(QLatin1String("--help"))) {
        std::printf(kUsage, versionString(), ditherCount);
        return arguments.isEmpty() ? 2 : 0;
    }

    texts::setLanguage(QStringLiteral("EN"));
    Document document;
    int status = 0;
    QObject::connect(&document, &Document::failed, [&status](const QString& text) {
        std::fprintf(stderr, "tuxscr: %s\n", qPrintable(text));
        status = 1;
    });
    document.param.withCode = true;
    document.param.withPalette = true;
    document.packMethod = PackMethod::None;
    bool asAssembler = false, lockFirst = false;

    const auto convertFile = [&](const QString& file) {
        std::printf("%s\n", qPrintable(QDir::toNativeSeparators(file)));
        if (!document.readFile(file))
            return;
        document.convert(true, true);
        const QFileInfo info(file);
        const QString start = info.dir().filePath(info.completeBaseName());
        // The labels ConvImgCpc's window proposes: the file's name.
        const AsmLabels labels{info.completeBaseName().toStdString(), ("Palette" + info.completeBaseName()).toStdString()};
        QString written;
        bool ok;
        if (asAssembler && document.packMethod == PackMethod::None) {
            written = start + QStringLiteral(".ASM");
            ok = document.saveSprite(written, labels, false);
        } else if (!asAssembler && document.packMethod == PackMethod::None) {
            written = start + QStringLiteral(".SCR");
            ok = document.saveScreen(written, false);
        } else {
            written = start + (asAssembler ? QStringLiteral(".ASM") : QStringLiteral(".CMP"));
            ok = document.savePacked(written, labels, asAssembler ? 1 : 0);
        }
        if (ok)
            std::printf("  -> %s\n", qPrintable(QDir::toNativeSeparators(written)));
        if (lockFirst)
            document.param.lockState.fill(1);
    };

    for (const QString& argument : arguments) {
        if (!argument.startsWith(QLatin1Char('-')) || argument.size() < 2) {
            // A file after -F: a shell that reads patterns itself gives them
            // one by one.
            convertFile(argument);
            continue;
        }
        const QString value = argument.mid(2);
        int v = 0;
        bool understood = true;
        switch (argument[1].toUpper().unicode()) {
        case 'A':
            asAssembler = true;
            break;
        case 'B':
            understood = number(value, 0, 800, document.param.pctBlue);
            break;
        case 'C':
            if (document.readPalette(value)) {
                bool oneLocked = false;
                for (const int lock : document.param.lockState)
                    oneLocked = oneLocked || lock != 0;
                if (!oneLocked)
                    document.param.lockState.fill(1);
            }
            break;
        case 'F': {
            const QFileInfo pattern(value);
            const QStringList found = pattern.dir().entryList({pattern.fileName()}, QDir::Files, QDir::Name);
            if (found.isEmpty()) {
                std::fprintf(stderr, "tuxscr: no file is %s\n", qPrintable(value));
                status = 1;
            }
            for (const QString& file : found)
                convertFile(pattern.dir().filePath(file));
            break;
        }
        case 'I':
            document.param.sizeMode = Param::SizeMode::KeepSmaller;
            break;
        case 'K':
            if ((understood = number(value, 0, 4, v))) {
                const PackMethod methods[5] = {PackMethod::None, PackMethod::Standard, PackMethod::ZX0, PackMethod::ZX1, PackMethod::ZX0_V2};
                document.packMethod = methods[v];
            }
            break;
        case 'L':
            lockFirst = true;
            break;
        case 'M':
            understood = number(value, 0, 10, document.screen.mode);
            break;
        case 'P':
            understood = number(value, 0, 400, document.param.pct);
            break;
        case 'R':
            understood = number(value, 0, 800, document.param.pctRed);
            break;
        case 'S':
            document.param.sizeMode = Param::SizeMode::KeepLarger;
            break;
        case 'T':
            if ((understood = number(value, 0, ditherCount, v))) {
                document.param.method = v == 0 ? "Aucun" : ditherNames()[static_cast<size_t>(v - 1)];
                if (v > 0 && document.param.pct == 0)
                    document.param.pct = 100;
            }
            break;
        case 'U':
            if ((understood = number(value, 0, 1, v)))
                document.setPlus(v != 0);
            break;
        case 'V':
            understood = number(value, 0, 800, document.param.pctGreen);
            break;
        case 'X':
            document.param.diffErr = true;
            break;
        case 'Z':
            if ((understood = number(value, 0, 1, v))) {
                document.screen.lines = v ? 272 : 200;
                document.screen.columns = v ? 96 : 80;
            }
            break;
        default:
            understood = false;
            break;
        }
        if (!understood) {
            std::fprintf(stderr, "tuxscr: %s is not understood (-help lists the options)\n", qPrintable(argument));
            return 2;
        }
    }
    return status;
}
