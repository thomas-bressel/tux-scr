#include "document.h"

#include <QDir>
#include <QFileInfo>

#include "base/version.h"
#include "formats/amsdos.h"
#include "formats/asmgen.h"
#include "formats/paramfile.h"
#include "pictures.h"
#include "prompts.h"
#include "texts.h"

namespace tuxscr {

namespace {

QImage blankSource()
{
    QImage blank(768, 544, QImage::Format_RGB32);
    blank.fill(Qt::black);
    return blank;
}

std::string local(const QString& path)
{
    return path.toStdString();
}

QString withoutExtension(const QString& path)
{
    const QFileInfo info(path);
    return info.dir().filePath(info.completeBaseName());
}

}  // namespace

Document::Document(QObject* parent)
    : QObject(parent)
{
    frames_.resize(1);
    frames_[0].source = blankSource();
    screen.copyPatterns(0);
}

tuxscr::AsmStamp Document::stamp()
{
    return {std::string("TuxSCR ") + versionString(), {}};
}

tuxscr::PackProgress Document::progress()
{
    return [this](int percent) { emit packing(percent); };
}

void Document::fail(const QString& text)
{
    emit said(tr("Error - %1").arg(text));
    emit failed(text);
}

bool Document::write(const QString& path, const std::vector<uint8_t>& bytes)
{
    if (writeHostFile(path, bytes))
        return true;
    fail(tr("Cannot write %1").arg(QDir::toNativeSeparators(path)));
    return false;
}

bool Document::write(const QString& path, const std::string& text)
{
    return write(path, std::vector<uint8_t>(text.begin(), text.end()));
}

// What the screen is, said to the settings too: files keep them there.
void Document::syncParam()
{
    screenIntoParam(screen, param);
}

// ---- Frames ----

void Document::select(int frame, bool quiet)
{
    if (frame < 0 || frame >= frameCount())
        return;
    current_ = frame;
    if (!quiet && frameCount() > 1)
        say(texts::text("Main.prg.TxtInfo7") + QString::number(frame));
    emit framesChanged();
    emit changed();
}

void Document::setSources(const QList<QImage>& pictures, int time)
{
    frames_.clear();
    for (const QImage& picture : pictures) {
        frames_.emplace_back();
        frames_.back().source = picture;
        frames_.back().time = time > 0 ? time : 100;
    }
    if (frames_.empty()) {
        frames_.emplace_back();
        frames_.back().source = blankSource();
    }
    current_ = 0;
    frameTime_ = time;
    emit framesChanged();
}

void Document::setBlankFrames(int count, int time)
{
    QList<QImage> blanks;
    for (int n = 0; n < std::max(1, count); ++n)
        blanks << blankSource();
    setSources(blanks, time);
}

void Document::importSource(const QImage& picture, int frame)
{
    if (frame < 0 || frame >= frameCount() || picture.isNull())
        return;
    frames_[static_cast<size_t>(frame)].source = picture;
    emit framesChanged();
}

void Document::removeFrame(int frame)
{
    if (frameCount() < 2 || frame < 0 || frame >= frameCount())
        return;
    frames_.erase(frames_.begin() + frame);
    current_ = std::min(current_, frameCount() - 1);
    emit framesChanged();
    emit changed();
}

// ---- Conversion ----

void Document::convert(bool force, bool quiet, int searchInterrupt)
{
    syncParam();
    if (param.autoRecalc || force) {
        const int selected = current_;
        const int first = allPictures ? 0 : selected;
        const int last = allPictures ? frameCount() - 1 : selected;
        for (int i = first; i <= last; ++i) {
            current_ = i;
            Frame& frame = frames_[static_cast<size_t>(i)];
            Bitmap resized = resizeSource(frame.source, screen, param);
            if (!quiet && force)
                say(texts::text("Main.Prg.TxtInfo1"));
            const Conversion result = tuxscr::convert(resized, screen, frame.picture, param, searchInterrupt);
            if (!quiet && force) {
                say(tr("Conversion done, colours in the picture: %1").arg(result.colours));
                if (result.splitColours > 0)
                    say(tr("Colours made with splits: %1").arg(result.splitColours));
            }
        }
        current_ = selected;
    }
    emit changed();
}

void Document::resetPictures(bool all)
{
    if (all) {
        for (Frame& frame : frames_)
            frame.picture.clear();
    } else if (!picture().computed) {
        picture().clear();
    }
}

void Document::setPlus(bool plus)
{
    if (screen.plus == plus)
        return;
    if (!plus) {
        // The colours stay where the pens have them: nothing is searched
        // again while the palette is brought back to the CPC's.
        const std::array<int, 16> locks = param.lockState;
        param.lockState.fill(0);
        changeMachine(screen, false);
        convert(false);
        param.lockState = locks;
    } else {
        changeMachine(screen, true);
        convert(false);
    }
}

QImage Document::shown() const
{
    return shown(current_);
}

QImage Document::shown(int frame) const
{
    return toQImage(render(screen, frames_[static_cast<size_t>(frame)].picture));
}

std::array<bool, 16> Document::usedPens() const
{
    std::array<bool, 16> used{};
    const Picture& shownPicture = picture();
    for (int y = 0; y < screen.height(); y += 2) {
        const int tx = screen.pixelWidth(y);
        for (int x = 0; x < screen.width(); x += tx)
            used[static_cast<size_t>(shownPicture.pen(x, y) & 15)] = true;
    }
    return used;
}

// ---- Reading ----

bool Document::readFile(const QString& path, bool single)
{
    const std::optional<std::vector<uint8_t>> read = readHostFile(path);
    if (!read || read->empty()) {
        fail(texts::text("Main.prg.TxtInfo6"));
        return false;
    }
    const std::vector<uint8_t>& bytes = *read;
    bool keepPictures = false;
    showSprites = false;
    if (hasAmsdosHeader(bytes)) {
        const AmsdosHeader header = readAmsdosHeader(bytes);
        const auto endsWith = [&header](const char* ext) { return header.name.size() >= 3 && header.name.compare(header.name.size() - 3, 3, ext) == 0; };
        const bool impdrawScreen = endsWith("SCR") && header.type == 0 && header.address == 0x170;
        bool animation = false;
        int count = 1, width = 1, height = 1;
        if (!impdrawScreen && endsWith("IMP") && bytes.size() > 131) {
            // Sprites one after the other, then their width in bytes, their
            // height and how many they are.
            const size_t l = bytes.size();
            width = bytes[l - 3];
            height = bytes[l - 2];
            count = bytes[l - 1];
            animation = count > 0 && static_cast<int>(l) - 131 == count * width * height;
        }
        const std::vector<uint8_t> data(bytes.begin() + kAmsdosHeaderSize, bytes.end());
        if (animation) {
            setBlankFrames(count, frameTime_);
            say(tr("Animation creation (IMP) with %n picture(s).", nullptr, count));
            screen.setWidth(width << 3);
            screen.setHeight(height << 1);
            size_t at = 0;
            for (int i = 0; i < count; ++i) {
                const std::vector<uint8_t> sprite(data.begin() + static_cast<long>(at), data.begin() + static_cast<long>(at) + width * height);
                at += static_cast<size_t>(width * height);
                Frame& frame = frames_[static_cast<size_t>(i)];
                frame.source = toQImage(drawSprite(sprite, width, height, screen, &frame.picture));
            }
            keepPictures = true;
        } else {
            Screen read = screen;
            Picture pens;
            const LoadedScreen loaded = impdrawScreen ? loadImpdrawScreen(bytes, read, pens) : loadScreen(data, read, pens);
            if (!loaded.ok) {
                fail(texts::text("Main.prg.TxtInfo6"));
                return false;
            }
            if (single) {
                // One picture of an animation: the screen stays what it is.
                importSource(toQImage(loaded.shown), current_);
            } else {
                screen = read;
                setSources({toQImage(loaded.shown)}, 0);
                frames_[0].picture = pens;
                keepPictures = true;
            }
            say(texts::text("Main.prg.TxtInfo3"));
        }
        syncParam();
        emit screenChanged();
    } else {
        int time = 0;
        const QList<QImage> pictures = readPictures(QByteArray(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size())), &time);
        if (pictures.isEmpty()) {
            fail(texts::text("Main.prg.TxtInfo6"));
            return false;
        }
        if (!single) {
            setSources(pictures, time);
            say(pictures.size() > 1 ? tr("PC picture read: an animation of %n picture(s).", nullptr, static_cast<int>(pictures.size()))
                                    : tr("PC picture read."));
        } else {
            importSource(pictures.first(), current_);
            say(texts::text("Main.prg.TxtInfo4"));
        }
    }
    emit titleChanged(QFileInfo(path).fileName());
    if (param.sizeMode == Param::SizeMode::Origin) {
        param.sizeX = source().width();
        param.sizeY = source().height();
        param.posX = param.posY = 0;
        emit screenChanged();
    }
    if (!keepPictures) {
        if (!single)
            current_ = 0;
        resetPictures(true);
    }
    emit framesChanged();
    convert(false);
    return true;
}

bool Document::readPalette(const QString& path)
{
    const std::optional<std::vector<uint8_t>> file = readHostFile(path);
    if (!file || !readPaletteFile(*file, screen)) {
        fail(tr("The palette could not be read."));
        return false;
    }
    say(tr("Palette read."));
    screen.tidyInterrupts();
    emit screenChanged();
    return true;
}

bool Document::readKit(const QString& path)
{
    const std::optional<std::vector<uint8_t>> file = readHostFile(path);
    std::array<int, 16> colours;
    std::copy_n(screen.palette.begin(), 16, colours.begin());
    if (!file || !readKitFile(*file, colours)) {
        fail(tr("The palette could not be read."));
        return false;
    }
    std::copy(colours.begin(), colours.end(), screen.palette.begin());
    say(tr("Palette read."));
    screen.tidyInterrupts();
    emit screenChanged();
    return true;
}

bool Document::readParam(const QString& path)
{
    const std::optional<std::vector<uint8_t>> file = readHostFile(path);
    Param read = param;
    Screen readScreen = screen;
    if (!file || !readParamXml(std::string(file->begin(), file->end()), read, readScreen)) {
        fail(texts::text("Main.prg.TxtInfo9"));
        return false;
    }
    param = read;
    screen.interruptsOn = readScreen.interruptsOn;
    screen.interrupts = readScreen.interrupts;
    screen.columns = std::clamp(param.nbCols, 1, kMaxColumns);
    screen.lines = std::clamp(param.nbLignes, 1, kMaxLines);
    screen.mode = std::clamp(param.modeVirtuel, 0, kModeCount - 1);
    setPlus(param.cpcPlus);
    screen.tidyInterrupts();
    say(texts::text("Main.prg.TxtInfo8"));
    emit screenChanged();
    convert(false);
    return true;
}

bool Document::saveParam(const QString& path)
{
    syncParam();
    if (!write(path, paramXml(param, screen)))
        return false;
    say(texts::text("Main.prg.TxtInfo10"));
    return true;
}

// ---- Writing ----

bool Document::saveScreen(const QString& path, bool toDsk)
{
    syncParam();
    const ScreenData data = screenData(screen, picture(), param, PackMethod::None);
    if (!toDsk) {
        if (!write(path, screenFile(local(path), data)))
            return false;
    } else {
        Dsk dsk;
        if (const auto there = readHostFile(path); there && !dsk.load(*there)) {
            fail(texts::text("GestDSK.TxtInfo2"));
            return false;
        }
        std::string name;
        const Dsk::Error error = addScreenToDsk(dsk, data, false, &name);
        if (error != Dsk::Error::None) {
            fail(texts::text(error == Dsk::Error::NoBlock ? "Main.prg.TxtInfo35" : "Main.prg.TxtInfo34"));
            return false;
        }
        if (!write(path, dsk.save()))
            return false;
        say(texts::text("Main.prg.TxtInfo33") + QString::fromStdString(name));
    }
    say(tr("CPC picture saved."));
    return true;
}

bool Document::savePacked(const QString& path, const AsmLabels& labels, int format)
{
    syncParam();
    if (format == 1 && screen.mode >= kModeAscUt) {
        // A picture of an ASCII mode is its characters and the routine that
        // draws them: an animation of one frame.
        AnimOptions options;
        options.singlePicture = true;
        const AnimSource source = animationAsm(screen, {&picture()}, {frameTime(current_)}, param, packMethod, options, stamp(), "Palette", progress());
        if (!write(path, source.text))
            return false;
    } else if (format == 1) {
        const AsmSource source = screenAsm(screen, picture(), param, packMethod, labels, inTwoFiles, stamp(), progress());
        if (source.second.empty()) {
            if (!write(path, source.text))
                return false;
        } else {
            const QFileInfo info(path);
            const QString start = info.dir().filePath(info.completeBaseName());
            const QString ext = info.suffix().isEmpty() ? QString() : QLatin1Char('.') + info.suffix();
            if (!write(start + QStringLiteral("_01") + ext, source.text) || !write(start + QStringLiteral("_02") + ext, source.second))
                return false;
        }
    } else {
        const ScreenData data = screenData(screen, picture(), param, packMethod, progress());
        if (format == 0) {
            if (!write(path, screenFile(local(path), data)))
                return false;
        } else {
            Dsk dsk;
            if (const auto there = readHostFile(path); there && !dsk.load(*there)) {
                fail(texts::text("GestDSK.TxtInfo2"));
                return false;
            }
            std::string name;
            const Dsk::Error error = addScreenToDsk(dsk, data, packMethod != PackMethod::None, &name);
            if (error != Dsk::Error::None) {
                fail(texts::text(error == Dsk::Error::NoBlock ? "Main.prg.TxtInfo35" : "Main.prg.TxtInfo34"));
                return false;
            }
            if (!write(path, dsk.save()))
                return false;
            say(texts::text("Main.prg.TxtInfo33") + QString::fromStdString(name));
        }
    }
    say(tr("Packed picture saved."));
    return true;
}

bool Document::savePng(const QString& path)
{
    if (screen.mode == kModeSplit) {
        // The colours the rasters give, as a picture of their own, and the
        // screen without them.
        const QString single = withoutExtension(path);
        QImage rasters(screen.width() >> 1, screen.height() >> 1, QImage::Format_RGB32);
        rasters.fill(Qt::black);
        const Picture& pens = picture();
        for (int y = 0; y < rasters.height(); ++y) {
            QRgb last = qRgb(0, 0, 0);
            bool met = false;
            for (int x = 0; x < rasters.width(); ++x) {
                const int pen = pens.pen(x << 1, y << 1);
                if (pen > 2) {
                    last = toQColor(screen.rgb(screen.lineColours[static_cast<size_t>(y)][static_cast<size_t>(pen)])).rgb();
                    for (int r = met ? x & 0xFF8 : 0; r < x; ++r)
                        rasters.setPixel(r, y, last);
                    met = true;
                }
                rasters.setPixel(x, y, last);
            }
        }
        if (!rasters.save(single + QStringLiteral("_Rasters.png"), "PNG")) {
            fail(tr("Cannot write %1").arg(QDir::toNativeSeparators(single + QStringLiteral("_Rasters.png"))));
            return false;
        }
        syncParam();
        if (!write(single + QStringLiteral(".scr"), screenFile(local(single + QStringLiteral(".scr")), screenData(screen, picture(), param, PackMethod::None))))
            return false;
    } else if (!shown().save(path, "PNG")) {
        fail(tr("Cannot write %1").arg(QDir::toNativeSeparators(path)));
        return false;
    }
    say(tr("PNG picture saved."));
    return true;
}

bool Document::saveSprite(const QString& path, const AsmLabels& labels, bool packed)
{
    syncParam();
    if (!write(path, spriteAsm(screen, picture(), param, labels, stamp(), packed ? packMethod : PackMethod::None, progress())))
        return false;
    say(packed ? tr("Packed sprite saved as a source.")
               : tr("Sprite saved as a source."));
    return true;
}

bool Document::saveImp(const QString& path)
{
    std::vector<uint8_t> data;
    convertMissing();
    for (const Frame& frame : frames_) {
        const std::vector<uint8_t> sprite = spriteBytes(screen, frame.picture);
        data.insert(data.end(), sprite.begin(), sprite.end());
    }
    data.push_back(static_cast<uint8_t>(screen.width() >> 3));
    data.push_back(static_cast<uint8_t>(screen.height() >> 1));
    data.push_back(static_cast<uint8_t>(frameCount()));
    if (!write(path, withAmsdosHeader(local(path), 0x4000, 0xCACA, data)))
        return false;
    say(tr("Impdraw animation saved."));
    return true;
}

bool Document::saveTiles(const QString& path, int tileWidth, int tileHeight)
{
    std::vector<std::vector<uint8_t>> tiles;
    std::vector<uint8_t> map;
    bool tooMany = false;
    for (int y = 0; y < screen.height() && !tooMany; y += tileHeight)
        for (int x = 0; x < screen.width(); x += tileWidth) {
            const std::vector<uint8_t> tile = spriteBytes(screen, picture(), x, y, tileWidth, tileHeight);
            const auto found = std::find(tiles.begin(), tiles.end(), tile);
            if (found != tiles.end()) {
                map.push_back(static_cast<uint8_t>(found - tiles.begin()));
            } else if (tiles.size() < 255) {
                map.push_back(static_cast<uint8_t>(tiles.size()));
                tiles.push_back(tile);
            } else {
                tooMany = true;
                break;
            }
        }
    if (tooMany || tiles.size() >= 255) {
        fail(tr("Too many tiles."));
        return false;
    }
    say(tr("Tiles: %1").arg(tiles.size()));
    std::vector<uint8_t> data;
    for (const std::vector<uint8_t>& tile : tiles)
        data.insert(data.end(), tile.begin(), tile.end());
    data.push_back(static_cast<uint8_t>(tileWidth >> 3));
    data.push_back(static_cast<uint8_t>(tileHeight >> 1));
    data.push_back(static_cast<uint8_t>(tiles.size()));
    if (!write(path, withAmsdosHeader(local(path), 0x4000, 0xCACA, data)))
        return false;
    // The map of the tiles, beside it.
    const QString start = withoutExtension(path);
    const QString til = start + QStringLiteral(".TIL");
    std::vector<uint8_t> tilFile = amsdosHeader(local(til), 0x4000, static_cast<int>(data.size()), 0xCACA);
    tilFile.insert(tilFile.end(), map.begin(), map.end());
    if (!write(til, tilFile))
        return false;
    return screen.plus ? saveKit(start + QStringLiteral(".KIT"), true) : savePalette(start + QStringLiteral(".PAL"));
}

bool Document::savePalette(const QString& path)
{
    syncParam();
    if (!write(path, paletteFile(local(path), screen, param)))
        return false;
    say(tr("Palette saved."));
    return true;
}

bool Document::saveKit(const QString& path, bool forPicture)
{
    std::array<int, 16> colours;
    if (forPicture)
        std::copy_n(screen.palette.begin(), 16, colours.begin());
    else
        colours = screen.spritePalette;
    if (!write(path, kitFile(local(path), colours, forPicture)))
        return false;
    say(texts::text("Main.prg.TxtInfo13"));
    return true;
}

bool Document::saveEgx(const QString& path)
{
    syncParam();
    QString start = path;
    if (start.endsWith(QLatin1String(".scr"), Qt::CaseInsensitive))
        start.chop(4);
    // Two screens, each with the lines of one mode: the finer first.
    const int modes[2] = {screen.mode == kModeEgx1 ? 1 : 2, screen.mode == kModeEgx1 ? 0 : 1};
    for (int n = 0; n < 2; ++n) {
        Screen one = screen;
        one.mode = modes[n];
        const int parity = (screen.yEgx == 2 ? 1 : 0) ^ n;
        Param settings = param;
        settings.modeVirtuel = one.mode;
        const QString name = start + QString::number(n) + QStringLiteral(".SCR");
        if (!write(name, screenFile(local(name), screenData(one, screenMemory(one, picture(), true, parity), settings, PackMethod::None))))
            return false;
    }
    say(tr("The two CPC pictures are saved."));
    return true;
}

bool Document::saveBump(const QString& path)
{
    syncParam();
    if (!write(path, bumpAsm(screen, picture(), param, stamp())))
        return false;
    say(tr("Bump saved."));
    return true;
}

bool Document::saveMatrix(const QString& path)
{
    syncParam();
    if (!write(path, matrixAsm(screen, picture(), param, packMethod, stamp())))
        return false;
    say(tr("Matrix saved as a source."));
    return true;
}

bool Document::saveTunnel(const QString& path)
{
    using namespace asmgen;
    AsmText sw(screen);
    const int incy = std::max(2, screen.height() / 12);
    const int incx = std::max(2, screen.width() / 16);
    for (int i = 0; i < frameCount(); ++i) {
        const Bitmap drawn = render(screen, picture(i));
        sw.WriteLine(S("DataImage") + dec(i, 3));
        for (int y = 0; y < screen.height(); y += incy) {
            int totv = 0, divv = 0;
            Text s = S("");
            for (int x = 0; x < screen.width(); x += incx) {
                int div = 0, totr = 0, totb = 0;
                for (int yy = 0; yy < incy; yy += 2)
                    for (int xx = 0; xx < incx; xx += 2) {
                        const Rgb colour = drawn.colour(x + xx, y + yy);
                        totr += colour.r;
                        totv += colour.g;
                        totb += colour.b;
                        div += 17;
                        divv += 17;
                    }
                totr = std::min(15, (totr / div) + (totv / (div * 2)));
                totb = std::min(15, (totb / div) + (totv / (div * 8)));
                s += S("#") + hex(totr, 1) + hex(totb, 1);
                if (x < screen.width() - incx)
                    s += S(",");
            }
            sw.WriteLine(S("\tDB\t#") + hex(totv / divv, 2) + S(",") + s);
        }
    }
    sw.WriteLine(S(""));
    sw.WriteLine(S("Anim"));
    for (int i = 0; i < frameCount(); ++i)
        sw.WriteLine(S("	DW	DataImage") + dec(i, 3));
    sw.WriteLine(S("	DW	0"));
    return write(path, sw.text());
}

bool Document::saveDiffImages(const QString& path)
{
    using namespace asmgen;
    say(tr("Saving the frames' differences as a source..."));
    AsmText sw(screen);
    WriteHeader(sw, Text(stamp().version), true);
    sw.WriteLine(S("\tnolist"));
    std::vector<uint8_t> before;
    for (int i = 0; i < frameCount(); ++i) {
        const std::vector<uint8_t> now = screenMemory(screen, picture(i));
        if (i > 0) {
            sw.WriteLine(S("DiffImage_") + dec(i) + S(":"));
            // Runs of bytes that changed, each with its address.
            size_t m = 0;
            bool any = false;
            while (m < now.size()) {
                if (now[m] == before[m]) {
                    ++m;
                    continue;
                }
                size_t end = m;
                while (end < now.size() && now[end] != before[end])
                    ++end;
                any = true;
                const int length = static_cast<int>(end - m);
                sw.WriteLine(S("\tDW\t#") + hex(static_cast<int>(m), 4) + S("\t;adresse"));
                sw.WriteLine(S("\tDB\t#") + hex(length, 2) + S("\t;nbre octets"));
                Text line = S("\tDB\t");
                int count = 0;
                for (size_t b = m; b < end; ++b) {
                    line += S("#") + hex(now[b], 2) + S(",");
                    if (++count >= std::min(32, screen.columns)) {
                        sw.WriteLine(line.Substring(0, line.Length() - 1));
                        line = S("\tDB\t");
                        count = 0;
                    }
                }
                if (count > 0)
                    sw.WriteLine(line.Substring(0, line.Length() - 1));
                m = end;
            }
            if (any)
                sw.WriteLine(S("\tDW\t#0000\t;fin de table"));
        }
        before = now;
    }
    sw.WriteLine(S("\tlist"));
    sw.WriteLine(S("\tDW\t#0000;\tFin anim"));
    return write(path, sw.text());
}

// The frames that were never converted are, before all of them are saved.
void Document::convertMissing()
{
    const int selected = current_;
    const bool all = allPictures;
    allPictures = false;
    for (int i = 0; i < frameCount(); ++i) {
        current_ = i;
        if (!picture().computed)
            convert(true, true);
    }
    allPictures = all;
    current_ = selected;
}

bool Document::saveAnimation(const QString& path, const AnimOptions& options)
{
    if (screen.columns * screen.lines > 0x4000) {
        fail(tr("Animations of screens of more than 16K are not supported."));
        return false;
    }
    syncParam();
    convertMissing();
    std::vector<const Picture*> pictures;
    std::vector<int> times;
    for (const Frame& frame : frames_) {
        pictures.push_back(&frame.picture);
        times.push_back(frame.time);
    }
    const AnimSource source = animationAsm(screen, pictures, times, param, packMethod, options, stamp(), "Palette", progress());
    if (!write(path, source.text))
        return false;
    for (const AnimNote& note : source.notes) {
        switch (note.kind) {
        case AnimNote::Started:
            say(tr("Saving the animation as a source..."));
            break;
        case AnimNote::BytesLost:
            say(tr("%Ln byte(s) lost...", nullptr, note.bytes));
            break;
        case AnimNote::TotalLength:
            say(tr("The animation's data take %Ln byte(s) in all.", nullptr, note.bytes));
            break;
        case AnimNote::Overflow:
            say(tr("More than the memory holds..."));
            break;
        case AnimNote::Saved:
            say(tr("Animation saved as a source."));
            break;
        }
    }
    if (source.overflow)
        fail(tr("Beware: the whole (animation and the room to unpack it) will be over %1: the screen's memory may be written "
                "over, and the program crash.")
                 .arg(options.memory128K ? QStringLiteral("112K") : QStringLiteral("48K")));
    return true;
}

bool Document::saveGif(const QString& path)
{
    convertMissing();
    std::vector<Bitmap> pictures;
    std::vector<int> times;
    for (const Frame& frame : frames_) {
        pictures.push_back(render(screen, frame.picture));
        times.push_back(frame.time);
    }
    const std::vector<uint8_t> gif = animatedGif(pictures, times);
    if (gif.empty() || !write(path, gif)) {
        fail(tr("The GIF could not be saved."));
        return false;
    }
    return true;
}

bool Document::savePaletteInterrupts(const QString& path)
{
    screen.tidyInterrupts();
    if (!write(path, asmgen::paletteInterruptsText(screen, stamp().version, {})))
        return false;
    say(texts::text("Interrupts.exported") + QDir::toNativeSeparators(path));
    return true;
}

std::string Document::showProgram()
{
    syncParam();
    Param settings = param;
    settings.withCode = true;
    settings.withPalette = true;
    // ZX0 over an overscan screen's unused bytes has no routine that fits
    // every size: the plain one is used.
    const PackMethod method = packMethod == PackMethod::None || packMethod == PackMethod::ZX0Ovs ? PackMethod::ZX0_V2 : packMethod;
    return screenAsm(screen, picture(), settings, method, {}, false, stamp(), progress()).text;
}

}  // namespace tuxscr
