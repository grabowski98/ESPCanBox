# Build time in the FwIdent marker (PlatformIO "pre:" script, all environments).
#
# The "build" field (src/version.cpp: __DATE__ __TIME__) is the compile time of version.cpp ITSELF. SCons
# recompiles that file only when version.cpp / version.h change, so after a change in another file
# the panel and "ver" showed an old time although the image was new (measured 17.11.2026: version.cpp.o from 09:59,
# firmware.bin from 10:56; both program slots with the same "Zbudowano" (built) time).
#
# What it does: when any file in src/, lib/ or web/ is newer than version.cpp.o of this environment,
# it deletes that object. SCons then recompiles it and "build" = the time of that compilation.
# Without changes in the sources it deletes nothing: "pio run -t upload" alone does not change the image (same ELF hash).

Import("env")  # noqa: F821 (name provided by PlatformIO)

import os

_obj = os.path.join(env.subst("$BUILD_DIR"), "src", "version.cpp.o")  # noqa: F821
_proj = env.subst("$PROJECT_DIR")  # noqa: F821


def _newest_source():
    newest = 0.0
    for d in ("src", "lib", "web"):
        for root, _dirs, files in os.walk(os.path.join(_proj, d)):
            for f in files:
                try:
                    newest = max(newest, os.path.getmtime(os.path.join(root, f)))
                except OSError:
                    pass
    return newest


if os.path.exists(_obj) and _newest_source() > os.path.getmtime(_obj):
    os.remove(_obj)
