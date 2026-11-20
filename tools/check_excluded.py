#!/usr/bin/env python3
"""Check that nothing from the exclusion list is in the tree that will be published.

The working project of the author holds material that must never be published: raw captures and
logs (VIN, serial numbers, e-mail addresses), videos, third-party firmware and its analysis,
vendor files, decompiled apps, private notes, editor lock files, firmware images built on a
private PC. This tool lists every file or directory of the tree that matches the list below.

Build output that .gitignore keeps out of the repository (.git, .pio, .build_host, __pycache__,
.venv, site) is not checked.

Usage:
    python -I tools/check_excluded.py PATH       exit 0 = nothing excluded found, 1 = hits
Uses only the Python standard library.
"""
import fnmatch
import os
import sys

SKIP_DIRS = {".git", ".pio", ".build_host", "__pycache__", ".venv", "node_modules", "site"}

# Directory names (any depth). Reason in the comment.
EXCLUDED_DIRS = {
    "_staging": "raw material of the publishing work, deleted after use",
    "logs": "raw captures and logs",
    "zrodla": "pictures and files from other people (shops, forums)",
    "wydania": "firmware images built on a private PC",
    "_kopie": "working copies",
    "platform-tools": "third-party tools",
    "diagbox": "dealer diagnostic software data",
    "narzedzia": "working scripts with private paths",
    "teyes_canbus": "third-party app",
    "publikacja": "working notes about publishing",
    "gen": "intermediate files of the KiCad generators",
    ".claude": "assistant settings",
}
# Directory name patterns (any depth).
EXCLUDED_DIR_PATTERNS = {
    "nagrania*": "videos",
    "SavvyCAN*": "third-party binaries",
    "adb_*": "raw radio logs",
    "diagbox_*": "dealer diagnostic printouts",
    "uart_radio_*": "screen photos of the radio",
}
# Path fragments (relative path, '/' separated).
EXCLUDED_PATHS = {
    "firmware/rp5": "third-party CAN box firmware and its analysis",
    "firmware/radio": "decompiled radio apps",
    "docs/dekoder": "working notes of the decoder development",
    "hardware/rp5/zrodla": "pictures from shops and forums",
    "hardware/radio/zdjecia": "photos of the radio screen",
}
# File name patterns.
EXCLUDED_FILES = {
    "STAN.md": "working diary",
    "PLAN*.md": "working plans",
    "ROZKMINY*": "working notes",
    "TODO_SAM.md": "private to-do list",
    "PYTANIA_GEMINI.md": "working notes",
    "CLAUDE.md": "assistant instructions",
    "MEMORY.md": "assistant memory",
    "BAZA_WIEDZY_508.md": "knowledge base with VIN and private data (published only as rewritten pages)",
    "ADB_KOMENDY.md": "contains the radio serial number",
    "KRYTYKA.md": "working notes",
    "AUDYT.md": "working notes",
    "ODSTEPSTWA.md": "working notes",
    "STAN_WYJSCIOWY.md": "working notes",
    "RAPORT_*.md": "working reports",
    "KODY_INTERNET.md": "working notes with quotes from GPL sources",
    "CO_SKOPIOWAC_PRZY_PUBLIKACJI.md": "working notes",
    "test_panel_auth_wynik.txt": "test output with a password run",
    "adb_*.bat": "scripts with the radio serial number",
    "sniffer_zapisz.*": "old Windows-only scripts with private paths",
    "rejestrator_zapisz.*": "old Windows-only scripts",
    "*.lck": "KiCad lock file",
    "*.kicad_prl": "KiCad local view settings",
    "*-bak": "KiCad backup",
    "*.bak": "backup",
    "_autosave-*": "KiCad autosave",
    "fp-info-cache": "KiCad cache",
    "*_drc.rpt": "KiCad report with local paths",
    "*_erc.rpt": "KiCad report with local paths",
    "*.UPDE": "vendor firmware file",
    "*.upde": "vendor firmware file",
    "*.upg": "vendor firmware file",
    "*.apk": "third-party app",
    "*.pac": "radio firmware image",
    "*.img": "firmware image",
    "*.elf": "build output",
    "*.mov": "video",
    "*.MOV": "video",
    "*.mp4": "video",
    "*.mkv": "video",
    "*.zip": "archive (unknown content)",
    "*.rar": "archive (unknown content)",
    "*.7z": "archive (unknown content)",
    "*.java": "decompiled code",
    "*.smali": "decompiled code",
    "ghidra_*": "decompiler log",
}
# *.bin is allowed only here (a 72-byte test fixture cut from an own firmware image).
BIN_ALLOWED = ("firmware/test_data/fixtures/",)


def check(root):
    hits = []
    for base, dirs, files in os.walk(root):
        rel_base = os.path.relpath(base, root).replace(os.sep, "/")
        rel_base = "" if rel_base == "." else rel_base + "/"
        keep = []
        for d in sorted(dirs):
            if d in SKIP_DIRS:
                continue
            rel = rel_base + d
            reason = EXCLUDED_DIRS.get(d)
            if reason is None:
                for pat, why in EXCLUDED_DIR_PATTERNS.items():
                    if fnmatch.fnmatchcase(d, pat):
                        reason = why
                        break
            if reason is None:
                for frag, why in EXCLUDED_PATHS.items():
                    if rel == frag or rel.endswith("/" + frag):
                        reason = why
                        break
            if reason:
                hits.append((rel + "/", reason))
            else:
                keep.append(d)
        dirs[:] = keep
        for name in sorted(files):
            rel = rel_base + name
            reason = None
            for pat, why in EXCLUDED_FILES.items():
                if fnmatch.fnmatchcase(name, pat):
                    reason = why
                    break
            if reason is None and name.lower().endswith(".bin") and not rel.startswith(BIN_ALLOWED):
                reason = "firmware image (only the test fixture is allowed)"
            if reason:
                hits.append((rel, reason))
    return hits


def main():
    if len(sys.argv) != 2 or sys.argv[1].startswith("-"):
        print(__doc__)
        return 2
    root = sys.argv[1]
    if not os.path.isdir(root):
        print("no such directory: " + root, file=sys.stderr)
        return 2
    hits = check(root)
    for rel, reason in hits:
        print("EXCLUDED %s: %s" % (rel, reason))
    print("check_excluded: %d hits" % len(hits))
    return 1 if hits else 0


if __name__ == "__main__":
    sys.exit(main())
