#!/usr/bin/env python3
"""Check that every protocol constant in the firmware has a source in provenance.md.

The constants of the radio protocol, the key map, the vehicle settings and the transmit allow
list are extracted from the firmware sources (the files are searched by name anywhere under firmware/lib, so moving them between
libraries does not break the check). Each constant becomes a key that contains its value, for
example `xp_keys row 21F/0/0C/08->14` or `xp_protocol HEADER=2E`. A changed value is a new key
and needs a new row.

provenance.md lists the keys in table rows whose first cell is the key in backticks:
    | `xp_protocol HEADER=2E` | measured | can_20261111_191045.log ... |
Column 2 = status, column 3 = source. Allowed status values:
    measured
    design
    source: firmware analysis / needs owner decision

Errors (exit 1): a key from the code missing in the table, a row without status or source, an
unknown status, an extraction anchor not found in the code (update this tool).
Rows whose key is no longer in the code are reported as warnings.

Usage:
    python -I tools/check_provenance.py [--root .] [--doc radios/xp-simple-soft/provenance.md] [--list]
--list prints the keys extracted from the code and exits.
Uses only the Python standard library.
"""
import argparse
import os
import re
import sys

STATUSES = {"measured", "design", "source: firmware analysis / needs owner decision"}
SKIP_DIRS = {".pio", ".build_host", "__pycache__", ".git"}


def find_file(root, name):
    hits = []
    for base, dirs, files in os.walk(os.path.join(root, "firmware", "lib")):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        if name in files:
            hits.append(os.path.join(base, name))
    if len(hits) != 1:
        raise LookupError("%s: found %d times under firmware/lib" % (name, len(hits)))
    with open(hits[0], encoding="utf-8") as f:
        return f.read()


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def body(text, signature):
    """Text of the function whose definition line contains `signature`, up to the closing brace
    in column 0."""
    i = text.find(signature)
    if i < 0:
        raise LookupError("anchor not found: " + signature)
    j = text.find("\n}", i)
    if j < 0:
        raise LookupError("end of function not found: " + signature)
    return text[i:j]


def hx(v):
    return "%02X" % int(v, 0)


def extract(root):
    keys = []
    # --- xp_protocol.h: named constants and enum values ---
    t = strip_comments(find_file(root, "xp_protocol.h"))
    for name, val in re.findall(r"\b([A-Z][A-Z0-9_]+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\s*[,;]", t):
        keys.append("xp_protocol %s=%s" % (name, hx(val) if val.startswith("0x") else val))
    # (The box command numbers BOX_TRIP_*, BOX_OBSTACLE and BOX_FEATURES moved from xp_car.h / xp_settings.h
    # into the enum of xp_protocol.h in stage F4; they are extracted above as "xp_protocol ...".)
    # --- key map: CAN side in the car profile, XP codes in the radio protocol ---
    # The keys keep the old name `xp_keys row ID/byte/mask/value->code` (one row per CAN bit and
    # XP code) so the table did not need to change when the map moved out of xp_keys.cpp (stage E8).
    ids = {}
    for name, val in re.findall(r"\b(ID_[A-Z0-9_]+)\s*=\s*(0x[0-9A-Fa-f]+)\s*;",
                                strip_comments(find_file(root, "can_ids.h"))):
        ids[name] = int(val, 16)
    codes = dict(re.findall(r"case\s+KeyId::([A-Z0-9_]+)\s*:\s*return\s+(0x[0-9A-Fa-f]+)\s*;",
                            body(strip_comments(find_file(root, "protocol_xp.cpp")), "keyCode(")))
    t = strip_comments(find_file(root, "profile_psa_aee2010.cpp"))
    rows = re.findall(r"\{\s*psa::(ID_[A-Z0-9_]+)\s*,\s*(\d+)\s*,\s*(0x[0-9A-Fa-f]+)\s*,\s*(0x[0-9A-Fa-f]+)\s*,\s*KeyId::([A-Z0-9_]+)\s*\}", t)
    if not rows:
        raise LookupError("profile_psa_aee2010.cpp: KEYS rows not found")
    for idname, byte, mask, val, key in rows:
        if idname not in ids or key not in codes:
            raise LookupError("key row %s/%s: CAN id or XP code not found" % (idname, key))
        keys.append("xp_keys row %03X/%s/%s/%s->%s" % (ids[idname], byte, hx(mask), hx(val), hx(codes[key])))
    knobs = re.findall(r"\{\s*psa::ID_[A-Z0-9_]+\s*,\s*\d+\s*,\s*KeyId::([A-Z0-9_]+)\s*,\s*KeyId::([A-Z0-9_]+)\s*\}", t)
    if not knobs:
        raise LookupError("profile_psa_aee2010.cpp: KNOBS rows not found")
    knob_names = {"NEXT": "CODE_NEXT", "PREV": "CODE_PREV",
                  "PANEL_KNOB_UP": "CODE_KNOB_UP", "PANEL_KNOB_DOWN": "CODE_KNOB_DOWN"}
    for pair in knobs:
        for key in pair:
            if key not in codes:
                raise LookupError("knob %s: XP code not found" % key)
            keys.append("xp_keys %s=%s" % (knob_names.get(key, "CODE_" + key), hx(codes[key])))
    # --- xp_parking.cpp: zone map ---
    t = strip_comments(find_file(root, "xp_parking.cpp"))
    m = re.search(r"MAP\[8\]\s*=\s*\{([^}]*)\}", t)
    if not m:
        raise LookupError("xp_parking.cpp: MAP[8] not found")
    keys.append("xp_parking MAP=" + ",".join(x.strip() for x in m.group(1).split(",")))
    # --- protocol_xp.cpp: version frame (until 2026-11-19 in radio_link.cpp) ---
    t = strip_comments(find_file(root, "protocol_xp.cpp"))
    m = re.search(r"VERSION_DATA\[\d+\]\s*=\s*\{([^}]*)\}", t)
    if not m:
        raise LookupError("protocol_xp.cpp: VERSION_DATA not found")
    keys.append("protocol_xp VERSION_DATA=" + " ".join(hx(x.strip()) for x in m.group(1).split(",")))
    # --- radio_commands: command number, name tables ---
    # (The 364 climate bits, keys "radio_commands air364 ...", were removed from the code on 2026-11-19.
    # A function named buildAir364 coming back is refused below, so such bits cannot return without rows.)
    t = strip_comments(find_file(root, "radio_commands.h"))
    for name, val in re.findall(r"\b(RADIO_CMD_[A-Z0-9_]+)\s*=\s*(0x[0-9A-Fa-f]+)\s*;", t):
        keys.append("radio_commands %s=%s" % (name, hx(val)))
    t = strip_comments(find_file(root, "radio_commands.cpp"))
    if "buildAir364" in t:
        raise LookupError("radio_commands.cpp: buildAir364 is back; extend this tool so its bits get rows")
    for fn, prefix in (("airCommandName(", "air"), ("soundCommandName(", "sound")):
        for n in re.findall(r"case\s+(\d+)\s*:", body(t, fn)):
            keys.append("radio_commands %s %02X" % (prefix, int(n)))
    # --- vehicle_settings: frame numbers, setting table, language table, 1A9 buttons ---
    t = strip_comments(find_file(root, "vehicle_settings.h"))
    for name, val in re.findall(r"\b(CMD_[A-Z0-9_]+|ID_[A-Z0-9_]+)\s*=\s*(0x[0-9A-Fa-f]+)\s*;", t):
        keys.append("vehicle_settings %s=%s" % (name, hx(val)))
    t = strip_comments(find_file(root, "vehicle_settings.cpp"))
    rows = re.findall(r"\{\s*(0x[0-9A-Fa-f]+)\s*,\s*\"", t)
    if not rows:
        raise LookupError("vehicle_settings.cpp: setting table not found")
    for sid in rows:
        keys.append("vehicle_settings setting %s" % hx(sid))
    for psa, radio in re.findall(r"case\s+(\d+)\s*:\s*return\s+(\d+)\s*;", body(t, "radioLanguage(")):
        keys.append("vehicle_settings language %s->%s" % (psa, radio))
    b = body(t, "buildButton(")
    for sid, var, val in re.findall(r"case\s+(0x[0-9A-Fa-f]+)\s*:\s*(b4|b7)\s*=\s*(0x[0-9A-Fa-f]+)", b):
        keys.append("vehicle_settings button %s %s=%s" % (hx(sid), var, hx(val)))
    base = re.findall(r"out\[(\d)\]\s*=\s*(0x[0-9A-Fa-f]+)\s*;", b)
    keys.append("vehicle_settings button frame=" + " ".join("%s:%s" % (i, hx(v)) for i, v in base))
    # --- xp_settings.h: settings frame numbers to the radio ---
    t = strip_comments(find_file(root, "xp_settings.h"))
    for name, val in re.findall(r"\b(CMD_[A-Z0-9_]+)\s*=\s*(0x[0-9A-Fa-f]+)\s*;", t):
        keys.append("xp_settings %s=%s" % (name, hx(val)))
    # --- transmit allow list of the car profile (stage E9: moved out of can_tx_gate.h) ---
    t = strip_comments(find_file(root, "profile_psa_aee2010.h"))
    txids = dict(re.findall(r"\b(ID_TX_[A-Z0-9_]+)\s*=\s*(0x[0-9A-Fa-f]+)\s*;", t))
    t = strip_comments(find_file(root, "tx_allowlist.cpp"))
    rows = re.findall(r"\{\s*(ID_TX_[A-Z0-9_]+)\s*,\s*\d+\s*,", t)
    if not rows:
        raise LookupError("tx_allowlist.cpp: TX_ALLOW rows not found")
    for name in rows:
        if name not in txids:
            raise LookupError("tx_allowlist.cpp: %s not defined in profile_psa_aee2010.h" % name)
        keys.append("tx_allowlist %s=%03X" % (name, int(txids[name], 16)))
    # dedupe, keep order
    seen = set()
    out = []
    for k in keys:
        if k not in seen:
            seen.add(k)
            out.append(k)
    return out


def read_table(path):
    rows = {}
    problems = []
    with open(path, encoding="utf-8") as f:
        for no, line in enumerate(f, 1):
            m = re.match(r"\|\s*`([^`]+)`\s*\|(.*)$", line.rstrip())
            if not m:
                continue
            cells = [c.strip() for c in m.group(2).rstrip("|").split("|")]
            key = m.group(1)
            status = cells[0] if cells else ""
            source = cells[1] if len(cells) > 1 else ""
            if key in rows:
                problems.append("%d: duplicate key `%s`" % (no, key))
            if status not in STATUSES:
                problems.append("%d: `%s`: unknown status '%s'" % (no, key, status))
            if not source:
                problems.append("%d: `%s`: no source" % (no, key))
            rows[key] = (status, source)
    return rows, problems


def main():
    ap = argparse.ArgumentParser(description="Every protocol constant in the firmware needs a source.")
    ap.add_argument("--root", default=".")
    ap.add_argument("--doc", default=os.path.join("radios", "xp-simple-soft", "provenance.md"))
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()
    try:
        keys = extract(args.root)
    except LookupError as e:
        print("ERROR extraction: %s (update tools/check_provenance.py)" % e)
        return 1
    if args.list:
        for k in keys:
            print(k)
        return 0
    rows, problems = read_table(os.path.join(args.root, args.doc))
    errors = len(problems)
    for p in problems:
        print("ERROR " + p)
    missing = [k for k in keys if k not in rows]
    for k in missing:
        print("ERROR not in the table: `%s`" % k)
    errors += len(missing)
    stale = [k for k in rows if k not in set(keys)]
    for k in stale:
        print("WARN  in the table but not in the code: `%s`" % k)
    counts = {}
    for k in keys:
        if k in rows:
            counts[rows[k][0]] = counts.get(rows[k][0], 0) + 1
    print("check_provenance: %d constants, %d without source, %s"
          % (len(keys), len(missing), ", ".join("%s: %d" % (s, n) for s, n in sorted(counts.items()))))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
