#!/usr/bin/env python3
"""Takes ConvImgCpc's tables out of its sources, as C++.

    tools/make_tables.py [ConvImgCpc's folder]

Writes, from the C# files of ConvImgCpc/ConvImgCpc:
    src/engine/convert/patterns.inc   the eight sets of patterns of mode ASC-UT
    src/engine/formats/codes.inc     the Z80 routines put in the screens it saves
    src/plugin/windowtexts.h          the windows' texts, in English, for Qt's translation

    tools/make_tables.py --french translations/tuxscr_fr.ts

gives the texts of ConvImgCpc's windows that the translation has not yet the
French ConvImgCpc has for them (after lupdate has put them there).

The tables are copied, not typed again: a byte of a routine that is wrong is
a screen that does not show.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SOURCE = sys.argv[1] if len(sys.argv) > 1 and sys.argv[1] != "--french" else os.path.join(ROOT, "ConvImgCpc", "ConvImgCpc")


def read(name):
    with open(os.path.join(SOURCE, name), encoding="utf-8-sig") as file:
        return file.read().replace("\r\n", "\n")


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def write(path, text):
    with open(os.path.join(ROOT, path), "w", encoding="utf-8") as file:
        file.write(text)
    print(path)


HEADER = "// Made by tools/make_tables.py from ConvImgCpc's %s: not to be changed by hand.\n\n"


def patterns():
    text = strip_comments(read("TramesAscUt.cs"))
    body = text[text.index("TramesAscUt = {") + len("TramesAscUt = "):text.index("};") + 1]
    numbers = [int(n) for n in re.findall(r"\d+", body)]
    assert len(numbers) == 8 * 16 * 4 * 4, len(numbers)
    out = HEADER % "TramesAscUt.cs"
    out += "// Set, pattern, x, y.\nconst uint8_t kPatternSets[8][16][4][4] = {\n"
    at = 0
    for _ in range(8):
        out += "    {\n"
        for _ in range(16):
            rows = []
            for _ in range(4):
                rows.append("{" + ", ".join(str(n) for n in numbers[at:at + 4]) + "}")
                at += 4
            out += "        {" + ", ".join(rows) + "},\n"
        out += "    },\n"
    out += "};\n"
    write("src/engine/convert/patterns.inc", out)


def codes():
    text = strip_comments(read("SauveImage.cs"))
    out = HEADER % "SauveImage.cs"
    names = []
    for match in re.finditer(r"static readonly byte\[\] (\w+) = \{(.*?)\};", text, flags=re.S):
        name, body = match.groups()
        values = [int(v, 16) for v in re.findall(r"0[xX]([0-9A-Fa-f]{1,2})", body)]
        names.append((name, len(values)))
        out += "const uint8_t k%s[%d] = {" % (name[0].upper() + name[1:], len(values))
        for i, value in enumerate(values):
            out += ("\n    " if i % 16 == 0 else " ") + "0x%02X," % value
        out += "\n};\n\n"
    assert [n for n, _ in names] == ["CodeStd", "CodeP0", "CodeP1", "CodeP3", "CodeOv", "CodeOvP", "codeEgx0",
                                    "codeEgx1", "codeDepack", "codeDZX0", "codeDZX0_V2", "codeDZX1"], names
    write("src/engine/formats/codes.inc", out.rstrip("\n") + "\n")
    return names


def c_string(text):
    """A C++ string literal of `text`; two question marks would begin a trigraph."""
    text = text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\r", "\\r")
    if "??" in text:
        text = text.replace("?", "\\?")
    return '"%s"' % text


def multilingue():
    """ConvImgCpc's texts: for each name, in its order, the French and the English."""
    text = read("Multilingue.cs")
    found = {}
    order = []
    for key, value in re.findall(r'dic\.Add\("([^"]+)",\s*"((?:[^"\\]|\\.)*)"\);', text):
        name, language = key.rsplit(".", 1)
        if name not in found:
            found[name] = {}
            order.append(name)
        value = re.sub(r"\\(.)", lambda m: {"n": "\n", "r": "\r", "t": "\t"}.get(m.group(1), m.group(1)), value)
        # A caption ConvImgCpc breaks in two for a narrow button: a line end
        # of one character, as Qt's translations keep them.
        found[name][language] = value.replace("\r\n", "\n").replace("\n\r", "\n").replace("\r", "\n")
    return order, found


def context(name):
    """The context of a window's text for Qt's translation: its window's."""
    return "tuxscr::" + name.split(".", 1)[0]


def strings():
    order, found = multilingue()
    out = HEADER % "Multilingue.cs"
    out += "#pragma once\n\n#include \"texts.h\"\n\n"
    out += "// The texts of ConvImgCpc's windows, in English, each marked for Qt's\n"
    out += "// translation in its window's context, with its name there (\"Main.bpLoad\")\n"
    out += "// to tell it from another of the same English. Their French is in\n"
    out += "// translations/tuxscr_fr.ts (make_tables.py --french puts ConvImgCpc's\n"
    out += "// there). Those ConvImgCpc has in French only are TuxSCR's (texts.cpp),\n"
    out += "// which gives them an English.\n"
    out += "namespace tuxscr::texts {\n\n"
    out += "const WindowText kWindowTexts[] = {\n"
    written = 0
    for name in order:
        english = found[name].get("EN")
        if english is None:
            continue
        out += "    QT_TRANSLATE_NOOP3(%s, %s, %s),\n" % (c_string(context(name)), c_string(english), c_string(name))
        written += 1
    out += "};\n\n}  // namespace tuxscr::texts\n"
    write("src/plugin/windowtexts.h", out)
    return written


def french(ts_path):
    """ConvImgCpc's French, as the translation of its texts not yet translated in `ts_path`."""
    import xml.etree.ElementTree as ET
    _, found = multilingue()
    tree = ET.parse(ts_path)
    given = 0
    for message in tree.getroot().iter("message"):
        comment = message.find("comment")
        translation = message.find("translation")
        if comment is None or comment.text not in found or "FR" not in found[comment.text]:
            continue
        if translation.get("type") != "unfinished" and translation.text:
            continue
        translation.text = found[comment.text]["FR"]
        translation.attrib.pop("type", None)
        given += 1
    ET.indent(tree, "    ")
    with open(ts_path, "wb") as file:
        file.write(b'<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE TS>\n')
        tree.write(file, encoding="utf-8", xml_declaration=False)
        file.write(b"\n")
    return given


if __name__ == "__main__":
    if len(sys.argv) > 2 and sys.argv[1] == "--french":
        print(french(sys.argv[2]), "texts given ConvImgCpc's French")
        sys.exit(0)
    patterns()
    print(codes())
    print(strings(), "texts")
