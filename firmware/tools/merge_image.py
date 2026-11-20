#!/usr/bin/env python3
# Merges the images of one built environment into a single file that is written at flash address 0
# (bootloader, partition table, boot_app0, application). Used by .github/workflows/release.yml for
# flashing from the browser (ESP Web Tools) or with one esptool command.
#
#   uvx platformio project metadata -e listen --json-output > meta.json
#   python -I tools/merge_image.py meta.json listen merged.bin
#
# The addresses come from PlatformIO itself (the "flash_images" and "application_offset" fields of the
# project metadata), not from this file, so they always match what "pio run -t upload" would write.
# Typical ESP32-S3 values: bootloader 0x0, partition table 0x8000, boot_app0 0xe000, application 0x10000.
# Flash mode, speed and size stay as they are in the built bootloader header ("keep").
#
# Refuses the transmitting environment decoder_tx_wifi: release images never contain CAN transmit.
# Only writes the output file. Talks to no board and opens no serial port (esptool merge-bin works on
# files only). esptool: variable ESPTOOL, otherwise the copy PlatformIO installed (tool-esptool package).
import json
import os
import subprocess
import sys

sys.dont_write_bytecode = True

FORBIDDEN_ENVS = {"decoder_tx_wifi"}


def esptool_cmd():
    if os.environ.get("ESPTOOL"):
        return os.environ["ESPTOOL"].split()
    base = os.path.join(os.path.expanduser("~"), ".platformio", "packages", "tool-esptool")
    for name in ("esptool", "esptool.exe"):
        path = os.path.join(base, name)
        if os.path.isfile(path):
            return [path]
    sys.exit("FAIL   no esptool: build once (PlatformIO installs tool-esptool) or set ESPTOOL")


def main():
    if len(sys.argv) != 4:
        sys.exit("usage: merge_image.py METADATA.json ENV OUTPUT.bin")
    meta_path, env, out = sys.argv[1:]
    if env in FORBIDDEN_ENVS or "tx" in env.split("_"):
        sys.exit("FAIL   %s: release images never contain CAN transmit" % env)
    with open(meta_path, encoding="utf-8") as f:
        meta = json.load(f)
    if env not in meta:
        sys.exit("FAIL   %s not in %s" % (env, meta_path))
    data = meta[env]
    extra = data.get("extra") or {}
    images = extra.get("flash_images") or []
    app_offset = extra.get("application_offset")
    app = os.path.splitext(data["prog_path"])[0] + ".bin"
    if not images or not app_offset:
        sys.exit("FAIL   %s: no flash_images / application_offset in the metadata" % env)
    pairs = [(img["offset"], img["path"]) for img in images] + [(app_offset, app)]
    for off, path in pairs:
        if not os.path.isfile(path):
            sys.exit("FAIL   %s: missing image %s (build the environment first)" % (env, path))
    pairs.sort(key=lambda p: int(p[0], 16))
    cmd = esptool_cmd() + ["--chip", "esp32s3", "merge-bin", "-o", out,
                           "--flash-mode", "keep", "--flash-freq", "keep", "--flash-size", "keep"]
    for off, path in pairs:
        cmd += [off, path]
    print("merge: " + ", ".join("%s %s" % (off, os.path.basename(path)) for off, path in pairs), flush=True)
    rc = subprocess.call(cmd, stdin=subprocess.DEVNULL)
    if rc != 0:
        sys.exit("FAIL   esptool merge-bin returned %d" % rc)
    print("OK     %s -> %s (%d B)" % (env, out, os.path.getsize(out)))


if __name__ == "__main__":
    main()
