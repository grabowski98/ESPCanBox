#!/usr/bin/env bash
# Every check of the repository in one command (the same list CI runs, .github/workflows/ci.yml).
#
#   bash tools/release_check.sh                 run every step; exit code 0 only when all passed
#   bash tools/release_check.sh --skip-build    leave out the PlatformIO builds and the two checks that need
#                                               them (quick run while editing; NOT a release check)
#   bash tools/release_check.sh --binaries DIR  also scan the images in DIR for private data
#                                               (release.yml passes its dist/ directory)
#
# Steps (run from the repository root; every step runs even after a failure, a summary is printed at the end):
#   firmware/  host tests (tools/run_host_tests.sh; compiler from CXX, default zig through uvx)
#              can_diff self test on the synthetic capture
#              build of the six environments, check_no_can_tx.sh, check_tx_profile_lock.sh
#              check_layering.py, check_i18n.py (all six panel languages)
#   root       set_project_name.py --check, scan_private.py, check_excluded.py, strip_exif.py --check,
#              check_provenance.py, check_doc_links.py, check_translations.py, mkdocs build --strict (into a temporary directory),
#              actionlint on .github/workflows/
#
# Variables: PIO (default "uvx platformio"), PYTHON (default "python"), CXX (host tests),
#            MKDOCS (default: mkdocs-material with mkdocs-static-i18n through uvx),
#            ACTIONLINT (default: actionlint-py through uvx, with shellcheck for the run: scripts).
# Uploads nothing and opens no serial port. Needs network the first time (uvx packages, PlatformIO platform).
# Exit code: 0 = all steps passed, 1 = at least one step failed, 2 = wrong usage.
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 2

PIO=${PIO:-uvx platformio}
PYTHON=${PYTHON:-python}
MKDOCS=${MKDOCS:-uvx --from mkdocs-material==9.8.1 --with mkdocs-static-i18n==1.3.1 mkdocs}
ACTIONLINT=${ACTIONLINT:-uvx --from actionlint-py==1.7.12.25 --with shellcheck-py==0.11.0.1 actionlint}
ENVS="listen decoder listen_wifi decoder_wifi decoder_tx_wifi listen_generic_wifi"
LANGS="en,pl,de,es,ru,uk"
export PIO

SKIP_BUILD=0
BIN_DIR=""
while [ $# -gt 0 ]; do
    case "$1" in
        --skip-build) SKIP_BUILD=1 ;;
        --binaries)
            [ $# -ge 2 ] || { echo "--binaries needs a directory" >&2; exit 2; }
            BIN_DIR=$2
            shift
            ;;
        *) echo "unknown parameter: $1" >&2; exit 2 ;;
    esac
    shift
done

TMP=$(mktemp -d 2> /dev/null || echo "${TMPDIR:-/tmp}/release_check_$$")
mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT

PASSED=""
FAILED=""
SKIPPED=""
# step NAME DIRECTORY COMMAND...: runs the command in DIRECTORY (relative to the root), output to the console
step() {
    local name=$1 dir=$2
    shift 2
    echo
    echo "=== $name"
    if (cd "$ROOT/$dir" && "$@" < /dev/null); then
        PASSED="$PASSED $name"
    else
        echo "FAIL   step $name"
        FAILED="$FAILED $name"
    fi
}
skip() {
    echo
    echo "=== $1: SKIPPED (--skip-build)"
    SKIPPED="$SKIPPED $1"
}

# Wrappers for commands that consist of several words in a variable.
# shellcheck disable=SC2086,SC2329  # called through step()
pio_build() { local args=""; for e in $ENVS; do args="$args -e $e"; done; $PIO run $args; }
# shellcheck disable=SC2086,SC2329  # called through step()
mkdocs_build() { $MKDOCS build --strict --site-dir "$TMP/site"; }
# shellcheck disable=SC2086,SC2329  # called through step()
actionlint_run() { $ACTIONLINT .github/workflows/*.yml; }

# --- firmware/ ---
step host_tests firmware bash tools/run_host_tests.sh
step can_diff_selftest firmware "$PYTHON" -I tools/can_diff.py test_data/synthetic/marker_demo.log --selftest
if [ $SKIP_BUILD -eq 0 ]; then
    step build_envs firmware pio_build
    step check_no_can_tx firmware bash tools/check_no_can_tx.sh
    step check_tx_profile_lock firmware bash tools/check_tx_profile_lock.sh
else
    skip build_envs
    skip check_no_can_tx
    skip check_tx_profile_lock
fi
step check_layering firmware "$PYTHON" -I tools/check_layering.py
step check_i18n firmware "$PYTHON" -I tools/check_i18n.py --langs "$LANGS"

# --- repository root ---
step project_identity . "$PYTHON" -I tools/set_project_name.py --check
step scan_private . "$PYTHON" -I tools/scan_private.py .
if [ -n "$BIN_DIR" ]; then
    step scan_private_binaries . "$PYTHON" -I tools/scan_private.py "$BIN_DIR"
fi
step check_excluded . "$PYTHON" -I tools/check_excluded.py .
step strip_exif . "$PYTHON" -I tools/strip_exif.py --check .
step check_provenance . "$PYTHON" -I tools/check_provenance.py --root .
step check_doc_links . "$PYTHON" -I tools/check_doc_links.py
step check_translations . "$PYTHON" -I tools/check_translations.py
step mkdocs_strict . mkdocs_build
step actionlint . actionlint_run

echo
echo "=== summary"
echo "passed: ${PASSED:- none}"
[ -n "$SKIPPED" ] && echo "skipped:$SKIPPED (not a full release check)"
if [ -n "$FAILED" ]; then
    echo "FAILED:$FAILED"
    exit 1
fi
echo "All steps passed."
exit 0
