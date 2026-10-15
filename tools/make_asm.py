#!/usr/bin/env python3
"""Turns the parts of ConvImgCpc that write assembler sources into C++.

    tools/make_asm.py [ConvImgCpc's folder]

ConvImgCpc writes its sources line by line, thousands of them, with the
picture's figures put in here and there (SaveAsm.cs, GenSplitAsm.cs). The
lines are not typed again: this turns each C# method into the same C++
function, text for text. It writes
    src/formats/asmgen_decl.inc   the functions' declarations
    src/formats/asmgen.inc        the functions
which src/formats/asmgen.h and asmgen.cpp include; what the functions are
written with (Text, AsmText, hex, dec...) is in asmtext.h.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SOURCE = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "ConvImgCpc", "ConvImgCpc")

# The methods that are not taken: they open files, and are written by hand.
SKIPPED = {"OpenAsm", "CloseAsm", "CreeAsm"}

# Lines C++ does not read as C# does, put right before anything else.
PATCHES = [
    ("Convert.ToInt32(Math.Max(1, Math.Min(255, speed[i] / 3.333333)))",
     "roundToInt(std::max(1.0, std::min(255.0, speed[i] / 3.333333)))"),
    ("private readonly static string CpcVGA", "//"),
    ("private readonly static byte[] bufPack", "//"),
    # GenSplitAsm has ConvImgCpc's letters for the colours in a copy of its own.
    ("int col = CpcVGA[", "int col = Cpc.CpcVGA["),
]

STRING_PARAMS = set()


def read(name):
    with open(os.path.join(SOURCE, name), encoding="utf-8-sig") as file:
        return file.read().replace("\r\n", "\n")


def match_back(text, at):
    """The start of the expression that ends just before text[at]."""
    i = at - 1
    while i >= 0:
        c = text[i]
        if c in ")]":
            depth = 0
            closer, opener = c, "(" if c == ")" else "["
            while i >= 0:
                if text[i] == closer:
                    depth += 1
                elif text[i] == opener:
                    depth -= 1
                    if depth == 0:
                        break
                i -= 1
            i -= 1
        elif c.isalnum() or c in "_.":
            i -= 1
        else:
            break
    return i + 1


def split_literals(text):
    """The text cut into what is code and what is a string or a character."""
    parts = []
    i = 0
    start = 0
    while i < len(text):
        c = text[i]
        if c in "\"'":
            j = i + 1
            while j < len(text) and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            parts.append(("code", text[start:i]))
            parts.append(("str" if c == '"' else "chr", text[i:j + 1]))
            i = j + 1
            start = i
        else:
            i += 1
    parts.append(("code", text[start:]))
    return parts


def strip_comments(text):
    out = ""
    for line in text.split("\n"):
        cut = None
        quote = None
        i = 0
        while i < len(line):
            c = line[i]
            if quote:
                if c == "\\":
                    i += 1
                elif c == quote:
                    quote = None
            elif c in "\"'":
                quote = c
            elif line.startswith("//", i):
                cut = i
                break
            i += 1
        out += (line if cut is None else line[:cut].rstrip()) + "\n"
    return out[:-1]


def protect(text):
    """Strings put aside, so that code can be changed without touching them."""
    kept = []
    out = ""
    for kind, part in split_literals(text):
        if kind == "code":
            out += part
        else:
            kept.append((kind, part))
            out += "\x00%d\x00" % (len(kept) - 1)
    return out, kept


def restore(text, kept):
    def back(match):
        kind, part = kept[int(match.group(1))]
        if kind == "chr":
            return part
        if part == '"\\r\\n"':
            return "NL"
        return "S(" + part + ")"
    return re.sub("\x00(\\d+)\x00", back, text)


def indexers(text, names):
    """NAME[a, b, c] as NAME[a][b][c]."""
    for name in names:
        at = 0
        while True:
            found = re.compile(r"\b" + name + r"\[").search(text, at)
            if not found:
                break
            i = found.end()
            depth = 1
            pieces = [""]
            while depth:
                c = text[i]
                if c in "[(":
                    depth += 1
                elif c in "])":
                    depth -= 1
                    if depth == 0:
                        break
                if c == "," and depth == 1:
                    pieces.append("")
                else:
                    pieces[-1] += c
                i += 1
            new = name + "".join("[" + p.strip() + "]" for p in pieces)
            text = text[:found.start()] + new + text[i + 1:]
            at = found.start() + len(name) + 1
    return text


def finish_strings(text, kept):
    def done(match):
        receiver, fmt = match.group(1), match.group(2)
        if not fmt:
            if "CpcVGA[" in receiver and "(int)" not in receiver:
                return "chr(" + receiver + ")"
            return "dec(" + receiver + ")"
        literal = kept[int(fmt.strip("\x00"))][1].strip('"')
        if literal.startswith("X"):
            return "hex(%s, %s)" % (receiver, literal[1:])
        if set(literal) == {"0"}:
            return "dec(%s, %d)" % (receiver, len(literal))
        raise SystemExit("a format this does not know: " + literal)
    previous = None
    while previous != text:
        previous = text
        text = re.sub("\x01([^\x01\x02]*)\x02([^\x03]*)\x03", done, text)
    return text


TYPES = [
    (r"\bStreamWriter\s+(\w+)", r"AsmText& \1"),
    (r"\bMain\.PackMethode\.(\w+)", r"PackMethod::\1"),
    (r"\bMain\.PackMethode\b", "PackMethod"),
    (r"\bint\[,,\]\s+(\w+)", r"const SplitPalette& \1"),
    (r"\bint\[,\]\s+(\w+)", r"const LineTable& \1"),
    (r"\bint\[\]\s+(\w+)\s*=\s*new int\[(\d+)\]\s*\{", r"const int \1[\2] = {"),
    (r"\bint\[\]\s+(\w+)", r"const int* \1"),
    (r"\bbyte\[\]\s+(\w+)", r"const uint8_t* \1"),
    (r"\bParam\s+(\w+)", r"const Param& \1"),
    (r"\bbyte\b", "uint8_t"),
    (r"\bstring\.IsNullOrEmpty\((\w+)\)", r"\1.empty()"),
    (r"\bstring\b", "Text"),
    (r"\bEnvironment\.NewLine\b", "NL"),
    (r"\bMath\.Min\b", "std::min"),
    (r"\bMath\.Max\b", "std::max"),
    (r"\.Length\b", ".Length()"),
    (r"\bSaveAsm\.", ""),
]


def method_bodies(source):
    """Each static method: its return type, name, parameters and body."""
    for found in re.finditer(r"^\s*static\s+(?:public|private)\s+([\w\[\]]+)\s+(\w+)\(([^)]*)\)\s*\{", source, flags=re.M):
        i = found.end()
        depth = 1
        in_string = None
        while depth:
            c = source[i]
            if in_string:
                if c == "\\":
                    i += 1
                elif c == in_string:
                    in_string = None
            elif c == "/" and source[i + 1] == "/":
                i = source.index("\n", i)
            elif c in "\"'":
                in_string = c
            elif c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
            i += 1
        yield found.group(1), found.group(2), found.group(3), source[found.end():i - 1]


def convert(code, writer):
    code = strip_comments(code)
    code, kept = protect(code)
    code = re.sub(r"^\s*#(region|endregion)[^\n]*\n", "", code, flags=re.M)
    # x.ToString(...)
    while True:
        found = re.search(r'\.ToString\((\x00\d+\x00)?\)', code)
        if not found:
            break
        start = match_back(code, found.start())
        code = code[:start] + ("\x01%s\x02%s\x03" % (code[start:found.start()], found.group(1) or "")) + code[found.end():]
    code = finish_strings(code, kept)
    for pattern, replacement in TYPES:
        code = re.sub(pattern, replacement, code)
    def null_test(match):
        name, op = match.group(1), match.group(2)
        if name in STRING_PARAMS:
            return ("!" if op == "!=" else "") + name + ".isNull()"
        return name + " " + op + " nullptr"
    code = re.sub(r"\b(\w+)\s*(!=|==)\s*null\b", null_test, code)
    code = re.sub(r"=\s*null\b", "= Text()", code)
    code = indexers(code, ["trameM1", "colMode5", "palette", "spritesHard"])
    code = re.sub(r"\bCpc\.", writer + ".Cpc.", code)
    return restore(code, kept)


def parameters(text, with_defaults):
    out = []
    for parameter in [p.strip() for p in re.split(r",(?![,\]])", text) if p.strip()]:
        default = None
        if "=" in parameter:
            parameter, default = [p.strip() for p in parameter.split("=", 1)]
        if " " not in parameter:
            raise SystemExit("a parameter this does not read: %r in (%s)" % (parameter, text))
        kind, name = parameter.rsplit(" ", 1)
        if kind == "string":
            STRING_PARAMS.add(name)
            kind = "const Text&"
            if default == "null":
                default = "Text()"
        elif kind == "StreamWriter":
            kind = "AsmText&"
        elif kind == "Main.PackMethode":
            kind = "PackMethod"
        elif kind == "int[,,]":
            kind = "const SplitPalette&"
        elif kind == "int[,]":
            kind = "const LineTable&"
        elif kind == "int[]":
            kind = "const int*"
            default = "nullptr" if default == "null" else default
        elif kind == "byte[]":
            kind = "const uint8_t*"
        elif kind == "Param":
            kind = "const Param&"
        elif kind == "BitmapCpc":
            kind = "const SplitScreen&"
        out.append("%s %s%s" % (kind, name, " = " + default if with_defaults and default is not None else ""))
    return ", ".join(out)


def main():
    declarations = "// Made by tools/make_asm.py from ConvImgCpc's sources: not to be changed by hand.\n\n"
    definitions = declarations
    for name in ["SaveAsm.cs", "GenSplitAsm.cs"]:
        source = read(name)
        for old, new in PATCHES:
            source = source.replace(old, new)
        declarations += "// " + name + "\n"
        definitions += "// ---- " + name + " ----\n\n"
        # GenSplitAsm has functions of the same names as SaveAsm's: they are
        # kept apart.
        inner = name == "GenSplitAsm.cs"
        if inner:
            declarations += "namespace split {\n"
            definitions += "namespace split {\n\n"
        for kind, method, params, body in method_bodies(source):
            if method in SKIPPED:
                continue
            STRING_PARAMS.clear()
            kind = {"string": "Text", "void": "void", "bool": "bool", "int": "int"}[kind]
            # The names of the string parameters are known before the body is read.
            parameters(params, False)
            strings = set(STRING_PARAMS)
            writer = re.search(r"StreamWriter\s+(\w+)", params)
            STRING_PARAMS.update(strings)
            text = convert(body, writer.group(1) if writer else "sw")
            declarations += "%s %s(%s);\n" % (kind, method, parameters(params, True))
            definitions += "%s %s(%s)\n{%s\n}\n\n" % (kind, method, parameters(params, False), text.rstrip())
        if inner:
            declarations += "}  // namespace split\n"
            definitions += "}  // namespace split\n\n"
        declarations += "\n"
    with open(os.path.join(ROOT, "src", "formats", "asmgen_decl.inc"), "w", encoding="utf-8") as file:
        file.write(declarations.rstrip("\n") + "\n")
    with open(os.path.join(ROOT, "src", "formats", "asmgen.inc"), "w", encoding="utf-8") as file:
        file.write(definitions.rstrip("\n") + "\n")
    print("src/formats/asmgen_decl.inc", "src/formats/asmgen.inc")


if __name__ == "__main__":
    main()
