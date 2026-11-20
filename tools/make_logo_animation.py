#!/usr/bin/env python3
"""Animated version of the project logo for the README header.

Reads the static logo docs/assets/brand/logo-transparent.svg (the dot-matrix display) and writes
    docs/assets/brand/logo-animated.svg        animated SVG (one file for the light and the dark theme:
                                               the display carries its own bezel, like the static logo)
and, with --gif, the fallbacks for viewers that do not animate SVG, at twice the README width
(1054 px, shown at width="527" like the static logo, sharp on HiDPI screens):
    docs/assets/brand/logo-animated.gif        on white (#ffffff)
    docs/assets/brand/logo-animated-dark.gif   on GitHub dark (#0d1117)

The animation repeats every 10 s, like a character LCD driven by the device:
  0.0 s  empty display (unlit dots only)
  0.3 s  a block cursor appears in the first character cell
  0.6 s  the name is typed one character every 0.16 s, the cursor moves ahead
  1.9 s  the cursor stops in the cell after the last letter and blinks three times (0.5 s on, 0.5 s
         off), but only when the grid has a free cell there: the current logo has none, so the cursor
         simply goes away after the last letter
  2.4 s  the middle part ("Can") is inverted: dark block, letters in the glass colour
  9.7 s  the display is cleared; the loop starts again at 10.0 s
From 2.4 s to 9.7 s the display is exactly the static logo, plus the cursor while it blinks (until
4.4 s). When the grid has no free cell after the last letter, there is no blinking cursor.

Every dot is taken from the static SVG, so the animation always matches the logo: when the project
name changes (project.json, tools/set_project_name.py), the static logo is redrawn by hand
(docs/en/contributing/brand.md) and this script is run again. The script checks that the number
of character cells in the drawing equals the length of "project_name" in project.json, and uses
that name for the <title> of the SVG files.

How it is built (why it works in a README):
  - GitHub shows images from a README through <img>; a browser runs no scripts inside an SVG used
    as an image, but CSS animations inside it run. So the files use only an inline <style> with
    @keyframes: no scripts, no fonts, no external references.
  - Every element is drawn in its FINAL state (the static logo). The keyframes only switch
    opacity between 0 and 1 (step-end, no fades). Under prefers-reduced-motion: reduce the
    animation is switched off, so the viewer sees the static logo.
  - Same viewBox, width and height as logo-transparent.svg: the README layout does not change.

One SVG serves both themes, as the static README logo does. The GIF files differ: GIF has no smooth
transparency, so each one is drawn on the page colour of its theme.

Usage (from the repository root):
    python tools/make_logo_animation.py                write the two SVG files
    python tools/make_logo_animation.py --gif          also write the two GIF files (needs Pillow)
    python tools/make_logo_animation.py --frames DIR   also write PNG frames for review (needs Pillow)
    python tools/make_logo_animation.py --check        exit 1 if the SVG files on disk are not what
                                                       this script writes (standard library only)
Options: --root DIR (another checkout), --out DIR (output directory, default docs/assets/brand).
"""
import argparse
import json
import os
import re
import sys

SOURCE_REL = os.path.join("docs", "assets", "brand", "logo-transparent.svg")
OUT_REL = os.path.join("docs", "assets", "brand")
SVG_NAMES = ("logo-animated.svg",)
GIF_NAMES = (("logo-animated.gif", "#ffffff"), ("logo-animated-dark.gif", "#0d1117"))

# --- timeline, seconds --------------------------------------------------------------------------
LOOP = 10.0
T_CURSOR = 0.3      # cursor appears in the first cell
T_FIRST = 0.6       # first character
T_STEP = 0.16       # one character every T_STEP
T_INVERT = 2.4      # middle part inverted (final look)
BLINK = 0.5         # end cursor: on BLINK s, off BLINK s, three times from the last letter
BLINKS = 3
T_CLEAR = 9.7       # display cleared until the loop restarts

DOT_RE = re.compile(r"M(-?\d+(?:\.\d+)?) (-?\d+(?:\.\d+)?)h(\d+(?:\.\d+)?)v\d+(?:\.\d+)?h-\d+(?:\.\d+)?Z")
PATH_RE = re.compile(r"<path\b([^>]*)/>")
ATTR_RE = re.compile(r'([\w:-]+)="([^"]*)"')
NUM_RE = re.compile(r"-?\d+(?:\.\d+)?")


class LogoError(Exception):
    pass


# --- reading the static logo --------------------------------------------------------------------
def rounded_rects(d):
    """Rounded rectangles of a path made of M/H/A/V commands: [(x0, y0, x1, y1, r)]."""
    out = []
    for sub in re.findall(r"M[^M]*", d):
        r = float(re.search(r"A(-?\d+(?:\.\d+)?)", sub).group(1))
        xs, ys = [], []
        m = re.match(r"M(-?\d+(?:\.\d+)?) (-?\d+(?:\.\d+)?)", sub)
        xs.append(float(m.group(1)))
        ys.append(float(m.group(2)))
        xs += [float(v) for v in re.findall(r"H(-?\d+(?:\.\d+)?)", sub)]
        ys += [float(v) for v in re.findall(r"V(-?\d+(?:\.\d+)?)", sub)]
        out.append((min(xs) - r, min(ys), max(xs) + r, max(ys) + r, r))
    return out


def read_logo(text):
    """Split the static logo into its parts. Roles are found by structure, not by colour."""
    head = re.search(r"\A(?:\s*<!--.*?-->\s*)?", text, re.S).group(0).strip()
    svg_open = re.search(r"<svg\b[^>]*>", text).group(0)
    paths = []
    for m in PATH_RE.finditer(text):
        attrs = dict(ATTR_RE.findall(m.group(1)))
        paths.append(attrs)
    frame = [p for p in paths if "A" in p["d"]]
    dots = [p for p in paths if DOT_RE.search(p["d"])]
    rects = [p for p in paths if "A" not in p["d"] and not DOT_RE.search(p["d"])]
    if len(frame) != 3 or len(rects) != 1 or len(dots) != 3:
        raise LogoError("unexpected structure of %s: %d frame paths, %d plain rectangles, %d dot paths "
                        "(expected 3, 1, 3)" % (SOURCE_REL, len(frame), len(rects), len(dots)))
    glass = frame[2]["fill"]
    block = rects[0]
    # block path "M x0 y0 H x1 V y1 H x0 Z"
    nums = [float(v) for v in NUM_RE.findall(block["d"])]
    block_box = (min(nums[0], nums[2]), min(nums[1], nums[3]), max(nums[0], nums[2]), max(nums[1], nums[3]))

    def dot_list(p):
        return [(float(a), float(b)) for a, b, _ in DOT_RE.findall(p["d"])]

    size = float(DOT_RE.search(dots[0]["d"]).group(3))
    inverted = [p for p in dots if p["fill"].upper() == glass.upper()]
    lit = [p for p in dots if p["fill"].upper() == block["fill"].upper()]
    unlit = [p for p in dots if p not in inverted and p not in lit]
    if len(inverted) != 1 or len(lit) != 1 or len(unlit) != 1:
        raise LogoError("cannot tell lit, unlit and inverted dots apart by their fill colours")
    return {
        "head": head,
        "svg_open": svg_open,
        "frame_paths": frame,
        "bezel": rounded_rects(frame[0]["d"])[0],
        "rim": rounded_rects(frame[1]["d"]),
        "glass_rect": rounded_rects(frame[2]["d"])[0],
        "c_bezel": frame[0]["fill"], "c_rim": frame[1]["fill"], "c_glass": glass,
        "c_dot": block["fill"], "c_unlit": unlit[0]["fill"],
        "block": block_box,
        "size": size,
        "lit": dot_list(lit[0]),
        "unlit": dot_list(unlit[0]),
        "inverted": dot_list(inverted[0]),
    }


def analyse(logo):
    """Grid, character cells in typing order, position of the end cursor (None: no free cell)."""
    allx = sorted({x for x, _ in logo["unlit"] + logo["lit"]})
    ally = sorted({y for _, y in logo["unlit"] + logo["lit"]})
    pitch = min(b - a for a, b in zip(allx, allx[1:]))
    x0, x1, y0, y1 = allx[0], allx[-1], ally[0], ally[-1]
    cols = int(round((x1 - x0) / pitch)) + 1
    rows = int(round((y1 - y0) / pitch)) + 1
    bx0, by0, bx1, by1 = logo["block"]
    # every grid position must be drawn: unlit, lit, or under the block
    have = set(logo["unlit"]) | set(logo["lit"])
    for i in range(cols):
        for j in range(rows):
            x, y = x0 + i * pitch, y0 + j * pitch
            if (x, y) not in have and not (bx0 <= x < bx1 and by0 <= y < by1):
                raise LogoError("grid position (%g, %g) is not drawn in the static logo" % (x, y))
    cell = 6 * pitch                       # 5 dots + 1 dot gap (brand.md, Geometry)
    block_last = bx0 + (round((bx1 - bx0 - logo["size"]) / pitch)) * pitch
    parts = [
        ("plain", [d for d in logo["lit"] if d[0] < bx0], x0 + 2 * pitch),       # 2 dots margin
        ("inverted", logo["inverted"], bx0 + pitch),                               # 1 dot inside block
        ("plain", [d for d in logo["lit"] if d[0] > bx1], block_last + 3 * pitch),  # 3 dots between parts
    ]
    letter_rows = sorted({y for _, y in logo["lit"] + logo["inverted"]})
    ly0 = letter_rows[0]
    if len(letter_rows) > 7 or letter_rows[-1] - ly0 > 6 * pitch:
        raise LogoError("letters are taller than 7 dots")
    cells = []
    for kind, ds, start in parts:
        groups = {}
        for x, y in ds:
            off = (x - start) / pitch
            if off < 0 or int(round(off)) % 6 > 4:
                raise LogoError("dot (%g, %g) is not inside a 5 x 7 character cell" % (x, y))
            groups.setdefault(int(round(off)) // 6, []).append((x, y))
        n = max(groups) + 1 if groups else 0
        for k in range(n):
            cells.append({"kind": kind, "x": start + k * cell, "dots": sorted(groups.get(k, []))})
    end_x = cells[-1]["x"] + cell if cells else None
    end = (end_x, ly0) if end_x is not None and end_x + 4 * pitch <= x1 else None
    return {"pitch": pitch, "x0": x0, "y0": y0, "x1": x1, "y1": y1, "cols": cols, "rows": rows,
            "ly0": ly0, "cells": cells, "end": end}


# --- timeline -------------------------------------------------------------------------------
def timeline(geo):
    """{element id: [(time, opacity), ...]} as switch points; opacity before the first point is
    the value of the last point of the previous loop. Also the final (static) opacity."""
    n = len(geo["cells"])
    tl = {}
    final = {}
    for i, c in enumerate(geo["cells"]):
        t_on = T_FIRST + i * T_STEP
        if c["kind"] == "plain":
            tl["l%d" % i] = [(0.0, 0), (t_on, 1), (T_CLEAR, 0)]
            final["l%d" % i] = 1
        else:   # typed dark, then replaced by the inverted block
            tl["l%d" % i] = [(0.0, 0), (t_on, 1), (T_INVERT, 0)]
            final["l%d" % i] = 0
        c_on = T_CURSOR if i == 0 else T_FIRST + (i - 1) * T_STEP
        tl["u%d" % i] = [(0.0, 0), (c_on, 1), (t_on, 0)]
        final["u%d" % i] = 0
    tl["v"] = [(0.0, 0), (T_INVERT, 1), (T_CLEAR, 0)]
    final["v"] = 1
    t_end = T_FIRST + (n - 1) * T_STEP          # last letter typed: the cursor moves to the next cell
    if geo["end"] is not None:
        pts = [(0.0, 0)]
        for k in range(BLINKS):
            pts += [(t_end + 2 * k * BLINK, 1), (t_end + (2 * k + 1) * BLINK, 0)]
        tl["e"] = pts
        final["e"] = 0
    assert t_end < T_INVERT and t_end + 2 * BLINKS * BLINK < T_CLEAR < LOOP
    return tl, final


def opacity_at(points, t):
    value = points[-1][1]
    for tp, v in points:
        if t >= tp:
            value = v
    return value


def pct(t):
    s = "%.2f" % (t / LOOP * 100)
    s = s.rstrip("0").rstrip(".")
    return s + "%"


def keyframes(name, points):
    parts = []
    for t, v in points:
        parts.append("%s{opacity:%d}" % (pct(t), v))
    parts.append("100%%{opacity:%d}" % points[-1][1])
    return "@keyframes %s{%s}" % (name, "".join(parts))


# --- SVG ------------------------------------------------------------------------------------
def fmt(v):
    return ("%d" % v) if float(v).is_integer() else ("%g" % v)


def dots_d(dots, size, dx=0.0, dy=0.0):
    s = fmt(size)
    return " ".join("M%s %sh%sv%sh-%sZ" % (fmt(x - dx), fmt(y - dy), s, s, s) for x, y in dots)


def build_svg(logo, geo, name):
    tl, final = timeline(geo)
    p, size = geo["pitch"], logo["size"]
    full = [(geo["x0"] + i * p, geo["y0"] + j * p) for j in range(geo["rows"]) for i in range(geo["cols"])]
    cursor = [(c * p, r * p) for c in range(5) for r in range(7)]
    css = [".k{animation:%gs step-end infinite}" % LOOP]
    for el in tl:
        css.append("#%s{animation-name:%s}" % (el, el))
    for el, pts in tl.items():
        css.append(keyframes(el, pts))
    css.append("@media (prefers-reduced-motion:reduce){.k{animation:none}}")
    out = [logo["head"], logo["svg_open"]]
    out.append("<title>%s</title>" % name)
    out.append("<desc>Animated logo: the name is typed on a dot-matrix display. Generated by "
               "tools/make_logo_animation.py from logo-transparent.svg.</desc>")
    out.append("<style>%s</style>" % "".join(css))
    out.append('<defs><path id="cur" fill="%s" d="%s"/></defs>' % (logo["c_dot"], dots_d(cursor, size)))
    for fp in logo["frame_paths"]:
        attrs = " ".join('%s="%s"' % kv for kv in fp.items() if kv[0] != "d")
        out.append('<path %s d="%s"/>' % (attrs, fp["d"]))
    out.append('<path fill="%s" d="%s"/>' % (logo["c_unlit"], dots_d(full, size)))
    for i, c in enumerate(geo["cells"]):
        op = "" if final["l%d" % i] else ' opacity="0"'
        out.append('<path id="l%d" class="k"%s fill="%s" d="%s"/>' % (i, op, logo["c_dot"], dots_d(c["dots"], size)))
    bx0, by0, bx1, by1 = logo["block"]
    out.append('<g id="v" class="k"><path fill="%s" d="M%s %sH%sV%sH%sZ"/><path fill="%s" d="%s"/></g>' % (
        logo["c_dot"], fmt(bx0), fmt(by0), fmt(bx1), fmt(by1), fmt(bx0), logo["c_glass"],
        dots_d(logo["inverted"], size)))
    for i, c in enumerate(geo["cells"]):
        out.append('<use id="u%d" class="k" opacity="0" href="#cur" x="%s" y="%s"/>' % (i, fmt(c["x"]), fmt(geo["ly0"])))
    if geo["end"] is not None:
        out.append('<use id="e" class="k" opacity="0" href="#cur" x="%s" y="%s"/>' % (fmt(geo["end"][0]), fmt(geo["end"][1])))
    out.append("</svg>")
    return "\n".join(out) + "\n", tl


# --- raster (Pillow, only for --gif and --frames) ---------------------------------------------
def render(logo, geo, tl, t, bg, width, ss=4):
    from PIL import Image, ImageDraw
    vb = [float(v) for v in re.search(r'viewBox="([^"]+)"', logo["svg_open"]).group(1).split()]
    scale = width / vb[2] * ss
    height = int(round(vb[3] * width / vb[2]))
    img = Image.new("RGB", (width * ss, height * ss), bg)
    dr = ImageDraw.Draw(img)

    def X(x):
        return (x - vb[0]) * scale

    def Y(y):
        return (y - vb[1]) * scale

    def rr(box, fill):
        x0, y0, x1, y1, r = box
        dr.rounded_rectangle([X(x0), Y(y0), X(x1) - 1, Y(y1) - 1], radius=r * scale, fill=fill)

    def sq(x, y, fill, s=logo["size"]):
        dr.rectangle([X(x), Y(y), X(x + s) - 1, Y(y + s) - 1], fill=fill)

    rr(logo["bezel"], logo["c_bezel"])
    rr(logo["rim"][0], logo["c_rim"])
    rr(logo["rim"][1], logo["c_bezel"])
    rr(logo["glass_rect"], logo["c_glass"])
    p = geo["pitch"]
    for j in range(geo["rows"]):
        for i in range(geo["cols"]):
            sq(geo["x0"] + i * p, geo["y0"] + j * p, logo["c_unlit"])
    for i, c in enumerate(geo["cells"]):
        if opacity_at(tl["l%d" % i], t):
            for x, y in c["dots"]:
                sq(x, y, logo["c_dot"])
    if opacity_at(tl["v"], t):
        bx0, by0, bx1, by1 = logo["block"]
        dr.rectangle([X(bx0), Y(by0), X(bx1) - 1, Y(by1) - 1], fill=logo["c_dot"])
        for x, y in logo["inverted"]:
            sq(x, y, logo["c_glass"])
    cursors = [(c["x"], tl["u%d" % i]) for i, c in enumerate(geo["cells"])]
    if "e" in tl:
        cursors.append((geo["end"][0], tl["e"]))
    for cx, pts in cursors:
        if opacity_at(pts, t):
            for cc in range(5):
                for r in range(7):
                    sq(cx + cc * p, geo["ly0"] + r * p, logo["c_dot"])
    return img.resize((width, height), Image.LANCZOS)


def switch_times(tl):
    return sorted({t for pts in tl.values() for t, _ in pts if t < LOOP})


def write_gif(logo, geo, tl, path, bg, width):
    from PIL import Image
    times = switch_times(tl)
    frames = [render(logo, geo, tl, t, bg, width) for t in times]
    durations = [int(round(((times[i + 1] if i + 1 < len(times) else LOOP) - t) * 1000))
                 for i, t in enumerate(times)]
    # one shared palette, so a colour never changes between frames
    sheet = Image.new("RGB", (width, frames[0].height * len(frames)))
    for i, f in enumerate(frames):
        sheet.paste(f, (0, i * f.height))
    pal = sheet.quantize(colors=48, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    q = [f.quantize(palette=pal, dither=Image.Dither.NONE) for f in frames]
    q[0].save(path, save_all=True, append_images=q[1:], duration=durations, loop=0,
              optimize=True, disposal=1)


# --- main -----------------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description="Animated logo (SVG, GIF) from the static dot-matrix logo.")
    ap.add_argument("--root", default=".")
    ap.add_argument("--out", default=None, help="output directory (default: <root>/docs/assets/brand)")
    ap.add_argument("--gif", action="store_true", help="also write the GIF fallbacks (Pillow)")
    ap.add_argument("--gif-width", type=int, default=1054,
                    help="GIF width in px (1054 = twice the README width of 527 px, for HiDPI screens)")
    ap.add_argument("--frames", metavar="DIR", help="also write PNG frames for review (Pillow)")
    ap.add_argument("--check", action="store_true", help="only compare the SVG files on disk")
    args = ap.parse_args()
    root = args.root
    out_dir = args.out or os.path.join(root, OUT_REL)
    try:
        with open(os.path.join(root, SOURCE_REL), encoding="utf-8") as f:
            logo = read_logo(f.read())
        with open(os.path.join(root, "project.json"), encoding="utf-8") as f:
            name = json.load(f)["project_name"]
        geo = analyse(logo)
    except (OSError, ValueError, KeyError, LogoError) as e:
        print("ERROR: %s" % e)
        return 1
    letters = len(geo["cells"])
    if letters != len(name):
        print("ERROR: the static logo has %d character cells, project_name %r has %d characters: "
              "redraw the logo first (docs/en/contributing/brand.md)" % (letters, name, len(name)))
        return 1
    svg, tl = build_svg(logo, geo, name)
    if args.check:
        bad = 0
        for n in SVG_NAMES:
            path = os.path.join(out_dir, n)
            try:
                with open(path, encoding="utf-8") as f:
                    same = f.read() == svg
            except OSError:
                same = False
            if not same:
                print("OUTDATED %s (run tools/make_logo_animation.py)" % path)
                bad += 1
        print("make_logo_animation --check: %s" % ("OK" if not bad else "%d file(s) outdated" % bad))
        return 1 if bad else 0
    os.makedirs(out_dir, exist_ok=True)
    for n in SVG_NAMES:
        with open(os.path.join(out_dir, n), "w", encoding="utf-8", newline="\n") as f:
            f.write(svg)
        print("wrote %s (%d bytes)" % (os.path.join(out_dir, n), len(svg.encode("utf-8"))))
    if args.gif:
        for n, bg in GIF_NAMES:
            path = os.path.join(out_dir, n)
            write_gif(logo, geo, tl, path, bg, args.gif_width)
            print("wrote %s (%d bytes)" % (path, os.path.getsize(path)))
    if args.frames:
        os.makedirs(args.frames, exist_ok=True)
        times = [0.15, 0.45, 0.85, 1.25, 1.65, 2.10, 2.60, 3.00, 5.00, 9.80]
        for label, bg in (("light", "#ffffff"), ("dark", "#0d1117")):
            for k, t in enumerate(times):
                path = os.path.join(args.frames, "frame_%02d_%05.2fs_%s.png" % (k, t, label))
                render(logo, geo, tl, t, bg, 1054).save(path, optimize=True)
        print("wrote %d PNG frames to %s" % (2 * len(times), args.frames))
    return 0


if __name__ == "__main__":
    sys.exit(main())
