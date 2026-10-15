#!/usr/bin/env python3
"""Takes ConvImgCpc's tables out of its sources, as C++.

    tools/make_tables.py [ConvImgCpc's folder]

Writes, from the C# files of ConvImgCpc/ConvImgCpc:
    src/engine/patterns.inc   the eight sets of patterns of mode ASC-UT
    src/formats/codes.inc     the Z80 routines put in the screens it saves
    src/plugin/strings.inc    the windows' texts, in French and in English

The tables are copied, not typed again: a byte of a routine that is wrong is
a screen that does not show.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SOURCE = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "ConvImgCpc", "ConvImgCpc")


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
    write("src/engine/patterns.inc", out)


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
    write("src/formats/codes.inc", out.rstrip("\n") + "\n")
    return names


def strings():
    text = read("Multilingue.cs")
    found = {}
    order = []
    for key, value in re.findall(r'dic\.Add\("([^"]+)",\s*"((?:[^"\\]|\\.)*)"\);', text):
        name, language = key.rsplit(".", 1)
        if name not in found:
            found[name] = {}
            order.append(name)
        # Two question marks and a bracket are a trigraph to a C++ compiler.
        found[name][language] = value.replace("?", "\\?") if "??" in value else value
    out = HEADER % "Multilingue.cs"
    out += "// A text of a window: its name there, then French and English. One that\n"
    out += "// ConvImgCpc has in one language only is null in the other.\n"
    out += "const WindowText kWindowTexts[] = {\n"
    for name in order:
        french = found[name].get("FR")
        english = found[name].get("EN")
        out += '    {"%s", %s, %s},\n' % (name, '"%s"' % french if french is not None else "nullptr",
                                           '"%s"' % english if english is not None else "nullptr")
    out += "};\n"
    write("src/plugin/strings.inc", out)
    return len(order)


if __name__ == "__main__":
    patterns()
    print(codes())
    print(strings(), "texts")
