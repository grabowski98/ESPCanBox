#!/usr/bin/env python3
"""Proof that moving pins into boards/board_*.h changed no value seen by the firmware.

src/config.h is run through the C++ preprocessor (Arduino.h replaced by an empty stub: only macros
matter here) in four flag sets: listen, decoder, network, decoder + network + CAN transmit.
For every set the script compares:
  1. macros (-E -dM): every macro of the reference must exist with the same FULLY EXPANDED value
     (e.g. HAS_VBAT_DIVIDER -> BOARD_HAS_VBAT_DIVIDER -> 1 equals 1); new macros only with prefix BOARD_;
  2. code (-E): the token stream coming from src/config.h and include/project_identity.h must be
     identical (so "const int PIN_OUT_ACC = 12 ;" stays "... = 12 ;" after expansion).
     Tokens from boards/ (static_assert of the pin rules) are not compared.

Reference:
  --base DIR        a copy of the firmware tree from before the change (needs src/config.h and
                    include/project_identity.h); compared in full (1 and 2).
  snapshot file     boards/board_proto_v1.snapshot.txt: resolved pin and board values of the prototype
                    (always checked; written again only with --write-snapshot, after a deliberate change).

  python -I tools/check_board_macros.py [--base DIR] [--board PROTO_V1] [--write-snapshot]

Preprocessor: variable CXX (e.g. CXX=g++), otherwise clang from the ziglang package via uvx, as in
tools/run_host_tests.sh. Exit code 0 = identical, 1 = difference, 2 = tool error.
"""
import argparse
import os
import re
import shlex
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
FW = os.path.dirname(HERE)
SNAPSHOT_DEFAULT = os.path.join(FW, "boards", "board_proto_v1.snapshot.txt")

# Flag sets of the platformio.ini environments (only what config.h reads).
VARIANTS = [
    ("listen", []),
    ("decoder", ["WORK_MODE=MODE_DECODER"]),
    ("listen_wifi", ["NET_ENABLED=1"]),
    ("decoder_tx_wifi", ["WORK_MODE=MODE_DECODER", "NET_ENABLED=1", "CAN_TX_MODE=CAN_TX_SETTINGS"]),
]
# Names that belong to the board (snapshot): pins, the 12 V divider.
BOARD_NAME_RE = re.compile(r"^(PIN_\w+|VBAT_DIVIDER|HAS_VBAT_DIVIDER)$")
TOKEN_RE = re.compile(
    r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_]\w*|\d[\w.]*(?:[eEpP][+-]\d+)?\w*|'
    r"::|->|<<=|>>=|<<|>>|<=|>=|==|!=|&&|\|\||\+\+|--|[-+*/%&|^!~<>=?:;,.()\[\]{}#]"
)
LINEMARK_RE = re.compile(r'^#\s*(?:line\s+)?\d+\s+"((?:\\.|[^"\\])*)"')


def compiler_cmd():
    cxx = os.environ.get("CXX", "").strip()
    if cxx:
        return shlex.split(cxx)
    return ["uvx", "--offline", "--from", "ziglang", "python", "-I", "-m", "ziglang", "c++"]


def run_pp(args):
    cmd = compiler_cmd() + args
    try:
        r = subprocess.run(cmd, stdin=subprocess.DEVNULL, capture_output=True, text=True,
                           encoding="utf-8", errors="replace")
    except OSError as e:
        sys.exit("ERROR  cannot run the preprocessor %s: %s" % (cmd[0], e))
    if r.returncode != 0:
        sys.stderr.write(r.stderr)
        sys.exit("ERROR  preprocessor failed: %s" % " ".join(cmd))
    return r.stdout


def base_args(root, stub, defines, board):
    a = ["-x", "c++", "-std=c++17", "-I", stub, "-I", os.path.join(root, "include"),
         "-I", os.path.join(root, "boards"), "-I", os.path.join(root, "src")]
    # config.h includes the car profile and radio protocol headers from lib/<library>/.
    libdir = os.path.join(root, "lib")
    if os.path.isdir(libdir):
        for name in sorted(os.listdir(libdir)):
            if os.path.isdir(os.path.join(libdir, name)):
                a += ["-I", os.path.join(libdir, name)]
    if board:
        a.append("-DBOARD=" + board)
    a += ["-D" + d for d in defines]
    return a


def parse_macros(text):
    """name -> (is_function_like, definition text)"""
    out = {}
    for line in text.splitlines():
        m = re.match(r"#define\s+([A-Za-z_]\w*)(\(?)(.*)$", line)
        if m:
            out[m.group(1)] = (m.group(2) == "(", (m.group(2) + m.group(3)).strip())
    return out


def tokens(s):
    return TOKEN_RE.findall(s)


def preprocess(root, stub, tmp, defines, board, builtin):
    """Returns (macros {name: value}, code tokens {file: [tokens]})."""
    cfg = os.path.join(root, "src", "config.h")
    if not os.path.isfile(cfg):
        sys.exit("ERROR  no %s" % cfg)
    args = base_args(root, stub, defines, board)
    raw = parse_macros(run_pp(args + ["-E", "-dM", cfg]))
    names = sorted(n for n in raw if n not in builtin)
    macros = {}
    obj = [n for n in names if not raw[n][0]]
    for n in names:
        if raw[n][0]:
            macros[n] = "fn" + raw[n][1]
    # Full expansion of object-like macros: one probe line per macro after including config.h.
    probe = os.path.join(tmp, "probe.cpp")
    with open(probe, "w", encoding="utf-8") as f:
        f.write('#include "config.h"\n')
        for i, n in enumerate(obj):
            f.write("@@BM%d@@ %s\n" % (i, n))
    out = run_pp(args + ["-E", "-P", probe])
    seen = {}
    for line in out.splitlines():
        m = re.match(r"\s*@@BM(\d+)@@\s?(.*)$", line)
        if m:
            seen[obj[int(m.group(1))]] = " ".join(tokens(m.group(2)))
    for n in obj:
        if n not in seen:
            sys.exit("ERROR  macro %s missing in probe output" % n)
        macros[n] = seen[n]
    # Code tokens per source file (line markers of -E).
    code = {}
    cur = None
    rootn = os.path.normcase(os.path.abspath(root))
    for line in run_pp(args + ["-E", cfg]).splitlines():
        m = LINEMARK_RE.match(line)
        if m:
            path = m.group(1).replace("\\\\", "\\")
            ap = os.path.normcase(os.path.abspath(path)) if not path.startswith("<") else path
            cur = os.path.relpath(ap, rootn).replace("\\", "/") if ap.startswith(rootn) else None
            continue
        if line.startswith("#") or cur is None:
            continue
        code.setdefault(cur, []).extend(tokens(line))
    return macros, code


def board_items(macros, code):
    """Snapshot lines: board macros and 'const T NAME = ... ;' declarations from src/config.h."""
    items = []
    for n in sorted(macros):
        if BOARD_NAME_RE.match(n):
            items.append("macro %s = %s" % (n, macros[n]))
    toks = code.get("src/config.h", [])
    i = 0
    while i < len(toks):
        if toks[i] == "const" and "=" in toks[i:i + 6]:
            j = toks.index("=", i)
            name = toks[j - 1]
            k = toks.index(";", j)
            if BOARD_NAME_RE.match(name):
                items.append("const %s %s = %s" % (" ".join(toks[i + 1:j - 1]), name, " ".join(toks[j + 1:k])))
            i = k + 1
        else:
            i += 1
    return items


def compare(label, ref, new):
    bad = 0
    rm, rc = ref
    nm, nc = new
    for n in sorted(rm):
        if n not in nm:
            print("DIFF   %s: macro %s removed" % (label, n))
            bad += 1
        elif rm[n] != nm[n]:
            print("DIFF   %s: macro %s = %r, was %r" % (label, n, nm[n], rm[n]))
            bad += 1
    for n in sorted(set(nm) - set(rm)):
        if n != "BOARD" and not n.startswith("BOARD_"):
            print("DIFF   %s: new macro %s without prefix BOARD_" % (label, n))
            bad += 1
    files = sorted(f for f in set(rc) | set(nc) if not f.startswith("boards/"))
    for f in files:
        a, b = rc.get(f, []), nc.get(f, [])
        if a != b:
            k = next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), min(len(a), len(b)))
            print("DIFF   %s: code of %s differs at token %d: was %r, now %r"
                  % (label, f, k, " ".join(a[max(0, k - 6):k + 6]), " ".join(b[max(0, k - 6):k + 6])))
            bad += 1
    nmac = len(rm)
    ntok = sum(len(rc.get(f, [])) for f in files)
    if not bad:
        print("OK     %s: %d macros, %d code tokens identical" % (label, nmac, ntok))
    return bad


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--base", help="firmware tree from before the change (reference)")
    ap.add_argument("--board", default="PROTO_V1", help="value of -DBOARD for the checked tree")
    ap.add_argument("--snapshot", default=SNAPSHOT_DEFAULT)
    ap.add_argument("--write-snapshot", action="store_true")
    a = ap.parse_args()

    bad = 0
    with tempfile.TemporaryDirectory() as tmp:
        stub = os.path.join(tmp, "stub")
        os.mkdir(stub)
        open(os.path.join(stub, "Arduino.h"), "w").close()
        empty = os.path.join(tmp, "empty.cpp")
        open(empty, "w").close()
        builtin = set(parse_macros(run_pp(["-x", "c++", "-std=c++17", "-E", "-dM", empty])))

        snap = []
        for label, defs in VARIANTS:
            new = preprocess(FW, stub, tmp, defs, a.board, builtin)
            if a.base:
                ref = preprocess(os.path.abspath(a.base), stub, tmp, defs, None, builtin)
                bad += compare(label, ref, new)
            snap += ["%s %s" % (label, s) for s in board_items(*new)]

    snap_text = "\n".join(snap) + "\n"
    if a.write_snapshot and bad:
        print("RESULT %d difference(s): snapshot not written" % bad)
        return 1
    if a.write_snapshot:
        with open(a.snapshot, "w", encoding="utf-8", newline="\n") as f:
            f.write("# Resolved board values (tools/check_board_macros.py --write-snapshot). Do not edit by hand.\n")
            f.write(snap_text)
        print("WROTE  %s (%d lines)" % (os.path.relpath(a.snapshot, FW), len(snap)))
    else:
        try:
            with open(a.snapshot, encoding="utf-8") as f:
                want = [l.rstrip("\n") for l in f if not l.startswith("#")]
        except OSError:
            sys.exit("ERROR  no snapshot %s (create it with --write-snapshot)" % a.snapshot)
        got = snap_text.splitlines()
        if want != got:
            for l in sorted(set(want) - set(got)):
                print("DIFF   snapshot: missing  %s" % l)
            for l in sorted(set(got) - set(want)):
                print("DIFF   snapshot: new      %s" % l)
            bad += 1
        else:
            print("OK     snapshot %s: %d board values identical" % (os.path.basename(a.snapshot), len(got)))
    print("RESULT %s" % ("identical" if not bad else "%d difference(s)" % bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
