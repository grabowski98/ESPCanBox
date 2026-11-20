#!/usr/bin/env python3
"""Prove that an edit changed only comments (or, with --strings-only, only string literals).

Two trees are compared file by file (same relative path). Each file is reduced to a stream that
ignores what the edit is allowed to change, and the streams must be identical:

  default mode     comments and layout are ignored, every code token must stay the same
                   (used when comments are translated)
  --strings-only   like the default mode, and in addition the text of string literals is ignored;
                   the number and position of the literals must stay the same (console
                   texts changed through tools/console_map.tsv, nothing else)

How each language is reduced:
  .c .cc .cpp .h .hpp .ino .js   tokens without // and /* */ comments; string, character, raw
                                 string and template literals are single tokens; adjacent string
                                 literals are merged (so "a" "b" may become "ab" in --strings-only)
  .css                           tokens without /* */ comments
  .py                            the AST (ast.dump without positions); docstrings are removed,
                                 comments are not part of the AST anyway
  .sh                            tokens without # comments (the shebang line is kept as is)
  .ini                           lines without ';' / '#' comments, whitespace collapsed

Files present in only one tree are listed but are not an error (a new tool, a file moved in a
later stage). A renamed file is compared with its old name in --base: the 'file' rows of
tools/rename_map.tsv (--renames, '' = none) give old path TAB new path, relative to the firmware directory. A file whose code changed on purpose in an earlier stage can be pinned in an allow
file (--allow, default tools/check_comment_only_allow.tsv next to this script):
    <relative path> TAB <mode: code|strings> TAB <sha256 of the reduced stream of the NEW file> TAB <reason>
Such a file passes only while its reduced stream still has exactly that hash, so a later edit of
its code is still caught. --print-hash prints the hashes for an allow file.

Usage (from the firmware directory):
    python -I tools/check_comment_only.py --base OLD --new .
    python -I tools/check_comment_only.py --base OLD --new . --strings-only test_host/ref_glue_060.h
    python -I tools/check_comment_only.py --base OLD --new . --print-hash src/version.h
Non-ASCII text in the printed tokens is shown with backslash escapes (works on any console code page).
Positional PATHS (files or directories, relative to --new) limit the comparison.
The allow files and tools/rename_map.tsv in this directory are the record of the one-off move
from the author's private tree (stages E0-E3); a new comment-only edit needs neither.
Exit code: 0 = no code change, 1 = code changed in at least one file, 2 = bad usage.
Uses only the Python standard library.
"""
import argparse
import ast
import hashlib
import os
import re
import sys

SKIP_DIRS = {".git", ".pio", ".build_host", "__pycache__", ".venv", "node_modules", "generated"}
C_LIKE = {".c", ".cc", ".cpp", ".h", ".hpp", ".ino"}
EXTENSIONS = C_LIKE | {".js", ".css", ".py", ".sh", ".ini"}

STR = "<str>"          # placeholder for a string literal in --strings-only mode


class ReduceError(Exception):
    pass


# ---------- C, C++, JavaScript, CSS ----------
_PUNCT3 = ("<<=", ">>=", "...", "->*", "===", "!==", "**=", "&&=", "||=", "??=", ">>>")
_PUNCT2 = ("::", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", "&&", "||", "+=", "-=",
           "*=", "/=", "%=", "&=", "|=", "^=", "##", "=>", "**", "??", "?.")
# A '/' starts a JavaScript regular expression when the previous token cannot end an expression.
_JS_REGEX_AFTER_WORDS = {"return", "typeof", "instanceof", "in", "of", "new", "delete", "void",
                         "throw", "case", "do", "else", "yield", "await"}


def _scan_quoted(text, i, quote):
    """Index just past the closing quote of a literal starting at text[i] == quote."""
    n = len(text)
    j = i + 1
    while j < n:
        c = text[j]
        if c == "\\":
            j += 2
            continue
        if c == quote:
            return j + 1
        if c == "\n" and quote != "`":
            raise ReduceError("unterminated literal at offset %d" % i)
        j += 1
    raise ReduceError("unterminated literal at offset %d" % i)


def _scan_template(text, i):
    """JavaScript template literal `...${expr}...`; nested braces and literals inside ${} are skipped."""
    n = len(text)
    j = i + 1
    while j < n:
        c = text[j]
        if c == "\\":
            j += 2
            continue
        if c == "`":
            return j + 1
        if c == "$" and j + 1 < n and text[j + 1] == "{":
            depth = 1
            j += 2
            while j < n and depth:
                d = text[j]
                if d in "\"'":
                    j = _scan_quoted(text, j, d)
                    continue
                if d == "`":
                    j = _scan_template(text, j)
                    continue
                if d == "{":
                    depth += 1
                elif d == "}":
                    depth -= 1
                j += 1
            continue
        j += 1
    raise ReduceError("unterminated template literal at offset %d" % i)


def _scan_js_regex(text, i):
    n = len(text)
    j = i + 1
    in_class = False
    while j < n:
        c = text[j]
        if c == "\\":
            j += 2
            continue
        if c == "\n":
            raise ReduceError("unterminated regular expression at offset %d" % i)
        if c == "[":
            in_class = True
        elif c == "]":
            in_class = False
        elif c == "/" and not in_class:
            j += 1
            while j < n and (text[j].isalnum() or text[j] == "_"):
                j += 1
            return j
        j += 1
    raise ReduceError("unterminated regular expression at offset %d" % i)


def tokens_c_like(text, lang, strings_only):
    """Token list of C/C++ ('c'), JavaScript ('js') or CSS ('css') source without comments."""
    out = []
    n = len(text)
    i = 0
    line_comments = lang != "css"
    while i < n:
        c = text[i]
        if c.isspace():
            i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            end = text.find("*/", i + 2)
            if end < 0:
                raise ReduceError("unterminated /* comment at offset %d" % i)
            i = end + 2
            continue
        if line_comments and c == "/" and i + 1 < n and text[i + 1] == "/":
            end = text.find("\n", i)
            i = n if end < 0 else end
            continue
        kind = None
        start = i
        if lang == "c" and c == "R" and text.startswith('R"', i) or \
                lang == "c" and re.match(r'(u8|u|U|L)R"', text[i:i + 4]):
            # C++ raw string R"delim( ... )delim"
            q = text.index('"', i)
            paren = text.find("(", q)
            if paren < 0:
                raise ReduceError("bad raw string at offset %d" % i)
            delim = text[q + 1:paren]
            end = text.find(")" + delim + '"', paren)
            if end < 0:
                raise ReduceError("unterminated raw string at offset %d" % i)
            i = end + len(delim) + 2
            kind = "str"
        elif lang == "c" and re.match(r'(u8|u|U|L)["\']', text[i:i + 3]):
            q = i + (2 if text.startswith("u8", i) else 1)
            i = _scan_quoted(text, q, text[q])
            kind = "str" if text[q] == '"' else "chr"
        elif c == '"':
            i = _scan_quoted(text, i, '"')
            kind = "str"
        elif c == "'":
            if lang == "c" and out and re.fullmatch(r"[0-9][0-9A-Za-z_.']*", out[-1]):
                # C++14 digit separator inside a number (1'000): part of the number token
                i += 1
                while i < n and (text[i].isalnum() or text[i] in "_.'"):
                    i += 1
                out[-1] += text[start:i]
                continue
            i = _scan_quoted(text, i, "'")
            kind = "str" if lang != "c" else "chr"
        elif c == "`" and lang == "js":
            i = _scan_template(text, i)
            kind = "str"
        elif c == "/" and lang == "js" and _js_regex_allowed(out):
            i = _scan_js_regex(text, i)
            kind = "re"
        if kind is not None:
            tok = text[start:i]
            if kind == "str":
                if strings_only:
                    tok = STR
                if out and out[-1] == STR and tok == STR:
                    continue                      # adjacent literals merge into one placeholder
                if out and out[-1].startswith('"') and tok.startswith('"') and lang == "c":
                    out[-1] = out[-1][:-1] + tok[1:]
                    continue
            out.append(tok)
            continue
        if c.isalnum() or c in "_$" or (lang == "css" and c in "-#.%"):
            while i < n and (text[i].isalnum() or text[i] in "_$" or
                             (lang == "css" and text[i] in "-#.%") or
                             (text[i] == "." and text[start].isdigit())):
                i += 1
            if i == start:
                i += 1
            out.append(text[start:i])
            continue
        for p in _PUNCT3 + _PUNCT2:
            if text.startswith(p, i):
                out.append(p)
                i += len(p)
                break
        else:
            out.append(c)
            i += 1
    return out


def _js_regex_allowed(out):
    if not out:
        return True
    prev = out[-1]
    if prev in (")", "]", "}"):
        return False
    if re.fullmatch(r"[A-Za-z0-9_$]+", prev):
        return prev in _JS_REGEX_AFTER_WORDS
    if prev[0] in "\"'`" or (prev.startswith("/") and len(prev) > 1):
        return False
    return True


# ---------- shell ----------
def tokens_sh(text, strings_only):
    """Words of a bash script without # comments. Quoted parts stay verbatim inside the word."""
    out = []
    lines = text.split("\n")
    if lines and lines[0].startswith("#!"):
        out.append(lines[0].rstrip("\r"))
        text = "\n".join(lines[1:])
    n = len(text)
    i = 0
    word = ""
    while i < n:
        c = text[i]
        if c in " \t\r\n":
            if word:
                out.append(word)
                word = ""
            if c == "\n":
                out.append("\\n")
            i += 1
            continue
        if c == "#" and not word:
            end = text.find("\n", i)
            i = n if end < 0 else end
            continue
        if c == "\\" and i + 1 < n:
            word += text[i:i + 2]
            i += 2
            continue
        if c in "'\"":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if (c == '"' and text[j] == "\\") else 1
            if j >= n:
                raise ReduceError("unterminated quote at offset %d" % i)
            word += (STR if strings_only else text[i:j + 1])
            i = j + 1
            continue
        word += c
        i += 1
    if word:
        out.append(word)
    # empty lines (only comments) do not count
    res = []
    for t in out:
        if t == "\\n" and (not res or res[-1] == "\\n"):
            continue
        res.append(t)
    return res


# ---------- ini (platformio.ini) ----------
def tokens_ini(text, strings_only):
    out = []
    for line in text.splitlines():
        s = line.strip()
        if not s or s[0] in ";#":
            continue
        s = re.split(r"\s[;#]", s, maxsplit=1)[0]
        out.append(" ".join(s.split()))
    return out


# ---------- Python ----------
class _StripDoc(ast.NodeTransformer):
    def __init__(self, strings_only):
        self.strings_only = strings_only

    def _strip(self, node):
        body = getattr(node, "body", None)
        if body and isinstance(body[0], ast.Expr) and isinstance(body[0].value, ast.Constant) \
                and isinstance(body[0].value.value, str):
            node.body = body[1:] or [ast.Pass()]
        self.generic_visit(node)
        return node

    visit_Module = visit_ClassDef = visit_FunctionDef = visit_AsyncFunctionDef = _strip

    def visit_Constant(self, node):
        if self.strings_only and isinstance(node.value, str):
            return ast.Constant(value=STR)
        return node


def tokens_py(text, strings_only):
    try:
        tree = ast.parse(text)
    except SyntaxError as e:
        raise ReduceError("syntax error: %s" % e)
    tree = _StripDoc(strings_only).visit(tree)
    dump = ast.dump(tree, annotate_fields=True, include_attributes=False)
    # split into small pieces only so that a difference can be shown in context
    return _PY_PIECE.findall(dump)


_PY_PIECE = re.compile(r"'(?:[^'\\]|\\.)*'|\"(?:[^\"\\]|\\.)*\"|\w+|[^\w\s]")


def reduce_file(path, strings_only):
    with open(path, "rb") as f:
        raw = f.read()
    if raw.startswith(b"\xef\xbb\xbf"):
        raw = raw[3:]
    text = raw.decode("utf-8", "surrogateescape").replace("\r\n", "\n")
    ext = os.path.splitext(path)[1].lower()
    if ext in C_LIKE:
        return tokens_c_like(text, "c", strings_only)
    if ext == ".js":
        return tokens_c_like(text, "js", strings_only)
    if ext == ".css":
        return tokens_c_like(text, "css", strings_only)
    if ext == ".py":
        return tokens_py(text, strings_only)
    if ext == ".sh":
        return tokens_sh(text, strings_only)
    if ext == ".ini":
        return tokens_ini(text, strings_only)
    raise ReduceError("unsupported file type")


def stream_hash(tokens):
    h = hashlib.sha256()
    for t in tokens:
        h.update(t.encode("utf-8", "surrogateescape"))
        h.update(b"\x00")
    return h.hexdigest()


def first_difference(a, b):
    i = 0
    while i < len(a) and i < len(b) and a[i] == b[i]:
        i += 1
    ctx = lambda s: " ".join(s[max(0, i - 4):i + 4])[:200]   # noqa: E731
    return i, ctx(a), ctx(b)


def collect(root, limits):
    found = set()
    starts = [os.path.join(root, p) for p in limits] if limits else [root]
    for start in starts:
        if os.path.isfile(start):
            found.add(os.path.relpath(start, root).replace("\\", "/"))
            continue
        for d, dirs, files in os.walk(start):
            dirs[:] = sorted(x for x in dirs if x not in SKIP_DIRS)
            for f in files:
                if os.path.splitext(f)[1].lower() in EXTENSIONS:
                    found.add(os.path.relpath(os.path.join(d, f), root).replace("\\", "/"))
    return found


def load_allow(path):
    allow = {}
    if not path or not os.path.isfile(path):
        return allow
    with open(path, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\r\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) < 3 or parts[1] not in ("code", "strings") or not re.fullmatch(r"[0-9a-f]{64}", parts[2]):
                raise SystemExit("%s:%d: expected 'path TAB code|strings TAB sha256 TAB reason'" % (path, n))
            allow[(parts[0], parts[1])] = parts[2]
    return allow


def load_renames(path):
    """{new path: old path} from the 'file' rows of rename_map.tsv (kind TAB old TAB new TAB note)."""
    renames = {}
    if not path or not os.path.isfile(path):
        return renames
    with open(path, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\r\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) < 3:
                raise SystemExit("%s:%d: expected 'kind TAB old TAB new TAB note'" % (path, n))
            if parts[0] == "file":
                renames[parts[2]] = parts[1]
    return renames


def main():
    # Console code pages (e.g. cp1250) cannot show every character of a token: escape instead of crashing.
    try:
        sys.stdout.reconfigure(errors="backslashreplace")
    except (AttributeError, ValueError):
        pass
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description="Check that only comments (or string literals) changed.")
    ap.add_argument("--base", required=True, help="tree before the edit")
    ap.add_argument("--new", required=True, help="tree after the edit")
    ap.add_argument("--strings-only", action="store_true", help="also ignore the text of string literals")
    ap.add_argument("--allow", default=os.path.join(here, "check_comment_only_allow.tsv"),
                    help="pinned code changes from earlier stages ('' = none)")
    ap.add_argument("--renames", default=os.path.join(here, "rename_map.tsv"),
                    help="file renames of stage E2b, 'file' rows: old TAB new ('' = none)")
    ap.add_argument("--print-hash", action="store_true", help="print allow-file lines for the given files and exit")
    ap.add_argument("--quiet", action="store_true", help="print only problems and the summary")
    ap.add_argument("paths", nargs="*", help="files or directories relative to --new")
    args = ap.parse_args()
    if not os.path.isdir(args.base) or not os.path.isdir(args.new):
        print("--base and --new must be directories", file=sys.stderr)
        return 2
    mode = "strings" if args.strings_only else "code"

    if args.print_hash:
        for rel in sorted(collect(args.new, args.paths)):
            try:
                h = stream_hash(reduce_file(os.path.join(args.new, rel), args.strings_only))
            except ReduceError as e:
                print("# %s: %s" % (rel, e))
                continue
            print("%s\t%s\t%s\t<reason>" % (rel, mode, h))
        return 0

    allow = load_allow(args.allow)
    renames = load_renames(args.renames)
    new_files = collect(args.new, args.paths)
    base_files = collect(args.base, [])
    if args.paths:
        limited = collect(args.base, args.paths)
        base_files = limited | {renames[r] for r in new_files if r in renames and renames[r] in base_files}
    renamed_from = set()
    bad = same = pinned = 0
    for rel in sorted(new_files):
        old = rel
        if rel not in base_files and renames.get(rel) in base_files:
            old = renames[rel]
            renamed_from.add(old)
            if not args.quiet:
                print("RENAMED %s (compared with %s in base)" % (rel, old))
        if old not in base_files:
            if not args.quiet:
                print("NEW     %s (not in base, not compared)" % rel)
            continue
        try:
            a = reduce_file(os.path.join(args.base, old), args.strings_only)
            b = reduce_file(os.path.join(args.new, rel), args.strings_only)
        except ReduceError as e:
            print("ERROR   %s: %s" % (rel, e))
            bad += 1
            continue
        if a == b:
            same += 1
            continue
        h = stream_hash(b)
        if allow.get((rel, mode)) == h:
            pinned += 1
            if not args.quiet:
                print("PINNED  %s (code change from an earlier stage, hash matches the allow file)" % rel)
            continue
        i, ca, cb = first_difference(a, b)
        print("CHANGED %s: token %d differs" % (rel, i))
        print("    base: %s" % ca)
        print("    new:  %s" % cb)
        bad += 1
    if not args.paths and not args.quiet:
        for rel in sorted(base_files - new_files - renamed_from):
            print("GONE    %s (only in base)" % rel)
    print("check_comment_only (%s): %d identical, %d pinned, %d changed" % (mode, same, pinned, bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
