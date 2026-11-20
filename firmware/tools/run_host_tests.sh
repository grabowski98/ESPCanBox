#!/usr/bin/env bash
# Logic tests (lib/*: core, car_psa_aee2010, radio_xp, app) on the PC: builds into .build_host/, runs the tests, compares with the references.
#
#   bash tools/run_host_tests.sh                   compare with test_host/expected/<test>.txt
#                                                  and with test_host/golden/ (stream references: once
#                                                  the 0.6.0 glue, once the Decoder class);
#                                                  exit code != 0 at the first difference
#   bash tools/run_host_tests.sh --record-golden   records the references again (only in steps named
#                                                  in the plan, with a description of the difference)
#
# Compiler: variable CXX (e.g. CXX=g++ or CXX=clang++ bash tools/run_host_tests.sh). Without CXX:
# clang from the ziglang package through uvx (as before; nothing has to be installed apart from uv).
#
# Run from Git Bash. Every uvx call has "< /dev/null" (without it uvx can hang), Python runs with -I.
# Flashes nothing and opens no ports: only the compiler on the PC and anonymized captures
# from the directory test_data/captures/ (frames 336/3B6/2B6 overwritten by ../tools/sanitize_capture.py).
# The capture directory can be given with the variable LOGS (e.g. LOGS=/other/captures bash tools/run_host_tests.sh).
#
# Reference = the whole program output (stdout and stderr together, without CR characters) for every run
# from the list below, with the header "### test arguments" and the footer "### code=N" (program exit code).
set -u
cd "$(dirname "$0")/.." || exit 2

LOGS=${LOGS:-test_data/captures}
FIXTURES=test_data/fixtures
OUT=.build_host
EXP=test_host/expected
REC=0
if [ "${1:-}" = "--record-golden" ]; then
    REC=1
elif [ -n "${1:-}" ]; then
    echo "unknown parameter: $1" >&2
    exit 2
fi

mkdir -p "$OUT" "$EXP"
rm -f "$OUT/.warned"

# Compiler command. CXX may consist of several words (e.g. "ccache g++"), hence no quotes.
CXX=${CXX:-}
compile() {
    if [ -n "$CXX" ]; then
        # shellcheck disable=SC2086
        $CXX "$@"
    else
        uvx --offline --from ziglang python -I -m ziglang c++ "$@"
    fi
}
# The compiler is called with --offline (fast, no network per file). On a fresh computer the
# ziglang package is not in the uv cache yet: fetch it once here (needs internet the first time).
if [ -z "$CXX" ] && ! uvx --offline --from ziglang python -I -m ziglang version < /dev/null > /dev/null 2>&1; then
    echo "ziglang not in the uv cache yet: downloading it once"
    uvx --from ziglang python -I -m ziglang version < /dev/null > /dev/null || {
        echo "FAIL   could not get the compiler (uvx --from ziglang); check the internet connection or set CXX" >&2
        exit 2
    }
fi
echo "compiler: ${CXX:-uvx ziglang c++}"

# Libraries: lib/core, lib/car_psa_aee2010, lib/radio_xp, lib/app;
# Also lib/car_generic, lib/car_example and lib/radio_example (host tests only), lib/radio_none.
# Every library directory gets its own -I; a file name given to build() is looked up in lib/*/ and must
# exist in exactly one library.
LIB_INC=""
for d in lib/*/; do LIB_INC="$LIB_INC -I ${d%/}"; done

# lib_file NAME.cpp: prints lib/<library>/NAME.cpp, or stops when the file is missing or ambiguous
lib_file() {
    local hits
    hits=$(ls lib/*/"$1" 2> /dev/null)
    if [ -z "$hits" ] || [ "$(printf '%s\n' "$hits" | wc -l)" -ne 1 ]; then
        echo "FAIL   no single lib/*/$1 (found: ${hits:-none})" >&2
        exit 1
    fi
    printf '%s\n' "$hits"
}

# build NAME file.cpp from lib/*/...
# Any compiler message (error or warning) aborts: the baseline is zero warnings.
# lib/core/vehicle_signals.cpp (car-neutral types) is linked into every test that uses
# any library file: earlier its three functions were part of psa_frames.cpp.
# Optional variables for one call: SRC=<file in test_host/ without .cpp> builds another source file
# under the name NAME; DEFS="-D..." adds preprocessor definitions (e.g. -DCAN_TX_EXPERIMENTAL=1).
build() {
    local name=$1
    shift
    local srcs="test_host/${SRC:-$name}.cpp"
    local f path
    for f in "$@"; do
        path=$(lib_file "$f") || exit 1
        srcs="$srcs $path"
    done
    if [ $# -gt 0 ]; then srcs="$srcs lib/core/vehicle_signals.cpp"; fi
    rm -f "$OUT/$name.exe"
    # shellcheck disable=SC2086
    compile -std=c++17 -O1 -Wall ${DEFS:-} $LIB_INC -I include \
        $srcs -o "$OUT/$name.exe" < /dev/null > "$OUT/$name.build.txt" 2>&1
    local rc=$?
    if [ $rc -ne 0 ] || [ -s "$OUT/$name.build.txt" ] || [ ! -x "$OUT/$name.exe" ]; then
        # KEEP_GOING=1 (CI, second compiler): a build that only warned still runs, every warning is
        # listed in one pass, and the script fails at the end.
        if [ -n "${KEEP_GOING:-}" ] && [ $rc -eq 0 ] && [ -x "$OUT/$name.exe" ]; then
            echo "WARN   $name: compiler messages"
            cat "$OUT/$name.build.txt"
            : > "$OUT/.warned"
            return 0
        fi
        echo "FAIL   $name: compilation (code $rc), messages in $OUT/$name.build.txt"
        head -20 "$OUT/$name.build.txt"
        exit 1
    fi
}

# run NAME [capture]: appends one run to $OUT/NAME.actual.txt
run() {
    local name=$1
    local log=${2:-}
    local rc
    if [ -n "$log" ]; then
        if [ ! -f "$LOGS/$log" ]; then
            echo "FAIL   $name: no capture $LOGS/$log"
            exit 1
        fi
        echo "### $name $log" >> "$OUT/$name.actual.txt"
        "$OUT/$name.exe" "$LOGS/$log" < /dev/null 2>&1 | tr -d '\r' >> "$OUT/$name.actual.txt"
        rc=${PIPESTATUS[0]}
    else
        echo "### $name" >> "$OUT/$name.actual.txt"
        "$OUT/$name.exe" < /dev/null 2>&1 | tr -d '\r' >> "$OUT/$name.actual.txt"
        rc=${PIPESTATUS[0]}
    fi
    echo "### code=$rc" >> "$OUT/$name.actual.txt"
    if [ "$rc" -ne 0 ]; then BAD_RC="$BAD_RC $name"; fi
}

# run_file NAME PATH: like run, but with a file given by a path relative to the firmware directory
# (e.g. a fixture); header "### NAME PATH"
run_file() {
    local name=$1
    local path=$2
    local rc
    if [ ! -f "$path" ]; then
        echo "FAIL   $name: no file $path"
        exit 1
    fi
    echo "### $name $path" >> "$OUT/$name.actual.txt"
    "$OUT/$name.exe" "$path" < /dev/null 2>&1 | tr -d '\r' >> "$OUT/$name.actual.txt"
    rc=${PIPESTATUS[0]}
    echo "### code=$rc" >> "$OUT/$name.actual.txt"
    if [ "$rc" -ne 0 ]; then BAD_RC="$BAD_RC $name"; fi
}

# finish NAME: compare with the reference or record the reference
finish() {
    local name=$1
    if [ $REC -eq 1 ]; then
        cp "$OUT/$name.actual.txt" "$EXP/$name.txt"
        echo "RECORDED $name ($(wc -l < "$EXP/$name.txt") lines)"
        return
    fi
    if [ ! -f "$EXP/$name.txt" ]; then
        echo "FAIL   $name: no reference $EXP/$name.txt"
        exit 1
    fi
    if ! diff -u "$EXP/$name.txt" "$OUT/$name.actual.txt" > "$OUT/$name.diff.txt"; then
        echo "DIFF   $name: reference $EXP/$name.txt, result $OUT/$name.actual.txt"
        head -40 "$OUT/$name.diff.txt"
        exit 1
    fi
    rm -f "$OUT/$name.diff.txt"
    echo "OK     $name"
}

# test NAME "lib/core files" [capture...]: without a capture, one run without arguments
test_one() {
    local name=$1
    local libs=$2
    shift 2
    # shellcheck disable=SC2086
    build "$name" $libs
    rm -f "$OUT/$name.actual.txt"
    if [ $# -eq 0 ]; then
        run "$name"
    else
        local log
        for log in "$@"; do run "$name" "$log"; done
    fi
    finish "$name"
}

BAD_RC=""

# Eight tests of the 0.6.0 state (file lists as in the firmware README of 0.6.0).
test_one conformance "psa_frames.cpp candump.cpp xp_protocol.cpp xp_keys.cpp xp_parking.cpp radio_session.cpp power_policy.cpp xp_car.cpp psa_signals.cpp xp_signals.cpp vehicle_settings.cpp profile_psa_aee2010.cpp tx_allowlist.cpp protocol_xp.cpp radio_commands.cpp" \
    can_20261111_190817.log can_20261111_191045.log
test_one replay_log "candump.cpp parking_beeper.cpp psa_frames.cpp" \
    can_20261111_191045.log centralka_can_20261117_184411.log
test_one beeper_scale "parking_beeper.cpp psa_frames.cpp"
test_one power_loss "power_policy.cpp psa_frames.cpp candump.cpp xp_car.cpp psa_signals.cpp xp_signals.cpp vehicle_settings.cpp xp_protocol.cpp" \
    can_20261111_191045.log
test_one custom_keys_doors "xp_keys_custom.cpp radio_commands.cpp xp_car.cpp psa_signals.cpp xp_signals.cpp vehicle_settings.cpp candump.cpp xp_protocol.cpp" \
    can_20261110_190421.log
test_one net_policy "net_policy.cpp"
test_one net_rings "line_log.cpp frame_trace.cpp"
# net_util: the FwIdent block of the 0.6.0 prototype image (old marker, must be rejected) from test_data/fixtures/ (tools/make_fw_fixture.py) instead of the full image.
build net_util web_auth.cpp json_writer.cpp fw_ident.cpp
rm -f "$OUT/net_util.actual.txt"
run_file net_util "$FIXTURES/fw_ident_060.bin"
finish net_util
# settings_backup: panel language codes stored in the device and the settings export / import text.
test_one settings_backup "ui_lang.cpp settings_backup.cpp json_writer.cpp"

# Basic tests: car state from CAN frames and XP protocol frames.
# car_state compares CarModel with the existing encoders (0F6, doors) on two captures, frame by frame.
build car_state car_state.cpp psa_frames.cpp xp_car.cpp psa_signals.cpp xp_signals.cpp vehicle_settings.cpp candump.cpp xp_protocol.cpp
rm -f "$OUT/car_state.actual.txt"
run car_state can_20261110_190421.log
run car_state centralka_can_20261117_184411.log
finish car_state
test_one xp_codec "xp_protocol.cpp"

# Frame 38 DATA[1..5] and frame 3C from CAN: vectors and the Decoder class (without
# arguments), then three CAN + UART captures from 11.11.2026 in ONE run (total: 38 of 38 and 9 of 9).
S38_LIBS="vehicle_settings.cpp radio_commands.cpp can_tx_gate.cpp settings_changer.cpp car_state.cpp decoder.cpp candump.cpp psa_frames.cpp xp_protocol.cpp xp_keys.cpp xp_keys_custom.cpp xp_parking.cpp xp_car.cpp psa_signals.cpp xp_signals.cpp radio_session.cpp power_policy.cpp parking_beeper.cpp xp_settings.cpp profile_psa_aee2010.cpp tx_allowlist.cpp protocol_xp.cpp profile_generic.cpp park_diag.cpp"
S38_LOGS="can_20261111_190551.log can_20261111_190817.log can_20261111_191045.log"
# shellcheck disable=SC2086
build settings_38 $S38_LIBS
rm -f "$OUT/settings_38.actual.txt"
run settings_38
S38_ARGS=""
for log in $S38_LOGS; do
    if [ ! -f "$LOGS/$log" ]; then
        echo "FAIL   settings_38: no capture $LOGS/$log"
        exit 1
    fi
    S38_ARGS="$S38_ARGS $LOGS/$log"
done
echo "### settings_38 $S38_LOGS" >> "$OUT/settings_38.actual.txt"
# shellcheck disable=SC2086
"$OUT/settings_38.exe" $S38_ARGS < /dev/null 2>&1 | tr -d '\r' >> "$OUT/settings_38.actual.txt"
rc=${PIPESTATUS[0]}
echo "### code=$rc" >> "$OUT/settings_38.actual.txt"
if [ "$rc" -ne 0 ]; then BAD_RC="$BAD_RC settings_38"; fi
finish settings_38

# Radio commands and the pure logic of frame 15B. Nothing is transmitted: the tests
# only compute bytes. Both use the Decoder class (console lines, command "vset"), hence the full file list.
test_one radio_commands "$S38_LIBS"
test_one settings_15b "$S38_LIBS"

# Radio link and main-loop decisions: the behaviour of RadioLink
# and Decoder::nextRadioTx recorded as a reference (ACC gate, slots, angle every 150 ms, 32 every 1 s,
# 90 71, 10 ms gap, counters) and the deferral of settings writes while the car needs the device.
test_one radio_link "$S38_LIBS"
test_one loop_policy "loop_policy.cpp"

# CAN transmit gate and vehicle setting change. LOGIC ONLY:
# the transmit hardware is replaced by a stub in the test; the device program does not contain it (tools/check_no_can_tx.sh).
# settings_change without arguments: cases with the stub; with capture _191045: radio commands 80 01 01 / 00
# from the recording and a comparison of the 15B frame with the one the Simple Soft box sent at that time.
# The gate takes the transmit allow list of the car profile. The trial
# frame 1A9 is in the PSA AEE2010 list only with -DCAN_TX_EXPERIMENTAL=1. Tests that exercise it
# (can_tx_gate, settings_change, tx_path) are built with =1 and must give the references recorded before the profile allow lists;
# can_tx_gate and tx_path are also built in the default variant (=0) against their own references (*_default).
TX_LIBS="can_tx_gate.cpp profile_psa_aee2010.cpp tx_allowlist.cpp psa_frames.cpp psa_signals.cpp vehicle_settings.cpp profile_generic.cpp"
EXP1="-DCAN_TX_EXPERIMENTAL=1"
DEFS="$EXP1" test_one can_tx_gate "$TX_LIBS"
DEFS="-DCAN_TX_EXPERIMENTAL=0" SRC=can_tx_gate test_one can_tx_gate_default "$TX_LIBS"
# shellcheck disable=SC2086
DEFS="$EXP1" build settings_change $S38_LIBS
rm -f "$OUT/settings_change.actual.txt"
run settings_change
run settings_change can_20261111_191045.log
finish settings_change

# Transmitting frame 15B, the part without hardware: arming tied to the build string,
# parsing of the commands "cantx" / "vset on|off" / "set" and who may issue them, the item mask (can_tx_arm), and
# the whole path radio command -> Decoder -> gate -> port with a hardware stub and a BSI stub (tx_path).
# The hardware itself (src/can_tx/can_tx_port.cpp) has no test on the PC: it is checked on the bench.
test_one can_tx_arm "can_tx_arm.cpp vehicle_settings.cpp car_state.cpp xp_protocol.cpp psa_frames.cpp"
DEFS="$EXP1" test_one tx_path "$S38_LIBS"
DEFS="-DCAN_TX_EXPERIMENTAL=0" SRC=tx_path test_one tx_path_default "$S38_LIBS"
# The old (Polish) console command name "vset" as an alias of "vset".
test_one console_alias "can_tx_arm.cpp vehicle_settings.cpp car_state.cpp xp_protocol.cpp psa_frames.cpp"

# Car profiles and radio protocols. key_tables: the old button table
# LEVEL_KEYS = car profile key map + XP key codes, row by row. radio_none: profile generic + radio none on all
# five captures (zero bytes for the radio, 12 V outputs off). example_car: the template profile lib/car_example
# on a synthetic capture (invented frames); lib/car_example is compiled only here, never into the firmware.
test_one key_tables "profile_psa_aee2010.cpp tx_allowlist.cpp protocol_xp.cpp xp_signals.cpp xp_protocol.cpp xp_parking.cpp radio_commands.cpp profile_generic.cpp protocol_none.cpp psa_frames.cpp psa_signals.cpp vehicle_settings.cpp"
# shellcheck disable=SC2086
test_one radio_none "$S38_LIBS protocol_none.cpp" \
    can_20261111_190551.log can_20261111_190817.log can_20261111_191045.log can_20261110_190421.log \
    centralka_can_20261117_184411.log
# Transmit allow lists of the profiles: one reference with both variants (=0, then =1).
PT_LIBS="can_tx_gate.cpp profile_psa_aee2010.cpp tx_allowlist.cpp psa_frames.cpp psa_signals.cpp vehicle_settings.cpp profile_generic.cpp profile_example.cpp"
rm -f "$OUT/profile_tables.actual.txt"
for v in 0 1; do
    # shellcheck disable=SC2086
    DEFS="-DCAN_TX_EXPERIMENTAL=$v" SRC=profile_tables build profile_tables_$v $PT_LIBS
    echo "### profile_tables CAN_TX_EXPERIMENTAL=$v" >> "$OUT/profile_tables.actual.txt"
    "$OUT/profile_tables_$v.exe" < /dev/null 2>&1 | tr -d '\r' >> "$OUT/profile_tables.actual.txt"
    rc=${PIPESTATUS[0]}
    echo "### code=$rc" >> "$OUT/profile_tables.actual.txt"
    if [ "$rc" -ne 0 ]; then BAD_RC="$BAD_RC profile_tables"; fi
done
# Third variant: the release frame after a 1A9 press (-DCAN_TX_1A9_RELEASE=1, only together with EXPERIMENTAL=1).
# shellcheck disable=SC2086
DEFS="-DCAN_TX_EXPERIMENTAL=1 -DCAN_TX_1A9_RELEASE=1" SRC=profile_tables build profile_tables_rel $PT_LIBS
echo "### profile_tables CAN_TX_EXPERIMENTAL=1 CAN_TX_1A9_RELEASE=1" >> "$OUT/profile_tables.actual.txt"
"$OUT/profile_tables_rel.exe" < /dev/null 2>&1 | tr -d '\r' >> "$OUT/profile_tables.actual.txt"
rc=${PIPESTATUS[0]}
echo "### code=$rc" >> "$OUT/profile_tables.actual.txt"
if [ "$rc" -ne 0 ]; then BAD_RC="$BAD_RC profile_tables"; fi
finish profile_tables
# 1A9 release after a press (settings_changer.cpp), built with CAN_TX_1A9_RELEASE=0 and =1 (both with the trial
# frame 1A9); one reference with both outputs. With 0 no release is ever queued and the gate refuses its content.
rm -f "$OUT/button_release.actual.txt"
for v in 0 1; do
    # shellcheck disable=SC2086
    DEFS="$EXP1 -DCAN_TX_1A9_RELEASE=$v" SRC=button_release build button_release_$v $S38_LIBS
    echo "### button_release CAN_TX_1A9_RELEASE=$v" >> "$OUT/button_release.actual.txt"
    "$OUT/button_release_$v.exe" < /dev/null 2>&1 | tr -d '\r' >> "$OUT/button_release.actual.txt"
    rc=${PIPESTATUS[0]}
    echo "### code=$rc" >> "$OUT/button_release.actual.txt"
    if [ "$rc" -ne 0 ]; then BAD_RC="$BAD_RC button_release"; fi
done
finish button_release
# Overnight parking sensors record (park_diag.h): stability, comparison after a wake-up, counters.
test_one park_diag "park_diag.cpp car_state.cpp vehicle_settings.cpp psa_frames.cpp"
# shellcheck disable=SC2086
build example_car $S38_LIBS profile_example.cpp
rm -f "$OUT/example_car.actual.txt"
run_file example_car test_host/synthetic/example_car.log
finish example_car
# radio_stub: a stub radio protocol (test_host/radio_stub.cpp) whose encoders print the car-neutral data they get
# (doors, trip computer, climate, settings ...): once from the template profile on the synthetic capture, once
# from the PSA AEE2010 profile on a recording from the car. Proves that a foreign radio protocol is served
# through RadioProtocol alone.
# shellcheck disable=SC2086
build radio_stub $S38_LIBS profile_example.cpp
rm -f "$OUT/radio_stub.actual.txt"
if [ ! -f "$LOGS/can_20261111_191045.log" ]; then
    echo "FAIL   radio_stub: no capture $LOGS/can_20261111_191045.log"
    exit 1
fi
echo "### radio_stub test_host/synthetic/example_car.log can_20261111_191045.log" >> "$OUT/radio_stub.actual.txt"
"$OUT/radio_stub.exe" test_host/synthetic/example_car.log "$LOGS/can_20261111_191045.log" < /dev/null 2>&1 |
    tr -d '\r' >> "$OUT/radio_stub.actual.txt"
rc=${PIPESTATUS[0]}
echo "### code=$rc" >> "$OUT/radio_stub.actual.txt"
if [ "$rc" -ne 0 ]; then BAD_RC="$BAD_RC radio_stub"; fi
finish radio_stub
# radio_example: the template radio protocol lib/radio_example (invented framing 55 AA LEN CMD DATA SUM, compiled
# only here, never into the firmware) through the unchanged Decoder: every transmission is one frame of its own
# framing, no XP frame and no 2E header anywhere in the stream; frames from the radio go through its parser and
# never become a setting command; latch key and rate limit come from the protocol table.
# shellcheck disable=SC2086
build radio_example $S38_LIBS profile_example.cpp protocol_example.cpp
rm -f "$OUT/radio_example.actual.txt"
for log in can_20261111_191045.log can_20261110_190421.log; do
    if [ ! -f "$LOGS/$log" ]; then
        echo "FAIL   radio_example: no capture $LOGS/$log"
        exit 1
    fi
done
echo "### radio_example test_host/synthetic/example_car.log can_20261111_191045.log can_20261110_190421.log" >> "$OUT/radio_example.actual.txt"
"$OUT/radio_example.exe" test_host/synthetic/example_car.log "$LOGS/can_20261111_191045.log" "$LOGS/can_20261110_190421.log" \
    < /dev/null 2>&1 | tr -d '\r' >> "$OUT/radio_example.actual.txt"
rc=${PIPESTATUS[0]}
echo "### code=$rc" >> "$OUT/radio_example.actual.txt"
if [ "$rc" -ne 0 ]; then BAD_RC="$BAD_RC radio_example"; fi
finish radio_example

# ---------- reference streams to the radio (golden) ----------
# golden_stream replays a capture step by step (1 ms) and writes three files per capture and variant:
# .radio_tx.txt, .outputs.txt, .console.txt. Three passes of 9 runs each (5 captures + 4 variants):
#   1. reference 0.6.0 (test_host/ref_glue_060.h)            -> must give test_host/golden_060/ (references from the 0.6.0 code)
#   2. Decoder --without 38,3C (like 0.7.0: constant 38, no 3C) -> must give test_host/golden_060/ byte for byte:
#                                                               proof that computing 38 / 3C from CAN changed only frames 38 and 3C
#   3. Decoder (what the device does)                        -> must give test_host/golden/
# test_host/golden_060/ holds references recorded on the 0.6.0 code: they are NOT recorded again.
# --record-golden records test_host/golden/ from pass 3 (earlier from pass 1).
# The compatibility report against the box ($OUT/golden_box_report*.txt) is informational.
GOLD=test_host/golden
GOLD060=test_host/golden_060
GREF=$OUT/golden_ref
GNO38=$OUT/golden_decoder_no38
GDECODER=$OUT/golden_decoder
REPORT=$OUT/golden_box_report.txt

# stream_run NAME DIR "header" "program parameters" capture.log [variant parameters...]
# Appends one run to $OUT/NAME.actual.txt. The header of passes 1 and 2 is the same
# (without --decoder), so that both summaries can be compared with test_host/expected/golden_stream.txt.
stream_run() {
    local name=$1 dir=$2 hdr=$3 extra=$4 log=$5
    shift 5
    if [ ! -f "$LOGS/$log" ]; then
        echo "FAIL   golden_stream: no capture $LOGS/$log"
        exit 1
    fi
    echo "### golden_stream $hdr$log $*" >> "$OUT/$name.actual.txt"
    # shellcheck disable=SC2086
    "$OUT/golden_stream.exe" "$LOGS/$log" "$dir" $extra "$@" < /dev/null 2>&1 | tr -d '\r' >> "$OUT/$name.actual.txt"
    local rc=${PIPESTATUS[0]}
    echo "### code=$rc" >> "$OUT/$name.actual.txt"
    if [ "$rc" -ne 0 ]; then BAD_RC="$BAD_RC $name"; fi
}

# stream_pass NAME DIR "header" "program parameters": 9 runs, report to $REPORT
stream_pass() {
    local name=$1 dir=$2 hdr=$3 extra=$4
    rm -rf "$dir"
    mkdir -p "$dir"
    rm -f "$OUT/$name.actual.txt" "$REPORT"
    # Five captures; the first three have UART lines, so they get a report.
    stream_run "$name" "$dir" "$hdr" "$extra" can_20261111_190551.log --box-report "$REPORT"
    stream_run "$name" "$dir" "$hdr" "$extra" can_20261111_190817.log --box-report "$REPORT"
    stream_run "$name" "$dir" "$hdr" "$extra" can_20261111_191045.log --box-report "$REPORT"
    stream_run "$name" "$dir" "$hdr" "$extra" can_20261110_190421.log
    stream_run "$name" "$dir" "$hdr" "$extra" centralka_can_20261117_184411.log
    # Four variants on capture _191045: loop stall and lost frames.
    stream_run "$name" "$dir" "$hdr" "$extra" can_20261111_191045.log --stall 10,50
    stream_run "$name" "$dir" "$hdr" "$extra" can_20261111_191045.log --stall 10,500
    stream_run "$name" "$dir" "$hdr" "$extra" can_20261111_191045.log --drop 0E1
    stream_run "$name" "$dir" "$hdr" "$extra" can_20261111_191045.log --drop 0F6
}

# same_dir REFERENCES RESULT "description": directories identical, or stop
same_dir() {
    if ! diff -rq "$1" "$2" > "$OUT/golden.diff.txt"; then
        echo "DIFF   $3: references $1, result $2"
        head -40 "$OUT/golden.diff.txt"
        exit 1
    fi
    rm -f "$OUT/golden.diff.txt"
}

build golden_stream candump.cpp psa_frames.cpp xp_protocol.cpp xp_keys.cpp xp_keys_custom.cpp xp_parking.cpp \
    xp_car.cpp psa_signals.cpp xp_signals.cpp radio_session.cpp power_policy.cpp parking_beeper.cpp car_state.cpp vehicle_settings.cpp radio_commands.cpp \
    can_tx_gate.cpp settings_changer.cpp park_diag.cpp decoder.cpp xp_settings.cpp profile_psa_aee2010.cpp tx_allowlist.cpp protocol_xp.cpp \
    profile_generic.cpp
if [ ! -d "$GOLD060" ]; then
    echo "FAIL   golden_stream: no reference directory $GOLD060 (references from the 0.6.0 code, stage E1)"
    exit 1
fi

# --- pass 1: reference 0.6.0 ---
stream_pass golden_stream "$GREF" "" ""
mv "$REPORT" "$OUT/golden_box_report_ref.txt"
same_dir "$GOLD060" "$GREF" "golden_stream (reference 0.6.0)"
echo "OK     golden 0.6.0 ($(ls "$GOLD060" | wc -l) files)"
finish golden_stream

# --- pass 2: Decoder without frames 38 / 3C from CAN ---
stream_pass golden_stream_no38 "$GNO38" "" "--decoder --without 38,3C"
mv "$REPORT" "$OUT/golden_box_report_no38.txt"
same_dir "$GOLD060" "$GNO38" "golden_stream --decoder --without 38,3C"
if ! diff -u "$EXP/golden_stream.txt" "$OUT/golden_stream_no38.actual.txt" > "$OUT/golden_stream_no38.diff.txt"; then
    echo "DIFF   golden_stream --decoder --without 38,3C: summary, reference $EXP/golden_stream.txt"
    head -40 "$OUT/golden_stream_no38.diff.txt"
    exit 1
fi
rm -f "$OUT/golden_stream_no38.diff.txt"
if ! cmp -s "$OUT/golden_box_report_ref.txt" "$OUT/golden_box_report_no38.txt"; then
    echo "DIFF   golden_stream --decoder --without 38,3C: box report differs from the reference pass"
    exit 1
fi
echo "OK     golden --decoder --without 38,3C ($(ls "$GNO38" | wc -l) files, identical to the 0.6.0 references)"

# --- pass 3: Decoder ---
stream_pass golden_stream_decoder "$GDECODER" "--decoder " "--decoder"
cp "$REPORT" "$OUT/golden_box_report_decoder.txt"     # $REPORT stays: report of the current program
if [ $REC -eq 1 ]; then
    rm -rf "$GOLD"
    mkdir -p "$GOLD"
    cp "$GDECODER"/*.txt "$GOLD"/
    echo "RECORDED golden: $(ls "$GOLD" | wc -l) files, $(cat "$GOLD"/*.txt | wc -c) B"
else
    if [ ! -d "$GOLD" ]; then
        echo "FAIL   golden_stream: no reference directory $GOLD"
        exit 1
    fi
    same_dir "$GOLD" "$GDECODER" "golden_stream --decoder"
    echo "OK     golden --decoder ($(ls "$GOLD" | wc -l) files)"
fi
finish golden_stream_decoder

if [ $REC -eq 1 ]; then
    if [ -n "$BAD_RC" ]; then
        echo "WARNING: references recorded, but these tests ended with code != 0:$BAD_RC"
        exit 3
    fi
    echo "References recorded in $EXP/"
    exit 0
fi
if [ -e "$OUT/.warned" ]; then
    echo "FAIL   compiler warnings (listed above as WARN)"
    exit 1
fi
echo "All tests match the references."
exit 0
