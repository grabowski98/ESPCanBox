#!/usr/bin/env bash
# Check of the compile-time transmit lock: the transmitting environment
# decoder_tx_wifi must NOT build with a listen-only car profile.
#
#   bash tools/check_tx_profile_lock.sh     exit code 0 = every forbidden combination failed to build,
#                                           with the expected message from src/config.h
#
# Cases: -DCAR_PROFILE=GENERIC (empty transmit allow list, listen only), -DCAR_PROFILE=EXAMPLE (host test
# template), -DCAR_PROFILE=CUSTOM (your own profile: listen only), -DRADIO_PROTOCOL=EXAMPLE (host test template)
# and -DRADIO_PROTOCOL=CUSTOM (your own radio protocol: listen only). The CUSTOM cases name a header and an
# object that exist (the generic profile, the "none" protocol), so the build reaches the transmit guard in src/config.h and does
# not stop earlier on a missing header. Each is built with PlatformIO into a temporary directory outside the project (the normal
# .pio/build/decoder_tx_wifi used by tools/check_no_can_tx.sh is not touched). Flashes nothing.
# A build that fails for another reason (no message from config.h) counts as a failure of this check.
set -u
cd "$(dirname "$0")/.." || exit 2

PIO=${PIO:-uvx platformio}
TMP_ROOT=$(mktemp -d 2> /dev/null || echo "${TMPDIR:-/tmp}/tx_profile_lock_$$")
mkdir -p "$TMP_ROOT"
trap 'rm -rf "$TMP_ROOT"' EXIT

bad=0
# check NAME "build flags" "expected message"
check() {
    local name=$1 flags=$2 msg=$3
    local log="$TMP_ROOT/$name.log"
    # shellcheck disable=SC2086
    PLATFORMIO_BUILD_DIR="$TMP_ROOT/build_$name" PLATFORMIO_BUILD_FLAGS="$flags" \
        $PIO run -e decoder_tx_wifi < /dev/null > "$log" 2>&1
    local rc=$?
    if [ "$rc" -eq 0 ]; then
        echo "FAIL   decoder_tx_wifi with $flags: build SUCCEEDED (must fail)"
        bad=1
    elif grep -q "$msg" "$log"; then
        echo "OK     decoder_tx_wifi with $flags: build failed (code $rc) with: $msg"
    else
        echo "FAIL   decoder_tx_wifi with $flags: build failed (code $rc), but not with the expected message"
        grep -m 5 -E "error|Error" "$log"
        bad=1
    fi
}

check GENERIC "-DCAR_PROFILE=GENERIC" "CAN transmit is not allowed with CAR_PROFILE=GENERIC"
check EXAMPLE "-DCAR_PROFILE=EXAMPLE" "CAR_PROFILE=EXAMPLE is a template"
# The header and object of an existing profile (lib/car_generic), as a user of CUSTOM would give their own.
check CUSTOM "-DCAR_PROFILE=CUSTOM -DCAR_PROFILE_CUSTOM_HEADER=\\\"profile_generic.h\\\" -DCAR_PROFILE_CUSTOM_OBJECT=kCarGeneric" \
    "CAN transmit is not allowed with CAR_PROFILE=CUSTOM"
# A radio protocol of your own is listen only too: its commands would reach the vehicle setting change.
# The header and object of an existing protocol (lib/radio_none), as a user of CUSTOM would give their own.
check RADIO_EXAMPLE "-DRADIO_PROTOCOL=EXAMPLE" "RADIO_PROTOCOL=EXAMPLE is a template"
check RADIO_CUSTOM "-DRADIO_PROTOCOL=CUSTOM -DRADIO_PROTOCOL_CUSTOM_HEADER=\\\"protocol_none.h\\\" -DRADIO_PROTOCOL_CUSTOM_OBJECT=kRadioNone" \
    "CAN transmit is not allowed with RADIO_PROTOCOL=CUSTOM"

if [ $bad -ne 0 ]; then
    exit 1
fi
echo "Transmitting environment does not build with the generic, example or custom profile, or with the example or custom radio protocol."
exit 0
