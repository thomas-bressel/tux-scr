// tuxscr: ConvImgCpc's command line. Its options are read in turn, each
// setting something of the conversion; -F converts the files it names with
// what was set before it, and writes each beside its picture.
//
// It is made of the engine alone, and of Qt's pictures to read the PC's: it
// has no window, and speaks English.

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringList>

#include "base/version.h"
#include "convert/convert.h"
#include "convert/dither.h"
#include "formats/amsdos.h"
#include "formats/animfile.h"
#include "formats/pack.h"
#include "formats/screenfile.h"
#include "pictures.h"

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

std::optional<std::vector<uint8_t>> readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    const QByteArray bytes = file.readAll();
    return std::vector<uint8_t>(bytes.begin(), bytes.end());
}

bool writeFile(const QString& path, const std::string& bytes)
{
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly)
           && file.write(bytes.data(), static_cast<qint64>(bytes.size())) == static_cast<qint64>(bytes.size()) && file.commit();
}

bool writeFile(const QString& path, const std::vector<uint8_t>& bytes)
{
    return writeFile(path, std::string(bytes.begin(), bytes.end()));
}

// What the options have set, and what -F does with it.
struct Run {
    Screen screen;
    Param param;
    PackMethod packMethod = PackMethod::None;
    bool asAssembler = false;
    bool lockFirst = false;
    int status = 0;

    void fail(const QString& text)
    {
        std::fprintf(stderr, "tuxscr: %s\n", qPrintable(text));
        status = 1;
    }

    // The file read and converted, and written beside it.
    void convertFile(const QString& file)
    {
        std::printf("%s\n", qPrintable(QDir::toNativeSeparators(file)));
        const std::optional<std::vector<uint8_t>> bytes = readFile(file);
        if (!bytes || bytes->empty()) {
            fail(QStringLiteral("%1 cannot be read").arg(QDir::toNativeSeparators(file)));
            return;
        }
        // A screen of the CPC's is converted again from what it shows; a
        // picture of the PC's, from its first frame.
        Picture picture;
        QImage source;
        if (hasAmsdosHeader(*bytes)) {
            const AmsdosHeader header = readAmsdosHeader(*bytes);
            const bool impdraw = header.name.size() >= 3 && header.name.compare(header.name.size() - 3, 3, "SCR") == 0
                                 && header.type == 0 && header.address == 0x170;
            Screen read = screen;
            const std::vector<uint8_t> data(bytes->begin() + kAmsdosHeaderSize, bytes->end());
            const LoadedScreen loaded = impdraw ? loadImpdrawScreen(*bytes, read, picture) : loadScreen(data, read, picture);
            if (loaded.ok) {
                screen = read;
                source = toQImage(loaded.shown);
            }
        } else {
            const QList<QImage> pictures =
                readPictures(QByteArray(reinterpret_cast<const char*>(bytes->data()), static_cast<qsizetype>(bytes->size())));
            if (!pictures.isEmpty())
                source = pictures.first();
        }
        if (source.isNull()) {
            fail(QStringLiteral("%1 is not a picture that can be read").arg(QDir::toNativeSeparators(file)));
            return;
        }
        if (param.sizeMode == Param::SizeMode::Origin) {
            param.sizeX = source.width();
            param.sizeY = source.height();
            param.posX = param.posY = 0;
        }
        picture.clear();
        screenIntoParam(screen, param);
        Bitmap resized = resizeSource(source, screen, param);
        tuxscr::convert(resized, screen, picture, param);

        const QFileInfo info(file);
        const QString start = info.dir().filePath(info.completeBaseName());
        // The labels ConvImgCpc's window proposes: the file's name.
        const AsmLabels labels{info.completeBaseName().toStdString(), ("Palette" + info.completeBaseName()).toStdString()};
        const AsmStamp stamp{std::string("TuxSCR ") + versionString(), {}};
        QString written;
        bool ok = false;
        if (packMethod == PackMethod::None && asAssembler) {
            written = start + QStringLiteral(".ASM");
            ok = writeFile(written, spriteAsm(screen, picture, param, labels, stamp));
        } else if (packMethod == PackMethod::None) {
            written = start + QStringLiteral(".SCR");
            ok = writeFile(written, screenFile(written.toStdString(), screenData(screen, picture, param, PackMethod::None)));
        } else if (asAssembler && screen.mode >= kModeAscUt) {
            // A picture of an ASCII mode is its characters and the routine
            // that draws them: an animation of one frame.
            written = start + QStringLiteral(".ASM");
            AnimOptions options;
            options.singlePicture = true;
            ok = writeFile(written, animationAsm(screen, {&picture}, {100}, param, packMethod, options, stamp).text);
        } else if (asAssembler) {
            written = start + QStringLiteral(".ASM");
            ok = writeFile(written, screenAsm(screen, picture, param, packMethod, labels, false, stamp).text);
        } else {
            written = start + QStringLiteral(".CMP");
            ok = writeFile(written, screenFile(written.toStdString(), screenData(screen, picture, param, packMethod)));
        }
        if (ok)
            std::printf("  -> %s\n", qPrintable(QDir::toNativeSeparators(written)));
        else
            fail(QStringLiteral("%1 cannot be written").arg(QDir::toNativeSeparators(written)));
        if (lockFirst)
            param.lockState.fill(1);
    }

    bool readPalette(const QString& path)
    {
        const std::optional<std::vector<uint8_t>> file = readFile(path);
        if (!file || !readPaletteFile(*file, screen)) {
            fail(QStringLiteral("the palette %1 cannot be read").arg(QDir::toNativeSeparators(path)));
            return false;
        }
        screen.tidyInterrupts();
        return true;
    }
};

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

    Run run;
    Param& param = run.param;
    Screen& screen = run.screen;
    screen.copyPatterns(0);
    param.withCode = true;
    param.withPalette = true;

    for (const QString& argument : arguments) {
        if (!argument.startsWith(QLatin1Char('-')) || argument.size() < 2) {
            // A file after -F: a shell that reads patterns itself gives them
            // one by one.
            run.convertFile(argument);
            continue;
        }
        const QString value = argument.mid(2);
        int v = 0;
        bool understood = true;
        switch (argument[1].toUpper().unicode()) {
        case 'A':
            run.asAssembler = true;
            break;
        case 'B':
            understood = number(value, 0, 800, param.pctBlue);
            break;
        case 'C':
            if (run.readPalette(value)) {
                bool oneLocked = false;
                for (const int lock : param.lockState)
                    oneLocked = oneLocked || lock != 0;
                if (!oneLocked)
                    param.lockState.fill(1);
            }
            break;
        case 'F': {
            const QFileInfo pattern(value);
            const QStringList found = pattern.dir().entryList({pattern.fileName()}, QDir::Files, QDir::Name);
            if (found.isEmpty())
                run.fail(QStringLiteral("no file is %1").arg(value));
            for (const QString& file : found)
                run.convertFile(pattern.dir().filePath(file));
            break;
        }
        case 'I':
            param.sizeMode = Param::SizeMode::KeepSmaller;
            break;
        case 'K':
            if ((understood = number(value, 0, 4, v))) {
                const PackMethod methods[5] = {PackMethod::None, PackMethod::Standard, PackMethod::ZX0, PackMethod::ZX1, PackMethod::ZX0_V2};
                run.packMethod = methods[v];
            }
            break;
        case 'L':
            run.lockFirst = true;
            break;
        case 'M':
            understood = number(value, 0, 10, screen.mode);
            break;
        case 'P':
            understood = number(value, 0, 400, param.pct);
            break;
        case 'R':
            understood = number(value, 0, 800, param.pctRed);
            break;
        case 'S':
            param.sizeMode = Param::SizeMode::KeepLarger;
            break;
        case 'T':
            if ((understood = number(value, 0, ditherCount, v))) {
                param.method = v == 0 ? "Aucun" : ditherNames()[static_cast<size_t>(v - 1)];
                if (v > 0 && param.pct == 0)
                    param.pct = 100;
            }
            break;
        case 'U':
            if ((understood = number(value, 0, 1, v)) && screen.plus != (v != 0))
                changeMachine(screen, v != 0);
            break;
        case 'V':
            understood = number(value, 0, 800, param.pctGreen);
            break;
        case 'X':
            param.diffErr = true;
            break;
        case 'Z':
            if ((understood = number(value, 0, 1, v))) {
                screen.lines = v ? 272 : 200;
                screen.columns = v ? 96 : 80;
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
    return run.status;
}
