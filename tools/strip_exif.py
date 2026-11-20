#!/usr/bin/env python3
"""Find and remove metadata from JPEG and PNG images before they are published.

Photos from a phone carry EXIF data: camera serial number, date and time, often the GPS position.
PNG files can carry text chunks (software, author, creation time).

What counts as metadata:
  JPEG  APP1 (EXIF, XMP), APP13 (IPTC / Photoshop) and COM segments
  PNG   eXIf, tEXt, zTXt, iTXt and tIME chunks
Image data, colour profiles (JPEG APP2 / PNG iCCP) and everything else are kept byte for byte.

Usage:
    python -I tools/strip_exif.py --check PATH [PATH ...]   list images with metadata; exit 1 if any
    python -I tools/strip_exif.py PATH [PATH ...]           remove the metadata in place
PATH may be a file or a directory (searched recursively; build directories are skipped).
Uses only the Python standard library.
"""
import argparse
import os
import struct
import sys

SKIP_DIRS = {".git", ".pio", ".build_host", "__pycache__", ".venv", "node_modules", "site"}
JPEG_DROP = {0xE1: "APP1 (EXIF/XMP)", 0xED: "APP13 (IPTC)", 0xFE: "COM"}
PNG_DROP = {b"eXIf", b"tEXt", b"zTXt", b"iTXt", b"tIME"}
PNG_SIG = b"\x89PNG\r\n\x1a\n"


class BadImage(Exception):
    pass


def jpeg_clean(data):
    """Return (clean bytes, list of removed segment names)."""
    if data[:2] != b"\xff\xd8":
        raise BadImage("no JPEG start marker")
    out = bytearray(b"\xff\xd8")
    removed = []
    i = 2
    while i < len(data):
        if data[i] != 0xFF:
            raise BadImage("segment marker expected at %d" % i)
        while i < len(data) and data[i] == 0xFF:     # fill bytes
            i += 1
        if i >= len(data):
            raise BadImage("truncated")
        marker = data[i]
        i += 1
        if marker == 0xD9:                            # end of image
            out += b"\xff\xd9"
            out += data[i:]
            return bytes(out), removed
        if 0xD0 <= marker <= 0xD7 or marker == 0x01:  # markers without length
            out += bytes((0xFF, marker))
            continue
        if i + 2 > len(data):
            raise BadImage("truncated segment length")
        length = struct.unpack(">H", data[i:i + 2])[0]
        if length < 2 or i + length > len(data):
            raise BadImage("bad segment length")
        segment = data[i:i + length]
        i += length
        if marker in JPEG_DROP:
            removed.append(JPEG_DROP[marker])
        else:
            out += bytes((0xFF, marker)) + segment
        if marker == 0xDA:                            # start of scan: the rest is image data
            out += data[i:]
            return bytes(out), removed
    raise BadImage("no start of scan")


def png_clean(data):
    if data[:8] != PNG_SIG:
        raise BadImage("no PNG signature")
    out = bytearray(PNG_SIG)
    removed = []
    i = 8
    while i < len(data):
        if i + 12 > len(data):
            raise BadImage("truncated chunk")
        length = struct.unpack(">I", data[i:i + 4])[0]
        ctype = data[i + 4:i + 8]
        end = i + 12 + length
        if end > len(data):
            raise BadImage("bad chunk length")
        if ctype in PNG_DROP:
            removed.append(ctype.decode("ascii"))
        else:
            out += data[i:end]
        i = end
        if ctype == b"IEND":
            out += data[i:]
            return bytes(out), removed
    raise BadImage("no IEND chunk")


def clean(path, data):
    low = path.lower()
    if low.endswith((".jpg", ".jpeg")):
        return jpeg_clean(data)
    if low.endswith(".png"):
        return png_clean(data)
    return None


def files(paths):
    for p in paths:
        if os.path.isfile(p):
            yield p
            continue
        for base, dirs, names in os.walk(p):
            dirs[:] = sorted(d for d in dirs if d not in SKIP_DIRS)
            for n in sorted(names):
                if n.lower().endswith((".jpg", ".jpeg", ".png")):
                    yield os.path.join(base, n)


def main():
    ap = argparse.ArgumentParser(description="Find or remove EXIF and text metadata in JPEG and PNG files.")
    ap.add_argument("paths", nargs="+")
    ap.add_argument("--check", action="store_true", help="only report, change nothing")
    args = ap.parse_args()
    found = errors = checked = 0
    for path in files(args.paths):
        with open(path, "rb") as f:
            data = f.read()
        try:
            result = clean(path, data)
        except BadImage as e:
            print("ERROR %s: %s" % (path, e))
            errors += 1
            continue
        if result is None:
            continue
        checked += 1
        cleaned, removed = result
        if not removed:
            continue
        found += 1
        if args.check:
            print("METADATA %s: %s" % (path, ", ".join(removed)))
        else:
            with open(path, "wb") as f:
                f.write(cleaned)
            print("stripped %s: %s" % (path, ", ".join(removed)))
    print("strip_exif: %d images, %d with metadata%s, %d errors"
          % (checked, found, "" if args.check else " (stripped)", errors))
    if errors or (args.check and found):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
