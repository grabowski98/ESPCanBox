#!/usr/bin/env python3
"""Cut the FwIdent block out of a firmware image and save it as a small test fixture.

The net_util host test feeds a real 0.6.0 image through IdentScanner (lib/core/fw_ident.h).
A whole image is 1.2 MB and contains build paths of the machine it was built on, so the
repository keeps only the FwIdent structure itself (72 bytes, copied unchanged):
    magic[12] product[16] version[16] build[24] mode net canTx reserved

test_data/fixtures/fw_ident_060.bin comes from the prototype firmware (before the project got its
name) and carries the prototype marker (LEGACY_MAGIC below); the test checks that this project does
NOT accept it. Cut such an image with --legacy. Without --legacy the current marker is used
(include/project_identity.h).

Usage (from the firmware directory):
    python -I tools/make_fw_fixture.py [--legacy] IMAGE.bin OUT.bin
    python -I tools/make_fw_fixture.py --check test_data/fixtures/fw_ident_060.bin
Uses only the Python standard library.
"""
import argparse
import os
import sys

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS_DIR)          # python -I does not add the script directory
sys.dont_write_bytecode = True
import project_identity  # noqa: E402

# FW_IDENT_MAGIC plus its terminating zero (12 bytes), from include/project_identity.h
MAGIC = project_identity.fwid_marker(project_identity.load(os.path.dirname(TOOLS_DIR)))
# Marker of the prototype firmware up to 1.0.0 (historical, fixed; test_host/net_util.cpp LEGACY_FWID_MAGIC)
LEGACY_MAGIC = b"\x7fC508-FWID\x7f\x00"
SIZE = 12 + 16 + 16 + 24 + 4         # sizeof(FwIdent), no padding (all fields are bytes)


def describe(block):
    def text(raw):
        return raw.split(b"\x00", 1)[0].decode("ascii", "replace")
    product = text(block[12:28])
    version = text(block[28:44])
    build = text(block[44:68])
    mode, net, can_tx, reserved = block[68], block[69], block[70], block[71]
    return "product %s, version %s, build %s, mode %u, net %u, canTx %u, reserved %u" % (
        product, version, build, mode, net, can_tx, reserved)


def marker_name(block):
    """'current' or 'prototype' for the marker at offset 0, None when neither is there."""
    if block.startswith(MAGIC):
        return "current"
    if block.startswith(LEGACY_MAGIC):
        return "prototype"
    return None


def check_block(block):
    """The block must be a lone FwIdent with printable text fields (no paths, no other data)."""
    if len(block) != SIZE:
        return "size %d, expected %d" % (len(block), SIZE)
    if marker_name(block) is None:
        return "magic missing at offset 0"
    for start, end in ((12, 28), (28, 44), (44, 68)):
        field = block[start:end].split(b"\x00", 1)[0]
        if any(c < 0x20 or c > 0x7E for c in field) or b"/" in field or b"\\" in field:
            return "unexpected bytes in a text field"
    return None


def main():
    ap = argparse.ArgumentParser(description="Extract the FwIdent block from a firmware image.")
    ap.add_argument("files", nargs="+", help="IMAGE OUT, or FIXTURE with --check")
    ap.add_argument("--check", action="store_true", help="only validate an existing fixture")
    ap.add_argument("--legacy", action="store_true", help="look for the prototype marker (image up to 1.0.0)")
    args = ap.parse_args()

    if args.check:
        rc = 0
        for path in args.files:
            with open(path, "rb") as f:
                block = f.read()
            err = check_block(block)
            print("%s: %s" % (path, err if err else "OK, %s marker, %s" % (marker_name(block), describe(block))))
            rc |= 1 if err else 0
        return rc

    if len(args.files) != 2:
        ap.error("give IMAGE and OUT")
    with open(args.files[0], "rb") as f:
        image = f.read()
    magic = LEGACY_MAGIC if args.legacy else MAGIC
    count = image.count(magic)
    if count != 1:
        print("expected exactly one FwIdent in the image, found %d" % count, file=sys.stderr)
        return 1
    pos = image.find(magic)
    block = image[pos:pos + SIZE]
    err = check_block(block)
    if err:
        print("block at offset %d rejected: %s" % (pos, err), file=sys.stderr)
        return 1
    with open(args.files[1], "wb") as f:
        f.write(block)
    print("offset %d, %d bytes: %s" % (pos, SIZE, describe(block)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
