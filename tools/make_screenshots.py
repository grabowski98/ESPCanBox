#!/usr/bin/env python3
"""Screenshots of the web panel, made from the mock device with one command.

Starts firmware/tools/mock_device.py on a free port of 127.0.0.1 (a fresh mock for every scene),
drives a headless Chromium-family browser with Playwright and writes the PNG files listed in
tools/screenshots.json into docs/assets/screenshots/, plus manifest.json (id, file, language,
viewport, size, sha256). Nothing touches a real device, a serial port or a network other than
127.0.0.1. The mock password is random, lives only in this process, reaches the mock through its
standard input (not the command line) and is never printed.

The mock runs with --screenshot: the same build string and ELF hash in every picture, a network
name like a real device's (ESPCanBox-A1B2) and console lines without "(mock)". Together with the
synthetic captures and the fixed page clock this makes a second run give the same pictures, except
for values that depend on timing (frame counters, ages, uptime).

Run from the repository root (needs uv, https://docs.astral.sh/uv/; nothing is added to the
firmware build):

    Windows (PowerShell or cmd), with Microsoft Edge, which every Windows 10/11 has:
        uvx --with playwright==1.65.2 --with pillow==13.0.0 python -I tools/make_screenshots.py
    Linux, with Google Chrome or Chromium installed:
        uvx --with playwright==1.65.2 --with pillow==13.0.0 python -I tools/make_screenshots.py --channel chrome
    Linux without a browser: let Playwright download its own Chromium once, then use it:
        uvx --with playwright==1.65.2 playwright install chromium
        uvx --with playwright==1.65.2 --with pillow==13.0.0 python -I tools/make_screenshots.py --channel chromium

Options:
    --list               print the scenes (id, languages, viewports, file) and exit
    --only ID[,ID...]    only these scenes (a scene id, or a prefix ending with *: "ota-*")
    --lang en,pl         only these languages (default: every language a scene lists)
    --viewport desktop   only these viewports (desktop, phone, tablet)
    --channel NAME       msedge, chrome or chromium (default: msedge, then chrome, then chromium)
    --out DIR            output directory (default docs/assets/screenshots)
    --firmware DIR       firmware directory with tools/mock_device.py (default firmware)
    --scenes FILE        scene list (default tools/screenshots.json)
    --keep-going         go on after a failed scene (the exit code is still 1)

Pillow is optional: without it the PNG files are written as the browser made them (larger,
no downscaling). The scene format is described at the top of tools/screenshots.json.
"""
import argparse
import base64
import datetime
import fnmatch
import hashlib
import io
import json
import os
import re
import secrets
import shutil
import socket
import string
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request

sys.dont_write_bytecode = True
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Viewports: CSS size, device pixel ratio of the browser and scale of the written file
# (1.0 = one file pixel per CSS pixel). Rendering at DPR 2 and scaling down gives smoother text
# than rendering at DPR 1; desktop files keep 1.25 x the CSS size for sharpness on large screens.
VIEWPORTS = {
    "desktop": {"width": 1280, "height": 800, "dpr": 2, "scale": 1.25},
    "tablet": {"width": 820, "height": 1180, "dpr": 2, "scale": 1.0},
    "phone": {"width": 390, "height": 844, "dpr": 2, "scale": 1.0, "mobile": True},
}
NO_MOTION_CSS = ("*,*::before,*::after{animation:none!important;transition:none!important;"
                 "caret-color:transparent!important;scroll-behavior:auto!important}")
# Text that must never appear on a screenshot: local paths, MAC addresses, a VIN, e-mail addresses.
PRIVATE_RE = re.compile(r"[A-Za-z]:\\|/home/|/Users/|\b[0-9A-Fa-f]{2}(?::[0-9A-Fa-f]{2}){5}\b|"
                        r"\b[A-HJ-NPR-Z0-9]{17}\b|[\w.+-]+@[\w-]+\.[\w.]+")
# Page clock of the scenes with "fixed_time" (the export file name carries it). Aware, so that
# Playwright does not read it in the local time zone of the machine.
FIXED_TIME = datetime.datetime(2026, 2, 10, 10, 0, 0, tzinfo=datetime.timezone.utc)
# Build date of the "new" firmware files: after the build of the running program in the mock
# (--screenshot: "Jan 10 2026 09:00:00"), so that an update is newer than what runs.
NEW_BUILD = "Feb  3 2026 09:00:00"
FW_IMAGE_BYTES = 1245184            # about the size of a real image (the slot holds 3 342 336 B)
MAX_BYTES = 300 * 1024


def log(msg):
    print(msg.encode(sys.stdout.encoding or "utf-8", "replace").decode(sys.stdout.encoding or "utf-8"), flush=True)


# ---------------------------------------------------------------- scenes
def load_scenes(path):
    with open(path, encoding="utf-8") as f:
        doc = json.load(f)
    defaults = doc.get("defaults", {})
    out = []
    seen = set()
    for sc in doc["scenes"]:
        sc = dict(defaults, **sc)
        if sc["id"] in seen:
            raise SystemExit("screenshots.json: duplicate id %s" % sc["id"])
        seen.add(sc["id"])
        out.append(sc)
    return doc, out


def variants(sc):
    """(lang, viewport) pairs of one scene."""
    return [(lang, vp) for vp in sc.get("viewports", ["desktop"]) for lang in sc.get("langs", ["en"])]


def out_name(sc, lang, vp):
    base = sc.get("file") or ("panel-%s.png" % sc["id"])
    if vp != "desktop":
        base = base.replace("panel-", "panel-%s-" % vp, 1)
    return base if lang == "en" else "%s/%s" % (lang, base)


# ---------------------------------------------------------------- synthetic CAN captures
# Frames of the PSA AEE2010 profile, decoded by the mock with the same rules as lib/car_psa_aee2010
# (see mock_device.py: decode_park, on_frame). Byte templates come from the anonymized captures in
# firmware/test_data/captures/ (can_20261111_191045.log); only the decoded bits are changed.
IGN = {"off": 0x80, "on": 0x88, "engine": 0x8E}
# Voltage of the 12 V input after the diode (about 0.8 V below the battery, firmware console_cmds.cpp)
# for the mock option --supply-v: alternator, battery with the ignition on / off; no car = USB only.
SUPPLY_V = {"engine": 13.42, "on": 11.63, "off": 11.71, "none": 0.0}
PARK_STATUS = {"none": 0, "fault": 1, "off": 2, "active": 4, "waiting": 5, "failed": 6}
DOOR_BITS = {"driver": 0x40, "passenger": 0x80, "rear_left": 0x10, "rear_right": 0x20, "boot": 0x08}


def park_frame(p):
    rs, fs = PARK_STATUS[p.get("rear", "waiting")], PARK_STATUS[p.get("front", "waiting")]
    r, fr = p.get("rear_zones", [7, 7, 7]), p.get("front_zones", [7, 7, 7])
    snd = p.get("sound")              # None, "rear" or "front"
    b0 = (rs << 5) | (fs << 2)
    b1 = (0x10 if snd else 0) | (0x20 if snd == "front" else 0)
    b2 = p.get("interval", 63) & 0x3F
    b3 = (r[0] << 5) | (r[1] << 2)
    b4 = (r[2] << 5) | (fr[0] << 2)
    b5 = (fr[1] << 5) | (fr[2] << 2)
    return bytes([b0, b1, b2, b3, b4, b5, 0xC2, 0x00])


def synthetic_capture(car, seconds=240):
    """Capture text for one car state, frames every 100 ms (the mock needs them younger than 0.4 / 1 s)."""
    lines = ["# synthetic capture made by tools/make_screenshots.py: %s" % json.dumps(car, sort_keys=True)]
    ign = car.get("ignition", "engine")
    temp = car.get("outside_temp", 6.5)
    doors = 0
    for d in car.get("doors", []):
        doors |= DOOR_BITS[d]
    for i in range(seconds * 10):
        t = 1.0 + i * 0.1
        if ign != "none":
            b6 = int(round((temp + 40) * 2)) & 0xFF
            b7 = 0x80 if car.get("reverse") else 0x24
            lines.append("(%.3f) can0 0F6#%02X591E23235D%02X%02X" % (t, IGN[ign], b6, b7))
        if ign in ("on", "engine"):
            lines.append("(%.3f) can0 036#0E0064%02X310001AC" % (t + 0.01, 0x2F if car.get("lights") else 0x0F))
            if "park" in car:
                lines.append("(%.3f) can0 0E1#%s" % (t + 0.02, park_frame(car["park"]).hex().upper()))
            if i % 5 == 0:
                lines.append("(%.3f) can0 260#A43497BA554000" % (t + 0.03))
                lines.append("(%.3f) can0 227#0000110000" % (t + 0.04))
            if "doors" in car:
                # 128 byte 4 bit 6 = courtesy lamp (psa_signals.cpp decodeDoors): lit while a door is
                # open, and only then does the 1A1 message with the door bits count
                lamp = 0x40 if doors else 0x00
                lines.append("(%.3f) can0 128#C0B00122%02X400400" % (t + 0.05, lamp))
                if i % 5 == 0:
                    lines.append("(%.3f) can0 1A1#00DEA6%02X00000000" % (t + 0.06, doors))
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------- firmware files for the update wizard
def fw_image(ident_mod, kind, version, build=NEW_BUILD):
    """Small fake image with the ESP32 header and the FwIdent marker (docs/en/reference/panel-api.md, ch. 6).
    kind: 'listen' | 'decoder' | 'decoder_tx' | 'nonet' | 'other_product' | 'not_app' | 'short'."""
    ident = ident_mod["ident"]
    if kind == "short":
        return b"\x00" * 4096
    buf = bytearray(FW_IMAGE_BYTES)
    if kind == "not_app":
        return bytes((i * 7 + 3) & 0xFF for i in range(98304))
    buf[0] = 0xE9
    buf[12:14] = b"\x09\x00"
    buf[32:36] = b"\x32\x54\xcd\xab"
    h = hashlib.sha256((kind + version + build).encode()).digest()
    buf[0xB0:0xB8] = h[:8]                     # stands for the ELF hash of a real image
    product = "otherproduct" if kind == "other_product" else ident["PROJECT_FW_PRODUCT"]
    m = 0x2000
    buf[m:m + 12] = ident_mod["marker"]
    def put(off, n, s):  # noqa: E306
        b = s.encode("ascii")[:n - 1]
        buf[m + off:m + off + len(b)] = b
    put(12, 16, product)
    put(28, 16, version)
    put(44, 24, build)
    buf[m + 68] = 1 if kind in ("decoder", "decoder_tx") else 0
    buf[m + 69] = 0 if kind == "nonet" else 1
    buf[m + 70] = 1 if kind == "decoder_tx" else 0
    return bytes(buf)


def next_patch(v):
    m = re.match(r"^(\d+)\.(\d+)\.(\d+)$", v)
    return "%s.%s.%d" % (m.group(1), m.group(2), int(m.group(3)) + 1) if m else v


# ---------------------------------------------------------------- mock device
def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


class Mock:
    def __init__(self, fw_dir, tmp, cfg, password, features):
        self.fw_dir, self.cfg, self.pw = fw_dir, cfg, password
        self.port = free_port()
        self.base = "http://127.0.0.1:%d" % self.port
        cap = cfg.get("capture")
        if cap:
            cap_path = os.path.join(fw_dir, cap)
        else:
            cap_path = os.path.join(tmp, "car_%s.log" % hashlib.sha1(
                json.dumps(cfg.get("car", {}), sort_keys=True).encode()).hexdigest()[:10])
            if not os.path.exists(cap_path):
                with open(cap_path, "w", encoding="utf-8") as f:
                    f.write(synthetic_capture(cfg.get("car", {})))
        # -I: no user site-packages, no PYTHON* variables; -B: no __pycache__ in firmware/tools
        args = [sys.executable, "-I", "-B", os.path.join(fw_dir, "tools", "mock_device.py"), cap_path,
                "--host", "127.0.0.1", "--port", str(self.port), "--mode", cfg.get("mode", "listen")]
        if "screenshot" in features:
            # the password goes through stdin, so it is not in the process list
            args += ["--password-stdin", "--screenshot"]
            car = cfg.get("car", {}) if not cfg.get("capture") else {}
            supply = cfg.get("supply_v", SUPPLY_V.get(car.get("ignition", "engine"), 0.0) if car else 0.0)
            if supply:
                args += ["--supply-v", "%.2f" % supply]
        else:
            args += ["--password", password]
        if cfg.get("can_tx"):
            args.append("--can-tx")
        if cfg.get("lan"):
            args.append("--lan")
        if cfg.get("lan_admin"):
            args.append("--lan-admin")
        if cfg.get("demo_radio"):
            args.append("--demo-radio")
        # stdout carries the password: it goes nowhere
        self.proc = subprocess.Popen(args, cwd=fw_dir, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
                                     stderr=subprocess.PIPE)
        if "screenshot" in features:
            self.proc.stdin.write((password + "\n").encode("ascii"))
        self.proc.stdin.close()
        t0 = time.time()
        while time.time() - t0 < 15:
            if self.proc.poll() is not None:
                raise RuntimeError("mock stopped: %s" % self.proc.stderr.read().decode("utf-8", "replace")[-400:])
            try:
                self.hello()
                return
            except OSError:
                time.sleep(0.15)
        raise RuntimeError("mock did not start")

    def hello(self):
        with urllib.request.urlopen(self.base + "/api/hello", timeout=2) as r:
            return json.loads(r.read())

    def request(self, method, path, form=None):
        data = urllib.parse.urlencode(form).encode() if form is not None else None
        req = urllib.request.Request(self.base + path, data=data, method=method)
        req.add_header("Authorization", "Basic " + base64.b64encode(("admin:" + self.pw).encode()).decode())
        if data is not None:
            req.add_header("Content-Type", "application/x-www-form-urlencoded")
        try:
            with urllib.request.urlopen(req, timeout=5) as r:
                return r.status, json.loads(r.read() or b"null")
        except urllib.error.HTTPError as e:
            return e.code, None

    def restart_and_wait(self):
        boot = self.hello()["boot"]
        code, _ = self.request("POST", "/api/restart", {})
        if code != 200:
            raise RuntimeError("restart refused: HTTP %d" % code)
        t0 = time.time()
        while time.time() - t0 < 20:
            time.sleep(0.3)
            try:
                if self.hello()["boot"] != boot:
                    return
            except OSError:
                pass
        raise RuntimeError("mock did not restart")

    def stop(self):
        if self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(5)
            except subprocess.TimeoutExpired:
                self.proc.kill()


# ---------------------------------------------------------------- browser
def launch(pw_api, channel, lang="en"):
    # --lang: language of the browser's own controls (the "Choose file" button of a file input);
    # a scene that switches the panel to another language sets "browser_lang"
    order = [channel] if channel else ["msedge", "chrome", "chromium"]
    errors = []
    args = ["--lang=%s" % ("en-US" if lang == "en" else lang)]
    for ch in order:
        try:
            if ch == "chromium":
                return pw_api.chromium.launch(headless=True, args=args), ch
            return pw_api.chromium.launch(headless=True, channel=ch, args=args), ch
        except Exception as e:      # noqa: BLE001  (the next channel is tried)
            errors.append("%s: %s" % (ch, str(e).splitlines()[0]))
    raise SystemExit("no browser could be started:\n  " + "\n  ".join(errors) +
                     "\nInstall Edge or Chrome, or run: uvx --with playwright==1.65.2 playwright install chromium")


TRANSLATION_KEY_RE = re.compile(r"^[a-z][a-z0-9_]*(\.[a-z0-9_]+){1,4}$")
CHECK_JS = """() => {
  const out = {keys: [], placeholders: [], text: ''};
  const walk = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
  let n;
  while ((n = walk.nextNode())) {
    const p = n.parentElement;
    if (!p || p.closest('[hidden]') || p.closest('script,style')) continue;
    const s = n.textContent.trim();
    if (!s) continue;
    out.text += s + '\\n';
    if (/^[a-z][a-z0-9_]*(\\.[a-z0-9_]+){1,4}$/.test(s) && !/^v?\\d/.test(s) && !/\\.(local|json|log|bin|png)$/.test(s)) out.keys.push(s);
    if (/\\{[A-Za-z0-9_]+\\}/.test(s)) out.placeholders.push(s);
  }
  // what is not a text node: values of fields, titles, labels, placeholders
  document.querySelectorAll('input,textarea,select').forEach(function (e) {
    if (e.type !== 'password' && e.type !== 'file' && e.value) out.text += e.value + '\\n';
  });
  document.querySelectorAll('[title],[aria-label],[placeholder],[alt]').forEach(function (e) {
    ['title', 'aria-label', 'placeholder', 'alt'].forEach(function (a) {
      const v = e.getAttribute(a);
      if (v) out.text += v + '\\n';
    });
  });
  return out;
}"""


def run_step(page, mock, step, ctx):
    """One step of a scene. Steps are small dicts; see the comment at the top of screenshots.json."""
    k = next(iter(step))
    v = step[k]
    tmo = step.get("timeout", 15000)
    if k == "tab":
        page.click(".tabbar button[data-tab=%s]" % v)
        page.wait_for_selector("#tab-%s:not([hidden])" % v, timeout=tmo)
    elif k == "click":
        page.click(v, timeout=tmo)
    elif k == "fill":
        page.fill(v, ctx["subst"](step["value"]), timeout=tmo)
    elif k == "press":
        page.press(v, step["key"], timeout=tmo)
    elif k == "check":
        page.check(v, timeout=tmo)
    elif k == "select":
        page.select_option(v, step["value"], timeout=tmo)
    elif k == "wait":
        page.wait_for_selector(v, state=step.get("state", "visible"), timeout=tmo)
    elif k == "wait_js":
        page.wait_for_function(ctx["subst"](v), timeout=tmo)
    elif k == "sleep":
        page.wait_for_timeout(v)
    elif k == "console":          # type a console command and send it, as a user does
        page.fill("#cmd-line", v)
        page.press("#cmd-line", "Enter")
    elif k == "api_cmd":          # state set up beforehand, as from the USB console or an earlier visit
        code, _ = mock.request("POST", "/api/cmd", {"line": v})
        if code != 200:
            raise RuntimeError("api_cmd %r: HTTP %d" % (v, code))
        page.wait_for_timeout(300)
    elif k == "api_restart":
        mock.restart_and_wait()
    elif k == "stop_mock":
        mock.stop()
    elif k == "upload_fw":        # firmware file for the update wizard
        data = fw_image(ctx["ident"], v, ctx["subst"](step.get("version", "{next}")))
        name = ctx["subst"](step.get("name", "firmware.bin"))
        page.set_input_files("#ota-file", files=[{"name": name, "mimeType": "application/octet-stream", "buffer": data}])
    elif k == "upload_json":      # settings backup file
        text = v if isinstance(v, str) else json.dumps(v)
        page.set_input_files("#set-file", files=[{"name": step.get("name", "espcanbox_settings.json"),
                                                  "mimeType": "application/json", "buffer": text.encode()}])
    elif k == "expand_select":    # show the options of a <select> as a list (a native drop-down cannot be captured)
        page.eval_on_selector(v, "e => { e.size = e.options.length; e.style.height = 'auto'; }")
    elif k == "scroll":
        page.eval_on_selector(v, "e => e.scrollIntoView({block: '%s'})" % step.get("block", "start"))
    elif k == "scroll_bottom":
        page.eval_on_selector(v, "e => { e.scrollTop = e.scrollHeight; }")
    elif k == "hover":
        page.hover(v, timeout=tmo)
    elif k == "eval":
        page.evaluate(v)
    else:
        raise RuntimeError("unknown step %r" % k)


def shoot(page, crop):
    if crop == "full":
        return page.screenshot(full_page=True, animations="disabled", caret="hide")
    if crop == "viewport":
        return page.screenshot(animations="disabled", caret="hide")
    sels = crop["elements"] if "elements" in crop else [crop["element"]]
    pad = crop.get("pad", 12)
    pad_top = crop.get("pad_top", pad)
    # the sticky header and the tab bar would cover the element in a page-sized shot: unpin them
    page.add_style_tag(content=".top,.tabbar{position:static!important}")
    if crop.get("hide"):              # cards that would fall inside the box but are not the subject
        page.add_style_tag(content="%s{display:none!important}" % ",".join(crop["hide"]))
    boxes = []
    for sel in sels:
        loc = page.locator(sel).first
        loc.scroll_into_view_if_needed()
        box = loc.bounding_box()
        if not box:
            raise RuntimeError("element %s not visible" % sel)
        sx, sy = page.evaluate("[window.scrollX, window.scrollY]")
        boxes.append((box["x"] + sx, box["y"] + sy, box["x"] + sx + box["width"], box["y"] + sy + box["height"]))
    width = page.evaluate("document.documentElement.scrollWidth")
    x0 = max(0, min(b[0] for b in boxes) - pad)
    y0 = max(0, min(b[1] for b in boxes) - pad_top)
    x1 = min(width, max(b[2] for b in boxes) + pad)
    y1 = max(b[3] for b in boxes) + pad
    if crop.get("full_width"):
        x0, x1 = 0, width
    return page.screenshot(full_page=True, animations="disabled", caret="hide",
                           clip={"x": x0, "y": y0, "width": x1 - x0, "height": y1 - y0})


HEX_RE = re.compile(r"#([0-9A-Fa-f]{6}|[0-9A-Fa-f]{3})\b")


def panel_colours(web_dir):
    """Every #rgb / #rrggbb colour written in the panel sources (app.css, index.html, app.js)."""
    out = []
    for fn in ("app.css", "index.html", "app.js"):
        try:
            with open(os.path.join(web_dir, fn), encoding="utf-8") as f:
                text = f.read()
        except OSError:
            continue
        for h in HEX_RE.findall(text):
            if len(h) == 3:
                h = "".join(c * 2 for c in h)
            rgb = tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))
            if rgb not in out:
                out.append(rgb)
    return out[:96]


def optimise(raw, scale, dpr, tokens=()):
    """Downscale to `scale` file pixels per CSS pixel, reduce to 256 colours, strip metadata.
    The colours of the panel (tokens) are put into the palette as they are, the rest of the palette
    is fitted to the picture, and every pixel takes the nearest palette colour (no dithering): so a
    zone swatch or a status dot keeps its exact colour instead of a brown or teal neighbour."""
    try:
        from PIL import Image
    except ImportError:
        return raw, None
    im = Image.open(io.BytesIO(raw)).convert("RGB")
    f = scale / dpr
    if abs(f - 1) > 1e-6:
        im = im.resize((max(1, round(im.width * f)), max(1, round(im.height * f))), Image.LANCZOS)
    tokens = list(tokens)
    for colors in (256, 192):                  # fewer colours only when a file would pass MAX_BYTES
        n_fit = colors - len(tokens)
        fit = im.quantize(colors=n_fit, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        pal = fit.getpalette()[:3 * n_fit]
        pal += [0] * (3 * n_fit - len(pal))
        for c in tokens:
            pal += list(c)
        pimg = Image.new("P", (1, 1))
        pimg.putpalette(pal + [0] * (768 - len(pal)))
        q = im.quantize(palette=pimg, dither=Image.Dither.NONE)
        out = io.BytesIO()
        q.save(out, "PNG", optimize=True)      # no info / EXIF chunks are written
        if out.tell() <= MAX_BYTES:
            break
    return out.getvalue(), im.size


def run_scene(browser, sc, lang, vpname, fw_dir, tmp, ident_mod, out_dir, features, tokens):
    vp = VIEWPORTS[vpname]
    password = "".join(secrets.choice(string.ascii_letters + string.digits) for _ in range(20))
    wrong = ["wrong-password-%d" % i for i in range(1, 9)]
    version = ident_mod["version"]
    subst_map = {"{password}": password, "{next}": next_patch(version), "{version}": version}
    for i, w in enumerate(wrong, 1):
        subst_map["{wrong%d}" % i] = w

    def subst(s):
        for a, b in subst_map.items():
            s = s.replace(a, b)
        return s
    mock = Mock(fw_dir, tmp, sc.get("mock", {}), password, features)
    ctxb = None
    try:
        if lang != "en":
            code, _ = mock.request("POST", "/api/lang", {"lang": lang})
            if code != 200:
                raise RuntimeError("POST /api/lang %s: HTTP %d" % (lang, code))
        for line in sc.get("before", []):        # console commands run before the page opens
            mock.request("POST", "/api/cmd", {"line": line})
        if sc.get("before_restart"):
            mock.restart_and_wait()
        time.sleep(sc.get("warmup", 1.2))        # the mock replays a few frames first
        ctxb = browser.new_context(
            viewport={"width": vp["width"],
                      "height": sc.get("height_" + vpname, sc.get("height") if vpname == "desktop" else None)
                      or vp["height"]},
            device_scale_factor=vp["dpr"], is_mobile=vp.get("mobile", False), has_touch=vp.get("mobile", False),
            color_scheme=sc.get("scheme", "dark"), reduced_motion="reduce", locale=lang,
            timezone_id="UTC", bypass_csp=True, accept_downloads=True)
        page = ctxb.new_page()
        if sc.get("fixed_time"):
            page.clock.set_fixed_time(FIXED_TIME)
        page.goto(mock.base + "/", wait_until="load")
        page.add_style_tag(content=NO_MOTION_CSS)
        page.wait_for_function("document.documentElement.lang === %s" % json.dumps(lang), timeout=10000)
        if sc.get("login", True):
            page.fill("#login-pw", password)
            if sc.get("remember"):
                page.check("#login-remember")
            page.click("#login-form button[type=submit]")
            page.wait_for_selector("#app:not([hidden])", timeout=10000)
            page.wait_for_function("document.getElementById('badge-ver').textContent !== 'v—'", timeout=10000)
        else:
            page.wait_for_function("document.getElementById('login-info').textContent !== '—'", timeout=10000)
        ctx = {"subst": subst, "ident": ident_mod}
        for step in sc.get("steps", []) + sc.get("steps_" + vpname, []):
            run_step(page, mock, step, ctx)
        page.wait_for_timeout(sc.get("settle", 900))
        page.evaluate("document.fonts.ready")
        chk = page.evaluate(CHECK_JS)
        problems = []
        if chk["keys"]:
            problems.append("untranslated keys: %s" % ", ".join(sorted(set(chk["keys"]))[:5]))
        if chk["placeholders"]:
            problems.append("unfilled placeholders: %s" % chk["placeholders"][0][:80])
        if password in chk["text"]:
            problems.append("the password is visible")
        for want in sc.get("expect_text", []):          # the state the title promises is on the page
            if subst(want) not in chk["text"]:
                problems.append("expected text missing: %r" % want)
        for bad in sc.get("absent_text", []):
            if subst(bad) in chk["text"]:
                problems.append("text that must not be there: %r" % bad)
        m = PRIVATE_RE.search(chk["text"])
        if m:
            problems.append("private-looking text: %r" % m.group(0))
        raw = shoot(page, sc.get("crop_" + vpname, sc.get("crop", "viewport")))
        data, size = optimise(raw, vp["scale"], vp["dpr"], tokens)
        rel = out_name(sc, lang, vpname)
        path = os.path.join(out_dir, rel.replace("/", os.sep))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(data)
        if size is None:
            size = page.evaluate("[window.innerWidth, window.innerHeight]")
        return {"id": sc["id"], "file": rel, "language": lang, "viewport": vpname,
                "scheme": sc.get("scheme", "dark"), "width": size[0], "height": size[1],
                "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                "title": sc.get("title", "")}, problems
    finally:
        if ctxb is not None:
            ctxb.close()
        mock.stop()


def write_manifest(path, manifest):
    files = sorted(manifest.values(), key=lambda e: e["file"])
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump({"generator": "tools/make_screenshots.py", "source": "firmware/tools/mock_device.py",
                   "files": files}, f, indent=1, ensure_ascii=False)
        f.write("\n")


def main():
    ap = argparse.ArgumentParser(description="Screenshots of the web panel from the mock device")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--only", default="")
    ap.add_argument("--lang", default="")
    ap.add_argument("--viewport", default="")
    ap.add_argument("--channel", default="", choices=("", "msedge", "chrome", "chromium"))
    ap.add_argument("--out", default=os.path.join(ROOT, "docs", "assets", "screenshots"))
    ap.add_argument("--firmware", default=os.path.join(ROOT, "firmware"))
    ap.add_argument("--scenes", default=os.path.join(ROOT, "tools", "screenshots.json"))
    ap.add_argument("--keep-going", action="store_true")
    a = ap.parse_args()

    _, scenes = load_scenes(a.scenes)
    only = [s for s in a.only.split(",") if s]
    langs = [s for s in a.lang.split(",") if s]
    vps = [s for s in a.viewport.split(",") if s]
    if only:
        scenes = [s for s in scenes if any(fnmatch.fnmatch(s["id"], o) for o in only)]
        if not scenes:
            raise SystemExit("no scene matches --only %s" % a.only)
    jobs = [(sc, lang, vp) for sc in scenes for lang, vp in variants(sc)
            if (not langs or lang in langs) and (not vps or vp in vps)]
    if a.list:
        for sc, lang, vp in jobs:
            log("%-40s %-3s %-8s %s" % (sc["id"], lang, vp, out_name(sc, lang, vp)))
        log("%d screenshots" % len(jobs))
        return 0

    fw_dir = os.path.abspath(a.firmware)
    mock_py = os.path.join(fw_dir, "tools", "mock_device.py")
    if not os.path.isfile(mock_py):
        raise SystemExit("no mock device at %s (use --firmware)" % mock_py)
    sys.path.insert(0, os.path.join(fw_dir, "tools"))
    import project_identity
    ident = project_identity.load(fw_dir)
    with open(os.path.join(fw_dir, "src", "version.h"), encoding="utf-8") as f:
        version = re.search(r'#define\s+FW_VERSION\s+"([^"]+)"', f.read()).group(1)
    ident_mod = {"ident": ident, "marker": project_identity.fwid_marker(ident), "version": version}
    with open(mock_py, encoding="utf-8") as f:
        mock_src = f.read()
    features = {name for name, opt in (("demo_radio", "--demo-radio"), ("screenshot", "--screenshot"))
                if opt in mock_src}
    has_demo = "demo_radio" in features
    if "screenshot" not in features:
        log("WARNING: the mock has no --screenshot option: build dates, network name and console lines "
            "will show the mock markers")
    tokens = panel_colours(os.path.join(fw_dir, "web"))
    try:
        from playwright.sync_api import sync_playwright
    except ImportError:
        raise SystemExit("Playwright is missing: run this script with uvx --with playwright ... (see the top of the file)")

    os.makedirs(a.out, exist_ok=True)
    man_path = os.path.join(a.out, "manifest.json")
    manifest = {}
    if os.path.exists(man_path):
        with open(man_path, encoding="utf-8") as f:
            for e in json.load(f).get("files", []):
                manifest[e["file"]] = e
    tmp = tempfile.mkdtemp(prefix="panel-shots-")
    failed = 0
    warned = 0
    t_start = time.time()
    try:
        with sync_playwright() as p:
            browsers = {}

            def browser_for(lang):
                if lang not in browsers:
                    browsers[lang], ch = launch(p, a.channel, lang)
                    log("browser for %s: %s %s" % (lang, ch, browsers[lang].version))
                return browsers[lang]
            log("%d screenshots" % len(jobs))
            for n, (sc, lang, vp) in enumerate(jobs, 1):
                if sc.get("mock", {}).get("demo_radio") and not has_demo:
                    log("[%d/%d] %s: skipped, the mock has no --demo-radio" % (n, len(jobs), sc["id"]))
                    continue
                t0 = time.time()
                try:
                    entry, problems = run_scene(browser_for(sc.get("browser_lang", lang)), sc, lang, vp, fw_dir,
                                                tmp, ident_mod, a.out, features, tokens)
                except Exception as e:      # noqa: BLE001  (reported, then the next scene or stop)
                    failed += 1
                    log("[%d/%d] %s %s %s: FAILED %s" % (n, len(jobs), sc["id"], lang, vp, str(e).splitlines()[0][:300]))
                    if not a.keep_going:
                        break
                    continue
                manifest[entry["file"]] = entry
                log("[%d/%d] %-34s %s %-7s %5d kB %4.1f s%s" % (n, len(jobs), sc["id"], lang, vp, entry["bytes"] // 1024,
                    time.time() - t0, ("  WARNING " + "; ".join(problems)) if problems else ""))
                if problems:
                    warned += 1
                write_manifest(man_path, manifest)
            for b in browsers.values():
                b.close()
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    write_manifest(man_path, manifest)
    log("done in %d s: %d failed, %d with warnings; manifest %s" % (time.time() - t_start, failed, warned, man_path))
    return 1 if failed or warned else 0


if __name__ == "__main__":
    sys.exit(main())
