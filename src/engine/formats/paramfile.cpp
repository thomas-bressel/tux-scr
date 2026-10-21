#include "formats/paramfile.h"

#include <cstdlib>
#include <sstream>

namespace tuxscr {

namespace {

const char* const kSizeModes[] = {"Fit", "KeepSmaller", "KeepLarger", "UserSize", "Origin"};

std::string escaped(const std::string& text)
{
    std::string out;
    for (const char c : text) {
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        default:
            out += c;
        }
    }
    return out;
}

std::string unescaped(std::string text)
{
    const std::pair<const char*, const char*> entities[] = {{"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"}, {"&amp;", "&"}};
    for (const auto& [from, to] : entities)
        for (size_t at = 0; (at = text.find(from, at)) != std::string::npos; at += 1)
            text.replace(at, std::char_traits<char>::length(from), to);
    return text;
}

// The text between <name> and </name>, if the element is there.
bool element(const std::string& xml, const std::string& name, std::string& value, size_t from = 0)
{
    const std::string open = "<" + name + ">";
    const size_t start = xml.find(open, from);
    if (start == std::string::npos)
        return false;
    const size_t end = xml.find("</" + name + ">", start);
    if (end == std::string::npos)
        return false;
    value = xml.substr(start + open.size(), end - start - open.size());
    return true;
}

void read(const std::string& xml, const char* name, int& value)
{
    std::string text;
    if (element(xml, name, text))
        value = std::atoi(text.c_str());
}

void read(const std::string& xml, const char* name, bool& value)
{
    std::string text;
    if (element(xml, name, text))
        value = text == "true";
}

void read(const std::string& xml, const char* name, std::string& value)
{
    std::string text;
    if (element(xml, name, text))
        value = unescaped(text);
}

template <size_t N>
void read(const std::string& xml, const char* name, std::array<int, N>& values)
{
    std::string list;
    if (!element(xml, name, list))
        return;
    size_t at = 0;
    for (size_t i = 0; i < N; ++i) {
        const size_t start = list.find("<int>", at);
        if (start == std::string::npos)
            break;
        values[i] = std::atoi(list.c_str() + start + 5);
        at = start + 5;
    }
}

}  // namespace

std::string paramXml(const Param& p, const Screen& screen)
{
    std::ostringstream out;
    const auto field = [&out](const char* name, const auto& value) { out << "  <" << name << ">" << value << "</" << name << ">\r\n"; };
    const auto flag = [&out](const char* name, bool value) {
        out << "  <" << name << ">" << (value ? "true" : "false") << "</" << name << ">\r\n";
    };
    const auto list = [&out](const char* name, const std::array<int, 16>& values) {
        out << "  <" << name << ">\r\n";
        for (const int value : values)
            out << "    <int>" << value << "</int>\r\n";
        out << "  </" << name << ">\r\n";
    };
    out << "<?xml version=\"1.0\"?>\r\n";
    out << "<Param xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\">\r\n";
    field("sMode", kSizeModes[static_cast<int>(p.sizeMode)]);
    field("sizex", p.sizeX);
    field("sizey", p.sizeY);
    field("posx", p.posX);
    field("posy", p.posY);
    field("methode", escaped(p.method));
    field("pct", p.pct);
    list("lockState", p.lockState);
    field("pctLumi", p.pctLumi);
    field("pctSat", p.pctSat);
    field("pctContrast", p.pctContrast);
    flag("cpcPlus", p.cpcPlus);
    flag("newMethode", p.newMethode);
    flag("reductPal1", p.reductPal1);
    flag("reductPal2", p.reductPal2);
    flag("reductPal3", p.reductPal3);
    flag("reductPal4", p.reductPal4);
    field("newSortPal", p.newSortPal);
    field("nbCols", p.nbCols);
    field("nbLignes", p.nbLignes);
    field("modeVirtuel", p.modeVirtuel);
    flag("withCode", p.withCode);
    flag("withPalette", p.withPalette);
    flag("setPalCpc", p.setPalCpc);
    flag("lissage", p.lissage);
    field("trackModeX", p.trackModeX);
    field("pctRed", p.pctRed);
    field("pctGreen", p.pctGreen);
    field("pctBlue", p.pctBlue);
    flag("trameTc", p.trameTc);
    if (!p.lastReadPath.empty())
        field("lastReadPath", escaped(p.lastReadPath));
    if (!p.lastSavePath.empty())
        field("lastSavePath", escaped(p.lastSavePath));
    field("coefR", p.coefR);
    field("coefV", p.coefV);
    field("coefB", p.coefB);
    field("cstR1", p.cstR1);
    field("cstR2", p.cstR2);
    field("cstR3", p.cstR3);
    field("cstR4", p.cstR4);
    field("cstV1", p.cstV1);
    field("cstV2", p.cstV2);
    field("cstV3", p.cstV3);
    field("cstV4", p.cstV4);
    field("cstB1", p.cstB1);
    field("cstB2", p.cstB2);
    field("cstB3", p.cstB3);
    field("cstB4", p.cstB4);
    flag("newReduc", p.newReduc);
    field("bitsRVB", p.bitsRVB);
    flag("diffErr", p.diffErr);
    field("langue", p.langue);
    flag("modeImpDraw", p.modeImpDraw);
    list("disableState", p.disableState);
    flag("filtre", p.filtre);
    field("kMeansDist", p.kMeansDist);
    field("kMeansColor", p.kMeansColor);
    field("kMeansPass", p.kMeansPass);
    flag("autoRecalc", p.autoRecalc);
    // TuxSCR's own.
    out << "  <paletteInterrupts on=\"" << (screen.interruptsOn ? "true" : "false") << "\">\r\n";
    for (const PaletteInterrupt& interrupt : screen.interrupts) {
        out << "    <interrupt line=\"" << interrupt.line << "\"";
        if (interrupt.matched) {
            out << " match=\"";
            for (size_t pen = 0; pen < 16; ++pen)
                out << (pen ? "," : "") << interrupt.matchColours[pen];
            out << "\"";
        }
        out << ">";
        for (size_t pen = 0; pen < 16; ++pen)
            out << (pen ? "," : "") << (interrupt.defined[pen] ? std::to_string(interrupt.colours[pen]) : std::string("-"));
        out << "</interrupt>\r\n";
    }
    out << "  </paletteInterrupts>\r\n";
    out << "</Param>";
    return out.str();
}

bool readParamXml(const std::string& xml, Param& p, Screen& screen)
{
    if (xml.find("<Param") == std::string::npos)
        return false;
    std::string mode;
    if (element(xml, "sMode", mode))
        for (int i = 0; i < 5; ++i)
            if (mode == kSizeModes[i])
                p.sizeMode = static_cast<Param::SizeMode>(i);
    read(xml, "sizex", p.sizeX);
    read(xml, "sizey", p.sizeY);
    read(xml, "posx", p.posX);
    read(xml, "posy", p.posY);
    read(xml, "methode", p.method);
    read(xml, "pct", p.pct);
    read(xml, "lockState", p.lockState);
    read(xml, "pctLumi", p.pctLumi);
    read(xml, "pctSat", p.pctSat);
    read(xml, "pctContrast", p.pctContrast);
    read(xml, "cpcPlus", p.cpcPlus);
    read(xml, "newMethode", p.newMethode);
    read(xml, "reductPal1", p.reductPal1);
    read(xml, "reductPal2", p.reductPal2);
    read(xml, "reductPal3", p.reductPal3);
    read(xml, "reductPal4", p.reductPal4);
    read(xml, "newSortPal", p.newSortPal);
    read(xml, "nbCols", p.nbCols);
    read(xml, "nbLignes", p.nbLignes);
    read(xml, "modeVirtuel", p.modeVirtuel);
    read(xml, "withCode", p.withCode);
    read(xml, "withPalette", p.withPalette);
    read(xml, "setPalCpc", p.setPalCpc);
    read(xml, "lissage", p.lissage);
    read(xml, "trackModeX", p.trackModeX);
    read(xml, "pctRed", p.pctRed);
    read(xml, "pctGreen", p.pctGreen);
    read(xml, "pctBlue", p.pctBlue);
    read(xml, "trameTc", p.trameTc);
    read(xml, "lastReadPath", p.lastReadPath);
    read(xml, "lastSavePath", p.lastSavePath);
    read(xml, "coefR", p.coefR);
    read(xml, "coefV", p.coefV);
    read(xml, "coefB", p.coefB);
    read(xml, "cstR1", p.cstR1);
    read(xml, "cstR2", p.cstR2);
    read(xml, "cstR3", p.cstR3);
    read(xml, "cstR4", p.cstR4);
    read(xml, "cstV1", p.cstV1);
    read(xml, "cstV2", p.cstV2);
    read(xml, "cstV3", p.cstV3);
    read(xml, "cstV4", p.cstV4);
    read(xml, "cstB1", p.cstB1);
    read(xml, "cstB2", p.cstB2);
    read(xml, "cstB3", p.cstB3);
    read(xml, "cstB4", p.cstB4);
    read(xml, "newReduc", p.newReduc);
    read(xml, "bitsRVB", p.bitsRVB);
    read(xml, "diffErr", p.diffErr);
    read(xml, "langue", p.langue);
    read(xml, "modeImpDraw", p.modeImpDraw);
    read(xml, "disableState", p.disableState);
    read(xml, "filtre", p.filtre);
    read(xml, "kMeansDist", p.kMeansDist);
    read(xml, "kMeansColor", p.kMeansColor);
    read(xml, "kMeansPass", p.kMeansPass);
    read(xml, "autoRecalc", p.autoRecalc);

    const size_t start = xml.find("<paletteInterrupts");
    if (start != std::string::npos) {
        screen.interrupts.clear();
        screen.interruptsOn = xml.compare(start, 28, "<paletteInterrupts on=\"true\"") == 0;
        const size_t end = xml.find("</paletteInterrupts>", start);
        size_t at = start;
        while ((at = xml.find("<interrupt line=\"", at)) != std::string::npos && at < end) {
            PaletteInterrupt interrupt;
            interrupt.line = std::atoi(xml.c_str() + at + 17);
            const size_t close = xml.find('>', at);
            if (const size_t match = xml.find(" match=\"", at); match != std::string::npos && match < close) {
                interrupt.matched = true;
                size_t number = match + 8;
                for (size_t pen = 0; pen < 16 && number < close; ++pen) {
                    interrupt.matchColours[pen] = std::atoi(xml.c_str() + number);
                    number = xml.find_first_of(",\"", number);
                    if (number == std::string::npos || xml[number] == '"')
                        break;
                    ++number;
                }
            }
            size_t value = close + 1;
            for (size_t pen = 0; pen < 16 && value < xml.size() && xml[value] != '<'; ++pen) {
                if (xml[value] != '-') {
                    interrupt.colours[pen] = std::atoi(xml.c_str() + value);
                    interrupt.defined[pen] = true;
                }
                value = xml.find_first_of(",<", value);
                if (value == std::string::npos || xml[value] == '<')
                    break;
                ++value;
            }
            screen.interrupts.push_back(interrupt);
            at += 17;
        }
    }
    return true;
}

}  // namespace tuxscr
