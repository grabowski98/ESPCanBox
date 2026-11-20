# Recording CAN frames and the device console to files, without gaps and independently of the browser.
# Run on a PC connected to the Wi-Fi network of the device:
#     python tools/record.py [address]          (default 10.50.8.1)
#
# What it does: polls /api/frames and /api/log (docs/PANEL_API.md, chapter 4) and appends to two files
# in the directory ../../../logs relative to tools/ (outside the repository):
#     <slug>_can_YYYYMMDD_HHMMSS.log      frames in the sniffer capture format: (seconds) can0 ID#DATA
#     <slug>_console_YYYYMMDD_HHMMSS.txt  console lines, e.g. "[radio_rx] ..."
# Marker: type a text and press Enter (e.g. "door lock on"). It goes to both files as a line "# MARKER".
# End: q + Enter or Ctrl+C.
#
# Read only: sends no commands to the device and changes nothing in it.
# The password is asked without echo (getpass), kept only in memory, never printed or saved.
# Frames with the VIN number (336, 3B6, 2B6) are written as "# masked".
# Python standard library, no extra packages. Screen messages without Polish characters
# (the Windows console may use a different code page).
import base64
import getpass
import http.client
import json
import os
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # python -I does not add the script directory
sys.dont_write_bytecode = True
import project_identity  # noqa: E402  (product name from include/project_identity.h)

IDENT = project_identity.load(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PRODUCT = IDENT["PROJECT_FW_PRODUCT"]
SLUG = IDENT["PROJECT_SLUG"]    # prefix of the file names (<slug> above)
if len(sys.argv) > 1 and sys.argv[1] in ("-h", "--help"):
    print("usage: python -I tools/record.py [address[:port]]   (default 10.50.8.1)")
    print("Records CAN frames and console lines from the device over Wi-Fi into the logs folder")
    print("next to the repository. Type a text + Enter = marker; q + Enter or Ctrl+C = end.")
    sys.exit(0)
ADDR = sys.argv[1] if len(sys.argv) > 1 else "10.50.8.1"
HOST, _, PORT = ADDR.partition(":")
PORT = int(PORT) if PORT else 80
VIN_IDS = ("336", "3B6", "2B6")
LOGS = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "logs"))


class AuthError(Exception):
    pass


class Recorder:
    def __init__(self, password):
        self.auth = "Basic " + base64.b64encode(("admin:" + password).encode()).decode()
        self.conn = None
        self.stop = threading.Event()
        self.lock = threading.Lock()          # files and counters: recording thread + markers from the keyboard
        self.frames = self.lines = self.dropped = self.markers = 0
        self.dev_ms = 0                       # last device time seen in a frame or a line
        self.fatal = None
        self.connected = False
        stamp = time.strftime("%Y%m%d_%H%M%S")
        os.makedirs(LOGS, exist_ok=True)
        self.can_path = os.path.join(LOGS, "%s_can_%s.log" % (SLUG, stamp))
        self.con_path = os.path.join(LOGS, "%s_console_%s.txt" % (SLUG, stamp))
        self.can = open(self.can_path, "w", encoding="utf-8", newline="\n")
        self.con = open(self.con_path, "w", encoding="utf-8", newline="\n")
        head = "# %s, recorded by tools/record.py; time in seconds since the device started; start %s\n" \
               % (PRODUCT, time.strftime("%Y-%m-%d %H:%M:%S"))
        self.can.write(head)
        self.con.write(head)

    # --- one GET request; the connection is kept open and renewed after an error ---
    def get(self, path):
        if self.conn is None:
            self.conn = http.client.HTTPConnection(HOST, PORT, timeout=5)
        try:
            self.conn.request("GET", path, headers={"Authorization": self.auth})
            r = self.conn.getresponse()
            data = r.read()
        except Exception:
            self.close_conn()
            raise
        if r.status == 401:
            raise AuthError()
        if r.status == 429:                   # login lockout: wait as long as the device says
            try:
                wait = int(json.loads(data.decode("utf-8")).get("retryInS", 5))
            except Exception:
                wait = 5
            self.stop.wait(min(max(wait, 1), 300))
            return None
        if r.status != 200:                   # 503 busy / not_ready and others: short pause, retry
            self.stop.wait(0.3)
            return None
        return json.loads(data.decode("utf-8"))

    def close_conn(self):
        try:
            if self.conn is not None:
                self.conn.close()
        except Exception:
            pass
        self.conn = None

    @staticmethod
    def sec(ms):
        return "(%d.%03d)" % (ms // 1000, ms % 1000)

    def both(self, text):
        self.can.write(text + "\n")
        self.con.write(text + "\n")

    def run(self):
        fc = lc = None                        # frame and line cursors; None = from the oldest one in memory
        boot = None
        next_log = 0.0
        while not self.stop.is_set():
            try:
                more = False
                j = self.get("/api/frames?max=400" + ("" if fc is None else "&c=%d" % fc))
                if j is not None:
                    if boot is not None and j["boot"] != boot:
                        # The device has restarted: the old cursors mean nothing, ask again from the beginning.
                        with self.lock:
                            self.both("# device restart")
                        print("\n[note] the device started again, still recording", flush=True)
                        boot, fc, lc = j["boot"], None, None
                        continue
                    boot = j["boot"]
                    with self.lock:
                        if j["dropped"]:
                            self.dropped += j["dropped"]
                            self.can.write("# lost %d frames\n" % j["dropped"])
                        for t, cid, data in j["frames"]:
                            if cid in VIN_IDS:
                                self.can.write("# masked\n")
                            else:
                                self.can.write("%s can0 %s#%s\n" % (self.sec(t), cid, data))
                            self.dev_ms = t
                        self.frames += len(j["frames"])
                        self.can.flush()
                    fc = j["next"]
                    more = j["next"] != j["head"]
                if time.time() >= next_log:
                    next_log = time.time() + 0.5
                    j = self.get("/api/log" + ("" if lc is None else "?c=%d" % lc))
                    if j is not None and j["boot"] == boot:
                        with self.lock:
                            if j["dropped"]:
                                self.con.write("# lost %d lines\n" % j["dropped"])
                            for t, text in j["lines"]:
                                self.con.write("%s %s\n" % (self.sec(t), text))
                                self.dev_ms = max(self.dev_ms, t)
                            self.lines += len(j["lines"])
                            self.con.flush()
                        lc = j["next"]
                if not self.connected:
                    self.connected = True
                    print("\n[ok] connected, recording", flush=True)
            except AuthError:
                self.fatal = "wrong password (401)"
                self.stop.set()
                print("\nERROR: wrong password. Press Enter to quit.", flush=True)
                return
            except (OSError, http.client.HTTPException, ValueError, KeyError) as e:
                if self.connected:
                    print("\n[note] no connection (%s), still trying" % type(e).__name__, flush=True)
                self.connected = False
                self.close_conn()
                self.stop.wait(1.0)
                continue
            if not more:
                self.stop.wait(0.2)           # the device memory holds about 9 s of frames, so 0.2 s is a large margin

    def marker(self, text):
        with self.lock:
            self.markers += 1
            self.both("# MARKER %d %s computer clock %s: %s"
                      % (self.markers, self.sec(self.dev_ms), time.strftime("%H:%M:%S"), text))
            self.can.flush()
            self.con.flush()
            return self.markers, self.frames, self.lines, self.dropped

    def finish(self):
        self.stop.set()
        self.close_conn()
        with self.lock:
            self.can.close()
            self.con.close()


def main():
    print("Recording the device %s:%d. Read only." % (HOST, PORT))
    if sys.stdin.isatty():
        pw = getpass.getpass("Device password (will not be shown): ")
    else:
        pw = sys.stdin.readline().rstrip("\r\n")   # run without a keyboard (tests against the mock device)
    if not pw:
        print("Empty password, quitting.")
        return 1
    rec = Recorder(pw)
    del pw
    th = threading.Thread(target=rec.run, daemon=True)
    th.start()
    print("Files:\n  %s\n  %s" % (rec.can_path, rec.con_path))
    print("Marker: type text and Enter. Quit: q + Enter.")
    try:
        while not rec.stop.is_set():
            line = sys.stdin.readline()
            if not line or rec.stop.is_set():  # end of input or an error reported by the recording thread
                break
            text = line.strip()
            if text.lower() == "q":
                break
            n, fr, ln, dr = rec.marker(text or "(no description)")
            print("  marker %d stored | frames %d, console lines %d, lost frames %d" % (n, fr, ln, dr), flush=True)
    except KeyboardInterrupt:
        pass
    rec.finish()
    th.join(timeout=6)
    if rec.fatal:
        print("ERROR: %s" % rec.fatal)
    print("Done. Frames %d, console lines %d, markers %d, lost frames %d."
          % (rec.frames, rec.lines, rec.markers, rec.dropped))
    print("Files:\n  %s\n  %s" % (rec.can_path, rec.con_path))
    return 1 if rec.fatal else 0


if __name__ == "__main__":
    sys.exit(main())
