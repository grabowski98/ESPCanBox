# Finds the CAN bits that change when you do something in the car.
#
# Reads a capture in the project format (one event per line, candump -L style):
#     (12.345678) can0 0F6#8E501E33BE5D5D24      CAN frame: time in s, interface, ID#DATA
#     # STEP 3 19:06:41.149 Volume + three times    comment line; some comments are markers
# Lines of other interfaces (uartA, uartB) and other comments are skipped.
#
# For every marker (a comment line written when you started an action) it compares two windows:
#     before = the seconds just before the marker   (the car "at rest")
#     after  = the seconds just after the marker    (while you do the action)
# and lists the bits that were steady in "before" and changed in "after".
# A comment line carries no time of its own: a marker gets the time of the first CAN frame after
# it (on a sleeping bus that is the moment the bus woke up). Markers of tools/record.py carry the
# device time "(seconds)" in their text; that time is used instead.
# Two filters remove most of the noise:
#     - counters: bytes or half-bytes that step by the same amount in nearly every frame of the
#       whole capture (rolling counters, timers); they are never reported;
#     - background: a bit that already changed in the "before" window is ignored for that marker.
#
# Usage (from the firmware/ directory or with full paths):
#     python -I tools/can_diff.py capture.log                       every marker, default windows
#     python -I tools/can_diff.py capture.log --list                list the markers and stop
#     python -I tools/can_diff.py capture.log --marker 17 --after 10
#     python -I tools/can_diff.py capture.log --match "volume" --summary
#     python -I tools/can_diff.py capture.log --export frames.log   CAN lines only, for other tools
#     python -I tools/can_diff.py test_data/synthetic/marker_demo.log --selftest
#
# Byte and bit numbers follow the project convention: byte 1 = the first data byte (data[0]),
# bits 7..0 inside a byte, short form byte.bit ("5.7" = byte 5 bit 7).
#
# Read only: it opens the capture for reading and writes nothing except the --export file.
# Python standard library only.
import argparse
import re
import sys

sys.dont_write_bytecode = True

FRAME_RE = re.compile(r"^\s*(?:\((\d+(?:\.\d+)?)\)\s+)?(can\w*)\s+([0-9A-Fa-f]{1,3})#([0-9A-Fa-f]*)\s*$")
OTHER_IFACE_RE = re.compile(r"^\s*(?:\(\d+(?:\.\d+)?\)\s+)?(uart\w*)\s")
DEFAULT_MARKER = r"^#\s*(STEP|KROK|MARK|MARKER)\b"
EMBEDDED_TIME_RE = re.compile(r"\((\d+\.\d+)\)")     # record.py writes the device time into its markers


class Capture:
    def __init__(self):
        self.frames = []     # (time, id, bytes)
        self.markers = []    # dict(index, line, text, time)
        self.bad = []        # (line number, text) of lines that look like frames but are broken
        self.expect = {}     # selftest: marker index -> set of "ID:byte.bit"
        self.expect_new = {} # selftest: marker index -> set of IDs
        self.expect_bad = None
        self.expect_counters = None


def load(path, marker_re):
    cap = Capture()
    pending = []             # markers waiting for the time of the next frame
    last_t = None
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for n, raw in enumerate(f, 1):
            line = raw.rstrip("\r\n")
            s = line.strip()
            if not s:
                continue
            if s.startswith("#"):
                parse_expect(cap, s)
                if marker_re.search(s):
                    m = {"index": len(cap.markers) + 1, "line": n, "text": s, "time": None}
                    e = EMBEDDED_TIME_RE.search(s)
                    if e:
                        m["time"] = float(e.group(1))
                    else:
                        pending.append(m)
                    cap.markers.append(m)
                continue
            if OTHER_IFACE_RE.match(s):
                continue
            fm = FRAME_RE.match(s)
            if not fm:
                if "#" in s:                      # looks like ID#DATA but does not parse
                    cap.bad.append((n, s[:80]))
                continue
            hexdata = fm.group(4)
            if len(hexdata) % 2 or len(hexdata) > 16:   # never trust the length: max 8 bytes, whole bytes
                cap.bad.append((n, s[:80]))
                continue
            t = float(fm.group(1)) if fm.group(1) else (last_t if last_t is not None else 0.0)
            fid = int(fm.group(3), 16)
            if fid > 0x7FF:
                cap.bad.append((n, s[:80]))
                continue
            data = bytes.fromhex(hexdata)
            cap.frames.append((t, fid, data))
            last_t = t
            for m in pending:
                m["time"] = t
            pending = []
    for m in pending:                             # markers after the last frame
        m["time"] = last_t if last_t is not None else 0.0
    return cap


def parse_expect(cap, s):
    # Selftest annotations inside a synthetic capture:
    #   # expect 2 300:2.5 310:2.3      marker 2 must report exactly these bits ("none" = nothing)
    #   # expect-new 1 450              IDs absent before marker 1 and present after it
    #   # expect-bad 4                  number of broken frame lines in the file
    #   # expect-counters 100:2 100:8L  counter fields (byte, L/H = low/high half-byte)
    p = s[1:].split()
    if not p:
        return
    if p[0] == "expect" and len(p) >= 2:
        cap.expect[int(p[1])] = set() if p[2:] == ["none"] else set(p[2:])
    elif p[0] == "expect-new" and len(p) >= 2:
        cap.expect_new[int(p[1])] = set(x.upper() for x in p[2:])
    elif p[0] == "expect-bad" and len(p) == 2:
        cap.expect_bad = int(p[1])
    elif p[0] == "expect-counters":
        cap.expect_counters = set(p[1:])


def find_counters(frames, min_frames=20, share=0.9):
    # A field is a counter when, in at least 90 % of consecutive frames of the same ID, it steps by
    # the same non-zero amount (modulo 256 for a byte, modulo 16 for a half-byte).
    by_id = {}
    for t, fid, data in frames:
        by_id.setdefault(fid, []).append(data)
    counters = {}            # id -> set of (byte index 0-based, "B"/"L"/"H")
    for fid, rows in by_id.items():
        if len(rows) < min_frames:
            continue
        found = set()
        width = max(len(r) for r in rows)
        for b in range(width):
            vals = [r[b] for r in rows if len(r) > b]
            if len(vals) < min_frames:
                continue
            flags = {}
            for kind, get, mod in (("B", lambda v: v, 256), ("L", lambda v: v & 15, 16),
                                   ("H", lambda v: v >> 4, 16)):
                steps = {}
                for a, c in zip(vals, vals[1:]):
                    d = (get(c) - get(a)) % mod
                    steps[d] = steps.get(d, 0) + 1
                best, cnt = max(steps.items(), key=lambda kv: kv[1])
                flags[kind] = best != 0 and cnt >= share * (len(vals) - 1)
                flags[kind + "0"] = steps.get(0, 0) >= 0.99 * (len(vals) - 1)    # (nearly) constant
            # A half-byte counter next to a constant half-byte also looks like a byte counter
            # (it steps by 1 in 15 of 16 frames): report the half-byte, not the byte.
            if flags["L"] and flags["H0"]:
                found.add((b, "L"))
            elif flags["H"] and flags["L0"]:
                found.add((b, "H"))
            elif flags["B"]:
                found.add((b, "B"))
            else:
                if flags["L"]:
                    found.add((b, "L"))
                if flags["H"]:
                    found.add((b, "H"))
        if found:
            counters[fid] = found
    return counters


def counter_mask(fields, length):
    mask = [0] * length
    for b, kind in fields:
        if b < length:
            mask[b] |= {"B": 0xFF, "L": 0x0F, "H": 0xF0}[kind]
    return mask


def window(frames, t0, t1):
    # Frames are in file order, which is time order in a normal capture; scan linearly so that a
    # capture with a time jump (device restart) still works.
    by_id = {}
    for t, fid, data in frames:
        if t0 <= t < t1:
            by_id.setdefault(fid, []).append((t, data))
    return by_id


def analyse(cap, marker, before, after, counters, only_ids=None):
    t = marker["time"]
    pre = window(cap.frames, t - before, t)
    post = window(cap.frames, t, t + after)
    result = {"changed": {}, "new": [], "gone": [], "noisy_bits": 0, "counter_ids": 0}
    for fid in sorted(post):
        if only_ids and fid not in only_ids:
            continue
        rows = post[fid]
        if fid not in pre:
            result["new"].append((fid, len(rows)))
            continue
        prev = pre[fid]
        length = max(len(d) for _, d in prev + rows)
        cmask = counter_mask(counters.get(fid, ()), length)
        if any(cmask):
            result["counter_ids"] += 1
        bits = []
        for b in range(length):
            for bit in range(7, -1, -1):
                m = 1 << bit
                if cmask[b] & m:
                    continue
                pv = [(d[b] & m) != 0 for _, d in prev if len(d) > b]
                av = [(d[b] & m) != 0 for _, d in rows if len(d) > b]
                if not pv or not av:
                    continue
                if len(set(pv)) > 1:              # already moving before the marker: background
                    if len(set(av)) > 1 or av[0] != pv[0]:
                        result["noisy_bits"] += 1
                    continue
                base = pv[0]
                seq = [base] + av
                changes = sum(1 for x, y in zip(seq, seq[1:]) if x != y)
                if changes:
                    first = next(tt for (tt, d), v in zip(rows, av) if v != base)
                    bits.append((b, bit, int(base), changes, first - t))
        if bits:
            byte_vals = {}
            for b in sorted(set(x[0] for x in bits)):
                seen = []
                for _, d in [prev[-1]] + rows:
                    if len(d) > b and (not seen or seen[-1] != d[b]):
                        seen.append(d[b])
                byte_vals[b] = seen
            result["changed"][fid] = (bits, byte_vals)
    for fid in sorted(pre):
        if fid not in post and (not only_ids or fid in only_ids):
            result["gone"].append(fid)
    return result


def fmt_id(fid):
    return "%03X" % fid


def bit_name(b, bit):
    return "%d.%d" % (b + 1, bit)


def print_marker(cap, marker, res, before, after, max_values=8):
    print("=" * 78)
    print("marker %d, line %d, at %.3f s" % (marker["index"], marker["line"], marker["time"]))
    print("  %s" % marker["text"][:150])
    print("  window: %.1f s before, %.1f s after; ignored: %d background bits, counters in %d IDs"
          % (before, after, res["noisy_bits"], res["counter_ids"]))
    if not res["changed"]:
        print("  no bit changed")
    for fid, (bits, byte_vals) in res["changed"].items():
        print("  %s" % fmt_id(fid))
        for b in sorted(byte_vals):
            vals = byte_vals[b]
            shown = " ".join("%02X" % v for v in vals[:max_values]) + (" ..." if len(vals) > max_values else "")
            bl = ["%s %d->%d x%d @+%.2fs" % (bit_name(bb, bit), base, 1 - base, ch, ft)
                  for bb, bit, base, ch, ft in bits if bb == b]
            print("    byte %d: %s" % (b + 1, shown))
            print("      bits: %s" % ", ".join(bl))
    if res["new"]:
        print("  new IDs (absent before, present after): %s"
              % " ".join("%s(%d)" % (fmt_id(f), n) for f, n in res["new"][:40])
              + (" ..." if len(res["new"]) > 40 else ""))
    if res["gone"]:
        print("  IDs that stopped: %s" % " ".join(fmt_id(f) for f in res["gone"][:40]))


def print_summary(selected, results):
    # bits that changed at many of the selected markers: an action repeated 3 times should give 3 hits
    hits = {}
    for res in results:
        for fid, (bits, _) in res["changed"].items():
            for b, bit, _base, _ch, _ft in bits:
                hits.setdefault((fid, b, bit), 0)
                hits[(fid, b, bit)] += 1
    n = len(results)
    print("=" * 78)
    print("summary of %d markers: %s" % (n, " ".join(str(m["index"]) for m in selected)))
    rows = sorted(hits.items(), key=lambda kv: (-kv[1], kv[0]))
    shown = 0
    for (fid, b, bit), h in rows:
        if h < max(2, n) and shown >= 20:
            break
        print("  %d/%d  %s %s" % (h, n, fmt_id(fid), bit_name(b, bit)))
        shown += 1
    if not rows:
        print("  no bit changed at any of them")


def selftest(cap, counters, before, after):
    errors = []
    if cap.expect_bad is not None and len(cap.bad) != cap.expect_bad:
        errors.append("broken lines: expected %d, found %d" % (cap.expect_bad, len(cap.bad)))
    if cap.expect_counters is not None:
        got = set()
        for fid, fields in counters.items():
            for b, kind in fields:
                got.add("%s:%d%s" % (fmt_id(fid), b + 1, "" if kind == "B" else kind))
        if got != cap.expect_counters:
            errors.append("counters: expected %s, found %s" % (sorted(cap.expect_counters), sorted(got)))
    if not cap.expect and not cap.expect_new:
        errors.append("no '# expect' lines in the file: --selftest needs a synthetic capture")
    for m in cap.markers:
        res = analyse(cap, m, before, after, counters)
        got = set()
        for fid, (bits, _) in res["changed"].items():
            for b, bit, *_ in bits:
                got.add("%s:%s" % (fmt_id(fid), bit_name(b, bit)))
        if m["index"] in cap.expect and got != cap.expect[m["index"]]:
            errors.append("marker %d: expected %s, found %s"
                          % (m["index"], sorted(cap.expect[m["index"]]), sorted(got)))
        if m["index"] in cap.expect_new:
            gotn = set(fmt_id(f) for f, _ in res["new"])
            if gotn != cap.expect_new[m["index"]]:
                errors.append("marker %d new IDs: expected %s, found %s"
                              % (m["index"], sorted(cap.expect_new[m["index"]]), sorted(gotn)))
    # parser checks that do not depend on the file
    probe = Capture()   # only its list is used
    for s in ("(1.0) can0 123#0011", "(1.0) can0 1234#00", "(1.0) can0 123#001", "(1.0) can0 123#" + "00" * 9):
        fm = FRAME_RE.match(s)
        ok = bool(fm) and len(fm.group(4)) % 2 == 0 and len(fm.group(4)) <= 16
        probe.frames.append(ok)
    if probe.frames != [True, False, False, False]:
        errors.append("frame line checks: %s" % probe.frames)
    wrap = [(i * 0.01, 0x10, bytes([(250 + i) % 256, 0x30 | (i % 16)])) for i in range(40)]
    if find_counters(wrap).get(0x10) != {(0, "B"), (1, "L")}:
        errors.append("counter with wrap-around not found: %s" % find_counters(wrap).get(0x10))
    for e in errors:
        print("FAIL: %s" % e)
    print("selftest: %d markers, %d frames, %d errors" % (len(cap.markers), len(cap.frames), len(errors)))
    return 1 if errors else 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="Show CAN bits that change around the markers of a capture.")
    ap.add_argument("capture", help="capture file, format '(time) can0 ID#DATA'")
    ap.add_argument("--before", type=float, default=3.0, help="seconds before the marker (default 3)")
    ap.add_argument("--after", type=float, default=5.0, help="seconds after the marker (default 5)")
    ap.add_argument("--marker", type=int, action="append", help="only this marker number (repeatable)")
    ap.add_argument("--match", help="only markers whose text contains this (case-insensitive)")
    ap.add_argument("--id", help="only these IDs, comma separated, e.g. 0F6,21F")
    ap.add_argument("--summary", action="store_true", help="count how many selected markers each bit changed at")
    ap.add_argument("--list", action="store_true", help="list the markers and stop")
    ap.add_argument("--marker-regex", default=DEFAULT_MARKER,
                    help="which comment lines are markers (default: STEP, KROK, MARK, MARKER)")
    ap.add_argument("--export", metavar="OUT", help="write only the CAN frame lines to OUT and stop")
    ap.add_argument("--selftest", action="store_true", help="check the results against '# expect' lines")
    a = ap.parse_args(argv)
    if a.before <= 0 or a.after <= 0:
        ap.error("--before and --after must be greater than 0")
    try:
        cap = load(a.capture, re.compile(a.marker_regex))
    except OSError as e:
        print("cannot read %s: %s" % (a.capture, e))
        return 2
    if a.export:
        with open(a.export, "w", encoding="utf-8", newline="\n") as out:
            for t, fid, data in cap.frames:
                out.write("(%.6f) can0 %s#%s\n" % (t, fmt_id(fid), data.hex().upper()))
        print("%d frames written to %s" % (len(cap.frames), a.export))
        return 0
    counters = find_counters(cap.frames)
    if a.selftest:
        return selftest(cap, counters, a.before, a.after)
    print("%s: %d frames, %d IDs, %d markers, %d broken lines skipped"
          % (a.capture, len(cap.frames), len(set(f for _, f, _ in cap.frames)), len(cap.markers), len(cap.bad)))
    if counters:
        print("counters (never reported): %s" % " ".join(
            "%s:%s" % (fmt_id(f), ",".join("%d%s" % (b + 1, "" if k == "B" else k) for b, k in sorted(fl)))
            for f, fl in sorted(counters.items())))
    if a.list:
        for m in cap.markers:
            print("  %3d  line %6d  %10.3f s  %s" % (m["index"], m["line"], m["time"], m["text"][:110]))
        return 0
    only_ids = None
    if a.id:
        try:
            only_ids = set(int(x, 16) for x in a.id.split(",") if x.strip())
        except ValueError:
            ap.error("--id takes hex IDs separated by commas, e.g. 0F6,21F")
    selected = [m for m in cap.markers
                if (not a.marker or m["index"] in a.marker)
                and (not a.match or a.match.lower() in m["text"].lower())]
    if not cap.markers:
        print("the file has no markers (comment lines such as '# STEP ...'); see --marker-regex")
        return 1
    if not selected:
        print("no marker selected (use --list to see them)")
        return 1
    results = []
    for m in selected:
        res = analyse(cap, m, a.before, a.after, counters, only_ids)
        results.append(res)
        if not a.summary:
            print_marker(cap, m, res, a.before, a.after)
    if a.summary:
        print_summary(selected, results)
    return 0


if __name__ == "__main__":
    sys.exit(main())
