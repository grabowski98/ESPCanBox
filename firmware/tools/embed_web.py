# Embedding the web panel in the device firmware (PlatformIO "pre:" script, standard library only).
#
# What it does on every build of an environment with networking (listen_wifi, decoder_wifi):
#   1. reads web/index.html, web/app.css, web/app.js, the dictionaries web/i18n/<lang>.json and the vehicle
#      profile web/profiles/<profile>.json;
#   2. puts the English dictionary ({"en": {...}}), the list of languages ({"pl": "Polski", ...}) and the
#      profile into app.js in place of the markers /*@I18N@*/{}, /*@LANGS@*/{} and /*@PROFILE@*/{} (BEFORE
#      the CSP hash is computed, so they are part of the one script); inserts the CSS in place of
#      <link rel="stylesheet" href="app.css"> and the JS in place of <script src="app.js"></script>, i.e.
#      makes one file of them. The other dictionaries are NOT in the page: each one is a separate gzip
#      resource served at /i18n/<lang>.json (step 6), fetched by the panel only when that language is
#      chosen. Until it arrives, and when the fetch fails, the panel shows English;
#   3. checks that the page has EXACTLY ONE <script> tag and no "on...=" attributes (onclick
#      etc.): the Content-Security-Policy header allows only this one script, recognized by its hash;
#   4. computes the SHA-256 hash (base64) of the script text for the CSP header, and the ETag (hash of the compressed bytes);
#   5. compresses the page with gzip (level 9, mtime=0: the same file always gives the same bytes);
#   6. writes $BUILD_DIR/generated/web_assets.h ONLY when the content changed (otherwise the compiler
#      would rebuild http_server.cpp every time) and adds that directory to the compiler include paths.
#      The file holds the page and the table WEB_I18N of the other dictionaries (each gzip level 9, own ETag),
#      all as static const arrays: they stay in flash and are sent from there without a copy in RAM;
#   7. writes $BUILD_DIR/generated/web_langs.h (same rule): WEB_UI_LANGS, the comma-separated codes of the
#      dictionaries built into the page ("de,en,es,pl,ru,uk"; "" for the fallback page). The firmware accepts
#      a stored panel language only from this list (lib/core/ui_lang.h). A separate small header, so that
#      src/net/net.cpp and api_handlers.cpp do not include the page array a second time.
# Contract for the content of web_assets.h: src/net/http_server.h. Description of the headers: docs/PANEL_API.md, "GET /".
#
# The script creates nothing in src/ or lib/. When web/index.html does not exist, it generates a fallback page
# (without a script) and prints a warning: the program can still be built and checked without the panel.
#
# The same file is imported by the mock device (tools/mock_device.py), so that it serves a page assembled
# identically and with an identical CSP header. The PlatformIO part runs only when Import() exists
# (PlatformIO scripts have no __file__, so the project directory is taken from env.subst).
#
# Report without building (run from the firmware directory):
#   python -I tools/embed_web.py --report [--budget 81920]
# assembles the page, checks it again (one <script>, the CSP hash equals the hash of the script text in
# the page), prints the sizes and fails when the gzip page or one gzip dictionary is larger than its budget.

import base64
import gzip
import hashlib
import json
import os
import re
import sys

CSP_TEMPLATE = ("default-src 'none'; script-src %s; style-src 'unsafe-inline'; connect-src 'self'; "
                "img-src data:; base-uri 'none'; form-action 'none'; frame-ancestors 'none'")

CSS_TAG = '<link rel="stylesheet" href="app.css">'
JS_TAG = '<script src="app.js"></script>'

FALLBACK_HTML = ('<!doctype html><html lang="en"><head><meta charset="utf-8">'
                 '<meta name="viewport" content="width=device-width,initial-scale=1">'
                 '<title>{{PROJECT_NAME}}</title></head><body>'
                 '<p>{{PROJECT_NAME}}: the panel was not built (no web/index.html). '
                 'The /api interface works normally.</p></body></html>')

# Dictionaries and vehicle profile inserted into app.js (web/i18n/README.md).
I18N_MARK = "/*@I18N@*/{}"
LANGS_MARK = "/*@LANGS@*/{}"
PROFILE_MARK = "/*@PROFILE@*/{}"
DEFAULT_PROFILE = "psa_aee2010"         # web/profiles/<name>.json; the only profile with panel data so far
PAGE_LANG = "en"                        # the only dictionary inside the page (source language, fallback)
I18N_URI = "/i18n/%s.json"              # address of every other dictionary (src/net/http_server.cpp)
# Budgets (gzip bytes). Flash is not the limit (the app slot has about 3.3 MB); the limits keep the first
# load short over the slow device Wi-Fi and in the radio browser, and leave room for new panel features.
# Page: about 41 KB with English only (after the update wizard and the settings backup were added);
# 56 KB leaves roughly a third for growth and is still well below the 80 KB of the page with six languages.
# Dictionary: the largest (Russian, Cyrillic = 2 bytes per letter in UTF-8) is about 12 KB; 16 KB gives
# a similar margin. A language over its budget fails the build, it never silently goes into the page.
BUDGET_GZ = 56 * 1024
BUDGET_I18N_GZ = 16 * 1024

# Project name in index.html and in the fallback page: the {{PROJECT_NAME}} tag is replaced during
# assembly by the value from include/project_identity.h (project.json, tools/set_project_name.py).
NAME_TAG = "{{PROJECT_NAME}}"


class WebError(Exception):
    pass


def project_name(project_dir):
    """PROJECT_NAME from include/project_identity.h (imports tools/project_identity.py without __file__)."""
    tools_dir = os.path.join(project_dir, "tools")
    if tools_dir not in sys.path:
        sys.path.insert(0, tools_dir)
    sys.dont_write_bytecode = True     # no __pycache__ directory in tools/
    import project_identity
    try:
        return project_identity.load(project_dir)["PROJECT_NAME"]
    except (OSError, KeyError, ValueError) as e:
        raise WebError("project_identity.h: %s" % e)


def _read_text(path):
    with open(path, "rb") as f:
        data = f.read()
    if data.startswith(b"\xef\xbb\xbf"):          # BOM marker written by Windows editors
        data = data[3:]
    text = data.decode("utf-8")
    # The browser HTML parser turns CR LF and a lone CR into LF BEFORE it computes the script hash for CSP.
    # Files from Windows have CR LF: without this replacement the hash would not match and the page would not work.
    return text.replace("\r\n", "\n").replace("\r", "\n")


def _js_json(obj):
    """JSON as a JS expression: UTF-8 kept (smaller than escapes), "</" and U+2028/2029 escaped."""
    text = json.dumps(obj, ensure_ascii=False, separators=(",", ":"), sort_keys=True)
    return text.replace("</", "<\\/").replace("\u2028", "\\u2028").replace("\u2029", "\\u2029")


def load_i18n(web_dir):
    """{lang: {key: text}} from web/i18n/*.json; English is required."""
    d = os.path.join(web_dir, "i18n")
    out = {}
    if os.path.isdir(d):
        for fn in sorted(os.listdir(d)):
            if not fn.endswith(".json"):
                continue
            try:
                obj = json.loads(_read_text(os.path.join(d, fn)))
            except ValueError as e:
                raise WebError("i18n/%s: %s" % (fn, e))
            if not isinstance(obj, dict):
                raise WebError("i18n/%s: the top level must be an object" % fn)
            out[fn[:-5]] = obj
    if "en" not in out:
        raise WebError("web/i18n/en.json is missing (the source dictionary of the panel)")
    return out


def lang_names(dicts):
    """{code: name of the language in that language} for the language pickers (key lang.name)."""
    out = {}
    for code, d in dicts.items():
        name = d.get("lang.name")
        out[code] = name if isinstance(name, str) and name else code
    return out


def i18n_assets(web_dir):
    """[(code, uri, json_bytes)] of the dictionaries served separately (every language except PAGE_LANG).
    Plain JSON (not a JS expression): the panel reads it with fetch().json(). "_meta" is kept (the
    machine translation note)."""
    out = []
    for code, d in sorted(load_i18n(web_dir).items()):
        if code == PAGE_LANG:
            continue
        body = json.dumps(d, ensure_ascii=False, separators=(",", ":"), sort_keys=True).encode("utf-8")
        out.append((code, I18N_URI % code, body))
    return out


def gz(data):
    """gzip level 9 with mtime=0: the same input always gives the same bytes (and the same ETag)."""
    return gzip.compress(data, 9, mtime=0)


def load_profile(web_dir, name=DEFAULT_PROFILE):
    path = os.path.join(web_dir, "profiles", name + ".json")
    if not os.path.isfile(path):
        return {}
    try:
        obj = json.loads(_read_text(path))
    except ValueError as e:
        raise WebError("profiles/%s.json: %s" % (name, e))
    return {k: v for k, v in obj.items() if not k.startswith("_")}


def build_page(web_dir):
    """Returns (html_bytes, csp) or raises WebError. When index.html is missing: (None, None)."""
    index = os.path.join(web_dir, "index.html")
    if not os.path.isfile(index):
        return None, None
    html = _read_text(index).replace(NAME_TAG, project_name(os.path.dirname(os.path.abspath(web_dir))))
    css = _read_text(os.path.join(web_dir, "app.css"))
    js = _read_text(os.path.join(web_dir, "app.js"))
    for mark in (I18N_MARK, LANGS_MARK, PROFILE_MARK):
        if js.count(mark) != 1:
            raise WebError("app.js must contain exactly once: %s" % mark)
    dicts = load_i18n(web_dir)
    js = js.replace(I18N_MARK, _js_json({PAGE_LANG: dicts[PAGE_LANG]}))
    js = js.replace(LANGS_MARK, _js_json(lang_names(dicts)))
    js = js.replace(PROFILE_MARK, _js_json(load_profile(web_dir)))

    if html.count(CSS_TAG) != 1:
        raise WebError("index.html must contain exactly once: " + CSS_TAG)
    if html.count(JS_TAG) != 1:
        raise WebError("index.html must contain exactly once: " + JS_TAG)
    if re.search(r"</script", js, re.IGNORECASE):
        raise WebError("app.js must not contain the text </script (it would close the tag)")
    if re.search(r"</style", css, re.IGNORECASE):
        raise WebError("app.css must not contain the text </style")

    page = html.replace(CSS_TAG, "<style>" + css + "</style>")
    page = page.replace(JS_TAG, "<script>" + js + "</script>")

    # CSP checks: one inline script and no event handlers in HTML attributes.
    # Attributes are checked in the HTML alone (without the script and style content).
    if len(re.findall(r"<script\b", page, re.IGNORECASE)) != 1:
        raise WebError("the page must have exactly one <script> tag")
    if re.search(r"<[a-zA-Z][^>]*\son[a-zA-Z]+\s*=", html):
        raise WebError("index.html must not have on...= attributes (CSP would block them); events only in app.js")
    if re.search(r"javascript:", html, re.IGNORECASE):
        raise WebError("index.html must not have javascript: addresses")

    digest = hashlib.sha256(js.encode("utf-8")).digest()
    csp = CSP_TEMPLATE % ("'sha256-" + base64.b64encode(digest).decode("ascii") + "'")
    return page.encode("utf-8"), csp


def fallback_page(project_dir):
    page = FALLBACK_HTML.replace(NAME_TAG, project_name(project_dir))
    return page.encode("utf-8"), CSP_TEMPLATE % "'none'"


def _c_bytes(lines, name, data):
    lines.append("static const uint8_t %s[] = {" % name)
    for i in range(0, len(data), 16):
        lines.append("    " + ",".join("0x%02x" % b for b in data[i:i + 16]) + ",")
    lines.append("};")


def _etag(data):
    return hashlib.sha256(data).hexdigest()[:16]      # lower case, as in the contract (http_server.h)


def make_header(page, csp, assets=()):
    """web_assets.h text. assets = i18n_assets(): every dictionary becomes its own gzip array in flash."""
    gzp = gz(page)
    etag = _etag(gzp)
    lines = ["// FILE GENERATED by tools/embed_web.py from the web/ directory. Do not edit.",
             "#pragma once",
             "#include <stddef.h>",
             "#include <stdint.h>",
             ""]
    _c_bytes(lines, "WEB_INDEX_GZ", gzp)
    lines.append("static const size_t WEB_INDEX_GZ_LEN = %d;" % len(gzp))
    lines.append('static const char WEB_INDEX_ETAG[] = "\\"%s\\"";' % etag)
    # CSP has only ASCII characters without double quotes and backslashes, but just in case:
    csp_c = csp.replace("\\", "\\\\").replace('"', '\\"')
    lines.append('static const char WEB_INDEX_CSP[] = "%s";' % csp_c)
    lines.append("")
    lines.append("// Panel dictionaries served separately (GET /i18n/<lang>.json, no login), gzip, in flash.")
    lines.append("struct WebI18nAsset {")
    lines.append("    const char* uri;        // exact address, e.g. \"/i18n/pl.json\"")
    lines.append("    const uint8_t* gz;      // gzip bytes (static const array: flash)")
    lines.append("    size_t len;")
    lines.append("    const char* etag;       // with the quotes, as WEB_INDEX_ETAG")
    lines.append("};")
    rows = []
    for code, uri, body in assets:
        g = gz(body)
        ident = "WEB_I18N_GZ_" + re.sub(r"[^A-Za-z0-9]", "_", code)
        _c_bytes(lines, ident, g)
        rows.append('    {"%s", %s, %d, "\\"%s\\""},' % (uri, ident, len(g), _etag(g)))
    lines.append("// The last row (uri = nullptr) only ends the table; WEB_I18N_COUNT does not count it.")
    lines.append("static const WebI18nAsset WEB_I18N[] = {")
    lines.extend(rows)
    lines.append("    {nullptr, nullptr, 0, nullptr},")
    lines.append("};")
    lines.append("static const size_t WEB_I18N_COUNT = %d;" % len(rows))
    lines.append("")
    return "\n".join(lines).encode("ascii"), len(gzp), etag


def lang_codes(web_dir):
    """Sorted dictionary codes of the page; [] when there is no page (fallback)."""
    if not os.path.isfile(os.path.join(web_dir, "index.html")):
        return []
    return sorted(load_i18n(web_dir).keys())


def make_lang_header(codes):
    for c in codes:
        # The same shape lib/core/ui_lang.cpp accepts (uilang::wellFormed): a wrong file name stops the build.
        if not re.fullmatch(r"[A-Za-z][A-Za-z0-9-]{0,6}[A-Za-z0-9]", c):
            raise WebError("web/i18n/%s.json: the language code must be 2..8 letters, digits or '-'" % c)
    return ("// FILE GENERATED by tools/embed_web.py from web/i18n/. Do not edit.\n"
            "#pragma once\n"
            "// Panel languages built into the page (lib/core/ui_lang.h, uilang::inList).\n"
            "#define WEB_UI_LANGS \"%s\"\n" % ",".join(codes)).encode("ascii")


def write_if_changed(path, data):
    try:
        with open(path, "rb") as f:
            if f.read() == data:
                return False
    except OSError:
        pass
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        f.write(data)
    os.replace(tmp, path)
    return True


def generate(project_dir, build_dir):
    web_dir = os.path.join(project_dir, "web")
    page, csp = build_page(web_dir)
    assets = []
    if page is None:
        print("embed_web: WARNING: no web/index.html, embedding a placeholder page")
        page, csp = fallback_page(project_dir)
    else:
        assets = i18n_assets(web_dir)
        over = [(c, len(gz(b))) for c, _, b in assets if len(gz(b)) > BUDGET_I18N_GZ]
        if len(gz(page)) > BUDGET_GZ or over:
            raise WebError("over budget: page gzip %d B (budget %d B), dictionaries %s (budget %d B each)"
                           % (len(gz(page)), BUDGET_GZ, over or "OK", BUDGET_I18N_GZ))
    header, gz_len, etag = make_header(page, csp, assets)
    gen_dir = os.path.join(build_dir, "generated")
    os.makedirs(gen_dir, exist_ok=True)
    changed = write_if_changed(os.path.join(gen_dir, "web_assets.h"), header)
    write_if_changed(os.path.join(gen_dir, "web_langs.h"), make_lang_header(lang_codes(web_dir)))
    print("embed_web: panel %d B, gzip %d B, etag %s, %d separate dictionaries%s"
          % (len(page), gz_len, etag, len(assets), " (written)" if changed else " (unchanged)"))
    return gen_dir


def report(web_dir, budget, i18n_budget=BUDGET_I18N_GZ):
    """Size and consistency report of the assembled page; returns the exit code."""
    page, csp = build_page(web_dir)
    if page is None:
        print("embed_web: no web/index.html")
        return 1
    text = page.decode("utf-8")
    scripts = re.findall(r"<script\b[^>]*>(.*?)</script>", text, re.S | re.I)
    ok = True
    print("scripts in the page: %d" % len(scripts))
    if len(scripts) != 1:
        ok = False
    else:
        h = "'sha256-" + base64.b64encode(hashlib.sha256(scripts[0].encode("utf-8")).digest()).decode("ascii") + "'"
        good = ("script-src " + h + ";") in csp
        print("CSP hash: %s (%s)" % (h, "matches the script in the page" if good else "DOES NOT MATCH"))
        ok = ok and good
    gzp = gz(page)
    print("page: %d B, gzip %d B (budget %d B, %s); inside: dictionary %s only"
          % (len(page), len(gzp), budget, "OK" if len(gzp) <= budget else "EXCEEDED", PAGE_LANG))
    ok = ok and len(gzp) <= budget
    dicts = load_i18n(web_dir)
    print("  dictionary %s: %d keys, in the page" % (PAGE_LANG, len(dicts[PAGE_LANG])))
    assets = i18n_assets(web_dir)
    for code, uri, body in assets:
        n = len(gz(body))
        print("  dictionary %s: %d keys, %d B, gzip %d B at %s (budget %d B, %s)"
              % (code, len(dicts[code]), len(body), n, uri, i18n_budget, "OK" if n <= i18n_budget else "EXCEEDED"))
        ok = ok and n <= i18n_budget
    print("  WEB_UI_LANGS (web_langs.h): %s" % make_lang_header(lang_codes(web_dir)).decode("ascii").split('"')[1])
    prof = _js_json(load_profile(web_dir)).encode("utf-8")
    print("  profile %s: %d B" % (DEFAULT_PROFILE, len(prof)))
    # The page and the dictionaries must stay in flash: static const arrays (in .rodata = flash on the ESP32)
    # handed straight to httpd_resp_send, without a copy in RAM.
    hdr = make_header(page, csp, assets)[0].decode("ascii")
    hdr_ok = (hdr.count("static const uint8_t WEB_INDEX_GZ[] = {") == 1 and
              hdr.count("static const uint8_t WEB_I18N_GZ_") == len(assets))
    srv = os.path.join(os.path.dirname(os.path.abspath(web_dir)), "src", "net", "http_server.cpp")
    try:
        with open(srv, encoding="utf-8") as f:
            src = f.read()
        srv_ok = ("httpd_resp_send(req, (const char*)WEB_INDEX_GZ, (ssize_t)WEB_INDEX_GZ_LEN)" in src and
                  "httpd_resp_send(req, (const char*)a->gz, (ssize_t)a->len)" in src)
    except OSError:
        srv_ok = False
    print("served from flash: %s (web_assets.h static const arrays: %s; http_server.cpp sends them directly: %s)"
          % ("yes" if hdr_ok and srv_ok else "NOT CONFIRMED", hdr_ok, srv_ok))
    ok = ok and hdr_ok and srv_ok
    print("embed_web report: %s" % ("OK" if ok else "FAILED"))
    return 0 if ok else 1


# ---------- PlatformIO part ----------
try:
    Import("env")  # noqa: F821  (the function exists only in PlatformIO / SCons scripts)
except NameError:
    env = None

if env is None and __name__ == "__main__":
    import argparse
    _ap = argparse.ArgumentParser(description="Report of the embedded web panel (nothing is written)")
    _ap.add_argument("--report", action="store_true", help="assemble the page and print the size report")
    _ap.add_argument("--budget", type=int, default=BUDGET_GZ, help="gzip page budget in bytes (default %(default)s)")
    _ap.add_argument("--i18n-budget", type=int, default=BUDGET_I18N_GZ,
                     help="gzip budget of each separate dictionary in bytes (default %(default)s)")
    _a = _ap.parse_args()
    if not _a.report:
        _ap.error("nothing to do: use --report")
    _fw = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    try:
        sys.exit(report(os.path.join(_fw, "web"), _a.budget, _a.i18n_budget))
    except (WebError, OSError, UnicodeDecodeError) as _e:
        print("embed_web: ERROR: %s" % _e)
        sys.exit(1)

if env is not None:
    try:
        _gen = generate(env.subst("$PROJECT_DIR"), env.subst("$BUILD_DIR"))
    except (WebError, OSError, UnicodeDecodeError) as e:
        print("embed_web: ERROR: %s" % e)
        env.Exit(1)
    else:
        env.Append(CPPPATH=[_gen])
