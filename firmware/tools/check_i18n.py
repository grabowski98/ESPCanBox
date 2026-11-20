#!/usr/bin/env python3
# Check of the web panel translations (web/i18n/<lang>.json). Python standard library only, used in CI.
#
#   python -I tools/check_i18n.py                     all dictionaries in web/i18n
#   python -I tools/check_i18n.py --langs en,pl       only these (each must exist)
#   python -I tools/check_i18n.py --todo de           keys missing in de, with the English text (for translators)
#   python -I tools/check_i18n.py --format            rewrite the dictionaries in the canonical layout
#                                                     (sorted keys, one line per key, UTF-8 without BOM)
#
# ERRORS (exit code 1):
#   - a file is not valid JSON, has a BOM, is not a flat object of strings (only "_meta" may be an object),
#     or has a key that is not lower-case "group.name" (letters, digits, "_", upper-case hex allowed after a dot);
#   - a key used by the panel (web/app.js, web/index.html, web/profiles/*.json) is missing in en.json;
#   - a key of en.json is used nowhere (orphan in the source dictionary);
#   - a key of another language does not exist in en.json (orphan, usually a typo or a renamed key);
#   - placeholders {name} of a translation differ from en.json, or the number of ` (code marks) is odd;
#   - a plural key (base_one / _few / _many / _other) lacks a form the language needs
#     (en, de, es: one + other; pl, ru, uk: one + few + many + other);
#   - web/app.js contains a text literal outside the dictionary (list of allowed technical literals below);
#   - web/index.html contains visible text or a title / placeholder / aria-label without a data-i18n* key.
# WARNINGS (exit code 0): a key missing in a language other than en (the panel falls back to English),
#   a file not in the canonical layout (fix with --format).
# At the end a size report: raw and gzip bytes of each dictionary.
import argparse
import gzip
import json
import os
import re
import sys
from html.parser import HTMLParser

FW = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEB = os.path.join(FW, "web")
I18N = os.path.join(WEB, "i18n")
PROFILES = os.path.join(WEB, "profiles")

KEY_RE = re.compile(r"^[a-z][a-z0-9_]*(\.[A-Za-z0-9_]+)+$")
PH_RE = re.compile(r"\{([A-Za-z0-9_]+)\}")
PLURAL_SUFFIXES = ("_zero", "_one", "_two", "_few", "_many", "_other")
PLURAL_NEEDS = {"pl": ("one", "few", "many", "other"), "ru": ("one", "few", "many", "other"),
                "uk": ("one", "few", "many", "other")}
PLURAL_DEFAULT = ("one", "other")

# Literals of app.js that look like text but are technical (commands, CSS classes, file comments,
# fault codes of the API). Anything else that reads like a sentence must come from the dictionary.
ALLOWED_LITERALS = {
    "use strict",
    "cantx on", "cantx off",                                     # console commands sent by buttons
    "btn ghost small", "toast show", "zone off", ".tabbar button",   # CSS classes and selectors
    "3 failed sends", "transmit error counter", "transmitter window exceeded",   # API fault codes
    "controller event", "foreign bits of 260 changed", "external lock",
    "# device restart", "# masked",                              # comments of the .log file format
    "; time in seconds since the device started",
}
# Symbols allowed in literals (shown as they are in every language).
ALLOWED_CHARS = set("—−→•°µ")   # em dash ("no value"), minus, arrow, bullet, degree, micro


def err(msgs, text):
    msgs.append(text)


def load_dict(path, errors):
    name = os.path.basename(path)
    with open(path, "rb") as f:
        raw = f.read()
    if raw.startswith(b"\xef\xbb\xbf"):
        err(errors, "%s: has a BOM (save as UTF-8 without BOM)" % name)
        raw = raw[3:]
    try:
        text = raw.decode("utf-8")
        pairs = json.loads(text, object_pairs_hook=lambda p: p)
    except (UnicodeDecodeError, ValueError) as e:
        err(errors, "%s: not valid UTF-8 JSON: %s" % (name, e))
        return None, None
    if not isinstance(pairs, list):
        err(errors, "%s: the top level must be an object" % name)
        return None, None
    d = {}
    for k, v in pairs:
        if k in d:
            err(errors, "%s: key %s appears twice" % (name, k))
        if k == "_meta":
            if not isinstance(v, list):
                err(errors, "%s: _meta must be an object" % name)
            d[k] = dict(v) if isinstance(v, list) else v
            continue
        if not isinstance(v, str):
            err(errors, "%s: %s: the value must be a string (flat dictionary)" % (name, k))
            continue
        if not KEY_RE.match(k):
            err(errors, "%s: bad key %r (lower-case group.name)" % (name, k))
        d[k] = v
    return d, text


def canonical(d):
    keys = sorted(k for k in d if k != "_meta")
    lines = ["{"]
    items = []
    if "_meta" in d:
        items.append('  "_meta": %s' % json.dumps(d["_meta"], ensure_ascii=False, sort_keys=True))
    for k in keys:
        items.append("  %s: %s" % (json.dumps(k), json.dumps(d[k], ensure_ascii=False)))
    for i, it in enumerate(items):
        lines.append(it + ("," if i < len(items) - 1 else ""))
    lines.append("}")
    return "\n".join(lines) + "\n"


def plural_base(k):
    for s in PLURAL_SUFFIXES:
        if k.endswith(s):
            return k[:-len(s)], s[1:]
    return None, None


# ---------- source scanning ----------
def js_literals(src):
    """String literals of a JS file (quotes ' and "), skipping comments and regular expressions."""
    out = []
    i, n = 0, len(src)
    prev = ""            # last significant character, to tell a regex from a division
    while i < n:
        c = src[i]
        if c == "/" and i + 1 < n and src[i + 1] == "/":
            j = src.find("\n", i)
            i = n if j < 0 else j
            continue
        if c == "/" and i + 1 < n and src[i + 1] == "*":
            j = src.find("*/", i + 2)
            i = n if j < 0 else j + 2
            continue
        if c in "'\"`":
            q = c
            j = i + 1
            buf = []
            while j < n and src[j] != q:
                if src[j] == "\\" and j + 1 < n:
                    buf.append(src[j:j + 2])
                    j += 2
                    continue
                buf.append(src[j])
                j += 1
            line = src.count("\n", 0, i) + 1
            out.append((line, "".join(buf)))
            i = j + 1
            prev = "a"
            continue
        if c == "/" and (prev == "" or prev in "(,=:[!&|?{};+-*%<>~^"):
            j = i + 1
            in_class = False
            while j < n:
                ch = src[j]
                if ch == "\\":
                    j += 2
                    continue
                if ch == "[":
                    in_class = True
                elif ch == "]":
                    in_class = False
                elif ch == "/" and not in_class:
                    break
                elif ch == "\n":
                    break
                j += 1
            i = j + 1
            prev = "a"
            continue
        if not c.isspace():
            prev = c
        i += 1
    return out


def unescape_js(s):
    try:
        return json.loads('"' + s.replace('"', '\\"').replace("\\'", "'") + '"')
    except ValueError:
        return s


def collect_profile_keys(obj, out):
    if isinstance(obj, str):
        if KEY_RE.match(obj):
            out.add(obj)
    elif isinstance(obj, list):
        for x in obj:
            collect_profile_keys(x, out)
    elif isinstance(obj, dict):
        for k, v in obj.items():
            if k.startswith("_"):
                continue
            collect_profile_keys(v, out)


class HtmlScan(HTMLParser):
    SKIP = {"script", "style", "code", "title"}

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.stack = []
        self.keys = set()
        self.problems = []

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        for attr, kattr in (("title", "data-i18n-title"), ("placeholder", "data-i18n-placeholder"),
                            ("aria-label", "data-i18n-aria")):
            if attr in a and a[attr]:
                self.problems.append("line %d: <%s %s=%r> without %s" % (self.getpos()[0], tag, attr, a[attr], kattr))
        for kattr in ("data-i18n", "data-i18n-title", "data-i18n-placeholder", "data-i18n-aria"):
            if kattr in a:
                self.keys.add(a[kattr])
        if tag not in ("meta", "link", "input", "br", "rect", "path", "img"):
            self.stack.append((tag, a))

    def handle_startendtag(self, tag, attrs):
        self.handle_starttag(tag, attrs)
        if self.stack and self.stack[-1][0] == tag:
            self.stack.pop()

    def handle_endtag(self, tag):
        for i in range(len(self.stack) - 1, -1, -1):
            if self.stack[i][0] == tag:
                del self.stack[i:]
                break

    def handle_data(self, data):
        txt = data.strip()
        if not txt or not re.search(r"[^\W\d_]", txt):
            return
        for tag, a in self.stack:
            if tag in self.SKIP or "data-cmd" in a or "data-i18n" in a:
                return
        if txt in ("v—", "{{PROJECT_NAME}}"):
            return
        self.problems.append("line %d: text without data-i18n: %r" % (self.getpos()[0], txt[:60]))


def main():
    try:                       # Windows consoles: print any character without failing
        sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    except (AttributeError, ValueError):
        pass
    ap = argparse.ArgumentParser(description="Check of the web panel dictionaries (web/i18n)")
    ap.add_argument("--langs", help="comma-separated list, e.g. en,pl (default: every file in web/i18n)")
    ap.add_argument("--todo", metavar="LANG", help="print the keys missing in LANG with the English text")
    ap.add_argument("--format", action="store_true", help="rewrite the dictionaries in the canonical layout")
    args = ap.parse_args()

    errors, warnings = [], []
    present = sorted(f[:-5] for f in os.listdir(I18N) if f.endswith(".json"))
    langs = [x.strip() for x in args.langs.split(",")] if args.langs else present
    if "en" not in langs:
        langs.insert(0, "en")
    dicts, texts = {}, {}
    for lang in langs:
        path = os.path.join(I18N, lang + ".json")
        if not os.path.isfile(path):
            err(errors, "web/i18n/%s.json does not exist" % lang)
            continue
        d, text = load_dict(path, errors)
        if d is not None:
            dicts[lang], texts[lang] = d, text

    if args.format:
        for lang, d in dicts.items():
            path = os.path.join(I18N, lang + ".json")
            new = canonical(d)
            if new != texts[lang]:
                with open(path, "w", encoding="utf-8", newline="\n") as f:
                    f.write(new)
                print("formatted web/i18n/%s.json" % lang)
        return 0

    en = dicts.get("en")
    if en is None:
        for e in errors:
            print("ERROR: " + e)
        return 1
    en_keys = set(k for k in en if k != "_meta")

    if args.todo:
        d = dicts.get(args.todo)
        if d is None:
            print("ERROR: no dictionary %s" % args.todo)
            return 1
        missing = sorted(k for k in en_keys if k not in d and not (plural_base(k)[0] and
                         any((plural_base(k)[0] + "_" + f) in d for f in ("other",))))
        for k in missing:
            print("%s\t%s" % (k, en[k]))
        print("# %d keys missing in %s" % (len(missing), args.todo), file=sys.stderr)
        return 0

    # ---- keys used by the panel ----
    used = set()
    js_path = os.path.join(WEB, "app.js")
    with open(js_path, encoding="utf-8") as f:
        js = f.read()
    groups = set(k.split(".")[0] for k in en_keys)
    text_problems = []
    js_scan = "\n".join("" if l.lstrip().startswith("const IDENT = ") else l for l in js.split("\n"))
    for line, lit in js_literals(js_scan):
        val = unescape_js(lit)
        if KEY_RE.match(val) and val.split(".")[0] in groups:
            used.add(val)
            continue
        if val in ALLOWED_LITERALS:
            continue
        nonascii = [ch for ch in val if ord(ch) > 127 and ch not in ALLOWED_CHARS]
        if nonascii or re.search(r"[A-Za-z]{2,}[ ,:;.]+[A-Za-z]{2,}", val) and " " in val:
            text_problems.append("web/app.js:%d: text literal outside the dictionary: %r" % (line, val[:70]))
    for p in text_problems:
        err(errors, p)

    hs = HtmlScan()
    with open(os.path.join(WEB, "index.html"), encoding="utf-8") as f:
        hs.feed(f.read())
    used |= hs.keys
    for p in hs.problems:
        err(errors, "web/index.html " + p)

    if os.path.isdir(PROFILES):
        for fn in sorted(os.listdir(PROFILES)):
            if fn.endswith(".json"):
                with open(os.path.join(PROFILES, fn), encoding="utf-8") as f:
                    try:
                        collect_profile_keys(json.load(f), used)
                    except ValueError as e:
                        err(errors, "web/profiles/%s: %s" % (fn, e))

    plural_bases_en = {}
    for k in en_keys:
        b, form = plural_base(k)
        if b:
            plural_bases_en.setdefault(b, set()).add(form)
    for k in sorted(used):
        if k in en_keys or k in plural_bases_en:
            continue
        err(errors, "key %s is used by the panel but missing in en.json" % k)
    for k in sorted(en_keys):
        b, _ = plural_base(k)
        if k not in used and not (b and b in used):
            err(errors, "en.json: key %s is used nowhere (orphan)" % k)

    # ---- every language ----
    for lang in langs:
        d = dicts.get(lang)
        if d is None:
            continue
        name = "%s.json" % lang
        if texts[lang] != canonical(d):
            warnings.append("%s: not in the canonical layout (run --format)" % name)
        keys = set(k for k in d if k != "_meta")
        for k in sorted(keys - en_keys):
            b, _ = plural_base(k)
            if not (b and b in plural_bases_en):
                err(errors, "%s: key %s does not exist in en.json" % (name, k))
        need = PLURAL_NEEDS.get(lang, PLURAL_DEFAULT)
        for b, en_forms in sorted(plural_bases_en.items()):
            forms = set(f for f in ("zero", "one", "two", "few", "many", "other") if (b + "_" + f) in d)
            if not forms:
                if lang != "en":
                    warnings.append("%s: plural %s missing (English is used)" % (name, b))
                continue
            for f in need:
                if f not in forms:
                    err(errors, "%s: plural %s lacks the form _%s" % (name, b, f))
        missing = 0
        for k in sorted(en_keys):
            b, form = plural_base(k)
            if b:
                continue
            if k not in d:
                missing += 1
                continue
        if missing and lang != "en":
            warnings.append("%s: %d keys missing (English is used; list: --todo %s)" % (name, missing, lang))
        for k in sorted(keys):
            v = d[k]
            b, form = plural_base(k)
            if b and b in plural_bases_en:
                ref = en.get(b + "_other", "")
                want = set(PH_RE.findall(ref))
                got = set(PH_RE.findall(v))
                if not got <= want or not (want - {"n"}) <= got:
                    err(errors, "%s: %s: placeholders %s, en.json has %s" % (name, k, sorted(got), sorted(want)))
            elif k in en:
                want = set(PH_RE.findall(en[k]))
                got = set(PH_RE.findall(v))
                if want != got:
                    err(errors, "%s: %s: placeholders %s, en.json has %s" % (name, k, sorted(got), sorted(want)))
            if v.count("`") % 2:
                err(errors, "%s: %s: odd number of ` marks" % (name, k))
            if "\n" in v or "\r" in v:
                err(errors, "%s: %s: line break inside the text" % (name, k))

    # ---- report ----
    for w in warnings:
        print("WARNING: " + w)
    for e in errors:
        print("ERROR: " + e)
    total_raw = total_gz = 0
    for lang in langs:
        if lang in texts:
            data = json.dumps(dicts[lang], ensure_ascii=False, separators=(",", ":")).encode("utf-8")
            gz = len(gzip.compress(data, 9, mtime=0))
            total_raw += len(data)
            total_gz += gz
            print("size %s: %d keys, %d B, gzip %d B" % (lang, len(dicts[lang]) - ("_meta" in dicts[lang]), len(data), gz))
    print("size total: %d B, gzip %d B (separately compressed; en is inside the page, the others are served as /i18n/<lang>.json)" % (total_raw, total_gz))
    print("check_i18n: %d errors, %d warnings, languages %s, keys in en %d, used %d" %
          (len(errors), len(warnings), ",".join(langs), len(en_keys), len(used)))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
