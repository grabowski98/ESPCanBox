#!/usr/bin/env python3
"""Anonymise a CAN capture before it is shared or committed.

What it changes (everything else is copied byte for byte, line endings included):
  * the DATA of frames whose ID is on the privacy list (default 336, 3B6, 2B6: on PSA AEE2010
    cars these three frames carry the VIN) is overwritten with zeros of the SAME length,
    so the number of lines, the timestamps, the ID and the DLC stay as they were;
  * a comment header that names the serial port ("# start <date> port COM6") is replaced
    with the plain line "# capture".

Line format (candump -L style, as written by the recorder and the decoder console):
    (12.345678) can0 0F6#8E501E33BE5D5D24
Lines that do not match are left alone (UART lines, '#' comments, blank lines).

Usage:
    python -I tools/sanitize_capture.py IN.log OUT.log [--ids 336,3B6,2B6]
    python -I tools/sanitize_capture.py --check FILE.log [...]   exit 1 if a private frame
                                                                 or a port header is left
Uses only the Python standard library.
"""
import argparse
import re
import sys

DEFAULT_IDS = ("336", "3B6", "2B6")

# (time) interface ID#DATA[rest]; ID 3 (standard) or 8 (extended) hex digits
FRAME_RE = re.compile(rb"^(\(\s*[0-9.]+\)\s+\S+\s+)([0-9A-Fa-f]{3}|[0-9A-Fa-f]{8})#([0-9A-Fa-f]*)")
# comment line that names a serial port (COMx on Windows, /dev/tty* elsewhere)
PORT_RE = re.compile(rb"^#.*\bport\s+(COM[0-9]+|/dev/\S+)", re.IGNORECASE)


def parse_ids(text):
    ids = set()
    for part in text.split(","):
        part = part.strip().upper()
        if not part:
            continue
        if not re.fullmatch(r"[0-9A-F]{1,8}", part):
            raise SystemExit("bad CAN ID: " + part)
        ids.add(int(part, 16))
    return ids


def split_eol(line):
    """Return (body, end_of_line) so CRLF / LF / no newline survive unchanged."""
    if line.endswith(b"\r\n"):
        return line[:-2], b"\r\n"
    if line.endswith(b"\n"):
        return line[:-1], b"\n"
    return line, b""


def sanitize_line(line, ids):
    """Return (new_line, changed_kind) where changed_kind is None, 'frame' or 'header'."""
    body, eol = split_eol(line)
    if PORT_RE.match(body):
        return b"# capture" + eol, "header"
    m = FRAME_RE.match(body)
    if m and int(m.group(2), 16) in ids:
        data = m.group(3)
        if data.strip(b"0"):
            new = body[: m.start(3)] + b"0" * len(data) + body[m.end(3):]
            return new + eol, "frame"
    return line, None


def sanitize(src, dst, ids):
    frames = headers = lines = 0
    with open(src, "rb") as fin, open(dst, "wb") as fout:
        for line in fin:
            lines += 1
            new, kind = sanitize_line(line, ids)
            if kind == "frame":
                frames += 1
            elif kind == "header":
                headers += 1
            fout.write(new)
    print("%s -> %s: %d lines, %d frames overwritten, %d headers replaced" % (src, dst, lines, frames, headers))


def check(paths, ids):
    bad = 0
    for path in paths:
        with open(path, "rb") as fin:
            for no, line in enumerate(fin, 1):
                _, kind = sanitize_line(line, ids)
                if kind is not None:
                    bad += 1
                    if bad <= 20:
                        print("%s:%d: %s left" % (path, no, "private frame" if kind == "frame" else "port header"))
    print("check: %d file(s), %d problem(s)" % (len(paths), bad))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description="Overwrite VIN frames and port headers in a CAN capture.")
    ap.add_argument("files", nargs="+", help="IN OUT, or files to check with --check")
    ap.add_argument("--ids", default=",".join(DEFAULT_IDS), help="comma separated CAN IDs (hex)")
    ap.add_argument("--check", action="store_true", help="only report, do not write")
    args = ap.parse_args()
    ids = parse_ids(args.ids)
    if args.check:
        return check(args.files, ids)
    if len(args.files) != 2:
        ap.error("give IN and OUT")
    if args.files[0] == args.files[1]:
        ap.error("IN and OUT must differ")
    sanitize(args.files[0], args.files[1], ids)
    return 0


if __name__ == "__main__":
    sys.exit(main())
