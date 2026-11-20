# CAN captures used by the host tests

Seven recordings from the reference car (Peugeot 508 SW, PSA AEE2010, INFO-DIV bus at 125 kbit/s,
listen-only). `tools/run_host_tests.sh` reads the first five from this directory (override with
`LOGS=...`); the two files of 2026-11-19 are kept as the evidence named in
[radios/xp-simple-soft/provenance.md](../../../radios/xp-simple-soft/provenance.md) and are not read by
the host tests.

## Format

One event per line, candump `-L` style:

```
(13.497421) can0 0F6#805C1E23225D5C20     CAN frame: time in s, interface, ID#DATA
(12.346900) uartA FD04021117              bytes seen on one of the two UART lines between the CAN box and the radio
# KROK 3 19:06:41.149 ...                 comment: a step of the recording script (Polish, kept as recorded)
# stat rx=... pc=...                      comment: recorder statistics and the PC clock
```

Lines starting with `#` are ignored by the frame parser (`lib/core/candump.cpp`); `replay_log`
reads the `# KROK` lines as step markers.

The recordings of 2026-11-19 were made with the console recorder of this firmware and use two more
line forms:

```
(1002.872) [od radia] 8A 01 01            console file: a radio frame accepted by the UART parser (command and data
                                          bytes, without the 2E header, the length and the checksum)
# ZNACZNIK 41 (1001.939) ...: auto        owner marker, typed BEFORE the action it names (Polish, kept as recorded);
                                          the time in brackets is the device time, the same clock as the frames
```

## Anonymisation

The files were processed with `../../../tools/sanitize_capture.py`:

- data of frames `336`, `3B6` and `2B6` (they carry the VIN) overwritten with zeros of the same
  length; the lines, timestamps, IDs and DLC are unchanged;
- the header line naming the serial port of the recording PC replaced with `# capture`.

The two files of 2026-11-19 were also cleaned before they were added: the line naming the Wi-Fi network
removed, 12 `# www>` lines (commands typed in the web panel; the device answers stayed) removed, and the
owner's remarks in markers 29 and 64 replaced with `[uwaga właściciela usunięta]` ("owner remark
removed"). The VIN frames were masked on the device (`# zamaskowano` lines).

Nothing else was changed. The host test results (`test_host/expected`, `test_host/golden`,
`test_host/golden_060`) are identical with the original and the anonymised files.
`python -I ../../../tools/sanitize_capture.py --check *.log` must report 0 problems.

## Files

| File | Recorded with | Scenario | Used by |
|---|---|---|---|
| `can_20261110_190421.log` | sniffer (CAN only), aftermarket CAN box (Simple Soft) connected | long scripted session: ignition, engine, steering wheel buttons, lights, doors, climate from the car panel and from the radio, parking sensors front/rear, parking tone tests, sleep | `custom_keys_doors`, `car_state`, golden |
| `can_20261111_190551.log` | recorder (CAN + both UART lines) | short: ignition off -> on, volume + three times | `settings_38`, golden |
| `can_20261111_190817.log` | recorder (CAN + UART) | steering wheel buttons, trip computer button, doors, boot | `conformance`, `settings_38`, golden |
| `can_20261111_191045.log` | recorder (CAN + UART) | doors, lights, engine, reverse, parking sensors, steering, climate, radio screens, vehicle settings from the radio, centre panel buttons, engine off | `conformance`, `replay_log`, `power_loss`, `settings_38`, `settings_change`, golden (+ 4 variants) |
| `centralka_can_20261117_184411.log` | this firmware 0.6.0 in decoder mode, no aftermarket box (VIN frames masked on the device: `# zamaskowano` lines) | CAN only, about 10 s of frames (140.5-148.3 s and 300.2-302.5 s), parking sensors inactive; one `# zgubiono 26593 ramek` gap in between | `replay_log`, `car_state`, golden |
| `centralka_konsola_20261119_150048.txt` | the prototype firmware 1.0.0, console of the device (radio frames received on the UART: `[od radia]` lines), CAN transmit disarmed | owner markers `# ZNACZNIK`: phone, radio tuner, climate panel of the car (markers 12-28), climate screen of the radio (33-46), sound screen of the radio (49-61), vehicle settings screen (pass without markers 1223-1263 s after marker 64, marker 65), trip computer and other screens (1265-1306 s), doors | provenance only (not read by the host tests) |
| `centralka_can_20261119_150048.log` | the prototype firmware 1.0.0, CAN only (VIN frames masked on the device), CAN transmit disarmed: no `15B`, `1A9` or `364` in the file | the same markers, 395-1388 s, engine running | provenance only (not read by the host tests) |

The file name prefix and the header line of `centralka_can_20261117_184411.log` come from the
working name of the prototype firmware, before the project was named; the file keeps its name because
the test references name it. The files of 2026-11-19 keep the names under which provenance.md cites
them; `konsola` is Polish for "console".
