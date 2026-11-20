#!/usr/bin/env python3
"""Look for private data in the repository tree before anything is published.

Text files are scanned line by line, binary files through their printable strings (like
`strings`, runs of 4+ printable ASCII bytes). File and directory names are scanned too.
Build output that never goes into the repository (.git, .pio, .build_host, __pycache__) is skipped.

Rules (all are errors unless noted):
  vin            a VIN of the PSA group (VF3 + 14 characters)
  vin-frame      a CAN frame 336#, 3B6# or 2B6# with non-zero data (these frames carry the VIN
                 on PSA AEE2010 cars; tools/sanitize_capture.py overwrites them with zeros)
  email          an e-mail address (except example.com / example.org / users.noreply.github.com, and
                 except in the verbatim third-party licence texts under LICENSES/third-party/ or
                 licenses/third-party/ of a release archive: their copyright lines name the upstream
                 authors and must not be changed; every other rule still applies there)
  user-path      a Windows profile path (drive letter, colon, slash, "Users")
  mac            a MAC address written with ':' or '-'
  ssid-suffix    the device access point name with the real 4-hex-digit suffix of a unit
                 (documentation placeholders A1B2, ABCD, 0000, 1234 are allowed)
  literal        a known private value of the author's car or devices (serial numbers, VIN parts,
                 part numbers). Kept here only as SHA-256 prefixes, so this file does not publish
                 them; extra values can be given in a local file (--extra, never committed).
  owner-name     the author's first name (also hashed); with --warn-only it is a warning in files
                 under firmware/ and extras/ (code comments are translated in stage E2)
With --warn-only every hit under _staging/ is a warning too: that directory is raw material for
stage E10, is never published and is removed in E10 (tools/check_excluded.py checks that).

Usage:
    python -I tools/scan_private.py PATH [--warn-only] [--extra FILE] [--quiet]
Exit code: 0 = no errors (warnings allowed), 1 = errors found, 2 = bad usage.
Uses only the Python standard library.
"""
import argparse
import hashlib
import os
import re
import sys

SKIP_DIRS = {".git", ".pio", ".build_host", "__pycache__", ".venv", "node_modules", "site"}

# --- plain patterns (safe to publish: they name no private value) ---
VIN_RE = re.compile(r"VF3[0-9A-Z]{14}")
VIN_FRAME_RE = re.compile(r"(?<![0-9A-Fa-f])(336|3B6|2B6)#([0-9A-Fa-f]+)")
EMAIL_RE = re.compile(r"[A-Za-z0-9._%+-]+@([A-Za-z0-9-]+\.)+[A-Za-z]{2,}")
EMAIL_OK = ("example.com", "example.org", "users.noreply.github.com")
# Verbatim upstream licence texts (repository: LICENSES/third-party/, release archive: licenses/third-party/).
THIRD_PARTY_RE = re.compile(r"(^|/)licenses/third-party/", re.IGNORECASE)
USER_PATH_RE = re.compile(r"[A-Za-z]:(\\\\|\\|/)+Users", re.IGNORECASE)
MAC_RE = re.compile(r"(?<![0-9A-Fa-f:-])([0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2}(?![0-9A-Fa-f:-])")
def _ssid_prefixes():
    """The access point prefix of the prototype firmware (historical) and the current one (project.json)."""
    prefixes = ["Centralka-508-"]
    try:
        import json
        with open(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "project.json"),
                  encoding="utf-8") as f:
            cur = json.load(f).get("ap_ssid_prefix")
        if isinstance(cur, str) and cur and cur not in prefixes:
            prefixes.append(cur)
    except (OSError, ValueError):
        pass
    return prefixes


SSID_RE = re.compile(r"(?:%s)([0-9A-F]{4})(?![0-9A-Za-z])" % "|".join(re.escape(p) for p in _ssid_prefixes()))
SSID_PLACEHOLDERS = ("A1B2", "ABCD", "0000", "1234")

# --- hashed private values: (rule, length, character class, sha256(lower(value))[:32]) ---
# The scanner checks every substring of the given length inside runs of the character class.
CLASSES = {
    "alpha": re.compile(r"[A-Za-z]+"),
    "digit": re.compile(r"[0-9]+"),
    "hex": re.compile(r"[0-9A-Fa-f]+"),
    "alnum": re.compile(r"[0-9A-Za-z]+"),
}
HASHED = [
    ("owner-name", 7, "alpha", "ff06535ac1029cca2fc2b86ac7355a7b"),
    ("literal", 8, "hex", "612c0c62419117f12493d56a05ca426a"),       # radio serial (prefix)
    ("literal", 9, "digit", "33c9eb6c23d46f1d4baab62fc170a60b"),     # battery part number
    ("literal", 8, "digit", "61aef485a26c18bad5f5278a01056f4a"),     # device identifier (prefix)
    ("literal", 8, "alnum", "effbd0599571c1822e0610e6df9b0b16"),     # VIN part
    ("literal", 6, "alnum", "aac521ba9eee7885a4beebaa6dcdeb31"),     # VIN part
    ("literal", 16, "hex", "48c1a2114ea5faf9c71518475799cf63"),      # VIN part, hex encoded
    ("literal", 12, "hex", "44a0f9571a8b0f8768f9b92d4a6a3ca8"),      # VIN part, hex encoded
]

STRINGS_RE = re.compile(rb"[\x20-\x7e]{4,}")


def h(text):
    return hashlib.sha256(text.lower().encode("utf-8")).hexdigest()[:32]


class Scanner:
    def __init__(self, root, warn_only, extra, quiet):
        self.root = root
        self.warn_only = warn_only
        self.quiet = quiet
        self.errors = 0
        self.warnings = 0
        self.files = 0
        hashed = list(HASHED)
        for value in extra:
            cls = "digit" if value.isdigit() else "hex" if re.fullmatch(r"[0-9A-Fa-f]+", value) else \
                "alpha" if value.isalpha() else "alnum"
            hashed.append(("literal", len(value), cls, h(value)))
        # group by (class, length) -> {hash: rule}
        self.groups = {}
        for rule, length, cls, digest in hashed:
            self.groups.setdefault((cls, length), {})[digest] = rule
        self.memo = {}

    def report(self, path, where, rule, snippet):
        rel = os.path.relpath(path, self.root).replace(os.sep, "/")
        top = rel.split("/")[0]
        warn = self.warn_only and (top == "_staging" or (rule == "owner-name" and top in ("firmware", "extras")))
        if warn:
            self.warnings += 1
        else:
            self.errors += 1
        if not self.quiet or not warn:
            masked = snippet if len(snippet) <= 4 else snippet[:2] + "*" * (len(snippet) - 4) + snippet[-2:]
            print("%s %s:%s: %s (%s)" % ("WARN " if warn else "ERROR", rel, where, rule, masked))

    def hashed_hits(self, text):
        hits = []
        for (cls, length), table in self.groups.items():
            for m in CLASSES[cls].finditer(text):
                run = m.group(0)
                if len(run) < length:
                    continue
                for i in range(len(run) - length + 1):
                    sub = run[i:i + length].lower()
                    key = (cls, sub)
                    rule = self.memo.get(key)
                    if rule is None:
                        rule = table.get(h(sub), "")
                        self.memo[key] = rule
                    if rule:
                        hits.append((rule, run[i:i + length]))
        return hits

    def check_text(self, path, where, text):
        for m in VIN_RE.finditer(text):
            self.report(path, where, "vin", m.group(0))
        for m in VIN_FRAME_RE.finditer(text):
            if m.group(2).strip("0"):
                self.report(path, where, "vin-frame", m.group(0))
        rel = os.path.relpath(path, self.root).replace(os.sep, "/")
        upstream_text = THIRD_PARTY_RE.search(rel) is not None
        for m in EMAIL_RE.finditer(text):
            if not m.group(0).lower().endswith(EMAIL_OK) and not upstream_text:
                self.report(path, where, "email", m.group(0))
        for m in USER_PATH_RE.finditer(text):
            self.report(path, where, "user-path", m.group(0))
        for m in MAC_RE.finditer(text):
            self.report(path, where, "mac", m.group(0))
        for m in SSID_RE.finditer(text):
            if m.group(1) not in SSID_PLACEHOLDERS:
                self.report(path, where, "ssid-suffix", m.group(0))
        for rule, value in self.hashed_hits(text):
            self.report(path, where, rule, value)

    def scan_file(self, path):
        self.files += 1
        self.check_text(path, "name", os.path.basename(path))
        try:
            with open(path, "rb") as f:
                data = f.read()
        except OSError as e:
            print("ERROR %s: cannot read (%s)" % (path, e))
            self.errors += 1
            return
        text = None
        if b"\x00" not in data:
            try:
                text = data.decode("utf-8")
            except UnicodeDecodeError:
                text = None
        if text is not None:
            for no, line in enumerate(text.splitlines(), 1):
                self.check_text(path, str(no), line)
        else:
            for m in STRINGS_RE.finditer(data):
                self.check_text(path, "@%d" % m.start(), m.group(0).decode("ascii"))

    def scan(self):
        if os.path.isfile(self.root):
            path = self.root
            self.root = os.path.dirname(path) or "."
            self.scan_file(path)
            return
        for base, dirs, files in os.walk(self.root):
            dirs[:] = sorted(d for d in dirs if d not in SKIP_DIRS)
            for d in dirs:
                self.check_text(os.path.join(base, d), "name", d)
            for name in sorted(files):
                self.scan_file(os.path.join(base, name))


def main():
    ap = argparse.ArgumentParser(description="Scan a tree for private data before publishing.")
    ap.add_argument("path", help="directory (or file) to scan")
    ap.add_argument("--warn-only", action="store_true",
                    help="author's first name under firmware/ and extras/ is a warning, not an error")
    ap.add_argument("--extra", help="local file with more private values, one per line (not committed)")
    ap.add_argument("--quiet", action="store_true", help="print errors only")
    args = ap.parse_args()
    if not os.path.exists(args.path):
        print("no such path: " + args.path, file=sys.stderr)
        return 2
    extra = []
    if args.extra:
        with open(args.extra, encoding="utf-8") as f:
            extra = [line.strip() for line in f if line.strip() and not line.startswith("#")]
    root = os.path.abspath(args.path)
    s = Scanner(root, args.warn_only, extra, args.quiet)
    s.scan()
    print("scan_private: %d files, %d errors, %d warnings" % (s.files, s.errors, s.warnings))
    return 1 if s.errors else 0


if __name__ == "__main__":
    sys.exit(main())
