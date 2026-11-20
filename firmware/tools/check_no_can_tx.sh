#!/usr/bin/env bash
# Check of CAN transmission in the built programs (after compiling every environment listed below):
#   - environments without transmission (listen, decoder, listen_wifi, decoder_wifi, listen_generic_wifi): firmware.elf MUST NOT contain the functions twai_transmit or
#     twai_transmit_v2 (the linker removes them when nobody calls them);
#   - the transmitting one, decoder_tx_wifi: the function twai_transmit MUST be present (otherwise it is the wrong program);
#   - in the sources (src/, lib/) the word twai_transmit may be called only in src/can_tx/can_tx_port.cpp.
#
#   bash tools/check_no_can_tx.sh      exit code 0 = all three conditions met
#
# Reads only the files .pio/build/<environment>/firmware.elf on disk. Flashes nothing.
# Another nm tool: variable NM (default: the one from the PlatformIO package, with or without .exe,
# so the same script works on Windows, Linux and macOS).
set -u
cd "$(dirname "$0")/.." || exit 2

if [ -z "${NM:-}" ]; then
    NM=$HOME/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32s3-elf-nm
    [ -x "$NM" ] || NM=$NM.exe
fi
if [ ! -x "$NM" ]; then
    echo "FAIL   no nm tool: $NM (build once so PlatformIO installs it, or set NM=<path to xtensa-esp32s3-elf-nm>)"
    exit 2
fi

bad=0
for env in listen decoder listen_wifi decoder_wifi listen_generic_wifi; do
    elf=.pio/build/$env/firmware.elf
    if [ ! -f "$elf" ]; then
        echo "FAIL   $env: no $elf (build first)"
        bad=1
        continue
    fi
    syms=$("$NM" "$elf" 2> /dev/null | tr -d '\r')
    total=$(printf '%s\n' "$syms" | wc -l)
    # Check of the check itself: the symbol list must be complete and contain the TWAI driver (receive).
    if [ "$total" -lt 1000 ] || ! printf '%s\n' "$syms" | grep -q ' twai_receive'; then
        echo "FAIL   $env: symbol list not trustworthy ($total symbols, no twai_receive)"
        bad=1
        continue
    fi
    hits=$(printf '%s\n' "$syms" | grep -c 'twai_transmit')
    if [ "$hits" -ne 0 ]; then
        echo "HIT    $env: $hits"
        printf '%s\n' "$syms" | grep 'twai_transmit'
        bad=1
    else
        echo "OK     $env: twai_transmit 0 hits ($total symbols)"
    fi
done

# The transmitting environment: the program that transmits frame 15B. The symbol must be present.
env=decoder_tx_wifi
elf=.pio/build/$env/firmware.elf
if [ ! -f "$elf" ]; then
    echo "FAIL   $env: no $elf (build first)"
    bad=1
else
    syms=$("$NM" "$elf" 2> /dev/null | tr -d '\r')
    total=$(printf '%s\n' "$syms" | wc -l)
    hits=$(printf '%s\n' "$syms" | grep -c 'twai_transmit')
    if [ "$total" -lt 1000 ] || ! printf '%s\n' "$syms" | grep -q ' twai_receive'; then
        echo "FAIL   $env: symbol list not trustworthy ($total symbols, no twai_receive)"
        bad=1
    elif [ "$hits" -eq 0 ]; then
        echo "FAIL   $env: no twai_transmit (this environment must be able to send frame 15B)"
        bad=1
    else
        echo "OK     $env: twai_transmit present, $hits hits ($total symbols)"
    fi
fi

# Sources: outside src/can_tx/can_tx_port.cpp the word twai_transmit does not occur at all (not even in comments),
# and in that file there is exactly one call (comment lines do not count).
outside=$(grep -rn 'twai_transmit' src lib web 2> /dev/null | tr -d '\r' | grep -v '^src/can_tx/can_tx_port.cpp:')
inside=$(grep -n 'twai_transmit[_v2]*[[:space:]]*(' src/can_tx/can_tx_port.cpp 2> /dev/null | tr -d '\r' |
    grep -v '^[0-9]*:[[:space:]]*//' | grep -c 'twai_transmit')
if [ -n "$outside" ] || [ "$inside" -ne 1 ]; then
    echo "FAIL   sources: twai_transmit calls in src/can_tx/can_tx_port.cpp $inside (must be 1); occurrences outside that file:"
    printf '%s\n' "$outside"
    bad=1
else
    echo "OK     sources: twai_transmit only in src/can_tx/can_tx_port.cpp, one call"
fi

if [ $bad -ne 0 ]; then
    echo "Check FAILED."
    exit 1
fi
echo "No twai_transmit in the five environments without transmit; present in decoder_tx_wifi."
exit 0
