#!/usr/bin/env python3
"""Offline check of relative links and anchors in the documentation.

Checks every Markdown file under docs/ (docs/STYLE.md included) and the README*.md files in the
repository root. Python standard library only, no network, no MkDocs needed.

    python -I tools/check_doc_links.py              check, exit 1 when a link is dead
    python -I tools/check_doc_links.py --root DIR   another checkout
    python -I tools/check_doc_links.py -v           also print how many links were checked

What counts as dead:
  - a relative link to a file or directory that does not exist on disk (the docs are read on the
    built site and as plain files on GitHub, so every link must work in both places);
  - an anchor (#part) that is not a heading id, an {#id} or an HTML id/name in the target page;
  - a link from a page of the site (docs/<lang>/) that leaves both docs/<lang>/ and docs/en/:
    MkDocs cannot follow it (write a repository path as `code` instead, see docs/STYLE.md).
    The exception is a file in docs/assets/ (images shared by every language);
  - a link from docs/<lang>/ into docs/en/ while the same page exists in docs/<lang>/: link the
    translation instead.
Also reported: a MkDocs admonition ("!!! note", "??? note") outside code. GitHub shows it as plain
text; boxes use the GitHub alert syntax ("> [!NOTE]") or <details markdown="1"> (docs/STYLE.md).
Text inside the ">" marks of a box is checked like any other text.
Not checked: http(s), mailto and other links with a scheme, links inside code.

Translations: a page of docs/<lang>/ that is not translated yet is linked straight to its English
file (for example ../en/getting-started/how-it-works.md from docs/de/index.md). On the built site
mkdocs-static-i18n serves that link as the English fallback page inside the <lang> site; on GitHub it
opens the English file. A link to a file that exists only in docs/en/ written as if it were in
docs/<lang>/ is dead.

Heading ids follow pymdownx.slugs.slugify(case="lower") as set in mkdocs.yml: markup removed,
lower case, characters other than letters, digits, "_", "-" and space removed, spaces -> "-";
a repeated id gets "_1", "_2"...
"""
import argparse
import os
import re
import sys
import unicodedata
from urllib.parse import unquote

DEFAULT_LANG = "en"
ASSETS_DIR = "assets"       # docs/assets/: images shared by every language (logo, screenshots)
SKIP_DIRS = {".git", ".pio", ".build_host", "__pycache__", "node_modules", "site"}

FENCE_RE = re.compile(r"^[ \t]*(`{3,}|~{3,})")
INLINE_CODE_RE = re.compile(r"(`+)(?:.+?)\1")
# [text](target "title") and ![alt](target); target may be in <...>
LINK_RE = re.compile(r"!?\[(?:[^\[\]]|\[[^\[\]]*\])*\]\(\s*(<[^>]*>|[^)\s]+)(?:\s+(?:\"[^\"]*\"|'[^']*'))?\s*\)")
REFDEF_RE = re.compile(r"^[ ]{0,3}\[[^\]]+\]:\s*(<[^>]*>|\S+)")
HTML_ATTR_RE = re.compile(r"""\b(?:href|src)\s*=\s*["']([^"']+)["']""", re.I)
HEADING_RE = re.compile(r"^(#{1,6})[ \t]+(.*?)[ \t]*#*[ \t]*$")
SETEXT_RE = re.compile(r"^(=+|-+)[ \t]*$")
ATTR_ID_RE = re.compile(r"\{[^}]*#([^\s}]+)[^}]*\}\s*$")
HTML_ID_RE = re.compile(r"""\b(?:id|name)\s*=\s*["']([^"']+)["']""", re.I)
QUOTE_RE = re.compile(r"^[ ]{0,3}(?:>[ ]?)+")          # ">" marks of a blockquote / GitHub alert
ADMONITION_RE = re.compile(r"^\s*(?:!!!|\?\?\?\+?)\s+\w")       # "!!! note", "??? note", "???+ tip"
SCHEME_RE = re.compile(r"^[a-zA-Z][a-zA-Z0-9+.-]*:")
INVALID_SLUG_RE = re.compile(r"[^\w\- ]", re.U)


def slugify(text):
    text = re.sub(r"</?[^>]*>", "", text)                       # HTML tags
    text = re.sub(r"!?\[([^\]]*)\]\([^)]*\)", r"\1", text)     # links -> their text
    text = text.replace("`", "")
    text = text.replace("**", "").replace("__", "").replace("*", "")   # emphasis marks
    text = re.sub(r"&[a-zA-Z]+;|&#\d+;", "", text)                     # entities (removed anyway)
    text = text.lower()
    text = unicodedata.normalize("NFC", text)
    return INVALID_SLUG_RE.sub("", text.strip()).replace(" ", "-")


def strip_code(lines, keep_inline=False):
    """Lines with fenced blocks blanked and inline code removed (line numbers kept).

    The ">" marks of blockquotes and GitHub alerts are removed first, so code inside a box is
    recognised as code."""
    out = []
    fence = None
    for line in lines:
        line = QUOTE_RE.sub("", line)
        m = FENCE_RE.match(line)
        if fence:
            if m and m.group(1)[0] == fence[0] and len(m.group(1)) >= len(fence):
                fence = None
            out.append("")
            continue
        if m:
            fence = m.group(1)
            out.append("")
            continue
        if line.startswith("    ") or line.startswith("\t"):
            # indented code block only after a blank line; list continuations are kept
            if out and out[-1].strip() == "" and not _in_list(out):
                out.append("")
                continue
        out.append(line if keep_inline else INLINE_CODE_RE.sub("", line))
    return out


def _in_list(previous):
    for line in reversed(previous):
        if line.strip() == "":
            continue
        return bool(re.match(r"^\s*([-*+]|\d+[.)])\s", line)) or line.startswith((" ", "\t"))
    return False


_anchor_cache = {}


def anchors_of(path):
    if path in _anchor_cache:
        return _anchor_cache[path]
    ids = set()
    counts = {}
    try:
        with open(path, encoding="utf-8") as f:
            raw = f.read().splitlines()
    except (OSError, UnicodeDecodeError):
        _anchor_cache[path] = ids
        return ids
    lines = strip_code(raw, keep_inline=True)
    prev = ""
    for line in lines:
        for hid in HTML_ID_RE.findall(line):
            ids.add(hid)
        text = None
        m = HEADING_RE.match(line)
        if m:
            text = m.group(2)
        elif SETEXT_RE.match(line) and prev.strip() and not prev.lstrip().startswith(("-", "*", "|", ">")):
            text = prev.strip()
        if text is not None:
            am = ATTR_ID_RE.search(text)
            if am:
                ids.add(am.group(1))
            else:
                slug = slugify(text)
                n = counts.get(slug, 0)
                counts[slug] = n + 1
                ids.add(slug if n == 0 else "%s_%d" % (slug, n))
        prev = line
    _anchor_cache[path] = ids
    return ids


def lang_root(root, path):
    """docs/<lang> directory that contains path, or None."""
    docs = os.path.join(root, "docs")
    rel = os.path.relpath(path, docs)
    if rel.startswith(".."):
        return None
    parts = rel.split(os.sep)
    if len(parts) < 2:
        return None
    return os.path.join(docs, parts[0])


def resolve(root, src, target_path):
    """Absolute path of the link target when it exists on disk, else None (no language fallback)."""
    base = os.path.dirname(src)
    cand = os.path.normpath(os.path.join(base, target_path)) if target_path else src
    return cand if os.path.exists(cand) else None


def _inside(path, directory):
    return not os.path.relpath(path, directory).startswith("..")


def links_in(lines):
    for no, line in enumerate(lines, 1):
        for m in LINK_RE.finditer(line):
            yield no, m.group(1)
        m = REFDEF_RE.match(line)
        if m:
            yield no, m.group(1)
        for t in HTML_ATTR_RE.findall(line):
            yield no, t


def check_file(root, path, errors):
    with open(path, encoding="utf-8") as f:
        lines = strip_code(f.read().splitlines())
    lr = lang_root(root, path)
    for no, line in enumerate(lines, 1):
        if ADMONITION_RE.match(line):
            errors.append("%s:%d: MkDocs admonition %r (GitHub shows it as text; use \"> [!NOTE]\", "
                          "see docs/STYLE.md)" % (os.path.relpath(path, root).replace(os.sep, "/"), no,
                                                  line.strip()[:40]))
    n = 0
    for no, target in links_in(lines):
        target = target.strip("<>")
        if not target or SCHEME_RE.match(target) or target.startswith("//"):
            continue
        n += 1
        where = "%s:%d" % (os.path.relpath(path, root).replace(os.sep, "/"), no)
        if target.startswith("/"):
            errors.append("%s: absolute link %s (use a relative one)" % (where, target))
            continue
        target_path, _, anchor = target.partition("#")
        target_path = unquote(target_path.split("?", 1)[0])
        dest = resolve(root, path, target_path)
        if dest is None:
            hint = ""
            if lr and os.path.basename(lr) != DEFAULT_LANG:
                cand = os.path.normpath(os.path.join(os.path.dirname(path), target_path))
                if _inside(cand, lr):
                    en = os.path.join(root, "docs", DEFAULT_LANG, os.path.relpath(cand, lr))
                    if os.path.exists(en):
                        hint = " (not translated: link %s)" % os.path.relpath(
                            en, os.path.dirname(path)).replace(os.sep, "/")
            errors.append("%s: missing file %s%s" % (where, target_path, hint))
            continue
        if lr:
            en_root = os.path.join(root, "docs", DEFAULT_LANG)
            if _inside(dest, os.path.join(root, "docs", ASSETS_DIR)) and os.path.isfile(dest):
                continue        # shared image: MkDocs copies docs/assets/ once and rewrites the address
            if not (_inside(dest, lr) or _inside(dest, en_root)):
                errors.append("%s: link %s leaves docs/%s/ (MkDocs cannot follow it)"
                              % (where, target_path, os.path.basename(lr)))
                continue
            if lr != en_root and _inside(dest, en_root):
                own = os.path.join(lr, os.path.relpath(dest, en_root))
                if os.path.exists(own):
                    errors.append("%s: link %s goes to English, but docs/%s/%s exists (link the translation)"
                                  % (where, target_path, os.path.basename(lr),
                                     os.path.relpath(own, lr).replace(os.sep, "/")))
                    continue
        if anchor:
            if os.path.isdir(dest):
                dest = os.path.join(dest, "index.md")
            if not dest.endswith(".md"):
                continue
            if unquote(anchor) not in anchors_of(dest):
                errors.append("%s: missing anchor #%s in %s"
                              % (where, anchor, os.path.relpath(dest, root).replace(os.sep, "/")))
    return n


def files_to_check(root):
    out = []
    for fn in sorted(os.listdir(root)):
        if fn.startswith("README") and fn.endswith(".md"):
            out.append(os.path.join(root, fn))
    docs = os.path.join(root, "docs")
    for dirpath, dirnames, filenames in os.walk(docs):
        dirnames[:] = sorted(d for d in dirnames if d not in SKIP_DIRS)
        for fn in sorted(filenames):
            if fn.endswith(".md"):
                out.append(os.path.join(dirpath, fn))
    return out


def main():
    ap = argparse.ArgumentParser(description="Offline check of relative links and anchors in docs/ and README*.md.")
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()
    root = os.path.abspath(args.root)
    errors = []
    files = files_to_check(root)
    total = sum(check_file(root, p, errors) for p in files)
    for e in errors:
        print("DEAD " + e)
    if args.verbose or errors:
        print("%d files, %d relative links, %d dead" % (len(files), total, len(errors)))
    else:
        print("0 dead links (%d files, %d relative links)" % (len(files), total))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
