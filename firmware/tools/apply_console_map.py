#!/usr/bin/env python3
"""Console and API texts from Polish to English through one word map (tools/console_map.tsv).

HISTORICAL: a one-off tool of the move from the author's private tree (stage E3). No script or CI
uses it; it needs the Polish tree, which is not part of this repository.

The map is a list of fixed text fragments, not whole lines. It is applied the same way to
string literals in the sources and to the reference outputs of the host tests, so the proof
that the translation changed only words is mechanical:

    references of the Polish tree, put through the map == references of the English tree

and the files *.radio_tx.txt / *.outputs.txt (what goes to the radio and to the 12 V outputs)
keep the same bytes (same sha256), only their names may change through 'file' rows.

Map rows (TAB separated, '#' starts a comment line):
    lit     PL  EN [PATH]  a string literal as written in C/C++/Python source (escapes and printf
                     conversions included). It is cut at the conversions (%u, %02X, %-26s, %%...)
                     and at the escapes (\\n, \\"...); both sides must have the same conversions and
                     escapes in the same order (printf arguments stay valid). Each changed piece
                     becomes a fragment. With PATH (relative to the firmware directory) the
                     row applies only to the literals of that one source file and not to the
                     references (test data that must differ from a console word of the same spelling).
    frag    PL  EN   a fragment used everywhere (sources and references)
    out     PL  EN   a fragment used only in the references (padded table columns, lines that
                     tools/run_host_tests.sh writes itself)
    file    PL  EN   a fragment of a reference file name (test_host/golden*/ variants)
    manual  PATH WHY a source file whose code was also edited by hand in the translation (command aliases), or a
                     reference of a new test; --check reports it but does not fail on it

Fragments are placed longest first over the whole text (a shorter fragment never cuts into a longer
one, wherever it starts), each piece of text is replaced at most once, and only as whole words: a fragment that
starts (ends) with a letter or digit does not match right after (before) a letter, digit or '_'.
In sources, the letter of an escape (\\n, \\t, \\r) does not count as a letter. A piece of a 'lit' row
that touches a printf conversion has no whole-word rule on that side (the printed value stands there).

Usage (from the firmware directory):
    python -I tools/apply_console_map.py --apply            rewrites the literals in lib/ src/
                                                            test_host/ tools/*.py and the references
                                                            (one time, on the Polish tree)
    python -I tools/apply_console_map.py --check --base DIR DIR = firmware tree before the translation
    python -I tools/apply_console_map.py --refs --base DIR  only the references again: DIR put through
                                                            the map (when a row is added after --apply)
    python -I tools/apply_console_map.py --scan             literals and reference lines that still
                                                            contain a word of the Polish side
Exit code: 0 = OK, 1 = difference or problem, 2 = bad usage. Python standard library only.
"""
import argparse
import hashlib
import io
import os
import re
import sys
import tokenize

HERE = os.path.dirname(os.path.abspath(__file__))
MAP = os.path.join(HERE, "console_map.tsv")
SELF = "tools/apply_console_map.py"
SRC_DIRS = ("lib", "src", "test_host")
C_EXT = {".c", ".cpp", ".h", ".hpp"}
REF_DIRS = ("test_host/expected", "test_host/golden", "test_host/golden_060")
KEEP_BYTES = (".radio_tx.txt", ".outputs.txt")       # must not change at all

# printf conversion or escape sequence: the points where a literal is cut into pieces
CUT = re.compile(r"%[-+ #0]*(?:\*|\d+)?(?:\.(?:\*|\d+))?(?:hh|h|ll|l|z|j|t|L)?[diouxXeEfgGcspn%]"
                 r"|\\(?:x[0-9A-Fa-f]+|[0-7]{1,3}|.)")
WORD = re.compile(r"[A-Za-z]{3,}")


class MapError(Exception):
    pass


# Pieces that touch a printf conversion (%u, %s...) on the left / right side. In the output the value of
# the conversion stands there (often a digit or a letter), so the whole-word rule is not used on that side.
OPEN_START, OPEN_END = set(), set()


# ---------- the map ----------
def split_pieces(s):
    """('piece', 'cut', 'piece', ...) of a literal; pieces may be empty."""
    out, pos = [], 0
    for m in CUT.finditer(s):
        out.append(s[pos:m.start()])
        out.append(m.group(0))
        pos = m.end()
    out.append(s[pos:])
    return out


def load_map(path):
    frags, outs, files, manual = {}, {}, {}, {}
    scoped = {}                      # {source path: {pl: en}}
    origin = {}

    def put(table, pl, en, where):
        if pl in table and table[pl] != en:
            raise MapError("%s: '%s' -> '%s', but %s already gave '%s'" % (where, pl, en, origin[(id(table), pl)], table[pl]))
        table[pl] = en
        origin[(id(table), pl)] = where

    with open(path, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\r\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            where = "%s:%d" % (os.path.basename(path), n)
            if len(parts) == 4 and parts[0] == "lit":
                kind, pl, en, only = parts
            elif len(parts) == 3:
                kind, pl, en = parts
                only = None
            else:
                raise MapError("%s: expected 'kind TAB pl TAB en' (lit: optional TAB path)" % where)
            if kind == "lit":
                target = scoped.setdefault(only, {}) if only else frags
                a, b = split_pieces(pl), split_pieces(en)
                if len(a) != len(b) or a[1::2] != b[1::2]:
                    raise MapError("%s: conversions/escapes differ: %s | %s" % (where, a[1::2], b[1::2]))
                for k in range(0, len(a), 2):
                    x, y = a[k], b[k]
                    if x == y:
                        continue
                    if not re.search(r"[A-Za-z]", x):
                        raise MapError("%s: changed piece without letters: '%s' -> '%s'" % (where, x, y))
                    put(target, x, y, where)
                    if k > 0 and a[k - 1].startswith("%") and a[k - 1] != "%%":
                        OPEN_START.add(x)
                    if k + 1 < len(a) and a[k + 1].startswith("%") and a[k + 1] != "%%":
                        OPEN_END.add(x)
            elif kind == "frag":
                put(frags, pl, en, where)
            elif kind == "out":
                put(outs, pl, en, where)
            elif kind == "file":
                put(files, pl, en, where)
            elif kind == "manual":
                manual[pl] = en
            else:
                raise MapError("%s: unknown kind '%s'" % (where, kind))
    for pl in outs:
        if pl in frags and frags[pl] != outs[pl]:
            raise MapError("out row '%s' contradicts a fragment" % pl)
    return frags, outs, files, manual, scoped


def table_for(rel, frags, scoped, cache):
    """Fragment table and regex for one source file (global rows + rows scoped to the file)."""
    if rel not in scoped:
        if None not in cache:
            cache[None] = (frags, compile_map(frags))
        return cache[None]
    if rel not in cache:
        t = dict(frags)
        t.update(scoped[rel])
        cache[rel] = (t, compile_map(t))
    return cache[rel]


def compile_map(table):
    """Fragments longest first, each with its whole-word rule (pre, post)."""
    if not table:
        return None
    out = []
    for pl in sorted(table, key=lambda s: (-len(s), s)):
        pre = pl[0].isalnum() and pl not in OPEN_START
        post = pl[-1].isalnum() and pl not in OPEN_END
        out.append((pl, pre, post))
    return out


def _word_char(c):
    return c.isalnum() or c == "_"


def subst(rx, table, s):
    """Replace the fragments in s. The longest fragment anywhere in s is placed first, then the next
    longest in what is left, and so on: a short fragment never cuts into a longer one, wherever it starts."""
    if rx is None or not s:
        return s
    taken = bytearray(len(s))
    picks = []
    for pl, pre, post in rx:
        n = len(pl)
        i = s.find(pl)
        while i >= 0:
            j = i + n
            ok = True
            if pre and i > 0 and _word_char(s[i - 1]) and not (i >= 2 and s[i - 2] == "\\" and s[i - 1] in "nrt"):
                ok = False
            if ok and post and j < len(s) and _word_char(s[j]):
                ok = False
            if ok and any(taken[i:j]):
                ok = False
            if ok:
                taken[i:j] = b"\x01" * n
                picks.append((i, j, pl))
                i = s.find(pl, j)
            else:
                i = s.find(pl, i + 1)
    if not picks:
        return s
    picks.sort()
    parts, pos = [], 0
    for i, j, pl in picks:
        parts.append(s[pos:i])
        parts.append(table[pl])
        pos = j
    parts.append(s[pos:])
    return "".join(parts)


# ---------- string literals in sources ----------
def c_literals(text):
    """[(start, end)] of the contents of C/C++ string literals (comments and char literals skipped)."""
    spans = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
            continue
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
            continue
        m = re.match(r'(?:u8|u|U|L)?R"([^(\s"]*)\(', text[i:i + 40])
        if m and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == "_")):
            end = text.find(")" + m.group(1) + '"', i + m.end())
            if end < 0:
                raise MapError("unterminated raw string at offset %d" % i)
            spans.append((i + m.end(), end))
            i = end + len(m.group(1)) + 2
            continue
        if c == '"' or c == "'":
            j = i + 1
            while j < n and text[j] != c:
                if text[j] == "\\":
                    j += 1
                if j < n and text[j] == "\n":
                    raise MapError("unterminated literal at offset %d" % i)
                j += 1
            if c == '"':
                spans.append((i + 1, j))
            i = j + 1
            continue
        i += 1
    return spans


def py_literals(text):
    """[(start, end)] of the contents of Python string literals (f-string parts are not included)."""
    lines = text.splitlines(keepends=True)
    offs = [0]
    for ln in lines:
        offs.append(offs[-1] + len(ln))
    spans = []
    for t in tokenize.generate_tokens(io.StringIO(text).readline):
        if t.type != tokenize.STRING:
            continue
        a = offs[t.start[0] - 1] + t.start[1]
        b = offs[t.end[0] - 1] + t.end[1]
        raw = text[a:b]
        m = re.match(r"[A-Za-z]*('''|\"\"\"|'|\")", raw)
        q = m.group(1)
        spans.append((a + m.end(), b - len(q)))
    return spans


def literal_spans(path, text):
    ext = os.path.splitext(path)[1]
    if ext in C_EXT:
        return c_literals(text)
    if ext == ".py":
        return py_literals(text)
    return []


def source_files(root):
    out = []
    for d in SRC_DIRS:
        for dp, dirs, files in os.walk(os.path.join(root, d)):
            dirs[:] = sorted(x for x in dirs if x not in ("expected", "golden", "golden_060", ".pio", "__pycache__"))
            for f in sorted(files):
                if os.path.splitext(f)[1] in C_EXT:
                    out.append(os.path.relpath(os.path.join(dp, f), root).replace("\\", "/"))
    tdir = os.path.join(root, "tools")
    for f in sorted(os.listdir(tdir)) if os.path.isdir(tdir) else []:
        rel = "tools/" + f
        if f.endswith(".py") and rel != SELF:
            out.append(rel)
    return out


def read(path):
    with open(path, encoding="utf-8", newline="") as f:
        return f.read()


def write(path, text):
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)


def map_source(path, text, rx, table):
    spans = literal_spans(path, text)
    parts, pos = [], 0
    for a, b in spans:
        parts.append(text[pos:a])
        parts.append(subst(rx, table, text[a:b]))
        pos = b
    parts.append(text[pos:])
    return "".join(parts)


# ---------- references ----------
def ref_files(root):
    out = []
    for d in REF_DIRS:
        p = os.path.join(root, d)
        if os.path.isdir(p):
            out += [d + "/" + f for f in sorted(os.listdir(p)) if f.endswith(".txt")]
    return out


def map_name(rel, rxf, files):
    d, f = rel.rsplit("/", 1)
    return d + "/" + subst(rxf, files, f)


def sha(b):
    return hashlib.sha256(b).hexdigest()


# ---------- commands ----------
def cmd_apply(root, m):
    frags, outs, files, _, scoped = m
    cache = {}
    both = dict(frags)
    both.update(outs)
    rxo = compile_map(both)
    rxf = compile_map(files)
    changed = 0
    for rel in source_files(root):
        p = os.path.join(root, rel)
        t = read(p)
        table, rx = table_for(rel, frags, scoped, cache)
        u = map_source(rel, t, rx, table)
        if u != t:
            write(p, u)
            changed += 1
    renamed = mapped = 0
    for rel in ref_files(root):
        p = os.path.join(root, rel)
        new = map_name(rel, rxf, files)
        if not rel.endswith(KEEP_BYTES):
            t = read(p)
            u = subst(rxo, both, t)
            if u != t:
                write(p, u)
                mapped += 1
        if new != rel:
            os.replace(p, os.path.join(root, new))
            renamed += 1
    print("apply_console_map: %d source files, %d references changed, %d references renamed" % (changed, mapped, renamed))
    return 0


def cmd_refs(root, base, m):
    """The references of the tree written again from the references of --base, through the map."""
    frags, outs, files, manual, _ = m
    both = dict(frags)
    both.update(outs)
    rxo = compile_map(both)
    rxf = compile_map(files)
    for rel in ref_files(root):
        if rel not in manual:              # references of new tests stay
            os.remove(os.path.join(root, rel))
    n = 0
    for rel in ref_files(base):
        with open(os.path.join(base, rel), "rb") as f:
            a = f.read()
        if not rel.endswith(KEEP_BYTES):
            a = subst(rxo, both, a.decode("utf-8")).encode("utf-8")
        with open(os.path.join(root, map_name(rel, rxf, files)), "wb") as f:
            f.write(a)
        n += 1
    print("apply_console_map --refs: %d references written from %s" % (n, base))
    return 0


def cmd_check(root, base, m):
    frags, outs, files, manual, scoped = m
    cache = {}
    both = dict(frags)
    both.update(outs)
    rxo = compile_map(both)
    rxf = compile_map(files)
    bad = 0
    # 1. references
    base_refs = ref_files(base)
    expect_names = set()
    same_text = same_bytes = 0
    for rel in base_refs:
        new = map_name(rel, rxf, files)
        expect_names.add(new)
        pn = os.path.join(root, new)
        if not os.path.isfile(pn):
            print("MISSING %s (from %s)" % (new, rel))
            bad += 1
            continue
        with open(os.path.join(base, rel), "rb") as f:
            a = f.read()
        with open(pn, "rb") as f:
            b = f.read()
        if rel.endswith(KEEP_BYTES):
            if sha(a) != sha(b):
                print("CHANGED %s: sha256 %s != %s (must stay identical)" % (new, sha(a), sha(b)))
                bad += 1
            else:
                same_bytes += 1
            continue
        u = subst(rxo, both, a.decode("utf-8")).encode("utf-8")
        if u != b:
            ul, bl = u.decode("utf-8").split("\n"), b.decode("utf-8").split("\n")
            k = next((i for i in range(min(len(ul), len(bl))) if ul[i] != bl[i]), min(len(ul), len(bl)))
            print("DIFF    %s line %d\n    map(base): %s\n    new:       %s" % (
                new, k + 1, ul[k] if k < len(ul) else "<end>", bl[k] if k < len(bl) else "<end>"))
            bad += 1
        else:
            same_text += 1
    for rel in ref_files(root):
        if rel not in expect_names:
            if rel in manual:
                print("NEWREF  %s (%s)" % (rel, manual[rel]))
                continue
            print("EXTRA   %s (no file in base maps to it)" % rel)
            bad += 1
    # 2. sources: literals of the base put through the map == literals of the new tree
    src_same = src_manual = 0
    for rel in source_files(root):
        pb = os.path.join(base, rel)
        if not os.path.isfile(pb):
            print("NEW     %s (not in base)" % rel)
            continue
        tb, tn = read(pb), read(os.path.join(root, rel))
        table, rx = table_for(rel, frags, scoped, cache)
        la = [subst(rx, table, tb[a:b]) for a, b in literal_spans(rel, tb)]
        lb = [tn[a:b] for a, b in literal_spans(rel, tn)]
        if la == lb:
            src_same += 1
            continue
        k = next((i for i in range(min(len(la), len(lb))) if la[i] != lb[i]), min(len(la), len(lb)))
        what = "literal %d: map(base) '%s', new '%s'" % (k + 1, la[k] if k < len(la) else "<none>",
                                                         lb[k] if k < len(lb) else "<none>")
        if rel in manual:
            src_manual += 1
            print("MANUAL  %s (%s): %d/%d literals, first difference %s" % (rel, manual[rel], len(la), len(lb), what))
        else:
            print("SOURCE  %s: %s" % (rel, what))
            bad += 1
    print("apply_console_map --check: references %d mapped equal, %d byte-identical (radio_tx/outputs); "
          "sources %d equal, %d edited by hand; %d problems" % (same_text, same_bytes, src_same, src_manual, bad))
    return 1 if bad else 0


def cmd_scan(root, m):
    frags, outs, _, _, scoped = m
    pl_words, en_words = set(), set()
    for table in [frags, outs] + list(scoped.values()):
        for pl, en in table.items():
            pl_words |= {w.lower() for w in WORD.findall(pl)}
            en_words |= {w.lower() for w in WORD.findall(en)}
    polish = pl_words - en_words
    hits = 0
    for rel in source_files(root):
        t = read(os.path.join(root, rel))
        for a, b in literal_spans(rel, t):
            ws = {w.lower() for w in WORD.findall(t[a:b])} & polish
            if ws:
                line = t.count("\n", 0, a) + 1
                print("%s:%d: %s   [%s]" % (rel, line, t[a:b][:120], " ".join(sorted(ws))))
                hits += 1
    for rel in ref_files(root):
        if rel.endswith(KEEP_BYTES):
            continue
        for k, line in enumerate(read(os.path.join(root, rel)).split("\n"), 1):
            ws = {w.lower() for w in WORD.findall(line)} & polish
            if ws:
                print("%s:%d: %s   [%s]" % (rel, k, line[:120], " ".join(sorted(ws))))
                hits += 1
    print("apply_console_map --scan: %d places with words of the Polish side" % hits)
    return 1 if hits else 0


def main():
    try:
        sys.stdout.reconfigure(errors="backslashreplace")
    except (AttributeError, ValueError):
        pass
    ap = argparse.ArgumentParser(description="Apply or check the Polish -> English console word map.")
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--apply", action="store_true", help="rewrite literals and references in place")
    g.add_argument("--check", action="store_true", help="compare with the tree before E3 (--base)")
    g.add_argument("--scan", action="store_true", help="list places that still contain Polish words")
    g.add_argument("--refs", action="store_true", help="write the references again from --base through the map")
    ap.add_argument("--base", help="firmware tree before E3 (for --check)")
    ap.add_argument("--root", default=os.path.dirname(HERE), help="firmware tree (default: this one)")
    ap.add_argument("--map", default=MAP, help="map file (default tools/console_map.tsv)")
    args = ap.parse_args()
    try:
        m = load_map(args.map)
        if args.apply:
            return cmd_apply(args.root, m)
        if args.check or args.refs:
            if not args.base or not os.path.isdir(args.base):
                print("--check and --refs need --base DIR (the firmware tree before E3)", file=sys.stderr)
                return 2
            if args.refs:
                return cmd_refs(args.root, args.base, m)
            return cmd_check(args.root, args.base, m)
        return cmd_scan(args.root, m)
    except MapError as e:
        print("apply_console_map: %s" % e, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
