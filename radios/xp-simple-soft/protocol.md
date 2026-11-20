# "XP" protocol: aftermarket CAN box <-> Android radio (FYT platform)

The protocol spoken between a Simple Soft CAN box (RP5-PA-001, the radio calls the family "XP")
and an Android head unit on the FYT platform, set to the CAN profile "XP Simple Soft -> Peugeot
508 -> Type 2". This page contains **only what was recorded** on the reference car: both UART
lines and the car CAN bus on one clock (`extras/sniffer`, environment `recorder`; recordings
`can_20261111_190817.log` and `can_20261111_191045.log` in `firmware/test_data/captures`).
Commands that were never seen on the wire are not described here.

Byte notation: DATA[0] = first data byte. CAN bytes start at 1.

## Transport (measured)

- UART 38400 8N1, 3.3 V logic. Box -> radio and radio -> box on separate wires (see
  [vehicles/peugeot-508-sw-2015/wiring.md](../../vehicles/peugeot-508-sw-2015/wiring.md)).
- Frame: `2E CMD LEN DATA[LEN] CHK`, with `CHK = ~(CMD + LEN + sum of DATA) & 0xFF`.
  Example: `2E 29 02 1C FF B9`: 0x29 + 0x02 + 0x1C + 0xFF = 0x146 -> 0x46 -> NOT = 0xB9.
- After every frame the other side sends one acknowledge byte: the radio `FF`, the box `F3`
  (`F0` once at start-up).
- Commands `01`-`7F` go from the box to the radio; `80`-`FF` from the radio to the box
  (exception: `EA` from the box).
- Box delay: 5-25 ms from the CAN change to the frame for the radio; 4-10 ms from a radio command
  to the CAN frame.
- At power-up the box prints ASCII text on its line (version, build date), then frames start.
  A receiver must skip bytes outside frames.

## Box -> radio (measured)

| CMD | Meaning | Content as recorded |
|---|---|---|
| `20` | key | DATA[0] = key code (table below), DATA[1] = 1 pressed, 2 held (repeats), 0 released. Wheel detents send only 1 |
| `21` | climate state | 9 bytes. DATA[2], DATA[3] = left / right temperature in 0.5 °C steps (`2D` = 22.5 °C; CAN `350` value + 0x23). DATA[1] bits 3-0 = fan level. Mode bits in DATA[0], DATA[1], DATA[8] change only with actions on the car panel. Example: `C4 E0 2D 2D 00 81 40 00 E0` |
| `29` | steering angle | DATA[0] = CAN `10B` byte 2, DATA[1] = byte 1 (155 of 155 frames) |
| `32` | parking sensors | DATA[0] = state (`01`, `02`; `00` with DATA[1..6] = `FF` when inactive; `03` + `FF` seen too); DATA[1..3] rear left / centre / right, DATA[4..6] front left / centre / right; `05` far, `00` nearest. From CAN `0E1` zones: 0->0, 1->1, 2->1, 3->2, 4->2, 5->3, 6->3, 7->5 (394 of 396 frames). About once a second plus on change |
| `33` | trip computer, current page | DATA[0..5] = CAN `221` bytes 2-7; DATA[6..8] = 0 in all frames |
| `34` | trip 1 | from CAN `2A1`: bytes 4, 5, `00`, 1, 2, 3 |
| `35` | trip 2 | from CAN `261`, same layout |
| `36` | outside temperature | bit 7 = minus, bits 6-0 = °C; from CAN `0F6` byte 7 (value / 2 - 40, halves rounded up; 21 of 21) |
| `38` | doors and vehicle settings | 6 bytes; see [settings-map.md](../../vehicles/peugeot-508-sw-2015/settings-map.md). Sent on change and after each radio `81 01` |
| `3C` | which setting rows to show | 2 bytes; one value recorded: `9E 80` |
| `3B` | stored speeds (limiter / cruise control) | `FC 28 3B 4F 63 82 01` = 40, 59, 79, 99, 130 km/h; the same values are in CAN `1E8` |
| `3D` | speed list | `32 46 50 5A 6E 82` = 50 ... 130 km/h, twice; same as CAN `2A7` / `267` |
| `3E` | obstacle | `80` while the sensors are active and a zone is below 7, else `00` (19 of 19) |
| `44` | economy mode | `01` with ignition on and engine off, `00` with the engine running; from CAN `036` byte 3 bit 7 |
| `71` | protocol version | `0A 05 16 10 20 56 02 19 06`; the radio shows it as "XP_PSA_2016_10_20 V2256" |
| `30` | box version text | ASCII |
| `EA` | power | `01 0A` within 30 ms after the ignition leaves `80` in CAN `0F6` byte 1, `00 0A` 2.5 s after it returns to `80` |
| `01`, `37`, `39` | not decoded | `01`: `00 00 00`, `00 00 10` after engine start; `37`, `39`: change with some car messages in CAN `1A1` |

After the ignition is switched on the box sends, four times about 1 s apart: `38`, `EA 01 0A`,
`71`, `01`, `3B`, `3C`, `30`, `3E 00`, `36`, `33`, `34`, `35`, `21`, `3D`, `44 01`.

### Key codes (command `20`, measured)

| Code | Button | CAN source |
|---|---|---|
| `14` | wheel: volume + | `21F` byte 1 = `08` |
| `15` | wheel: volume - | `21F` byte 1 = `04` |
| `16` | wheel: both volume buttons = mute; also panel power knob pressed | `21F` byte 1 = `0C`; `036` byte 7 = `02` |
| `17` / `18` | wheel: next / previous | `21F` byte 2 counter down / up |
| `21` | wheel: SRC | `0A2` byte 2 = `04` |
| `50` | wheel: LIST | `21F` byte 1 = `01` |
| `07` | wheel: scroll wheel pressed | `21F` byte 3 = `80` |
| `30` | wheel: phone (held: repeats `30 02`) | `21F` byte 1 = `02` |
| `23` | wiper stalk: trip computer | `221` byte 1 bit 0 |
| `22` | a panel button, not identified | `122` byte 1 = `02` |
| `81` / `82` | panel volume knob right / left | `122` byte 7 counter up / down |
| `9E` | panel: NAV | `122` byte 2 = `80` |
| `A0` | panel: MEDIA | `122` byte 2 = `20` |
| `97` | panel: seek left | `122` byte 2 = `01` |
| `98` | panel: seek right | `122` byte 3 = `80` |
| `9C` | panel: zoom + | `122` byte 3 = `40` |
| `9D` | panel: zoom - | `122` byte 3 = `20` |
| `9F` | panel: TEL | `122` byte 2 = `08` |
| `99` | panel: SETUP | `122` byte 2 = `02` |
| `9B` | panel: DRIVE | `122` byte 2 = `10` |
| `A4` | panel: screen off (moon) | `122` byte 2 = `40` |
| `A1` | panel: microphone mute | `122` byte 2 = `04` |
| `A3` | panel: WEB | `122` byte 1 = `01` |

Not passed to the radio by the box (measured): the left thumb wheel (`0A2` byte 4, counter) and its
press (`0A2` byte 5 = `20`). The box sent the wheel detents to the radio about 220 ms after the CAN frame; another
box (Raise) had no such delay (owner's observation).

## Radio -> box (measured)

| CMD | Meaning | Content as recorded | Effect on CAN |
|---|---|---|---|
| `A6` | date and time | year - 2000, month, day, hour, minute; every second | |
| `81` | radio state | `01` on, `00` switching off; 4 times about 1 s apart | the box answers each `81 01` with `38` + `3C` |
| `90` | request a frame | `90 71 00` with each `81` (version); `90 21 ...` on entering the climate screen; `90 33 00 00 00` on entering the trip computer screen | |
| `C4` | volume | 0-36, `80` = mute | `1A5` = level 4-10 ms later; mute: `165` byte 1 = `E0` |
| `C0` | source / playback | 8 bytes: `01 01 ...` radio, `0C 30 ...` others, `08 13 ...` media player with track data | `165` byte 3: `10` radio, `60` player, `40` others |
| `CA` | profile variant | `00`, at start, 4 times | |
| `CB` | text for the car display | type (`02`, `04`) + 32 bytes, empty in the recordings | |
| `80` | vehicle setting | number, value | `15B` 6 ms later (see settings-map.md) |
| `82` | trip computer command | `82 00 00 00 40 00 00` = "clear" button on the trip screen; `82 00 00 00 00 00 00` on entering the screen | |
| `8A` | climate screen command | `8A nn vv`, one frame per button press: `01` AUTO, `02` A/C, `03` MAX A/C, `04` / `05` left / right temperature (`01` plus, `02` minus), `06` face, `07` windscreen, `08` feet, `09` fan, `0A` fan speed (`01` plus, `02` minus), `0B` MONO, `0C` temperature unit (from the settings screen); recorded with owner markers 2026-11-19 | none: the car did not react, the firmware sends nothing |
| `84` | sound screen command | `84 nn vv`: `06` treble, `05` bass, `03` fader, `04` balance, `09` sound selection, `02` surround, `0A` volume / loudness; value `00` / `01`, meaning not determined; recorded with owner markers 2026-11-19 | none |

The radio sends the **target value** computed from its own state (from frame `38`). With a wrong
`38` the radio keeps sending the same value.

## Not known

- Which panel button sends code `22`.
- Tuner and phone commands in detail, and the value byte of `84`.
- The rule for the remaining bits of `38` and the meaning of `01`, `37`, `39`.
- Which CAN frames the box stops sending when it is removed (no recording without the box).
