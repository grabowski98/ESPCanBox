#!/usr/bin/env python3
"""Save the output of the sniffer or the recorder to a capture file, with step markers.

Usage:
    uv run --with pyserial python -I record_serial.py PORT [--scenario FILE] [--out DIR] [--baud-uart N]

PORT is the serial port of the ESP32 (COM7, /dev/ttyACM0, /dev/cu.usbmodem1101, ...).
Every line from the device is written unchanged to DIR/can_YYYYMMDD_HHMMSS.log.

While recording, type a command and press Enter:
    (empty line)   start the next step of the scenario: writes "# STEP n HH:MM:SS.mmm <text>"
    1 .. 9         extra marker: writes "# MARK n HH:MM:SS.mmm"
    q              stop

The scenario file has one step per line (empty lines are skipped), for example
scenarios/example_buttons.txt. Doing one action per step, repeated three times with a pause,
makes the changing frame easy to find later.

Only reads from the device, except one optional line: with --baud-uart N the script sends
"baud N" once (a recorder command that sets the UART rate of the box <-> radio lines; the sniffer
ignores it). Nothing is ever sent to the car: the device itself is listen-only.
Requires pyserial (https://pypi.org/project/pyserial/).
"""
import argparse
import datetime
import os
import queue
import sys
import threading
import time


def now_hms():
    return datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]


def read_steps(path):
    if not path:
        return []
    with open(path, encoding="utf-8") as f:
        return [line.strip() for line in f if line.strip()]


def keyboard(q):
    for line in sys.stdin:
        q.put(line.strip())
    q.put("q")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port")
    ap.add_argument("--scenario", help="text file, one step per line")
    ap.add_argument("--out", default="captures", help="output directory (default: captures)")
    ap.add_argument("--baud-uart", type=int, help="send 'baud N' to the recorder once at start")
    args = ap.parse_args()

    try:
        import serial  # pyserial
    except ImportError:
        print("pyserial is missing: run with 'uv run --with pyserial python -I record_serial.py ...'")
        return 2

    steps = read_steps(args.scenario)
    os.makedirs(args.out, exist_ok=True)
    name = "can_" + datetime.datetime.now().strftime("%Y%m%d_%H%M%S") + ".log"
    path = os.path.join(args.out, name)

    # Native USB of the ESP32-S3 sends data only after the PC raises DTR.
    port = serial.Serial(args.port, 115200, timeout=0.2)
    port.dtr = True
    if args.baud_uart:
        time.sleep(3)
        port.write(("baud %d\n" % args.baud_uart).encode("ascii"))

    keys = queue.Queue()
    threading.Thread(target=keyboard, args=(keys,), daemon=True).start()

    step = 0
    frames = 0
    last_show = time.monotonic()
    print("%s -> %s" % (args.port, path))
    print("Enter = next step, 1-9 + Enter = marker, q + Enter = stop.")
    if steps:
        print("NEXT STEP 1/%d: %s" % (len(steps), steps[0]))
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        out.write("# start %s\n" % datetime.datetime.now().isoformat(sep=" ", timespec="milliseconds"))
        buf = b""
        try:
            while True:
                buf += port.read(4096)
                while b"\n" in buf:
                    raw, buf = buf.split(b"\n", 1)
                    line = raw.decode("ascii", errors="replace").rstrip("\r")
                    if line.startswith("# stat"):
                        line += " pc=" + now_hms()
                    out.write(line + "\n")
                    if line.startswith("#"):
                        print(line)
                    else:
                        frames += 1
                while not keys.empty():
                    k = keys.get()
                    if k.lower() == "q":
                        raise KeyboardInterrupt
                    if k == "" and step < len(steps):
                        m = "# STEP %d %s %s" % (step + 1, now_hms(), steps[step])
                        out.write(m + "\n")
                        print(m)
                        step += 1
                        print("NEXT STEP %d/%d: %s" % (step + 1, len(steps), steps[step]) if step < len(steps)
                              else "That was the last step. q = stop.")
                    elif len(k) == 1 and k in "123456789":
                        m = "# MARK %s %s" % (k, now_hms())
                        out.write(m + "\n")
                        print(m)
                if time.monotonic() - last_show >= 5:
                    out.flush()
                    last_show = time.monotonic()
                    print("frames saved: %d" % frames)
        except KeyboardInterrupt:
            pass
        finally:
            out.write("# stop %s\n" % datetime.datetime.now().isoformat(sep=" ", timespec="milliseconds"))
            port.close()
    print("Saved: %s (frames: %d)" % (path, frames))
    return 0


if __name__ == "__main__":
    sys.exit(main())
