#!/usr/bin/env python3
"""Layering check of the firmware libraries.

    python -I tools/check_layering.py        exit code 0 = no violation

Layers (lib/<name>/, every library one directory):
    core       car- and radio-independent logic        may include: core
    car_*      a car profile (CAN identifiers, frames)  may include: core, itself
    radio_*    a radio protocol (UART frames)           may include: core, itself
    app        glue between a car and a radio           may include: everything in lib/

Rules (each violation = error, exit code 1):
    1. core includes nothing from car_*, radio_*, app;
    2. radio_* includes nothing from car_* (in particular not can_ids.h) and nothing from app;
    3. car_* includes nothing from radio_* and nothing from app;
    4. no file in lib/ includes a framework or SDK header: '#include <...>' may name only a C standard
       header (STD_HEADERS below) or a file in lib/, and '#include "..."' must name a file in lib/ or in
       include/ (Arduino.h, WString.h, HardwareSerial.h, esp_*.h, freertos/... are errors);
    5. every header name exists in only one library (all libraries share one flat include path);
    6. no file in lib/ includes a file from src/ (e.g. src/config.h) or from outside the firmware tree;
    7. no '#include MACRO' in lib/ (the target cannot be checked);
    8. names of the other side in code (comments and strings removed, #define lines included): lib/car_*
       uses no XP name (the word xp, box command names BOX_*, RADIO_*), lib/radio_* no CAN-side name (the
       words psa and vset, frame identifiers ID_*, CanFrame), lib/core and include/ none of the words xp,
       psa, vset. Car data reach a radio only as the car-neutral types of lib/core/vehicle_signals.h (F4).
    9. the radio path of the glue is protocol-neutral: the Decoder does not name the XP framing, parser or
       frame numbers (xp::encode, xp::Parser, xp::makeFrame, xp::HEADER, xp::ACK_*, xp::BOX_*, xp::RADIO_*,
       xp::buildParkingFrame, xp::buildObstacleFlag, parseRadioCmd; also written 'xp :: NAME' or split over
       lines) and makes no alias of the xp namespace ('namespace q = xp', 'using namespace xp', 'using xp::').
       Checked files: lib/app/decoder.*, every other file of lib/app whose name does not start with 'xp_'
       (the XP glue: xp_car, xp_keys, xp_settings), and any file in lib/ that defines a Decoder:: member.
       Every frame to or from the radio goes through the RadioProtocol of the DecoderConfig (encodeFrame,
       rxParser, encode..., decodeRadioFrame, decodeCommand) and the common session lib/core/radio_session.h,
       so a foreign protocol never gets an XP frame. (The core side is covered by rule 8: lib/core has no
       word xp, in any letter case.)
   10. an '#include' names a header (HEADER_EXT below), never a documentation or data file (.txt, .json,
       .md ...): those files are not scanned, so code hidden in them would pass every rule.

Files checked: every file in lib/ and include/ except documentation and library metadata (DOC_EXT); a
file with any other extension (.inc, .cc, .ipp ...) is a source file. Headers in include/ follow the core
rules (they may include only core, include/ and C standard headers), so they cannot pass a car or radio
header on. Before matching, the text is brought to what the compiler sees: comments removed
('#/**/include' is an include), backslash line continuations joined, the digraph '%:' and the trigraph
'??=' read as '#'. '#include', '#include_next' and '#import' are all include directives. A digit separator
(1'000) is not taken for the start of a character literal (found in a code review: these tricks used to
pass the check).

Both '#include "..."' and '#include <...>' lines are checked (a direct include, not the transitive closure);
the lookup order follows the compiler: the including file's directory (quotes only), then every library,
then include/. Not checked: numeric CAN identifiers written as literals in lib/radio_*, and numeric XP
key codes in lib/car_* (the default key remaps of a car profile are codes of the radio protocol named in
CarProfile::keyRemapProtocol, car_profile.h; the Decoder uses them only with that protocol).
Reads files only; no dependencies outside the Python standard library.
"""
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
LIB = os.path.join(ROOT, "lib")

# Rule 8: names that belong to the other side. Matched as whole words in code without comments and strings.
# Whole words, so an alias through a macro ('#define NS xp' then 'NS::Frame') is caught at the #define.
FOREIGN_NAMES = {
    "car": re.compile(r"\b(?i:xp)\b|\bBOX_[A-Z0-9_]+|\bRADIO_[A-Z0-9_]+"),
    "radio": re.compile(r"\b(?i:psa|vset)\b|\bID_[A-Z0-9_]+|\bCanFrame\b"),
    "core": re.compile(r"\b(?i:xp|psa|vset)\b"),
}
# Files exempt from rule 8, with the reason. Keep this list short; every entry is a reviewed decision.
# Rule 9: XP names that must not appear in the radio path of the Decoder (matched in code without comments).
RADIO_PATH_FILES = ("lib/app/decoder.h", "lib/app/decoder.cpp")
XP_RADIO_PATH = re.compile(r"\bxp\s*::\s*(encode|Parser|ParserStats|makeFrame|checksum|HEADER|ACK_[A-Z_]+|BOX_[A-Z0-9_]+|"
                           r"RADIO_[A-Z0-9_]+|buildParkingFrame|buildObstacleFlag|encodePower|encodeKey|"
                           r"encodeVersion|decodeRadioFrame|decodeCommand)\b|\bparseRadioCmd\b|"
                           r"\bnamespace\s+\w+\s*=\s*(::\s*)?xp\b|\busing\s+namespace\s+(::\s*)?xp\b|"
                           r"\busing\s+(::\s*)?xp\s*::")
DECODER_MEMBER = re.compile(r"\bDecoder\s*::\s*~?\w+\s*\(")
NAME_EXCEPTIONS = {
    # Old names psa::ParkingAid, psa::BodyState ... as aliases of the car-neutral types (frozen 0.6.0 code).
    "lib/core/compat/legacy_060.h",
}


def digit_separator(text, i):
    """True when the quote at text[i] is a C++14 digit separator (1'000, 0x1'F): the token before it is a
    number, i.e. starts with a digit. A character literal prefix (L'x', u8'x') starts with a letter."""
    j = i
    while j > 0 and (text[j - 1].isalnum() or text[j - 1] in "_.'"):
        j -= 1
    return j < i and text[j].isdigit()


def preprocess(text):
    """What the preprocessor reads, line count kept: backslash continuations joined (the joined line is
    followed by empty lines), the digraph '%:' and the trigraph '??=' read as '#'."""
    text = text.replace("??=", "#").replace("%:", "#")
    out = []
    buf = ""
    pending = 0
    for line in text.split("\n"):
        if line.endswith("\\"):
            buf += line[:-1]
            pending += 1
            continue
        out.append(buf + line)
        out.extend([""] * pending)
        buf, pending = "", 0
    if pending:
        out.append(buf)
        out.extend([""] * (pending - 1))
    return "\n".join(out)


def code_only(text):
    """Text without // and /* */ comments and without string and character literals (line count kept)."""
    text = preprocess(text)
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "'" and digit_separator(text, i):
            out.append(c)
            i += 1
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("\n" * text.count("\n", i, j))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            out.append(c + c)
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def strip_comments(text):
    """Text without // and /* */ comments (a block comment becomes one space, as for the compiler; line
    count kept). String and character literals stay, so '#include "a//b.h"' keeps its name."""
    text = preprocess(text)
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "'" and digit_separator(text, i):
            out.append(c)
            i += 1
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(" " + "\n" * text.count("\n", i, j))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


INCLUDE_RE = re.compile(r'^\s*#\s*(?:include|include_next|import)\s*(["<])([^">]+)[">]')
ANY_INCLUDE_RE = re.compile(r'^\s*#\s*(?:include|include_next|import)\b')
INCLUDE_DIR = os.path.join(ROOT, "include")
SRC = os.path.join(ROOT, "src")
# C standard headers a library may include with angle brackets (both the .h and the <c...> spelling).
STD_HEADERS = {"assert.h", "ctype.h", "errno.h", "float.h", "inttypes.h", "limits.h", "math.h", "stdarg.h",
               "stdbool.h", "stddef.h", "stdint.h", "stdio.h", "stdlib.h", "string.h"}
STD_HEADERS |= {"c" + h[:-2] for h in STD_HEADERS}
# Not source code: every other file in lib/ and include/ is scanned (.inc, .cc, .ipp ... included).
DOC_EXT = (".md", ".json", ".txt", ".properties")
HEADER_EXT = (".h", ".hh", ".hpp", ".hxx", ".inc", ".ipp", ".tpp")


def layer(lib_name):
    if lib_name in ("core", "include"):      # include/ (project_identity.h): the same rules as core
        return "core"
    if lib_name == "app":
        return "app"
    if lib_name.startswith("car_"):
        return "car"
    if lib_name.startswith("radio_"):
        return "radio"
    return "unknown"


def allowed(src_layer, src_lib, dst_layer, dst_lib):
    if dst_lib == src_lib or dst_layer == "core":
        return True
    if src_layer == "app":
        return True
    return False


def rel(path):
    return os.path.relpath(path, LIB).replace(os.sep, "/")


def shown(path):
    """Path for messages, relative to the firmware directory (lib/..., include/...)."""
    return os.path.relpath(path, ROOT).replace(os.sep, "/")


def main():
    if not os.path.isdir(LIB):
        print("FAIL   no directory %s" % LIB)
        return 2
    libs = sorted(d for d in os.listdir(LIB) if os.path.isdir(os.path.join(LIB, d)))
    files = []          # (library, absolute path)
    by_name = {}        # header base name -> [relative paths]
    roots = [(l, os.path.join(LIB, l)) for l in libs]
    if os.path.isdir(INCLUDE_DIR):
        roots.append(("include", INCLUDE_DIR))
    for lib_name, top in roots:
        for dirpath, _dirs, names in os.walk(top):
            for n in sorted(names):
                if not n.lower().endswith(DOC_EXT):
                    p = os.path.join(dirpath, n)
                    files.append((lib_name, p))
                    if n.endswith(HEADER_EXT):
                        by_name.setdefault(n, []).append(shown(p))

    errors = 0
    unknown = [l for l in libs if layer(l) == "unknown"]
    for l in unknown:
        print("ERROR  lib/%s: unknown layer (name must be core, app, car_* or radio_*)" % l)
        errors += 1

    for n, paths in sorted(by_name.items()):
        if len(paths) > 1:
            print("ERROR  header %s exists in more than one library: %s" % (n, ", ".join(paths)))
            errors += 1

    lib_dirs = [os.path.join(LIB, l) for l in libs]
    for lib_name, path in files:
        src_layer = layer(lib_name)
        with open(path, encoding="utf-8") as fh:
            lines = strip_comments(fh.read()).splitlines()
        for no, line in enumerate(lines, 1):
            m = INCLUDE_RE.match(line)
            if not m:
                if ANY_INCLUDE_RE.match(line):
                    print("ERROR  %s:%d: include through a macro (target cannot be checked)" % (shown(path), no))
                    errors += 1
                continue
            quoted, inc = m.group(1) == '"', m.group(2)
            # The same lookup order as the compiler: the including file's directory (quotes only), then
            # every library, then include/. Any existing file is found, also one outside lib/.
            target = None
            bases = ([os.path.dirname(path)] if quoted else []) + lib_dirs + [INCLUDE_DIR]
            for base in bases:
                cand = os.path.normpath(os.path.join(base, inc))
                if os.path.isfile(cand):
                    target = cand
                    break
            if target is None:
                if quoted or inc not in STD_HEADERS:
                    print("ERROR  %s:%d: includes %s%s%s (not a C standard header, not a project file;"
                          " framework and SDK headers are not allowed in lib/)"
                          % (shown(path), no, m.group(1), inc, '"' if quoted else ">"))
                    errors += 1
                continue
            if not target.lower().endswith(HEADER_EXT):
                print("ERROR  %s:%d: includes %s, which is not a header (%s); such files are not checked"
                      % (shown(path), no, inc, ", ".join(HEADER_EXT)))
                errors += 1
                continue
            if target.startswith(INCLUDE_DIR + os.sep):
                # include/project_identity.h (project identity, no hardware). Files in include/ are scanned
                # themselves with the core rules, so they cannot pass a car or radio header on.
                continue
            if not target.startswith(LIB + os.sep):
                where = "src/" if target.startswith(SRC + os.sep) else "outside lib/"
                print("ERROR  %s:%d: includes %s (%s)" % (shown(path), no, inc, where))
                errors += 1
                continue
            dst_lib = rel(target).split("/")[0]
            dst_layer = layer(dst_lib)
            if allowed(src_layer, lib_name, dst_layer, dst_lib):
                continue
            print("ERROR  %s:%d: %s layer includes lib/%s (%s layer)"
                  % (shown(path), no, src_layer, rel(target), dst_layer))
            errors += 1

    for lib_name, path in files:
        rule = FOREIGN_NAMES.get(layer(lib_name))
        if not rule or shown(path) in NAME_EXCEPTIONS:
            continue
        with open(path, encoding="utf-8") as fh:
            code = code_only(fh.read())
        for no, line in enumerate(code.splitlines(), 1):
            if ANY_INCLUDE_RE.match(line):
                continue      # include lines: rules 1-7 (#define and other directives are checked here)
            m = rule.search(line)
            if m:
                print("ERROR  %s:%d: %s layer uses the name %s of the other side"
                      % (shown(path), no, layer(lib_name), m.group(0)))
                errors += 1

    # Rule 9 files: the named Decoder files, the rest of lib/app except the XP glue (xp_*), and every file
    # that defines a Decoder member.
    radio_path = []
    for rel_path in RADIO_PATH_FILES:
        path = os.path.join(ROOT, rel_path)
        if not os.path.isfile(path):
            print("ERROR  %s: missing (rule 9 checks it)" % rel_path)
            errors += 1
            continue
        radio_path.append(os.path.normpath(path))
    for lib_name, path in files:
        if os.path.normpath(path) in radio_path:
            continue
        with open(path, encoding="utf-8") as fh:
            code = code_only(fh.read())
        if (lib_name == "app" and not os.path.basename(path).startswith("xp_")) or DECODER_MEMBER.search(code):
            radio_path.append(os.path.normpath(path))
    for path in radio_path:
        rel_path = shown(path)
        with open(path, encoding="utf-8") as fh:
            code = code_only(fh.read())
        for no, line in enumerate(code.splitlines(), 1):
            if ANY_INCLUDE_RE.match(line):
                continue
            m = XP_RADIO_PATH.search(line)
            if m:
                print("ERROR  %s:%d: the radio path of the Decoder names the XP protocol (%s); go through"
                      " RadioProtocol (lib/core/radio_protocol.h)" % (rel_path, no, m.group(0)))
                errors += 1

    print("libraries: %s; files: %d; errors: %d" % (", ".join(libs), len(files), errors))
    if errors:
        print("Layering check FAILED.")
        return 1
    print("Layering OK.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
