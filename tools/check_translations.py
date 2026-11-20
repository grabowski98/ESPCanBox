#!/usr/bin/env python3
"""Structure check of the documentation translations against the English source.

    python -I tools/check_translations.py              check every docs/<lang>/, exit 1 on a mismatch
    python -I tools/check_translations.py --root DIR   another checkout
    python -I tools/check_translations.py -v           also print one line per checked page

For every Markdown page in docs/<lang>/ (every directory of docs/ except en/ and names starting
with "_", such as docs/_hooks/):
  - the same file exists in docs/en/ (a translation never adds a page);
  - the page carries the translation banner: a GitHub alert ("> [!NOTE]") in its first 15 lines,
    with a link to contributing/translations.md inside it (docs/STYLE.md, "Translations");
  - the number of headings, of fenced code blocks and of table rows is the same as in English;
  - every fenced code block is identical to the English one at the same position (code, console
    output and commands are never translated). Leading indentation, the ">" marks of a box
    (GitHub alert) and trailing spaces are ignored.

Python standard library only, no network, no MkDocs needed. Uploads nothing.
Exit code: 0 = all pages fine, 1 = at least one mismatch, 2 = wrong usage.
"""
import argparse
import os
import re
import sys

SOURCE = "en"
FENCE_RE = re.compile(r"^([ \t]*)(`{3,}|~{3,})")
HEADING_RE = re.compile(r"^#{1,6}[ \t]+\S")
TABLE_RE = re.compile(r"^[ \t]*\|")
BANNER_LINES = 15
# ../contributing/translations.md, ../../en/contributing/translations.md, translations.md, #send-your-work
BANNER_LINK_RE = re.compile(r"\]\((?:[./]*(?:en/)?contributing/)?translations\.md[#)]|\]\(#send-your-work\)")
BANNER_RE = re.compile(r"^>[ ]?\[!NOTE\][ \t]*$")
QUOTE_RE = re.compile(r"^[ ]{0,3}(?:>[ ]?)+")     # ">" marks of a blockquote / GitHub alert


def parse(path):
    """Returns (headings, code_blocks, table_rows) of one page; code blocks as lists of lines."""
    with open(path, encoding="utf-8") as f:
        lines = f.read().splitlines()
    headings = 0
    rows = 0
    blocks = []
    fence = None          # (marker char, length) while inside a block
    current = None
    for line in (QUOTE_RE.sub("", raw) for raw in lines):   # boxes: look inside the ">" marks
        m = FENCE_RE.match(line)
        if fence is None:
            if m:
                fence = (m.group(2)[0], len(m.group(2)))
                current = [line.strip()]          # the opening line with its language
                continue
            if HEADING_RE.match(line):
                headings += 1
            elif TABLE_RE.match(line):
                rows += 1
        else:
            if m and m.group(2)[0] == fence[0] and len(m.group(2)) >= fence[1] \
                    and line.strip() == m.group(2):
                blocks.append(current)
                fence = None
                current = None
                continue
            current.append(line.strip())
    if current is not None:
        blocks.append(current + ["<unclosed fence>"])
    return headings, blocks, rows, lines


def has_banner(lines):
    head = lines[:BANNER_LINES]
    for i, line in enumerate(head):
        if BANNER_RE.match(line):
            body = []
            for nxt in lines[i + 1:]:
                if not nxt.startswith(">"):
                    break                               # first line outside the box
                body.append(nxt)
            # pages in contributing/ link to translations.md or to #send-your-work directly
            if BANNER_LINK_RE.search("\n".join(body)):
                return True
    return False


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--root", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
    ap.add_argument("-v", action="store_true", help="print every checked page")
    args = ap.parse_args()
    docs = os.path.join(os.path.abspath(args.root), "docs")
    src_dir = os.path.join(docs, SOURCE)
    if not os.path.isdir(src_dir):
        print(f"no {src_dir}", file=sys.stderr)
        return 2

    errors = 0
    pages = 0
    langs = sorted(d for d in os.listdir(docs)
                   if d != SOURCE and not d.startswith("_") and os.path.isdir(os.path.join(docs, d)))
    for lang in langs:
        base = os.path.join(docs, lang)
        for dirpath, _dirs, files in os.walk(base):
            for name in sorted(files):
                if not name.endswith(".md"):
                    continue
                path = os.path.join(dirpath, name)
                rel = os.path.relpath(path, base).replace(os.sep, "/")
                shown = f"docs/{lang}/{rel}"
                pages += 1
                problems = []
                src = os.path.join(src_dir, rel)
                if not os.path.isfile(src):
                    problems.append("no English page docs/en/" + rel)
                else:
                    th, tb, tr, tlines = parse(path)
                    sh, sb, sr, _ = parse(src)
                    if not has_banner(tlines):
                        problems.append("no translation banner (> [!NOTE] with a link to "
                                        "contributing/translations.md in the first "
                                        f"{BANNER_LINES} lines)")
                    if th != sh:
                        problems.append(f"headings: {th}, English {sh}")
                    if len(tb) != len(sb):
                        problems.append(f"code blocks: {len(tb)}, English {len(sb)}")
                    if tr != sr:
                        problems.append(f"table rows: {tr}, English {sr}")
                    for n, (a, b) in enumerate(zip(tb, sb), 1):
                        if a != b:
                            diff = next((i for i, (x, y) in enumerate(zip(a, b)) if x != y),
                                        min(len(a), len(b)))
                            got = a[diff] if diff < len(a) else "<end of block>"
                            want = b[diff] if diff < len(b) else "<end of block>"
                            problems.append(f"code block {n} differs at its line {diff + 1}: "
                                            f"{got!r}, English {want!r}")
                if problems:
                    errors += 1
                    for p in problems:
                        print(f"FAIL   {shown}: {p}")
                elif args.v:
                    print(f"ok     {shown}")
    print(f"{pages} translated pages in {len(langs)} languages, {errors} with problems")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
