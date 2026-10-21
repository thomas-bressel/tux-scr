#include "splits/rasters.h"

#include <algorithm>
#include <sstream>

#include "formats/asmgen.h"
#include "splits/xmltext.h"

namespace tuxscr {

using namespace asmgen;

// ---- The file ----

std::string rasterXml(const Raster& raster)
{
    std::ostringstream out;
    out << xmltext::header("Raster");
    out << "  <line>\r\n";
    for (const int colour : raster.line)
        out << "    <int>" << colour << "</int>\r\n";
    out << "  </line>\r\n";
    out << "  <minLine>" << raster.minLine << "</minLine>\r\n";
    out << "  <maxLine>" << raster.maxLine << "</maxLine>\r\n";
    out << "</Raster>";
    return out.str();
}

bool readRasterXml(const std::string& xml, Raster& raster)
{
    size_t at = 0;
    std::string root;
    if (!xmltext::nextElement(xml, "Raster", at, root))
        return false;
    Raster read;
    std::string list;
    at = 0;
    if (xmltext::nextElement(root, "line", at, list)) {
        size_t inside = 0;
        std::string number;
        for (size_t i = 0; i < read.line.size() && xmltext::nextElement(list, "int", inside, number); ++i)
            read.line[i] = std::atoi(number.c_str()) & 0xFFF;
    }
    xmltext::read(root, "minLine", read.minLine);
    xmltext::read(root, "maxLine", read.maxLine);
    read.minLine = std::clamp(read.minLine, 0, kMaxLines - 1);
    read.maxLine = std::clamp(read.maxLine, 0, kMaxLines - 1);
    raster = read;
    return true;
}

// ---- The lines ----

void scrollRaster(Raster& raster, int count, bool up)
{
    if (count <= 0 || count >= kMaxLines)
        return;
    const size_t by = static_cast<size_t>(count);
    if (up) {
        std::copy(raster.line.begin() + count, raster.line.end(), raster.line.begin());
        std::fill(raster.line.end() - count, raster.line.end(), 0);
    } else {
        std::copy_backward(raster.line.begin(), raster.line.end() - count, raster.line.end());
        std::fill_n(raster.line.begin(), by, 0);
    }
}

// ---- Sources ----

std::string rasterData(const Raster& raster, int start, int end)
{
    return GenereDatas(raster.line.data(), start, end, 16).str();
}

std::string rasterAsm(const Screen& screen, const Raster& raster, int start, int end)
{
    AsmText sw(screen);
    WriteHeader(sw);
    WriteDatas(sw, raster.line.data(), start, end, 16);
    return sw.text();
}

std::string rasterFadeAsm(const Screen& screen, const Raster& raster, int start, int end)
{
    std::array<std::array<int, kMaxLines>, 16> tabFade{};
    int maxR = 0, maxV = 0, maxB = 0;
    for (int c = start; c <= end; c++) {
        const size_t at = static_cast<size_t>(c);
        tabFade[0][at] = raster.line[at];
        maxR = std::max(maxR, (tabFade[0][at] & 0xF0) >> 4);
        maxV = std::max(maxV, tabFade[0][at] >> 8);
        maxB = std::max(maxB, tabFade[0][at] & 0x0F);
    }
    AsmText sw(screen);
    WriteHeader(sw);
    // Each table is the one before it, each of red, green and blue brought
    // down by what the brightest line has of it.
    for (size_t i = 0; i < 15; i++) {
        for (int c = start; c <= end; c++) {
            const size_t at = static_cast<size_t>(c);
            int r = ((tabFade[i][at] & 0xF0) >> 4) * maxR / 16;
            int v = ((tabFade[i][at]) >> 8) * maxV / 16;
            int b = (tabFade[i][at] & 0x0F) * maxB / 16;
            tabFade[i + 1][at] = (r << 4) + (v << 8) + b;
        }
    }
    for (int i = 15; i >= 0; i--) {
        sw.WriteLine(S("Fade_") + dec(15 - i));
        WriteDatas(sw, tabFade[static_cast<size_t>(i)].data(), start, end, 16);
    }
    return sw.text();
}

// ---- Colours that go from one to another ----

void gradientPens(int* palette, int start, int end, Shade from, Shade to)
{
    double rs = from.red * 17, vs = from.green * 17, bs = from.blue * 17;
    const double re = to.red * 17, ve = to.green * 17, be = to.blue * 17;
    const double kr = (re - rs) / (end - start);
    const double kv = (ve - vs) / (end - start);
    const double kb = (be - bs) / (end - start);
    for (int i = start; i < end; i++) {
        palette[i] = static_cast<int>((rs / 17) + (static_cast<int>(bs / 17) << 4) + (static_cast<int>(vs / 17) << 8));
        rs += kr;
        bs += kb;
        vs += kv;
    }
    palette[end] = static_cast<int>((re / 17) + (static_cast<int>(be / 17) << 4) + (static_cast<int>(ve / 17) << 8));
}

void gradientLines(int* lines, int start, int end, Shade from, Shade to)
{
    double rs = from.red, vs = from.green, bs = from.blue;
    const double re = to.red, ve = to.green, be = to.blue;
    const int sens = start < end ? 1 : -1;
    const double kr = (re - rs) / (end - start) * sens;
    const double kv = (ve - vs) / (end - start) * sens;
    const double kb = (be - bs) / (end - start) * sens;
    for (int i = start; i != end + sens; i += sens) {
        lines[i] = (static_cast<int>(bs) + (static_cast<int>(rs) << 4) + (static_cast<int>(vs) << 8));
        rs += kr;
        bs += kb;
        vs += kv;
    }
    lines[end] = (static_cast<int>(be) + (static_cast<int>(re) << 4) + (static_cast<int>(ve) << 8));
}

}  // namespace tuxscr
