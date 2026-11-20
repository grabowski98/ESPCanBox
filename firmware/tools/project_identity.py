# Project identity for the Python tools: reads the PROJECT_* macros from include/project_identity.h.
# That header is written by ../tools/set_project_name.py from project.json (the single source of the values).
# Standard library only. Used by: embed_web.py, mock_device.py, record.py, make_fw_fixture.py.
import os
import re

_DEFINE = re.compile(r'^\s*#define\s+(PROJECT_[A-Z0-9_]+)\s+"([^"\\]*)"', re.M)


def load(project_dir):
    """Returns {macro: value} from <project_dir>/include/project_identity.h; missing file = OSError."""
    path = os.path.join(project_dir, "include", "project_identity.h")
    with open(path, "rb") as f:
        text = f.read().decode("ascii")
    found = dict(_DEFINE.findall(text))
    for key in ("PROJECT_NAME", "PROJECT_FW_PRODUCT", "PROJECT_FWID_MAGIC"):
        if key not in found:
            raise ValueError("%s: none #define %s" % (path, key))
    return found


def fwid_marker(ident):
    """The 12 bytes of the FwIdent::magic field: 0x7F, the 9 marker characters, 0x7F, zero (lib/core/fw_ident.h)."""
    magic = ident["PROJECT_FWID_MAGIC"].encode("ascii")
    if len(magic) != 9:
        raise ValueError("PROJECT_FWID_MAGIC must have 9 characters")
    return b"\x7f" + magic + b"\x7f\x00"
