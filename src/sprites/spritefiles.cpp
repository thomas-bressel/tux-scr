#include "spritefiles.h"

#include <algorithm>

#include <QByteArray>
#include <QXmlStreamReader>

#include "formats/amsdos.h"
#include "formats/asmgen.h"

namespace tuxscr {

namespace {

bool spriteIsEmpty(const Screen& screen, int bank, int sprite)
{
    for (int y = 0; y < kSpriteSize; ++y)
        for (int x = 0; x < kSpriteSize; ++x)
            if (screen.sprites[bank][sprite][x][y] != 0)
                return false;
    return true;
}

const uint8_t* dataOf(const std::vector<uint8_t>& bytes)
{
    static const uint8_t none = 0;
    return bytes.empty() ? &none : bytes.data();
}

// A source as the editor starts it: what wrote it and when, and the mode.
// The sprites' lines are sixteen bytes whatever the width of the screen.
asmgen::AsmText startSource(const Screen& screen, const AsmStamp& stamp)
{
    asmgen::AsmText sw(screen);
    sw.Cpc.NbCol = std::max(sw.Cpc.NbCol, kSpriteSize);
    asmgen::WriteHeader(sw, stamp.version.empty() ? asmgen::Text() : asmgen::Text(stamp.version), false, stamp.time);
    return sw;
}

// The sprites' fifteen colours as the ASIC wants them, under their label.
void writePalette(asmgen::AsmText& sw, const Screen& screen, const std::string& label)
{
    using namespace asmgen;
    if (!label.empty())
        sw.WriteLine(Text(label));
    Text line = S("\tDW\t");
    for (size_t i = 1; i < 16; ++i) {
        line += S("#") + hex(asicWord(screen.spritePalette[i]), 3);
        if (i < 15)
            line += S(",");
    }
    sw.WriteLine(line);
}

// ConvImgCpc packs a screen's used bytes only with "ZX0Ovs": sprites are no
// screen, and are packed with ZX0.
PackMethod spriteMethod(PackMethod method)
{
    return method == PackMethod::ZX0Ovs ? PackMethod::ZX0 : method;
}

}  // namespace

SpriteSpan spriteSpan(const Screen& screen, int bank, bool allBanks)
{
    SpriteSpan span;
    span.firstBank = allBanks ? 0 : std::clamp(bank, 0, kSpriteBanks - 1);
    const int lastBank = allBanks ? kSpriteBanks - 1 : span.firstBank;
    int last = 0;
    bool found = false;
    for (int b = lastBank; b >= span.firstBank && !found; --b)
        for (int sprite = kSpritesPerBank - 1; sprite >= 0 && !found; --sprite)
            if (!spriteIsEmpty(screen, b, sprite)) {
                last = sprite + kSpritesPerBank * (b - span.firstBank);
                found = true;
            }
    span.banks = last / kSpritesPerBank + 1;
    span.sprites = last + 1;
    return span;
}

std::vector<uint8_t> hardSpriteBytes(const Screen& screen, const SpriteSpan& span)
{
    std::vector<uint8_t> bytes;
    bytes.reserve(static_cast<size_t>(span.banks) * kSpritesPerBank * kSpriteBytes);
    for (int bank = span.firstBank; bank < span.firstBank + span.banks && bank < kSpriteBanks; ++bank)
        for (int sprite = 0; sprite < kSpritesPerBank; ++sprite)
            for (int y = 0; y < kSpriteSize; ++y)
                for (int x = 0; x < kSpriteSize; ++x)
                    bytes.push_back(screen.sprites[bank][sprite][x][y]);
    return bytes;
}

std::vector<uint8_t> hardSpriteFile(const std::string& fileName, const Screen& screen, const SpriteSpan& span)
{
    return withAmsdosHeader(baseName(fileName), 0x4000, 0, hardSpriteBytes(screen, span));
}

bool readHardSpriteFile(const std::vector<uint8_t>& file, Screen& screen, int bank)
{
    if (!hasAmsdosHeader(file) || file.size() <= kAmsdosHeaderSize)
        return false;
    // The length the header says the file really has: a bank's own.
    const int realLength = file[64] | (file[65] << 8);
    size_t at = kAmsdosHeaderSize;
    for (int b = realLength == kSpritesPerBank * kSpriteBytes ? std::clamp(bank, 0, kSpriteBanks - 1) : 0; b < kSpriteBanks; ++b)
        for (int sprite = 0; sprite < kSpritesPerBank; ++sprite)
            for (int y = 0; y < kSpriteSize; ++y)
                for (int x = 0; x < kSpriteSize; ++x) {
                    screen.sprites[b][sprite][x][y] = file[at++];
                    if (at >= file.size())
                        return true;
                }
    return true;
}

std::string hardSpritesAsm(const Screen& screen, const SpriteSpan& span, const SpriteLabels& labels, bool withPalette,
                           const AsmStamp& stamp)
{
    using namespace asmgen;
    AsmText sw = startSource(screen, stamp);
    const std::vector<uint8_t> bytes = hardSpriteBytes(screen, span);
    const int length = std::min(span.sprites * kSpriteBytes, static_cast<int>(bytes.size()));
    // A label before each sprite; without one, the lines follow each other.
    if (labels.media.empty())
        GenereDatas(sw, dataOf(bytes), length, kSpriteSize);
    else
        GenereDatas(sw, dataOf(bytes), length, kSpriteSize, kSpriteSize, Text(labels.media));
    if (withPalette)
        writePalette(sw, screen, labels.palette);
    return sw.text();
}

std::string packedSpritesAsm(const Screen& screen, const SpriteSpan& span, const SpriteLabels& labels, bool withPalette,
                             PackMethod method, const AsmStamp& stamp)
{
    using namespace asmgen;
    AsmText sw = startSource(screen, stamp);
    const std::vector<uint8_t> bytes = hardSpriteBytes(screen, span);
    const int count = std::min(span.sprites, static_cast<int>(bytes.size()) / kSpriteBytes);
    const bool named = !labels.media.empty();
    for (int number = 0; number < count; ++number) {
        if (named)
            sw.WriteLine(Text(labels.media) + dec(number, 2));
        const auto start = bytes.begin() + number * kSpriteBytes;
        const std::vector<uint8_t> packed = pack(std::vector<uint8_t>(start, start + kSpriteBytes), spriteMethod(method));
        GenereDatas(sw, dataOf(packed), static_cast<int>(packed.size()), 16);
    }
    // The table of the sprites' addresses, four to a line: it names them by
    // their labels, and is left out when they have none.
    if (named) {
        sw.WriteLine(NL + Text(labels.pointers));
        Text line = S("\tDW\t");
        int words = 0;
        for (int number = 0; number < count; ++number) {
            line += Text(labels.media) + dec(number, 2);
            if (++words > 3) {
                words = 0;
                sw.WriteLine(line);
                line = S("\tDW\t");
            } else {
                line += S(",");
            }
        }
        if (line.str() != "\tDW\t")
            sw.WriteLine(line.Substring(0, line.Length() - 1));
        if (labels.zeroPointer)
            sw.WriteLine(S("\tDW\t0"));
    }
    if (withPalette) {
        sw.WriteLine(S(""));
        writePalette(sw, screen, labels.palette);
    }
    return sw.text();
}

std::string packedSpriteSetAsm(const Screen& screen, const SpriteSpan& span, int count, const SpriteLabels& labels,
                               bool withPalette, PackMethod method, const AsmStamp& stamp)
{
    using namespace asmgen;
    AsmText sw = startSource(screen, stamp);
    std::vector<uint8_t> bytes = hardSpriteBytes(screen, span);
    count = std::clamp(count, 1, static_cast<int>(bytes.size()) / kSpriteBytes);
    bytes.resize(static_cast<size_t>(count) * kSpriteBytes);
    sw.WriteLine(S("; ") + dec(count) + S(" sprites"));
    sw.WriteLine(Text(labels.media));
    const std::vector<uint8_t> packed = pack(bytes, spriteMethod(method));
    GenereDatas(sw, dataOf(packed), static_cast<int>(packed.size()), 16);
    if (withPalette)
        writePalette(sw, screen, labels.palette);
    return sw.text();
}

// ---- The sprites' places on the picture ----

std::string spritePlacesText(const Document::SpritePlaces& places)
{
    std::string text;
    for (int i = 0; i < 16; ++i) {
        // The zoom as the ASIC's register has it: across in bits 2 and 3,
        // down in bits 0 and 1.
        const std::string zoom = std::to_string(places.zoomX[i] * 4 + places.zoomY[i]);
        text += ";\tSprite " + std::to_string(i) + "\r\n";
        text += "\tDW\t" + std::to_string(places.x[i]) + "," + std::to_string(places.y[i]) + "\t\t; position\r\n";
        text += "\tDB\t" + zoom + "," + zoom + "," + zoom + "," + zoom + "\t\t; zoom\r\n";
    }
    return text;
}

std::string spritePlacesXml(const Document::SpritePlaces& places)
{
    // C#'s serializer writes an array of numbers as one element each, and an
    // array of bytes in base 64.
    const auto numbers = [](const char* name, const int* values) {
        std::string out = std::string("  <") + name + ">\r\n";
        for (int i = 0; i < 16; ++i)
            out += "    <int>" + std::to_string(values[i]) + "</int>\r\n";
        return out + "  </" + name + ">\r\n";
    };
    const auto bytes = [](const char* name, const int* values) {
        QByteArray raw;
        for (int i = 0; i < 16; ++i)
            raw.append(static_cast<char>(values[i]));
        return std::string("  <") + name + ">" + raw.toBase64().toStdString() + "</" + name + ">\r\n";
    };
    std::string xml = "<?xml version=\"1.0\"?>\r\n";
    xml += "<PosSprite xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\">\r\n";
    xml += numbers("posXSprite", places.x);
    xml += numbers("posYSprite", places.y);
    xml += bytes("zoomXSprite", places.zoomX);
    xml += bytes("zoomYSprite", places.zoomY);
    xml += "</PosSprite>";
    return xml;
}

bool readSpritePlacesXml(const std::string& xml, Document::SpritePlaces& places)
{
    Document::SpritePlaces read;
    QXmlStreamReader reader(QByteArray::fromStdString(xml));
    if (!reader.readNextStartElement() || reader.name() != QLatin1String("PosSprite"))
        return false;
    while (reader.readNextStartElement()) {
        const QString name = reader.name().toString();
        const bool isX = name == QLatin1String("posXSprite");
        if (isX || name == QLatin1String("posYSprite")) {
            int* values = isX ? read.x : read.y;
            const int most = isX ? 1024 : 272;
            int count = 0;
            while (reader.readNextStartElement()) {
                const int value = reader.readElementText(QXmlStreamReader::SkipChildElements).trimmed().toInt();
                if (count < 16)
                    values[count++] = std::clamp(value, 0, most);
            }
        } else if (name == QLatin1String("zoomXSprite") || name == QLatin1String("zoomYSprite")) {
            int* values = name == QLatin1String("zoomXSprite") ? read.zoomX : read.zoomY;
            const QByteArray raw = QByteArray::fromBase64(reader.readElementText(QXmlStreamReader::SkipChildElements).toLatin1());
            for (int i = 0; i < 16 && i < raw.size(); ++i)
                values[i] = std::clamp(static_cast<int>(static_cast<uint8_t>(raw[i])), 0, 3);
        } else {
            reader.skipCurrentElement();
        }
    }
    if (reader.hasError())
        return false;
    places = read;
    return true;
}

}  // namespace tuxscr
