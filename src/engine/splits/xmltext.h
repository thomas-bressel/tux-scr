#pragma once

#include <cstdlib>
#include <string>

// What the files of the split editor and of the raster table are read with:
// they are what C#'s serializer writes, elements inside elements and nothing
// else, so that looking for an element by its name is enough.

namespace tuxscr::xmltext {

// The next element named `name` from `at` on: its content, `at` being moved
// past it. An element written as empty (<name />) has no content. False if
// there is none.
inline bool nextElement(const std::string& xml, const std::string& name, size_t& at, std::string& content)
{
    const std::string open = "<" + name;
    for (size_t start = xml.find(open, at); start != std::string::npos; start = xml.find(open, start + 1)) {
        const size_t after = start + open.size();
        if (after >= xml.size())
            return false;
        // A longer name that starts the same is another element.
        const char next = xml[after];
        if (next != '>' && next != '/' && next != ' ' && next != '\t' && next != '\r' && next != '\n')
            continue;
        const size_t close = xml.find('>', after);
        if (close == std::string::npos)
            return false;
        if (xml[close - 1] == '/') {
            content.clear();
            at = close + 1;
            return true;
        }
        const std::string endTag = "</" + name + ">";
        const size_t end = xml.find(endTag, close + 1);
        if (end == std::string::npos)
            return false;
        content = xml.substr(close + 1, end - close - 1);
        at = end + endTag.size();
        return true;
    }
    return false;
}

// The first element of that name, as a number or as a yes or no: `value` is
// left as it is if the element is not there.
inline void read(const std::string& xml, const char* name, int& value)
{
    size_t at = 0;
    std::string text;
    if (nextElement(xml, name, at, text))
        value = std::atoi(text.c_str());
}

inline void read(const std::string& xml, const char* name, bool& value)
{
    size_t at = 0;
    std::string text;
    if (nextElement(xml, name, at, text))
        value = text.find("true") != std::string::npos;
}

// The two lines every file of the serializer starts with, for a root
// element of that name.
inline std::string header(const char* root)
{
    return std::string("<?xml version=\"1.0\"?>\r\n<") + root
        + " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\">\r\n";
}

}  // namespace tuxscr::xmltext
