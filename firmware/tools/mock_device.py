#!/usr/bin/env python3
# Mock of the 508 device for working on the web panel without the board (Python standard library only).
#
# Replays a CAN capture (sniffer format: "(seconds) can0 ID#DATA") and answers the same HTTP
# addresses as the firmware, in the same format: docs/PANEL_API.md (requirements: chapter 9).
#
# Running (from the firmware directory):
#     python tools/mock_device.py <capture.log>                 client "from the device network"
#     python tools/mock_device.py <capture.log> --lan           client "from the home network": login lockout
#                                                               and 403 ap_only on changes
#     python tools/mock_device.py <capture.log> --lan --lan-admin    as after "wifi lan on"
# Then open http://127.0.0.1:8080/ in a browser. The mock password is printed at startup
# (or --password ...). Other options: --port, --host, --speed (replay speed), --mode decoder,
# --no-loop (do not loop the capture).
#
# Privacy: when a capture is loaded, the data of frames with the VIN number (336, 3B6, 2B6) is replaced with zeros;
# the real content never leaves the mock. Raw captures from the car are not added to the repository
# (test_data/captures/ holds only anonymized ones).
#
# Simplifications compared to the device (the contract does not require them): the fields sys.*, radio.*, audio.*, wifi.*
# are constant (except uptimeMs, boot, elf, mode, otaState and what requests change); "the device is beeping"
# (park.beeping / continuous) is computed directly from the sound request in frame 0E1, without the beep generator;
# no key decoding (lastKey always null); counters do not wrap after 2^32.

import argparse
import base64
import collections
import gzip
import hashlib
import json
import math
import os
import random
import re
import secrets
import string
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qsl

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(TOOLS_DIR)
sys.path.insert(0, TOOLS_DIR)
sys.dont_write_bytecode = True     # no __pycache__ directory in tools/ (repository)
import embed_web  # noqa: E402  (the same page assembly and CSP computation as at build time)
import project_identity  # noqa: E402  (product name and marker from include/project_identity.h)

IDENT = project_identity.load(PROJECT_DIR)
PRODUCT = IDENT["PROJECT_FW_PRODUCT"]
API_VERSION = 1
VIN_IDS = (0x336, 0x3B6, 0x2B6)
MAGIC = project_identity.fwid_marker(IDENT)

LOG_LINES_MAX = 200          # line memory in the device
LOG_BYTES_MAX = 8192
LOG_TEXT_MAX = 200           # the device truncates longer lines
LOG_RESP_LINES = 64          # per /api/log response
LOG_RESP_BYTES = 4096
TRACE_MAX = 2048             # frames in memory
FRAMES_RESP_MAX = 400
FRAMES_DEFAULT = 256
TABLE_MAX = 128
CMD_QUEUE_MAX = 4
FORM_MAX = 1024
URI_MAX = 496                       # measured on the board 17.11.2026 (PANEL_API 1.4)
QUERY_MAX = 127
HEADERS_MAX = 2048
SLOT_SIZE = 3342336
MIN_IMAGE = 65536
RESTART_MS = 1500
APPLY_MS = 1000
CONFIRM_MS = 20000
DEFAULT_MAP = [("9F", "9B"), ("21", "1F")]      # lib/car_psa_aee2010/profile_psa_aee2010.cpp
MAP_SIZE = 16
VOLUME_MAX = 80
VOLUME_MIN_SAVED = 15                         # src/config.h AUDIO_VOLUME_MIN_SAVED
VOLUME_DEFAULT = 40                           # src/config.h AUDIO_VOLUME_DEFAULT
# Panel languages built into the page (WEB_UI_LANGS of the firmware, tools/embed_web.py web_langs.h).
UI_LANGS = embed_web.lang_codes(os.path.join(PROJECT_DIR, "web"))
# Addresses of the separate dictionaries (GET only), as WEB_I18N of the firmware.
I18N_URIS = set(embed_web.I18N_URI % c for c in UI_LANGS if c != embed_web.PAGE_LANG)


def u32():
    return random.getrandbits(32)


def read_fw_version():
    try:
        with open(os.path.join(PROJECT_DIR, "src", "version.h"), encoding="utf-8") as f:
            m = re.search(r'#define\s+FW_VERSION\s+"([^"]+)"', f.read())
            if m:
                return m.group(1)
    except OSError:
        pass
    return "0.0.0"


def build_stamp():
    t = time.localtime()
    return time.strftime("%b ", t) + ("%2d" % t.tm_mday) + time.strftime(" %Y %H:%M:%S", t)


# ---------- loading a capture ----------
LINE_RE = re.compile(r"^\((\d+(?:\.\d+)?)\)\s+\S+\s+([0-9A-Fa-f]{1,8})#([0-9A-Fa-f]*)\s*$")


def load_log(path):
    frames = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = LINE_RE.match(line.strip())
            if not m:
                continue
            cid = int(m.group(2), 16)
            data = m.group(3).upper()
            if cid > 0x7FF or len(data) % 2 or len(data) > 16:
                continue
            if cid in VIN_IDS:
                data = "0" * len(data)            # VIN blanked already while loading
            frames.append((float(m.group(1)), cid, bytes.fromhex(data)))
    return frames


# ---------- decoding: the same rules as lib/core ----------
def decode_park(d):
    # lib/car_psa_aee2010/psa_frames.cpp, decodeParkingAid
    return {
        "rearStatus": (d[0] >> 5) & 7, "frontStatus": (d[0] >> 2) & 7,
        "soundFront": bool(d[1] & 0x20), "sound": bool(d[1] & 0x10),
        "interval": d[2] & 0x3F,
        "rear": [(d[3] >> 5) & 7, (d[3] >> 2) & 7, (d[4] >> 5) & 7],
        "front": [(d[4] >> 2) & 7, (d[5] >> 5) & 7, (d[5] >> 2) & 7],
    }


class PowerPolicy:
    """Port of lib/core/power_policy.cpp (ACC, reverse, illumination)."""
    ACC_OFF_DELAY = 2500
    FRAME_TIMEOUT = 1000

    def __init__(self):
        self.raw = 0
        self.reverse_bit = False
        self.have = False
        self.last_ms = 0
        self.acc = False
        self.off_pending = False
        self.off_at = 0
        self.lights = False
        self.have_lights = False
        self.lights_ms = 0
        self.out = {"ignition": False, "acc": False, "reverse": False, "lights": False}

    def on_body(self, raw, reverse, now):
        self.raw, self.reverse_bit, self.have, self.last_ms = raw, reverse, True, now

    def on_lights(self, on, now):
        self.lights, self.have_lights, self.lights_ms = on, True, now

    def update(self, now):
        fresh = self.have and now - self.last_ms <= self.FRAME_TIMEOUT
        ign = fresh and self.raw not in (0x80, 0x00)
        if ign:
            self.acc = True
            self.off_pending = False
        elif self.acc:
            if not self.off_pending:
                self.off_pending = True
                self.off_at = now + self.ACC_OFF_DELAY
            elif now >= self.off_at:
                self.acc = False
                self.off_pending = False
        lf = self.have_lights and now - self.lights_ms <= self.FRAME_TIMEOUT
        self.out = {"ignition": ign, "acc": self.acc, "reverse": ign and self.reverse_bit,
                    "lights": ign and lf and self.lights}
        return self.out


# ---------- device ----------
class Device:
    def __init__(self, args, frames):
        self.args = args
        self.frames = frames
        self.lock = threading.RLock()
        self.password = args.password
        self.keymap = list(DEFAULT_MAP)
        self.keymap_saved = False           # "NVS" key table stored (False = profile defaults, export null)
        self.volume = VOLUME_DEFAULT
        self.volume_saved = None            # stored volume (None = nothing stored, export null)
        self.ui_lang = None                 # panel language stored in the device (None = English)
        self.wifi_sta = None                # name of the saved home network
        self.sta_reason = 0
        self.lan_admin = args.lan_admin
        # login lockout (only clients from the home network, contract 1.2)
        # login lockout: the same rules as webauth::LoginGuard (lib/core/web_auth.cpp), times in seconds
        self.fails = 0
        self.window_start = 0.0
        self.lock_len = 0.0          # length of the last lockout (0 = none, as lockMs_)
        self.lock_start = 0.0
        self.locked = False
        self.next_lock = 30.0
        self.last_fail = 0.0
        self.last_bad_pw = None      # last wrong password (by password only, as the device)
        self.last_bad_t = 0.0
        # programs in the two slots
        self.running = {"slot": "app0", "version": read_fw_version(), "build": build_stamp(),
                        "mode": args.mode, "net": True, "elf": "%016X" % random.getrandbits(64),
                        # canTx byte of the marker (+70): 0 = the program cannot transmit on the car CAN;
                        # 1 with the --can-tx option (program decoder_tx_wifi)
                        "canTx": 1 if getattr(args, "can_tx", False) else 0}
        # Transmitting frame 15B (since 1.0.0): arming = the build string of the program in which
        # "cantx on" was entered; valid only for the same program and only after a restart.
        self.tx_tag = None                  # build string saved in "NVS"
        self.tx_armed = False               # this boot is armed
        self.vset_mask = 0x02               # item mask: by default only item 01
        self.other = None                   # None = the second slot is empty
        self.ota_state = "none"
        self.last = {"result": "none", "error": "", "bytes": 0}
        self.restart_at = None
        self.restart_reason = "none"
        self.after_restart = None           # function called on a simulated restart
        self.timers = []                    # [time_ms, function]
        self.replay_t0 = time.monotonic()
        self.replay_idx = 0
        self.replay_base = None
        self.boot_reset()

    # --- device start (also after a restart) ---
    def boot_reset(self):
        self.boot = u32()
        self.t0 = time.monotonic()
        self.lines = collections.deque()
        self.lines_bytes = 0
        self.line_head = 0
        self.trace = collections.deque(maxlen=TRACE_MAX)
        self.trace_head = 0
        self.table = {}
        self.table_overflow = 0
        self.rx = 0
        self.fps = 0
        self.fps_count = 0
        self.fps_window = 0
        self.last_bus_ms = None
        self.power = PowerPolicy()
        self.park = decode_park(bytes([0, 0, 0x3F, 0xFC, 0xFC, 0xFC]))
        self.park["rearStatus"] = self.park["frontStatus"] = 0
        self.park_ms = None
        self.body_raw = 0
        self.body_ms = None                 # last 0F6 frame (ignitionRaw = null when it is older than 1 s)
        self.reverse_bit = False
        self.temp = None
        self.temp_ms = None
        self.doors = 0
        self.doors_known = False            # a door frame has arrived since start (otherwise doors = null)
        # vehicle settings (car.settings): last frames 260, 227, 3E1, 217 and the state of frame 38
        self.set_frames = {}                # frame id -> bytes of the last frame
        self.set260_ms = None
        self.set38 = [0, 0, 0, 0, 0]        # DATA[1..5]; zeros until the first 260 frame with 7 bytes
        self.set_have260 = False
        self.lamp_on = False
        self.test_until = None              # test beeping ("test"): park.beeping = true
        self.log_keys = True
        self.dump = False
        self.queue = []
        self.restart_at = None
        self.restart_reason = "none"
        self.confirm_at = None
        self.log("# " + IDENT["PROJECT_NAME"] + " (MOCK). CAN 125 kbit/s LISTEN ONLY. Mode: %s." %
                 ("DECODER" if self.running["mode"] == "decoder" else "alongside the box"))
        if self.ota_state == "pending":
            self.confirm_at = CONFIRM_MS

    def now(self):
        return int((time.monotonic() - self.t0) * 1000)

    # --- line ring ---
    def log(self, text):
        text = text.replace("\r", "").replace("\n", " ")[:LOG_TEXT_MAX]
        self.lines.append((self.line_head, self.now(), text))
        self.line_head += 1
        self.lines_bytes += len(text.encode("utf-8"))
        while len(self.lines) > LOG_LINES_MAX or self.lines_bytes > LOG_BYTES_MAX:
            old = self.lines.popleft()
            self.lines_bytes -= len(old[2].encode("utf-8"))

    # --- CAN frame (from the capture or pasted with a command) ---
    def on_frame(self, cid, data, from_bus):
        now = self.now()
        ids = "%03X" % cid
        hexd = data.hex().upper()
        self.trace.append((self.trace_head, now, ids, hexd))
        self.trace_head += 1
        self.fps_count += 1
        if from_bus:
            self.last_bus_ms = now
            self.rx += 1
        t = self.table.get(ids)
        if t is None:
            if len(self.table) >= TABLE_MAX:
                self.table_overflow += 1
            else:
                self.table[ids] = [1, 0, now, hexd]
        else:
            t[1] = now - t[2]
            t[0] += 1
            t[2] = now
            t[3] = hexd
        d = data
        if cid == 0x0E1 and len(d) >= 6:
            p = decode_park(d)
            old = self.park
            nf = lambda z: min(z)   # noqa: E731
            changed = (p["sound"] != old["sound"] or p["interval"] != old["interval"] or
                       p["rearStatus"] != old["rearStatus"] or p["frontStatus"] != old["frontStatus"] or
                       p["soundFront"] != old["soundFront"] or nf(p["front"]) != nf(old["front"]) or
                       nf(p["rear"]) != nf(old["rear"]))
            self.park = p
            self.park_ms = now
            if changed:
                self.print_park()
        elif cid == 0x0F6:
            if len(d) >= 8:
                rev = bool(d[7] & 0x80)
                if rev != self.reverse_bit:
                    self.log("[car] reverse=%d" % rev)
                self.body_raw, self.reverse_bit = d[0], rev
                self.body_ms = now
                self.power.on_body(d[0], rev, now)
            if len(d) >= 7:
                self.temp = d[6] / 2.0 - 40.0
                self.temp_ms = now
        elif cid == 0x036 and len(d) >= 4:
            self.power.on_lights(bool(d[3] & 0x20), now)
        elif cid in (0x260, 0x227, 0x3E1, 0x217):
            self.set_frames[cid] = bytes(d[:8])
            if cid == 0x260:
                self.set260_ms = now
            self.update38()
        elif cid == 0x1A1 and len(d) >= 4:
            # lib/car_psa_aee2010/psa_signals.cpp, decodeDoors
            if d[1] == 0xDE and self.lamp_on:
                self.set_doors(d[3] & 0xF8)
        elif cid == 0x128 and len(d) >= 5:
            self.lamp_on = bool(d[4] & 0x40)
            if not self.lamp_on:
                self.set_doors(0)

    # Settings state for frame 38, DATA[1..5]: rewritten from update38 (now lib/app/xp_settings.cpp; the device
    # builds the same bytes through psa::decodeSettings + xp::encodeCarState).
    # The mock has no radio, so there are no 80 commands: car.settings.last = null, counters 0.
    SET_LANG = {0: 3, 1: 0, 2: 4, 3: 5, 4: 6, 5: 6, 6: 7, 8: 6, 9: 8, 11: 1, 12: 9, 14: 2}

    def update38(self):
        p = self.set_frames.get(0x260)
        ok260 = p is not None and len(p) >= 7
        if ok260:
            self.set_have260 = True
        if not self.set_have260:
            return
        d1, d2, d3, d4, d5 = self.set38
        if ok260:
            b1, b2, b3, b4, b5, b7 = p[0], p[1], p[2], p[3], p[4], p[6]
            d1 = (d1 & 0x7B) | (0x80 if b5 & 0x20 else 0) | (0x04 if b7 & 0x10 else 0)
            amb = (((b2 >> 3) & 7) + 1) if b3 & 0x01 else 0
            d2 = (d2 & 0x0F) | ((amb & 7) << 5) | (0x10 if b5 & 0x80 else 0)
            fmh = (((b4 >> 5) & 3) + 1) if b4 & 0x80 else 0
            wel = (((b4 >> 1) & 3) + 1) if b4 & 0x08 else 0
            d3 = (d3 & 0x07) | ((fmh & 3) << 6) | ((wel & 3) << 4) | (0x08 if b3 & 0x02 else 0)
            if b2 & 0x04:
                d3 = (d3 & 0xF9) | ((b2 & 3) << 1)
            lang = self.SET_LANG.get((b1 >> 2) & 0x1F, d4 & 0x0F)
            d4 = (d4 & 0xC0) | (0x20 if b3 & 0x08 else 0) | (0 if b3 & 0x40 else 0x10) | lang
            unit = (b1 & 3, b2 >> 7)
            if unit == (1, 0):
                d3 &= 0xFE
                d5 &= 0x3F
            elif unit == (0, 0):
                d3 |= 0x01
                d5 = (d5 & 0x3F) | 0x40
            elif unit == (3, 1):
                d5 = (d5 & 0x3F) | 0x80
        f = self.set_frames.get(0x227)
        if f is not None and len(f) >= 3:
            d2 = (d2 & 0xF7) | (0x08 if f[0] & 0x40 else 0)
            d4 = (d4 & 0x7F) | (0x80 if f[2] & 0x01 else 0)
        f = self.set_frames.get(0x3E1)
        if f is not None and len(f) >= 4:
            v = f[3] & 0xC0
            if v == 0x80:
                d4 |= 0x40
            elif v == 0x40:
                d4 &= 0xBF
        f = self.set_frames.get(0x217)
        if f is not None and len(f) >= 8:
            d1 = (d1 & 0xFD) | (0x02 if f[7] & 0x02 else 0)
        self.set38 = [d1 & 0xFF, d2 & 0xFF, d3 & 0xFF, d4 & 0xFF, d5 & 0xFF]

    def set_doors(self, v):
        self.doors_known = True
        if v != self.doors:
            self.doors = v
            self.log("[car] doors=0x%02X" % v)

    def print_park(self):
        p = self.park
        self.log("[park] rear=%d front=%d sound=%d %s interval=0x%02X zones rear %d%d%d front %d%d%d" % (
            p["rearStatus"], p["frontStatus"], p["sound"], "front" if p["soundFront"] else "rear", p["interval"],
            p["rear"][0], p["rear"][1], p["rear"][2], p["front"][0], p["front"][1], p["front"][2]))

    def park_fresh(self, now):
        return self.park_ms is not None and now - self.park_ms <= 400

    def car_busy(self, now):
        a = self.park["rearStatus"] == 4 or self.park["frontStatus"] == 4
        return self.power.out["reverse"] or (self.park_fresh(now) and a)

    # --- main loop of the mock (every 10 ms) ---
    def tick(self):
        now = self.now()
        # replay the capture at the --speed rate
        if self.frames:
            if self.replay_base is None:
                self.replay_base = (time.monotonic(), self.frames[0][0])
            el = (time.monotonic() - self.replay_base[0]) * self.args.speed + self.replay_base[1]
            while self.replay_idx < len(self.frames) and self.frames[self.replay_idx][0] <= el:
                _, cid, data = self.frames[self.replay_idx]
                self.on_frame(cid, data, True)
                self.replay_idx += 1
            if self.replay_idx >= len(self.frames) and not self.args.no_loop:
                self.replay_idx = 0
                self.replay_base = (time.monotonic() + 2.0 / self.args.speed, self.frames[0][0])
        po = self.power.update(now)
        if po["acc"] != getattr(self, "_acc_shown", False):
            self._acc_shown = po["acc"]
            self.log("[car] acc=%d" % po["acc"])
        # one-second window for frames / s
        if now // 1000 != self.fps_window:
            self.fps_window = now // 1000
            self.fps, self.fps_count = self.fps_count, 0
        # command queue: one per pass, like the main loop
        if self.queue:
            item = self.queue.pop(0)
            item()
        for t in list(self.timers):
            if now >= t[0]:
                self.timers.remove(t)
                t[1]()
        if self.confirm_at is not None and now >= self.confirm_at and not self.car_busy(now):
            self.confirm_at = None
            self.ota_state = "valid"
            self.log("[ota] program confirmed")
        # Simplification (PANEL_API 9): before restarting, the firmware also waits for ACC off (net.cpp netRestartDue);
        # the mock does not, because the car capture has the ignition on almost all the time and the restart would never happen.
        if self.restart_at is not None and now >= self.restart_at and not self.car_busy(now):
            fn = self.after_restart
            self.after_restart = None
            if fn:
                fn()
            # Transmit arming: valid only for the program with the same build string.
            if self.running.get("canTx", 0) and self.tx_tag == self.running["build"]:
                self.tx_armed = True
            else:
                self.tx_armed = False
                self.tx_tag = None
            self.boot_reset()

    def schedule_restart(self, reason, fn=None):
        self.restart_at = self.now() + RESTART_MS
        self.restart_reason = reason
        self.after_restart = fn

    # --- console commands (executed from the queue) ---
    # CAN transmit commands from the panel (firmware: handleTxCommand in console_cmds.cpp). The mock
    # transmits nothing; only the state shown in /api/status changes.
    SENDABLE = (0x01, 0x02, 0x06, 0x07, 0x08, 0x09, 0x0F)

    def run_tx_cmd(self, line):
        built = self.running.get("canTx", 0) != 0
        if line == "cantx":
            if not built:
                self.log("[cantx] built=0: no CAN transmit in this build (CAN listen only)")
            else:
                self.log("[cantx] built=1 armed=%d after_restart=%d controller=%s mask=0x%08X" %
                         (self.tx_armed, self.tx_tag is not None, "NORMAL" if self.tx_armed else "listen", self.vset_mask))
                self.log("[cantx] state=%s sent=0 acked=0 failed=0 refused=0 tec=0 rec=0 fault=none" %
                         ("ready" if self.tx_armed else "disarmed"))
            return True
        m = re.match(r"^(cantx (on|off)|(?:vset|ust) (on|off) ([0-9A-Fa-f]{1,2}))$", line)
        if not m:
            return False
        if not built:
            self.log("[cantx] REFUSED: this program cannot transmit on CAN")
        elif line == "cantx on":
            self.tx_tag = self.running["build"]
            self.log("[cantx] arming stored. Takes effect AFTER A RESTART of the device (now armed=%d). "
                     "Flashing another program disarms." % self.tx_armed)
        elif line == "cantx off":
            self.tx_tag = None
            self.tx_armed = False
            self.log("[cantx] DISARMED immediately")
        else:
            on = m.group(3) == "on"
            nn = int(m.group(4), 16)
            if nn not in self.SENDABLE:
                self.log("[vset] item %02X cannot be enabled: fixed refusal in the program (no measurement or no frame)" % nn)
            else:
                self.vset_mask = (self.vset_mask | (1 << nn)) if on else (self.vset_mask & ~(1 << nn))
                self.log("[vset] item %02X %s, mask=0x%08X" % (nn, "ENABLED" if on else "disabled", self.vset_mask))
        return True

    def run_cmd(self, line):
        now = self.now()
        self.log("# www> " + line)
        if self.run_tx_cmd(line):
            return
        m = re.match(r"^([0-9A-Fa-f]{1,3})#([0-9A-Fa-f]{0,16})$", line)
        if m and len(m.group(2)) % 2 == 0 and int(m.group(1), 16) <= 0x7FF:
            self.on_frame(int(m.group(1), 16), bytes.fromhex(m.group(2)), False)
            return
        busy = self.car_busy(now)
        # main.cpp handleLine: "map reset" also writes flash (earlier it was let through by mistake).
        if line in ("test", "map reset") or line.startswith("vol ") or re.match(r"^map\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+", line):
            if busy:
                self.log("[www] skipped: the car needs the device")
                return
        if line == "stat":
            po = self.power.out
            self.log("[stat] mode=%s can=ok audio=ok radio=ok (mock)" %
                     ("DECODER" if self.running["mode"] == "decoder" else "alongside the box"))
            alive = self.last_bus_ms is not None and now - self.last_bus_ms < 1000
            self.log("[stat] rx=%d lost=0 errors=0 bus=%s volume=%d%% supply=no 12V (0.00V)" %
                     (self.rx, "active" if alive else "quiet", self.volume))
            self.log("[stat] ignition=%d (0x%02X) acc=%d reverse=%d lights=%d" %
                     (po["ignition"], self.body_raw, po["acc"], po["reverse"], po["lights"]))
            self.print_park()
        elif line == "radio":
            self.log("[radio] transmit=%s received frames=0 acks=0 bad checksum=0 truncated=0 garbage=0" %
                     ("YES" if self.running["mode"] == "decoder" else "no"))
            self.log("[radio] the radio has not spoken yet")
        elif line == "ver":
            self.log("[ver] %s %s %s, built %s (mock)" % (PRODUCT, self.running["version"],
                                                                 self.running["mode"], self.running["build"]))
        elif line.startswith("vol "):
            try:
                v = max(0, min(100, int(line[4:].strip() or "0")))
            except ValueError:
                v = 0
            self.volume = min(v, VOLUME_MAX)
            if self.volume >= VOLUME_MIN_SAVED:
                self.volume_saved = self.volume
                self.log("[audio] volume=%d%%" % self.volume)
            else:
                self.log("[audio] volume=%d%% (below 15%% only until restart)" % self.volume)
        elif line == "test":
            self.test_until = now + 1500
            self.log("[audio] test")
        elif line == "keys":
            self.log_keys = not self.log_keys
            self.log("[key] printing=%d" % self.log_keys)
        elif line.startswith("key "):
            self.log("[key] trial code %02X%s" % (hex_code(line[4:]), self.not_sent()))
        elif line.startswith("kd ") or line.startswith("ku "):
            self.log("[key] manual code=%02X state=%d%s" % (hex_code(line[3:]), 1 if line[1] == "d" else 0,
                                                         self.not_sent()))
        elif line == "map reset":
            self.keymap = list(DEFAULT_MAP)
            self.keymap_saved = False
            self.print_map()
        elif line.startswith("map"):
            m = re.match(r"^map\s+([0-9A-Fa-f]{1,2})\s+([0-9A-Fa-f]{1,2})$", line)
            if m:
                z, na = "%02X" % int(m.group(1), 16), "%02X" % int(m.group(2), 16)
                self.keymap = [p for p in self.keymap if p[0] != z]
                if z != na:
                    if len(self.keymap) >= MAP_SIZE:
                        self.log("[key] swap table full")
                    else:
                        self.keymap.append((z, na))
                self.keymap_saved = True
            self.print_map()
        elif line == "lang" or line.startswith("lang "):
            arg = line[5:]
            if arg == "default":
                self.ui_lang = None
            elif arg:
                if arg not in UI_LANGS:
                    self.log("[lang] unknown language; available: %s | default" % ",".join(UI_LANGS))
                    return
                self.ui_lang = arg
            self.log("[lang] panel language=%s; available: %s" %
                     ((self.ui_lang + " (stored in the device)") if self.ui_lang else "en (default, nothing stored)",
                      ",".join(UI_LANGS)))
        elif line == "settings":
            self.log("[settings] " + json.dumps(self.export(), separators=(",", ":")))
        elif line.startswith("settings import "):
            vals, bad = parse_backup(line[16:])
            if bad is None:
                bad = self.import_problem(vals)
            if bad is not None:
                self.log("[settings] import refused (%s), nothing changed" % bad)
            else:
                self.apply_import(vals)
        elif line == "doorswap":
            self.log("[car] door side swap=0; until restart")
        elif line == "rxlog":
            self.log("[radio_rx] printing=1")
        elif line == "dump":
            # Like the firmware: toggles the frame dump on the USB port (about 218 lines/s); the panel shows only this line.
            self.dump = not self.dump
            self.log("# CAN frame dump=%d" % self.dump)
        elif line:
            if line != "help":
                self.log("Unknown command: " + line)
            # From the panel the firmware does not show the wifi commands (USB only).
            self.log("Commands: stat | radio | ver | vset | cantx | vol 0-100 | test | keys | key/kd/ku CODE | map FROM TO | "
                     "map reset | lang [CODE|default] | settings [import JSON] | doorswap | rxlog | dump | "
                     "frame e.g. 0E1#B0F000FCFC1EC200")

    # --- settings backup (lib/core/settings_backup.h) ---
    def export(self):
        return {"v": 1, "lang": self.ui_lang, "vol": self.volume_saved,
                "keys": ";".join("%s=%s" % p for p in self.keymap) if self.keymap_saved else None,
                "net": True, "lan": self.lan_admin}

    def import_problem(self, vals):
        """Like console_cmds.cpp importProblem: name of the first bad field or None."""
        if vals.get("lang") is not None and vals["lang"] not in UI_LANGS:
            return "bad lang"
        if vals.get("vol") is not None and vals["vol"] < VOLUME_MIN_SAVED:
            return "bad vol"
        if vals.get("keys") is not None and parse_map(vals["keys"]) is None:
            return "bad keys"
        return None

    def apply_import(self, vals):
        res = {"lang": "-", "vol": "-", "keys": "-"}
        if "lang" in vals:
            self.ui_lang = vals["lang"]
            res["lang"] = "ok"
        if "vol" in vals:
            self.volume = VOLUME_DEFAULT if vals["vol"] is None else min(vals["vol"], VOLUME_MAX)
            self.volume_saved = None if vals["vol"] is None else self.volume
            res["vol"] = "ok"
        if "keys" in vals:
            self.keymap = list(DEFAULT_MAP) if vals["keys"] is None else parse_map(vals["keys"])
            self.keymap_saved = vals["keys"] is not None
            res["keys"] = "ok"
        self.log("[settings] import: lang=%s vol=%s keys=%s" % (res["lang"], res["vol"], res["keys"]))
        if "net" in vals or "lan" in vals:
            self.log("[settings] net and lan are not imported: USB commands wifi on|off and wifi lan on|off")

    def not_sent(self):
        return "" if self.running["mode"] == "decoder" else ": not sent (listen mode)"

    def print_map(self):
        for z, na in self.keymap:
            self.log("[key] swap %s -> %s" % (z, na))

    # --- login (contract 1.2) ---
    def lock_left(self):
        if not self.locked or self.lock_len == 0:
            return 0.0
        left = self.lock_len - (time.monotonic() - self.lock_start)
        if left <= 0:
            self.locked = False
            return 0.0
        return left

    def guard_failure(self, t):
        if self.lock_left() > 0:
            return
        if self.fails == 0 or t - self.window_start >= 60:      # fixed window from the first failure
            self.window_start = t
            self.fails = 0
        self.fails += 1
        self.last_fail = t
        if self.fails >= 5:
            self.lock_len = self.next_lock
            self.lock_start = t
            self.locked = True
            self.next_lock = 300.0 if self.next_lock >= 150 else self.next_lock * 2
            self.fails = 0
            self.log("[www] login lockout (mock)")

    def guard_success(self, t):
        if self.lock_len == 0 or self.lock_left() > 0:
            return
        if t - (self.lock_start + self.lock_len) < 60:
            return
        if self.fails > 0 and t - self.last_fail < 60:
            return
        self.lock_len = 0.0
        self.next_lock = 30.0

    @staticmethod
    def parse_basic(header):
        """As webauth::parseBasic: scheme in any letter case, spaces / tabs around. (user, pw) or (None, None)."""
        m = re.fullmatch(r"[ \t]*basic[ \t]+(\S+)[ \t]*", header or "", re.IGNORECASE)
        if not m:
            return None, None
        try:
            dec = base64.b64decode(m.group(1), validate=True).decode("latin-1")
        except (ValueError, UnicodeDecodeError):
            return None, None
        if ":" not in dec:
            return None, None
        user, _, pw = dec.partition(":")
        return user, pw

    def check_auth(self, header):
        """Returns None (allowed) or (code, error object). Order and rules as httpRequireAuth (http_server.cpp)."""
        lan = self.args.lan
        t = time.monotonic()
        if self.last_bad_pw is not None and t - self.last_bad_t >= 60:
            self.last_bad_pw = None
        if lan and self.lock_left() > 0:
            return 429, {"error": "locked", "retryInS": int(math.ceil(self.lock_left()))}
        if not header:
            return 401, {"error": "auth"}
        user, pw = self.parse_basic(header)
        if user == "admin" and pw == self.password and pw:
            if lan:
                self.guard_success(t)
            self.last_bad_pw = None
            return None
        if lan:
            given = pw if pw is not None else ""
            if given != self.last_bad_pw:
                self.guard_failure(t)
                self.last_bad_pw = given
            self.last_bad_t = t
        return 401, {"error": "auth"}

    # --- /api/status ---
    def status(self):
        now = self.now()
        po = self.power.out
        pf = self.park_fresh(now)
        p = self.park
        beeping = pf and p["sound"]
        testing = self.test_until is not None and now < self.test_until
        fresh0f6 = self.body_ms is not None and now - self.body_ms < 1000
        alive = self.last_bus_ms is not None and now - self.last_bus_ms <= 1000
        have_temp = self.temp_ms is not None and now - self.temp_ms <= 1000
        sta = self.wifi_sta is not None
        r = self.running
        f260 = self.set_frames.get(0x260)
        settings = {"raw260": f260.hex().upper() if f260 is not None else None,
                    "ageMs": max(0, now - self.set260_ms) if f260 is not None else None,
                    "state38": bytes(self.set38).hex().upper(),
                    "last": None, "accepted": 0, "notAccepted": 0, "refused": 0,
                    "mask": self.vset_mask}        # mask of the enabled items (since 1.0.0)
        tx_built = r.get("canTx", 0) != 0
        return {
            "sys": {"version": r["version"], "build": r["build"], "elf": r["elf"], "mode": r["mode"],
                    "canTx": r.get("canTx", 0) != 0,
                    "boot": self.boot, "uptimeMs": now, "resetReason": "poweron",
                    "heapFree": 182344, "heapMin": 151208, "heapLargest": 110580,
                    "loopBusyPct": 7, "cpu0Pct": 12, "loopMaxUs": 1840, "loopsPerSec": 992,
                    "stackLoopFree": 5120, "stackHttpFree": 4368, "stackWifiFree": 2210, "tempC": 41.5,
                    "otaState": self.ota_state, "otaBusy": False, "restartPending": self.restart_at is not None,
                    "hubDrops": 0,
                    # client in the device's own network (since 1.0.0): only such a client arms transmission from the panel
                    "clientAp": not self.args.lan},
            "can": {"ok": True, "rx": self.rx, "missed": 0, "overrun": 0, "busErrors": 0,
                    "framesPerSec": self.fps, "busAlive": alive, "listenOnly": not self.tx_armed,
                    # CAN transmit gate (since 0.9.0). Without --can-tx: a program without transmit hardware, all zeros.
                    # The mock transmits nothing: counters are always 0, only the arming state changes.
                    "tx": {"built": tx_built, "armed": self.tx_armed, "armNext": self.tx_tag is not None,
                           "state": "ready" if self.tx_armed else ("disarmed" if tx_built else "none"), "fault": None,
                           "sent": 0, "acked": 0, "failed": 0, "refused": 0, "tec": 0, "rec": 0}},
            "car": {"ignition": po["ignition"], "ignitionRaw": self.body_raw if fresh0f6 else None, "acc": po["acc"],
                    "reverse": po["reverse"], "lights": po["lights"], "doors": self.doors if self.doors_known else None,
                    "batteryV": None, "outsideTempC": round(self.temp, 1) if have_temp else None,
                    "settings": settings},
            "park": {"fresh": pf, "rearStatus": p["rearStatus"], "frontStatus": p["frontStatus"],
                     "sound": p["sound"], "soundFront": p["soundFront"], "interval": p["interval"],
                     "rear": list(p["rear"]), "front": list(p["front"]),
                     "beeping": bool(beeping or testing), "continuous": bool(beeping and p["interval"] == 0)},
            "radio": {"ok": True, "txEnabled": r["mode"] == "decoder", "heard": False, "ageMs": None,
                      "on": False, "volume": 0, "muted": False, "source": [0] * 8, "time": None,
                      "rxFrames": 0, "rxAcks": 0, "rxBadChecksum": 0, "rxBadLength": 0, "rxTimeouts": 0,
                      "rxStray": 0, "txFrames": 0, "txAcks": 0, "txSuppressed": 0, "txQueue": 0,
                      "txQueueCap": 24, "txDropped": 0, "keysDropped": 0, "txGated": 0, "slotsFull": 0,
                      "settingCount": 0,
                      "lastSetting": [0, 0], "airCount": 0, "lastAir": [0, 0], "unknownCount": 0},
            "audio": {"ok": True, "volume": self.volume, "volumeMax": VOLUME_MAX},
            "wifi": {"on": True, "phase": "running", "apSsid": IDENT["PROJECT_AP_SSID_PREFIX"] + "MOCK", "apIp": "10.50.8.1",
                     "apChannel": 1, "apStations": 1, "staConfigured": sta, "staSsid": self.wifi_sta or "",
                     "staConnected": False, "staIp": None, "staRssi": None, "staLastReason": self.sta_reason,
                     "hostname": IDENT["PROJECT_MDNS_HOST"], "maxStepMs": 180, "startFails": 0, "gaveUp": False,
                     "blocked": False, "lanAdmin": self.lan_admin},
            "log": {"head": self.line_head, "oldest": self.line_head - len(self.lines)},
            "trace": {"head": self.trace_head, "oldest": self.trace_head - len(self.trace)},
        }

    def ota_status(self):
        o = self.other
        if o is None:
            other = {"slot": "app1" if self.running["slot"] == "app0" else "app0", "image": False, "scanned": True,
                     "known": False, "bootable": False, "version": None, "build": None, "mode": None, "canTx": None,
                     "net": None, "elf": None}
        else:
            other = {"slot": o["slot"], "image": True, "scanned": True, "known": True, "bootable": True,
                     "version": o["version"], "build": o["build"], "mode": o["mode"],
                     "canTx": o.get("canTx", 0) != 0, "net": o["net"], "elf": o.get("elf")}
        now = self.now()
        return {"state": self.ota_state, "slotSize": SLOT_SIZE,
                "confirmInMs": max(0, self.confirm_at - now) if (self.ota_state == "pending" and self.confirm_at) else 0,
                "busy": False, "carBusy": self.car_busy(now), "restartPending": self.restart_at is not None,
                "restartReason": self.restart_reason, "canRollback": o is not None or self.ota_state == "pending",
                "running": dict(self.running), "other": other, "last": dict(self.last)}


# ---------- helpers ----------
def parse_map(text):
    """Key table "9F=9B;21=1F" -> list of pairs without X=X, or None when invalid (like KeyCustomizer::parseMap)."""
    parts = text.split(";")
    if parts and parts[-1] == "":
        parts.pop()
    if len(parts) > MAP_SIZE:
        return None
    new, seen = [], set()
    for prt in parts:
        if not re.fullmatch(r"[0-9A-Fa-f]{2}=[0-9A-Fa-f]{2}", prt):
            return None
        z, na = prt[:2].upper(), prt[3:].upper()
        if z in seen:
            return None
        seen.add(z)
        if z != na:
            new.append((z, na))
    return new


def parse_backup(text):
    """Settings backup text (lib/core/settings_backup.h) -> (dict of the given fields, None) or (None, error word).
    Simplified: strict JSON object of the known fields with the same types and ranges."""
    try:
        obj = json.loads(text, object_pairs_hook=lambda pairs: pairs)
    except ValueError:
        return None, "syntax"
    if not isinstance(obj, list):
        return None, "syntax"
    out = {}
    for k, v in obj:
        if k not in ("v", "lang", "vol", "keys", "net", "lan"):
            return None, "unknown_key"
        if k in out:
            return None, "duplicate"
        if k == "v":
            if type(v) is not int:
                return None, "bad_value"
            if v != 1:
                return None, "version"
        elif k == "lang":
            if v is not None and not (isinstance(v, str) and re.fullmatch(r"[A-Za-z][A-Za-z0-9-]{0,6}[A-Za-z0-9]", v)):
                return None, "bad_value"
        elif k == "vol":
            if v is not None and not (type(v) is int and 0 <= v <= 100):
                return None, "bad_value"
        elif k == "keys":
            if v is not None and not (isinstance(v, str) and len(v) <= 95 and re.fullmatch(r"[\x20-\x7e]*", v)
                                      and "\\" not in v):
                return None, "bad_value"
        elif type(v) is not bool:
            return None, "bad_value"
        out[k] = v
    out.pop("v", None)
    return out, None


def hex_code(text):
    """Like strtoul(..., 16) in main.cpp for a short code: the leading hex digits, the rest is ignored."""
    m = re.match(r"\s*([0-9A-Fa-f]+)", text)
    return int(m.group(1), 16) & 0xFF if m else 0


def cursor_slice(head, ring_len, c):
    """Returns (start, dropped) according to contract 1.5."""
    oldest = head - ring_len
    if c is None:
        return oldest, 0
    if c > head:
        c = head
    dropped = max(0, oldest - c)
    return max(c, oldest), dropped


def parse_query(q):
    """Like httpQueryValue in the device: no %XX decoding; a query longer than 127 B = no parameters."""
    out = {}
    if not q or len(q.encode("latin-1", "replace")) > QUERY_MAX:
        return out
    for part in q.split("&"):
        k, _, v = part.partition("=")
        if k and k not in out:
            out[k] = v
    return out


def u32_param(v):
    if v is None:
        return None, True
    if not re.fullmatch(r"\d{1,10}", v) or int(v) > 0xFFFFFFFF:
        return None, False
    return int(v), True


def find_ident(buf):
    i = buf.find(MAGIC)
    if i < 0 or i + 72 > len(buf):
        return None
    def cs(off, n):   # noqa: E306
        raw = buf[i + off:i + off + n].split(b"\x00", 1)[0]
        return "".join(chr(c) if 32 <= c < 127 else "?" for c in raw)
    return {"product": cs(12, 16), "version": cs(28, 16), "build": cs(44, 24),
            "mode": "decoder" if buf[i + 68] == 1 else "listen", "net": buf[i + 69] == 1,
            "canTx": buf[i + 70]}      # raw byte; up to 0.6.0 this place was a reserved byte = 0


# ---------- HTTP server ----------
class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = IDENT["PROJECT_SLUG"] + "-mock"
    sys_version = ""
    dev = None   # Device, set in main()

    def log_message(self, fmt, *a):
        if self.dev.args.verbose:
            sys.stderr.write("%s %s\n" % (self.address_string(), fmt % a))

    # --- responses ---
    def send_json(self, code, obj, close=False):
        body = json.dumps(obj, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Length", str(len(body)))
        if close:
            self.send_header("Connection", "close")
            self.close_connection = True
        self.end_headers()
        self.wfile.write(body)

    def send_err(self, code, word, detail=None, close=False, **extra):
        o = {"error": word}
        if detail is not None:
            o["detail"] = detail
        o.update(extra)
        self.send_json(code, o, close)

    def send_text(self, code, text):
        body = text.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    # --- request body ---
    def content_length(self):
        v = self.headers.get("Content-Length")
        if v is None or not v.strip().isdigit():
            return None
        return int(v.strip())

    def discard_body(self, n):
        if n is None:
            return
        if n > 65536:
            self.close_connection = True
            return
        while n > 0:
            chunk = self.rfile.read(min(n, 4096))
            if not chunk:
                break
            n -= len(chunk)

    def read_form(self, required=True):
        """Returns (dict, None) or (None, error_sent)."""
        n = self.content_length()
        if n is None or n == 0:
            if not required:
                return {}, None
            self.send_err(411, "length_required")
            return None, True
        if n > FORM_MAX:
            self.discard_body(n)
            self.send_err(413, "too_large", close=self.close_connection)
            return None, True
        raw = self.rfile.read(n)
        form = {}
        for k, v in parse_qsl(raw.decode("latin-1"), keep_blank_values=True, encoding="latin-1"):
            # parse_qsl decodes %XX as latin-1 (encoding="latin-1"; the default utf-8 made the next line fail on
            # non-ASCII characters and the mock dropped the connection): back to bytes, read them as UTF-8
            v = v.encode("latin-1").decode("utf-8", "replace")
            if k not in form:
                form[k] = v
        return form, None

    # --- common checks ---
    def reject_body(self, n, close):
        # Rejected body: a program file is not read (the connection is closed), a form is
        # read and discarded, so that the connection can be used for the next request.
        if close:
            self.close_connection = True
        else:
            self.discard_body(n)

    def auth(self, n=None, close=False):
        res = self.dev.check_auth(self.headers.get("Authorization"))
        if res is None:
            return True
        self.reject_body(n, close)
        code, obj = res
        self.send_json(code, obj, close=self.close_connection)
        return False

    def ap_only(self, n=None, close=False):
        if self.dev.args.lan and not self.dev.lan_admin:
            self.reject_body(n, close)
            self.send_err(403, "ap_only", close=self.close_connection)
            return True
        return False

    def pre(self):
        """Server limits (contract 1.4). Returns (path, query) or None."""
        if len(self.path.encode("latin-1", "replace")) > URI_MAX:
            self.send_text(414, "URI Too Long")
            self.close_connection = True
            return None
        hdr_len = sum(len(k) + len(v) + 4 for k, v in self.headers.items())
        if hdr_len > HEADERS_MAX:
            self.send_text(431, "Request Header Fields Too Large")
            self.close_connection = True
            return None
        if self.headers.get("Transfer-Encoding"):
            self.send_err(411, "length_required", close=True)
            return None
        path, _, q = self.path.partition("?")
        return path, parse_query(q)

    # --- GET ---
    def do_GET(self):
        p = self.pre()
        if p is None:
            return
        path, q = p
        dev = self.dev
        if path == "/":
            return self.page()
        if path.startswith("/i18n/"):
            return self.i18n(path)
        if path == "/favicon.ico":
            self.send_response(204)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if path == "/api/hello":
            with dev.lock:
                locked = int(math.ceil(dev.lock_left())) if dev.args.lan else 0
                return self.send_json(200, {"product": PRODUCT, "api": API_VERSION, "boot": dev.boot, "locked": locked,
                                            "lang": dev.ui_lang})
        routes = {"/api/status": self.g_status, "/api/log": self.g_log, "/api/frames": self.g_frames,
                  "/api/table": self.g_table, "/api/keymap": self.g_keymap, "/api/trace.log": self.g_trace,
                  "/api/ota/status": self.g_ota, "/api/settings": self.g_settings}
        fn = routes.get(path)
        if fn is None:
            if path in POST_ROUTES:
                return self.send_text(405, "Method Not Allowed")
            return self.send_text(404, "Not Found")
        with dev.lock:
            if not self.auth():
                return
            fn(q)

    def page(self):
        try:
            html, csp = embed_web.build_page(os.path.join(PROJECT_DIR, "web"))
        except (embed_web.WebError, OSError, UnicodeDecodeError) as e:
            return self.send_text(500, "embed_web: %s" % e)
        if html is None:
            html, csp = embed_web.fallback_page(PROJECT_DIR)
        etag = '"%s"' % hashlib.sha256(gzip.compress(html, 9, mtime=0)).hexdigest()[:16]
        common = [("Cache-Control", "no-cache"), ("ETag", etag), ("X-Frame-Options", "DENY"),
                  ("X-Content-Type-Options", "nosniff"), ("Referrer-Policy", "no-referrer"),
                  ("Content-Security-Policy", csp)]
        if self.headers.get("If-None-Match") == etag:
            self.send_response(304)
            for k, v in common:
                self.send_header(k, v)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        for k, v in common:
            self.send_header(k, v)
        self.send_header("Content-Length", str(len(html)))
        self.end_headers()
        self.wfile.write(html)

    def i18n(self, path):
        """GET /i18n/<lang>.json: a separate panel dictionary, gzip, as the firmware serves it (WEB_I18N)."""
        try:
            assets = embed_web.i18n_assets(os.path.join(PROJECT_DIR, "web"))
        except (embed_web.WebError, OSError, UnicodeDecodeError) as e:
            return self.send_text(500, "embed_web: %s" % e)
        body = next((b for _, uri, b in assets if uri == path), None)
        if body is None:
            return self.send_text(404, "Not Found")
        gzb = embed_web.gz(body)
        etag = '"%s"' % hashlib.sha256(gzb).hexdigest()[:16]
        if self.headers.get("If-None-Match") == etag:
            self.send_response(304)
            self.send_header("Cache-Control", "no-cache")
            self.send_header("ETag", etag)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Encoding", "gzip")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("ETag", etag)
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Length", str(len(gzb)))
        self.end_headers()
        self.wfile.write(gzb)

    def g_status(self, q):
        self.send_json(200, self.dev.status())

    def g_log(self, q):
        dev = self.dev
        c, ok = u32_param(q.get("c"))
        if not ok:
            return self.send_err(400, "bad_request", "c")
        start, dropped = cursor_slice(dev.line_head, len(dev.lines), c)
        oldest = dev.line_head - len(dev.lines)
        out, used, i = [], 0, start
        while i < dev.line_head and len(out) < LOG_RESP_LINES:
            _, ms, text = dev.lines[i - oldest]
            n = len(text.encode("utf-8"))
            if out and used + n > LOG_RESP_BYTES:
                break
            out.append([ms, text])
            used += n
            i += 1
        self.send_json(200, {"boot": dev.boot, "next": i, "head": dev.line_head, "dropped": dropped, "lines": out})

    def g_frames(self, q):
        dev = self.dev
        c, ok = u32_param(q.get("c"))
        if not ok:
            return self.send_err(400, "bad_request", "c")
        fid = q.get("id")
        if fid is not None:
            if not re.fullmatch(r"[0-9A-Fa-f]{1,3}", fid) or int(fid, 16) > 0x7FF:
                return self.send_err(400, "bad_request", "id")
            fid = "%03X" % int(fid, 16)
        mx = q.get("max")
        if mx is None:
            mx = FRAMES_DEFAULT
        else:
            if not re.fullmatch(r"\d{1,10}", mx) or int(mx) == 0:
                return self.send_err(400, "bad_request", "max")
            mx = min(int(mx), FRAMES_RESP_MAX)
        start, dropped = cursor_slice(dev.trace_head, len(dev.trace), c)
        oldest = dev.trace_head - len(dev.trace)
        out, i = [], start
        while i < dev.trace_head and len(out) < mx:
            _, ms, ids, data = dev.trace[i - oldest]
            if fid is None or ids == fid:
                out.append([ms, ids, data])
            i += 1
        self.send_json(200, {"boot": dev.boot, "next": i, "head": dev.trace_head, "dropped": dropped, "frames": out})

    def g_table(self, q):
        dev = self.dev
        now = dev.now()
        ids = [[k, v[0], v[1], max(0, now - v[2]), v[3]] for k, v in sorted(dev.table.items())]
        self.send_json(200, {"nowMs": now, "overflow": dev.table_overflow, "ids": ids})

    def g_keymap(self, q):
        dev = self.dev
        self.send_json(200, {"max": MAP_SIZE, "map": [list(p) for p in dev.keymap],
                             "defaults": [list(p) for p in DEFAULT_MAP], "lastKey": None})

    def g_settings(self, q):
        self.send_json(200, self.dev.export())

    def g_ota(self, q):
        self.send_json(200, self.dev.ota_status())

    def g_trace(self, q):
        dev = self.dev
        mask = q.get("mask") != "0"
        lines = ["# %s %s %s; time in seconds since the device started" % (PRODUCT, dev.running["version"], dev.running["mode"])]
        for _, ms, ids, data in list(dev.trace):
            if mask and int(ids, 16) in VIN_IDS:
                lines.append("# masked")
            else:
                lines.append("(%d.%03d) can0 %s#%s" % (ms // 1000, ms % 1000, ids, data))
        body = ("\n".join(lines) + "\n").encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Disposition", 'attachment; filename="%s_can.log"' % IDENT["PROJECT_SLUG"])
        self.send_header("Cache-Control", "no-store")
        self.send_header("Transfer-Encoding", "chunked")
        self.end_headers()
        for i in range(0, len(body), 4096):
            part = body[i:i + 4096]
            self.wfile.write(b"%X\r\n" % len(part) + part + b"\r\n")
        self.wfile.write(b"0\r\n\r\n")

    # --- POST ---
    def do_POST(self):
        p = self.pre()
        if p is None:
            return
        path, q = p
        fn = POST_ROUTES.get(path)
        if fn is None:
            n = self.content_length()
            self.discard_body(n)
            if path in ("/", "/favicon.ico", "/api/hello", "/api/status", "/api/log", "/api/frames",
                        "/api/table", "/api/trace.log", "/api/ota/status") or path in I18N_URIS:
                return self.send_text(405, "Method Not Allowed")
            return self.send_text(404, "Not Found")
        with self.dev.lock:
            fn(self, q)

    def p_cmd(self, q):
        dev = self.dev
        n = self.content_length()
        if not self.auth(n):
            return
        if self.ap_only(n):
            return
        form, err = self.read_form()
        if err:
            return
        line = form.get("line")
        if line is None or not re.fullmatch(r"[\x20-\x7e]{1,127}", line):
            return self.send_err(400, "bad_request", "line")
        # as the device (api_handlers.cpp, startsWithWifi): leading spaces skipped, "wifi" in any letter case,
        # followed by the end of the line or a space
        if re.match(r" *wifi(?: |$)", line, re.IGNORECASE):
            return self.send_err(400, "bad_request", "usb_only")
        # CAN transmission (since 1.0.0): "cantx reset" and "set NN VV" only from USB; "cantx on|off" and
        # "vset on|off NN" (old name "ust" accepted too) from the panel only from a client in the device's own network (also with "wifi lan on").
        if re.fullmatch(r"cantx +reset|set +[0-9A-Fa-f]{1,2} +[0-9A-Fa-f]{1,2}", line):
            return self.send_err(400, "bad_request", "usb_only")
        if re.fullmatch(r"cantx +(on|off)|(?:vset|ust) +(on|off) +[0-9A-Fa-f]{1,2}", line) and dev.args.lan:
            return self.send_err(403, "ap_only")
        if len(dev.queue) >= CMD_QUEUE_MAX:
            return self.send_err(503, "busy")
        dev.queue.append(lambda: dev.run_cmd(line))
        self.send_json(200, {"queued": True})

    def p_keymap(self, q):
        dev = self.dev
        n = self.content_length()
        if not self.auth(n):
            return
        if self.ap_only(n):
            return
        form, err = self.read_form()
        if err:
            return
        if form.get("reset") == "1":
            new = list(DEFAULT_MAP)
        elif "map" in form:
            text = form["map"]
            parts = text.split(";")
            if parts and parts[-1] == "":
                parts.pop()
            if len(parts) > MAP_SIZE:
                return self.send_err(400, "bad_request", "map")
            new, seen = [], set()
            for prt in parts:
                if not re.fullmatch(r"[0-9A-Fa-f]{2}=[0-9A-Fa-f]{2}", prt):
                    return self.send_err(400, "bad_request", "map")
                z, na = prt[:2].upper(), prt[3:].upper()
                if z in seen:
                    return self.send_err(400, "bad_request", "map")
                seen.add(z)
                if z != na:
                    new.append((z, na))
        else:
            return self.send_err(400, "bad_request", "map")
        if len(dev.queue) >= CMD_QUEUE_MAX:
            return self.send_err(503, "busy")

        saved = form.get("reset") != "1"

        def apply():
            dev.keymap = new
            dev.keymap_saved = saved
            dev.log("[key] swap table stored (%d)" % len(new))
        dev.queue.append(apply)
        self.send_json(200, {"queued": True})

    def p_lang(self, q):
        dev = self.dev
        n = self.content_length()
        if not self.auth(n):
            return
        if self.ap_only(n):
            return
        form, err = self.read_form()
        if err:
            return
        code = form.get("lang")
        if code is None or (code != "" and code not in UI_LANGS):
            return self.send_err(400, "bad_request", "lang")
        if len(dev.queue) >= CMD_QUEUE_MAX:
            return self.send_err(503, "busy")

        def apply():
            dev.ui_lang = code or None
            dev.log("[lang] from the panel: panel language=%s" % (code or "en (default)"))
        dev.queue.append(apply)
        self.send_json(200, {"queued": True})

    def p_settings(self, q):
        dev = self.dev
        n = self.content_length()
        if not self.auth(n):
            return
        if self.ap_only(n):
            return
        form, err = self.read_form()
        if err:
            return
        text = form.get("json")
        if text is None or len(text.encode("utf-8")) > 255:
            return self.send_err(400, "bad_request", "json")
        vals, bad = parse_backup(text)
        if bad is not None:
            return self.send_err(400, "bad_request", bad)
        bad = dev.import_problem(vals)
        if bad is not None:
            return self.send_err(400, "bad_request", bad.split(" ")[1])
        # The panel never imports net / lan (USB only); the firmware drops them before the queue.
        vals.pop("net", None)
        vals.pop("lan", None)
        if len(dev.queue) >= CMD_QUEUE_MAX:
            return self.send_err(503, "busy")
        dev.queue.append(lambda: dev.apply_import(vals))
        self.send_json(200, {"queued": True})

    def p_wifi(self, q):
        dev = self.dev
        n = self.content_length()
        if not self.auth(n):
            return
        if self.ap_only(n):
            return
        form, err = self.read_form()
        if err:
            return
        if "ssid" not in form:
            return self.send_err(400, "bad_request", "ssid")
        ssid = form["ssid"]
        if ssid != "":
            if len(ssid.encode("utf-8")) > 32 or re.search(r"[\x00-\x1f\x7f]", ssid) or "\ufffd" in ssid:
                return self.send_err(400, "bad_request", "ssid")
            pw = form.get("pass")
            if pw is None or not re.fullmatch(r"[\x20-\x7e]{8,64}", pw):
                return self.send_err(400, "bad_request", "pass")
            # like settings.cpp: 64 characters are a WPA2 key written in hex
            if len(pw) == 64 and not re.fullmatch(r"[0-9A-Fa-f]{64}", pw):
                return self.send_err(400, "bad_request", "pass")
        if len(dev.queue) >= CMD_QUEUE_MAX:
            return self.send_err(503, "busy")

        def apply():
            dev.wifi_sta = ssid or None
            dev.sta_reason = 0
            dev.log("[wifi] home network %s (mock)" % ("stored" if ssid else "forgotten"))
            if ssid:
                # the mock connects to no network: after a while "network not found"
                dev.timers.append([dev.now() + APPLY_MS + 3000, lambda: setattr(dev, "sta_reason", 201)])
        dev.queue.append(apply)
        self.send_json(200, {"queued": True, "applyInMs": APPLY_MS})

    def p_password(self, q):
        dev = self.dev
        n = self.content_length()
        if not self.auth(n):
            return
        if self.ap_only(n):
            return
        form, err = self.read_form()
        if err:
            return
        if "old" not in form:
            return self.send_err(400, "bad_request", "old")
        if "new" not in form:
            return self.send_err(400, "bad_request", "new")
        new = form["new"]
        if not re.fullmatch(r"[\x21-\x7e]{10,63}", new):
            return self.send_err(400, "weak_password")
        if form["old"] != dev.password:
            return self.send_err(403, "wrong_password")
        if len(dev.queue) >= CMD_QUEUE_MAX:
            return self.send_err(503, "busy")

        def apply():
            dev.password = new
            dev.log("[wifi] password changed (mock)")
        dev.queue.append(apply)
        self.send_json(200, {"queued": True, "applyInMs": APPLY_MS})

    def p_restart(self, q):
        dev = self.dev
        n = self.content_length()
        if not self.auth(n):
            return
        if self.ap_only(n):
            return
        self.discard_body(n)
        if dev.ota_state == "pending":
            return self.send_err(409, "ota_pending")
        if dev.restart_at is not None:
            return self.send_err(409, "restart_pending")
        if dev.car_busy(dev.now()):
            return self.send_err(409, "car_busy")
        dev.schedule_restart("user")
        dev.log("[www] restart in %d ms" % RESTART_MS)
        self.send_json(200, {"queued": True, "restartInMs": RESTART_MS})

    def p_ota(self, q):
        dev = self.dev
        n = self.content_length()
        if not self.auth(n, close=True):
            return
        if self.ap_only(n, close=True):
            return
        if n is None or n == 0:
            return self.send_err(411, "length_required", close=True)
        if n > SLOT_SIZE:
            return self.send_err(413, "too_large", close=True)
        # The file is read without the state lock: the replay goes on (in the device, the handling of other
        # requests stops meanwhile, but the main loop keeps running).
        dev.lock.release()
        try:
            buf = b""
            while len(buf) < n:
                chunk = self.rfile.read(min(65536, n - len(buf)))
                if not chunk:
                    break
                buf += chunk
        finally:
            dev.lock.acquire()
        if len(buf) < n:
            self.close_connection = True
            return
        force = q.get("force") == "1"
        mode_ok = q.get("mode") == "1"
        dev.last = {"result": "error", "error": "", "bytes": len(buf)}

        def fail(code, word, detail=None):
            dev.last["error"] = word
            self.send_err(code, word, detail)

        if len(buf) < MIN_IMAGE:
            return fail(400, "bad_image", "short")
        if dev.ota_state == "pending":
            return fail(409, "ota_pending")
        if dev.restart_at is not None:
            return fail(409, "restart_pending")
        if buf[0] != 0xE9 or buf[12:14] != b"\x09\x00" or buf[32:36] != b"\x32\x54\xcd\xab":
            return fail(400, "bad_image", "header")
        ident = find_ident(buf)
        if ident is None or ident["product"] != PRODUCT:
            return fail(400, "wrong_product")
        if not ident["net"] and not force:
            return fail(409, "no_net")
        # change of the operating mode or of the CAN transmit capability (canTx): one consent, mode=1
        mode_change = ident["mode"] != dev.running["mode"] or ident["canTx"] != dev.running.get("canTx", 0)
        if mode_change and not mode_ok:
            return fail(409, "mode_change")
        # a mode / canTx change only from the device's own network, like "cantx on" (also with "wifi lan on")
        if mode_change and dev.args.lan:
            return fail(403, "ap_only")
        elf = buf[0xB0:0xB8].hex().upper()
        dev.last = {"result": "ok", "error": "", "bytes": len(buf)}
        new = {"slot": "app1" if dev.running["slot"] == "app0" else "app0", "version": ident["version"],
               "build": ident["build"], "mode": ident["mode"], "net": True, "elf": elf, "canTx": ident["canTx"]}

        def switch():
            dev.other = dict(dev.running)
            dev.running = new
            # a program accepted with mode=1 is confirmed at once (contract, chapter 6)
            dev.ota_state = "valid" if mode_change else "pending"
        dev.schedule_restart("ota", switch)
        dev.log("[ota] accepted %d B, version %s, restart in %d ms" % (len(buf), ident["version"], RESTART_MS))
        self.send_json(200, {"ok": True, "bytes": len(buf), "version": ident["version"], "build": ident["build"],
                             "mode": ident["mode"], "canTx": ident["canTx"] != 0, "net": ident["net"],
                             "elf": elf, "restartInMs": RESTART_MS})

    def p_rollback(self, q):
        dev = self.dev
        n = self.content_length()
        if not self.auth(n):
            return
        if self.ap_only(n):
            return
        self.discard_body(n)
        force = q.get("force") == "1"
        mode_ok = q.get("mode") == "1"
        o = dev.other
        if dev.restart_at is not None:
            return self.send_err(409, "restart_pending")
        if o is None:
            return self.send_err(409, "no_rollback")
        if dev.ota_state != "pending" and not o["net"] and not force:
            return self.send_err(409, "no_net")
        if (o["mode"] != dev.running["mode"] or o.get("canTx", 0) != dev.running.get("canTx", 0)) and not mode_ok:
            return self.send_err(409, "mode_change")
        if (o["mode"] != dev.running["mode"] or o.get("canTx", 0) != dev.running.get("canTx", 0)) and dev.args.lan:
            return self.send_err(403, "ap_only")
        if dev.car_busy(dev.now()):
            return self.send_err(409, "car_busy")

        def switch():
            old = dev.running
            dev.running = o
            dev.other = old
            dev.ota_state = "valid"
        dev.schedule_restart("rollback", switch)
        dev.log("[ota] rollback, restart in %d ms" % RESTART_MS)
        self.send_json(200, {"queued": True, "restartInMs": RESTART_MS})


POST_ROUTES = {"/api/cmd": Handler.p_cmd, "/api/keymap": Handler.p_keymap, "/api/lang": Handler.p_lang,
               "/api/settings": Handler.p_settings, "/api/wifi": Handler.p_wifi,
               "/api/password": Handler.p_password, "/api/restart": Handler.p_restart, "/api/ota": Handler.p_ota,
               "/api/ota/rollback": Handler.p_rollback}


def ticker(dev):
    while True:
        with dev.lock:
            dev.tick()
        time.sleep(0.01)


def main():
    ap = argparse.ArgumentParser(description="Device mock (web panel without the board)")
    ap.add_argument("log", help="CAN capture in the sniffer format: (seconds) can0 ID#DATA")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--password", default=None, help="device password (10..63 characters); random by default")
    ap.add_argument("--lan", action="store_true", help="client from the home network: login lockout, 403 ap_only")
    ap.add_argument("--lan-admin", action="store_true", help="as after 'wifi lan on': changes allowed from the home network")
    ap.add_argument("--mode", choices=("listen", "decoder"), default="listen")
    ap.add_argument("--can-tx", action="store_true",
                    help="the running program can send frame 15B (canTx = 1, like decoder_tx_wifi); "
                         "the mock transmits nothing, it only shows the arming switch")
    ap.add_argument("--speed", type=float, default=1.0, help="capture replay speed (1 = real time)")
    ap.add_argument("--no-loop", action="store_true", help="do not loop the capture")
    ap.add_argument("--verbose", action="store_true", help="print HTTP requests")
    args = ap.parse_args()
    if args.speed <= 0:
        ap.error("--speed must be positive")
    if args.password is None:
        alphabet = string.ascii_letters + string.digits
        args.password = "".join(secrets.choice(alphabet) for _ in range(12))
    elif not re.fullmatch(r"[\x21-\x7e]{10,63}", args.password):
        ap.error("password: 10..63 ASCII characters without spaces")
    frames = load_log(args.log)
    if not frames:
        print("Note: the file has no frames in the format '(seconds) can0 ID#DATA'; the mock runs without CAN traffic.")
    dev = Device(args, frames)
    Handler.dev = dev
    threading.Thread(target=ticker, args=(dev,), daemon=True).start()
    # One thread per connection: the browser keeps several connections open (keep-alive), and a plain
    # HTTPServer serves one connection until it is closed, so the panel would stall.
    # The requests themselves are still processed one at a time (lock dev.lock), as in the device.
    ThreadingHTTPServer.daemon_threads = True
    srv = ThreadingHTTPServer((args.host, args.port), Handler)
    print("Device mock: http://%s:%d/  (frames in the capture: %d, VIN masked)" % (args.host, args.port, len(frames)))
    print("mock password: %s" % args.password)
    print("client: %s" % ("home network" + (" (wifi lan on)" if args.lan_admin else "") if args.lan else "device network"))
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
