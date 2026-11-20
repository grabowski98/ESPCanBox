# Tests of the device panel that need the device password. Run on a PC
# connected to the Wi-Fi network of the device:  python tools/test_panel_auth.py [address]
# The password is asked without echo (getpass), kept only in memory, never printed
# or saved. Result: OK / FAIL lines on the screen and in the file test_panel_auth_result.txt next to the script.
#
# What is NOT done: flashing a real program, rolling the program back, restart, saving a home
# network. The password is changed there and back within a few tens of ms, so the device
# network restarts with THE SAME password (Wi-Fi disappears for a few seconds).
# Python standard library, no extra packages.
import base64
import getpass
import http.client
import json
import os
import secrets
import string
import sys
import time

HOST = sys.argv[1] if len(sys.argv) > 1 else "10.50.8.1"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "test_panel_auth_result.txt")
results = []


def report(ok, name, detail=""):
    line = "%s  %s%s" % ("OK  " if ok else "FAIL", name, ("  [" + detail + "]") if detail else "")
    results.append(line)
    print(line, flush=True)


def req(method, path, body=None, auth=None, headers=None, timeout=10):
    h = dict(headers or {})
    if auth is not None:
        h["Authorization"] = "Basic " + base64.b64encode(("admin:" + auth).encode()).decode()
    if body is not None and isinstance(body, str):
        body = body.encode()
    c = http.client.HTTPConnection(HOST, 80, timeout=timeout)
    try:
        c.request(method, path, body=body, headers=h)
        r = c.getresponse()
        data = r.read()
        return r.status, data, dict(r.getheaders())
    finally:
        c.close()


def js(data):
    try:
        return json.loads(data.decode("utf-8"))
    except Exception:
        return None


def err(data):
    j = js(data)
    return (j or {}).get("error"), (j or {}).get("detail")


def form(**kw):
    from urllib.parse import urlencode
    return urlencode(kw)


def wait_log(pw, needle, since, secs=3.0):
    end = time.time() + secs
    while time.time() < end:
        s, d, _ = req("GET", "/api/log?c=%d" % since, auth=pw)
        j = js(d) or {}
        for ln in j.get("lines", []):
            # A log line is a pair [time ms, text] (PANEL_API 4).
            t = ln[1] if isinstance(ln, list) and len(ln) > 1 else None
            if isinstance(t, str) and needle in t:
                return True
        time.sleep(0.3)
    return False


def log_head(pw):
    s, d, _ = req("GET", "/api/status", auth=pw)
    j = js(d) or {}
    return ((j.get("log") or {}).get("head")) or 0


def main():
    pw = getpass.getpass("Device password (will not be shown): ")
    if not pw:
        print("No password.")
        return

    # --- login ---
    s, d, h = req("GET", "/api/status", auth=pw)
    report(s == 200 and js(d) is not None, "status with the password", "HTTP %d" % s)
    if s != 200:
        print("Without a valid login the other tests make no sense.")
        return
    st = js(d)
    report(h.get("Cache-Control") == "no-store" and "WWW-Authenticate" not in h, "API response headers")
    for k in ("sys", "can", "radio", "car", "park", "wifi", "log", "trace"):
        report(k in st, "status has the field %s" % k)
    sysj = st.get("sys") or {}
    report(isinstance(sysj.get("elf"), str) and len(sysj.get("elf", "")) == 16, "sys.elf 16 digits", str(sysj.get("elf")))

    for i in range(7):
        req("GET", "/api/status", auth=pw + "x%d" % i)
    s, d, _ = req("GET", "/api/status", auth=pw)
    report(s == 200, "7 wrong passwords from the device network do not lock out", "HTTP %d" % s)
    s, d, _ = req("GET", "/api/hello")
    report((js(d) or {}).get("locked") == 0, "hello: locked=0 for the device network")

    # --- live data ---
    for path in ("/api/log", "/api/log?c=4294967295", "/api/frames", "/api/frames?c=0&id=0E1", "/api/table",
                 "/api/keymap", "/api/ota/status"):
        s, d, _ = req("GET", path, auth=pw)
        report(s == 200 and js(d) is not None, "GET " + path, "HTTP %d" % s)
    s, d, h = req("GET", "/api/trace.log", auth=pw)
    report(s == 200 and "text" in h.get("Content-Type", ""), "GET /api/trace.log (text)", "HTTP %d" % s)
    # Cursor "from the future": numbers wrap after 2^32, so "past head" means head + 1..2^31.
    h0 = log_head(pw)
    s, d, _ = req("GET", "/api/log?c=%d" % ((h0 + 1000) & 0xFFFFFFFF), auth=pw)
    j = js(d) or {}
    report(j.get("next") == j.get("head") and j.get("lines") == [], "log: cursor past head = head (nothing)",
           "next=%s head=%s" % (j.get("next"), j.get("head")))

    # --- console ---
    head = log_head(pw)
    s, d, _ = req("POST", "/api/cmd", form(line="ver"), auth=pw)
    report(s == 200 and (js(d) or {}).get("queued") is True, "cmd ver accepted", "HTTP %d" % s)
    report(wait_log(pw, "# www> ver", head), "cmd ver visible in the log")
    report(wait_log(pw, "[ver]", head), "ver reply in the log")
    s, d, _ = req("POST", "/api/cmd", form(line="wifi showpass"), auth=pw)
    report(s == 400 and err(d) == ("bad_request", "usb_only"), "cmd wifi rejected (usb_only)", "HTTP %d %s" % (s, err(d)))
    s, d, _ = req("POST", "/api/cmd", form(line=""), auth=pw)
    report(s == 400, "cmd empty line rejected", "HTTP %d" % s)
    s, d, _ = req("POST", "/api/cmd", form(line="x" * 128), auth=pw)
    report(s == 400, "cmd 128 characters rejected", "HTTP %d" % s)
    s, d, _ = req("POST", "/api/cmd", form(line="za\u017c\u00f3\u0142\u0107"), auth=pw)
    report(s == 400, "cmd non-ASCII characters rejected", "HTTP %d" % s)
    s, d, _ = req("POST", "/api/cmd", "line=" + "a" * 2000, auth=pw)
    report(s == 413, "cmd body 2000 B = 413", "HTTP %d" % s)
    codes = [req("POST", "/api/cmd", form(line="ver"), auth=pw)[0] for _ in range(12)]
    report(all(c in (200, 503) for c in codes), "12 commands in a row: only 200 or 503 busy", str(codes))

    # --- key table ---
    for bad, name in (("9F=9", "bad syntax"), ("9F=9B;9F=1F", "duplicate"),
                      (";".join("%02X=B5" % (0x40 + i) for i in range(17)), "17 pairs"), ("9F = 9B", "space")):
        s, d, _ = req("POST", "/api/keymap", form(map=bad), auth=pw)
        report(s == 400, "keymap rejects: " + name, "HTTP %d %s" % (s, err(d)))
    s, d, _ = req("POST", "/api/keymap", form(map="9f=9b;21=1f;b1=b5;"), auth=pw)
    report(s == 200, "keymap: write of 3 pairs (lower case, semicolon)", "HTTP %d" % s)
    time.sleep(0.6)
    s, d, _ = req("GET", "/api/keymap", auth=pw)
    m = (js(d) or {}).get("map")
    report(m == [["9F", "9B"], ["21", "1F"], ["B1", "B5"]], "keymap: read after the write", str(m))
    s, d, _ = req("POST", "/api/keymap", "reset=1", auth=pw)
    time.sleep(0.6)
    s, d, _ = req("GET", "/api/keymap", auth=pw)
    j = js(d) or {}
    report(j.get("map") == j.get("defaults") == [["9F", "9B"], ["21", "1F"]], "keymap: reset to the built-in ones", str(j.get("map")))

    # --- home network: only invalid data (nothing is saved) ---
    for body, name in (("pass=12345678", "no ssid"), ("ssid=Test", "no pass"),
                       (form(ssid="Test") + "&pass=1234567", "password of 7 characters"),
                       (form(ssid="Test") + "&pass=" + "z" * 64, "64 non-hex characters"),
                       (form(ssid="S" * 33) + "&pass=12345678", "name of 33 bytes")):
        s, d, _ = req("POST", "/api/wifi", body, auth=pw)
        report(s == 400, "wifi rejects: " + name, "HTTP %d %s" % (s, err(d)))

    # --- update: only files rejected BEFORE anything is written ---
    s, d, _ = req("GET", "/api/ota/status", auth=pw)
    before = js(d) or {}
    s, d, _ = req("POST", "/api/ota", b"\x00" * 1000, auth=pw, timeout=20)
    report(s == 400 and err(d) == ("bad_image", "short"), "ota: 1000 B file rejected (short)", "HTTP %d %s" % (s, err(d)))
    s, d, _ = req("POST", "/api/ota", b"\x00" * 70000, auth=pw, timeout=30)
    report(s == 400 and err(d) == ("bad_image", "header"), "ota: 70 kB without an ESP32 header rejected (header)", "HTTP %d %s" % (s, err(d)))
    s, d, _ = req("GET", "/api/ota/status", auth=pw)
    after = js(d) or {}
    report(after.get("canRollback") == before.get("canRollback") and
           (after.get("other") or {}).get("elf") == (before.get("other") or {}).get("elf"),
           "ota: the other slot untouched after the rejection", "elf %s" % (after.get("other") or {}).get("elf"))
    s, d, _ = req("POST", "/api/ota?x=1", None, auth=pw)
    report(s == 411, "ota without a body = 411", "HTTP %d %s" % (s, err(d)))

    # --- password ---
    s, d, _ = req("POST", "/api/password", form(old=pw + "bad", new="NewPasswd123"), auth=pw)
    report(s == 403 and err(d)[0] == "wrong_password", "password: wrong current one = 403 wrong_password", "HTTP %d %s" % (s, err(d)))
    s, d, _ = req("GET", "/api/status", auth=pw)
    report(s == 200, "after wrong_password the login still works", "HTTP %d" % s)
    for bad, name in (("tooshort9", "9 characters"), ("space in pass01", "space"), ("x" * 64, "64 characters")):
        s, d, _ = req("POST", "/api/password", form(old=pw, new=bad), auth=pw)
        report(s == 400 and err(d)[0] == "weak_password", "password: rejects " + name, "HTTP %d %s" % (s, err(d)))
    tmp = "".join(secrets.choice(string.ascii_letters + string.digits) for _ in range(20))
    s1, d1, _ = req("POST", "/api/password", form(old=pw, new=tmp), auth=pw)
    s2, d2, _ = req("POST", "/api/password", form(old=tmp, new=pw), auth=tmp)
    if s2 != 200:
        time.sleep(0.3)
        s2, d2, _ = req("POST", "/api/password", form(old=tmp, new=pw), auth=tmp)
    report(s1 == 200, "password: change to a temporary one", "HTTP %d %s" % (s1, err(d1)))
    report(s2 == 200, "password: back to the current one", "HTTP %d %s" % (s2, err(d2)))
    if s1 == 200 and s2 != 200:
        # Emergency: the temporary password goes to a file (NOT to the screen), so that access is not lost.
        with open(os.path.join(os.path.dirname(OUT), "TEMPORARY_PASSWORD_delete_me.txt"), "w") as f:
            f.write(tmp + "\n")
        print("WARNING: the password stayed temporary; stored in tools/TEMPORARY_PASSWORD_delete_me.txt (or 'wifi showpass' on USB)")
    print("The device network restarts (the same password); waiting up to 40 s to reconnect...")
    ok = False
    end = time.time() + 40
    time.sleep(3)
    while time.time() < end:
        try:
            s, d, _ = req("GET", "/api/status", auth=pw, timeout=3)
            if s == 200:
                ok = True
                break
        except Exception:
            pass
        time.sleep(1)
    report(ok, "after changing there and back the old password works", "reconnect to the device network by hand if FAIL")

    nbad = sum(1 for r in results if r.startswith("FAIL"))
    summary = "RESULT: %d tests, %d errors" % (len(results), nbad)
    print(summary)
    with open(OUT, "w", encoding="utf-8") as f:
        f.write("\n".join(results + [summary]) + "\n")
    print("Saved: " + OUT)


if __name__ == "__main__":
    main()
