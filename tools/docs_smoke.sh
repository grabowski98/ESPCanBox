#!/usr/bin/env bash
# Runs the commands of the Quick start pages (docs/en/quick-start/) that need no board, so the pages
# stay true when the code changes:
#   1. uvx platformio --version                        (flash.md, "Check it")
#   2. uvx platformio run -e listen                    (flash.md, "Build"; checks .pio/build/listen/firmware.bin)
#   3. bash tools/run_host_tests.sh                    (host tests; the compiler is fetched once if needed)
#   4. python -I tools/mock_device.py <capture> ...    (first-boot.md, panel mock: GET /api/hello answers)
#
# Usage (from the repository root, Git Bash on Windows, any shell elsewhere):
#   bash tools/docs_smoke.sh                 work in a temporary copy of firmware/ (the working tree stays clean)
#   bash tools/docs_smoke.sh --in-place      run in firmware/ itself, exactly as a reader would
#   bash tools/docs_smoke.sh --skip-build    leave out step 2 (the slowest one)
# Variables: PIO (default "uvx platformio"), PYTHON (default "python"), CXX (passed to the host tests).
#
# Uploads nothing and opens no serial port. The mock listens on 127.0.0.1 only, on a free port.
# Needs network access the first time (PlatformIO platform, compiler); later runs reuse the cache.
# Exit code: 0 = every step passed, 1 = a step failed (its name is printed), 2 = wrong usage.
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
PIO=${PIO:-uvx platformio}
PYTHON=${PYTHON:-python}
IN_PLACE=0
SKIP_BUILD=0
for a in "$@"; do
    case "$a" in
        --in-place) IN_PLACE=1 ;;
        --skip-build) SKIP_BUILD=1 ;;
        *) echo "unknown parameter: $a" >&2; exit 2 ;;
    esac
done

fail() {
    echo "FAIL   $1"
    exit 1
}
ok() { echo "OK     $1"; }

# ---------- where to work ----------
if [ $IN_PLACE -eq 1 ]; then
    FW="$ROOT/firmware"
else
    WORK=$(mktemp -d) || fail "temporary directory"
    trap 'rm -rf "$WORK"' EXIT
    # Copy without the build directories (.pio, .build_host): a clean start, as after a download.
    tar -C "$ROOT" --exclude=firmware/.pio --exclude=firmware/.build_host -cf - firmware project.json \
        | tar -C "$WORK" -xf - || fail "copy of firmware/ to $WORK"
    FW="$WORK/firmware"
    echo "working copy: $FW"
fi
cd "$FW" || fail "cd $FW"

# ---------- 1. PlatformIO runs ----------
# shellcheck disable=SC2086
$PIO --version < /dev/null || fail "$PIO --version"
ok "platformio --version"

# ---------- 2. build of the default environment ----------
if [ $SKIP_BUILD -eq 0 ]; then
    # shellcheck disable=SC2086
    $PIO run -e listen < /dev/null > "$FW/docs_smoke_build.txt" 2>&1
    rc=$?
    if [ $rc -ne 0 ]; then
        tail -30 "$FW/docs_smoke_build.txt"
        fail "platformio run -e listen (code $rc)"
    fi
    [ -s .pio/build/listen/firmware.bin ] || fail "no .pio/build/listen/firmware.bin after the build"
    rm -f "$FW/docs_smoke_build.txt"
    ok "platformio run -e listen ($(wc -c < .pio/build/listen/firmware.bin) B firmware.bin)"
else
    echo "SKIP   platformio run -e listen (--skip-build)"
fi

# ---------- 3. host tests ----------
# run_host_tests.sh calls the compiler with "uvx --offline": fetch it once when it is not in the cache yet.
if [ -z "${CXX:-}" ]; then
    uvx --offline --from ziglang python -I -m ziglang version < /dev/null > /dev/null 2>&1 \
        || uvx --from ziglang python -I -m ziglang version < /dev/null > /dev/null \
        || fail "compiler for the host tests (uvx --from ziglang)"
fi
bash tools/run_host_tests.sh < /dev/null > "$FW/docs_smoke_tests.txt" 2>&1
rc=$?
if [ $rc -ne 0 ]; then
    tail -30 "$FW/docs_smoke_tests.txt"
    fail "tools/run_host_tests.sh (code $rc)"
fi
grep -q "All tests match the references." "$FW/docs_smoke_tests.txt" || fail "tools/run_host_tests.sh: no final line"
rm -f "$FW/docs_smoke_tests.txt"
if [ $IN_PLACE -eq 0 ]; then rm -rf .build_host; fi
ok "tools/run_host_tests.sh"

# ---------- 4. panel mock ----------
# Starts the mock on a free port of 127.0.0.1 with the capture named in first-boot.md, waits for
# GET /api/hello, stops it. Python standard library only.
"$PYTHON" -I - "$FW" << 'EOF' || fail "tools/mock_device.py"
import json, os, socket, subprocess, sys, time, urllib.request
fw = sys.argv[1]
s = socket.socket()
s.bind(("127.0.0.1", 0))
port = s.getsockname()[1]
s.close()
cmd = [sys.executable, "-I", os.path.join("tools", "mock_device.py"),
       os.path.join("test_data", "captures", "can_20261111_191045.log"),
       "--port", str(port), "--password", "smoke-test-only-1"]
p = subprocess.Popen(cmd, cwd=fw, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
try:
    hello = None
    for _ in range(100):
        if p.poll() is not None:
            break
        try:
            with urllib.request.urlopen("http://127.0.0.1:%d/api/hello" % port, timeout=1) as r:
                hello = json.loads(r.read().decode("utf-8"))
            break
        except OSError:
            time.sleep(0.1)
    if not hello or hello.get("api") != 1:
        err = p.stderr.read().decode("utf-8", "replace") if p.poll() is not None else ""
        print("mock did not answer /api/hello: %r %s" % (hello, err[-500:]))
        sys.exit(1)
    print("mock /api/hello: product=%s api=%s" % (hello.get("product"), hello.get("api")))
finally:
    if p.poll() is None:
        p.terminate()
        try:
            p.wait(5)
        except subprocess.TimeoutExpired:
            p.kill()
EOF
ok "tools/mock_device.py"

echo "Quick start commands without a board: all passed."
exit 0
