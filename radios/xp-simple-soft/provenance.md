# Provenance of the protocol constants in the firmware

Every constant of the radio protocol and of the vehicle settings that the firmware uses, with the
source it comes from. The table is checked against the code by `tools/check_provenance.py`
(it extracts the constants from `xp_protocol.h`, the key map in
`profile_psa_aee2010.cpp` with the codes and the version frame in `protocol_xp.cpp`, `xp_parking.cpp`,
`radio_commands.*`, `vehicle_settings.*`, `xp_settings.h` and the transmit allow list in
`tx_allowlist.cpp`; a constant
without a row, or with a changed value, fails the check).

## Why this page exists

The project may publish only what it found out itself. Three kinds of sources were used during
development:

1. **Own measurements**: recordings of the car CAN bus and of both UART lines between the
   aftermarket CAN box and the radio (`firmware/test_data/captures`), and the console of this
   device in the car. The session of 2026-11-19 is published in the same directory:
   `centralka_konsola_20261119_150048.txt` (console: every radio frame the device received, with the
   owner markers `# ZNACZNIK n` typed before each action) and `centralka_can_20261119_150048.log`
   (CAN frames of the same session, the same markers; CAN transmit was disarmed, so the file has no
   `15B`, `1A9` or `364`).
2. **Public descriptions**: PSA-RE (github.com/prototux/PSA-RE, Apache-2.0, commit 67f7581e) and
   a few other public projects. Facts about frames are used, no code or tables are copied.
   Projects under GPL-3.0 were read but nothing from them is in this repository.
3. **Analysis of third-party software**: the Android app of the radio (to see which commands it
   understands and how its settings screen is numbered) and the firmware of the Simple Soft
   CAN box (to see how it builds frames). A constant that has no other source is marked
   `source: firmware analysis / needs owner decision`, and such a constant **blocks a public
   release** (the project rule is that only own measurements are published). Since 2026-11-19 no
   constant has this status: 38 were measured in the session of that day and 33 were removed from
   the code (see [Summary](#summary-for-the-owner)).

Status values:

| Status | Meaning |
|---|---|
| **measured** | seen on the wire on the reference car (file named) |
| **design** | a choice of this project, not a fact about the car or the radio |
| **source: firmware analysis / needs owner decision** | no own measurement of this value (none since 2026-11-19; a new one blocks a release) |

## Constants

| Constant (key) | Status | Source |
|---|---|---|
| `xp_protocol HEADER=2E` | measured | every frame on both UART lines, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_protocol ACK_FROM_RADIO=FF` | measured | single byte after every box frame, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_protocol ACK_FROM_BOX=F3` | measured | single byte after every radio frame, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_protocol MAX_DATA=40` | design | buffer size; longest recorded frame is CB with 33 data bytes |
| `xp_protocol UART_BAUD=38400` | measured | valid checksums at 38400 8N1, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_protocol BOX_KEY=20` | measured | 36 of 36 key presses, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_protocol BOX_CLIMATE=21` | measured | 26 of 27 frames match CAN 350 + 2AD, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_protocol BOX_STEERING_ANGLE=29` | measured | 155 of 155 frames match CAN 10B, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_protocol BOX_PARKING=32` | measured | 394 of 396 frames match CAN 0E1, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_protocol BOX_OUTSIDE_TEMP=36` | measured | 21 of 21 frames match CAN 0F6 byte 7, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_protocol BOX_CAR_STATE=38` | measured | 38 frames recorded, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines), can_20261111_190551.log |
| `xp_protocol BOX_ECO_MODE=44` | measured | 10 of 10 frames match CAN 036 byte 3 bit 7, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_protocol BOX_VERSION=71` | measured | sent after radio request 90 71, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_protocol BOX_POWER=EA` | measured | 01 0A / 00 0A with ignition on / off, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_protocol RADIO_SETTING=80` | measured | 80 01 01 / 80 01 00, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines); more numbers in the console log of this device in the car (not published); 37 frames `80 nn vv` 1223.770-1329.446 s in centralka_konsola_20261119_150048.txt (radio screen "Function settings", see the setting rows) |
| `xp_protocol RADIO_STATE=81` | measured | 81 01 / 81 00 at radio start and stop, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_protocol RADIO_AIR=8A` | measured | `8A nn vv` frames 0.7-1.2 s after each owner marker 33-46 naming a button of the radio climate screen, centralka_konsola_20261119_150048.txt (the first measurement, one frame `8A 0B 00`, was in the console log of this device in the car, not published) |
| `xp_protocol RADIO_REQUEST=90` | measured | 90 71 00 with every 81, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_protocol RADIO_TIME=A6` | measured | every second, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_protocol RADIO_SOURCE=C0` | measured | with source changes, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines); `C0 05 40` 2 s after owner marker 4 "dzwonie" (calling, 579.574 s) and before marker 6 "odbieram" (answering, 695.165 s), `C0 0C 30` after markers 5 and 7 "rozlaczam" (hanging up), centralka_konsola_20261119_150048.txt |
| `xp_protocol RADIO_TUNER=C2` | measured | radio frame `2E C2 04 01 A8 25 04` 11 ms after the box sent key `17` (next), and `2E C2 04 01 1C 25 03` after key `18` (previous), radio on FM, valid checksums, can_20261111_190817.log (246.709 s, 249.994 s; step 7); bytes 2-3 change with the station. Used only to recognise and ignore the frame |
| `xp_protocol RADIO_VOLUME=C4` | measured | every volume change, CAN 1A5 follows, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_protocol RADIO_VARIANT=CA` | measured | CA 00 at radio start, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_protocol RADIO_TEXT=CB` | measured | CB 02 / 04 + 32 zero bytes at start, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_protocol BOX_TRIP_NOW=33` | measured | 9 of 9 frames match CAN 221, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_protocol BOX_TRIP_1=34` | measured | 10 of 10 frames match CAN 2A1, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_protocol BOX_TRIP_2=35` | measured | 11 of 11 frames match CAN 261, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_protocol BOX_OBSTACLE=3E` | measured | 19 of 19 frames match CAN 0E1, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 21F/0/0C/08->14` | measured | wheel volume +, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 21F/0/0C/04->15` | measured | wheel volume -, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 21F/0/0C/0C->16` | measured | wheel mute (both), can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 21F/0/02/02->30` | measured | wheel phone, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 21F/0/01/01->50` | measured | wheel LIST, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 21F/2/80/80->07` | measured | wheel scroll press, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 0A2/1/04/04->21` | measured | wheel SRC, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 221/0/01/01->23` | measured | stalk trip button, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 122/1/80/80->9E` | measured | panel NAV, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 122/1/20/20->A0` | measured | panel MEDIA, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 122/1/01/01->97` | measured | panel seek left, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 122/2/40/40->9C` | measured | panel zoom +, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 122/2/80/80->98` | measured | panel seek right, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 122/1/08/08->9F` | measured | panel TEL, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 122/1/02/02->99` | measured | panel SETUP, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 122/1/10/10->9B` | measured | panel DRIVE, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 122/1/40/40->A4` | measured | panel screen off, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 122/2/20/20->9D` | measured | panel zoom -, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 122/1/04/04->A1` | measured | panel mic mute, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 122/0/01/01->A3` | measured | panel WEB, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 122/0/02/02->22` | measured | panel button, not identified (CAN bit and code measured), can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys row 036/6/0F/02->16` | measured | panel power knob press, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys CODE_NEXT=17` | measured | wheel / panel knob detents, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys CODE_PREV=18` | measured | wheel / panel knob detents, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys CODE_KNOB_UP=81` | measured | wheel / panel knob detents, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_keys CODE_KNOB_DOWN=82` | measured | wheel / panel knob detents, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `xp_parking MAP=0,1,1,2,2,3,3,5` | measured | zone mapping, 394 of 396 frames, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `protocol_xp VERSION_DATA=0A 05 16 10 20 56 02 19 06` | measured | frame 71 of the box, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `radio_commands RADIO_CMD_SOUND=84` | measured | radio frames `84 nn vv`, 16 frames 1040.374-1120.619 s, each after an owner marker naming a row of the radio sound screen (markers 49-61), centralka_konsola_20261119_150048.txt. Used only to name the command in the console |
| `radio_commands air 01` | measured | `8A 01 01` at 1002.872 s, 0.9 s after owner marker 41 "auto" on the radio climate screen, centralka_konsola_20261119_150048.txt; the car did not react (`350` and `2AD` unchanged 930-1035 s, centralka_can_20261119_150048.log; CAN transmit disarmed). Used only to name the command in the console |
| `radio_commands air 02` | measured | `8A 02 01` at 1000.685 s, 1.0 s after owner marker 40 "ac" on the radio climate screen, centralka_konsola_20261119_150048.txt; the car did not react (`350` and `2AD` unchanged 930-1035 s, centralka_can_20261119_150048.log; CAN transmit disarmed). Used only to name the command in the console |
| `radio_commands air 03` | measured | `8A 03 01` at 1005.566 s, 0.9 s after owner marker 42 "max ac" on the radio climate screen, centralka_konsola_20261119_150048.txt; the car did not react (`350` and `2AD` unchanged 930-1035 s, centralka_can_20261119_150048.log; CAN transmit disarmed). Used only to name the command in the console |
| `radio_commands air 04` | measured | `8A 04 01` at 958.098 s after owner marker 33 "temperatura lewa +" (left temperature +), `8A 04 02` at 966.191 s after marker 34 "temperatura lewa -", centralka_konsola_20261119_150048.txt; the car did not react (`350` and `2AD` unchanged 930-1035 s, centralka_can_20261119_150048.log; CAN transmit disarmed). Used only to name the command in the console |
| `radio_commands air 05` | measured | `8A 05 01` at 974.731 s after owner marker 35 "temperatura prawa +" (right temperature +), `8A 05 02` at 980.008 s after marker 36, centralka_konsola_20261119_150048.txt; the car did not react (`350` and `2AD` unchanged 930-1035 s, centralka_can_20261119_150048.log; CAN transmit disarmed). Used only to name the command in the console |
| `radio_commands air 06` | measured | `8A 06 00` at 991.541 s, 1.2 s after owner marker 38 "nawiew na twarz" (air to face), centralka_konsola_20261119_150048.txt; the car did not react (`350` and `2AD` unchanged 930-1035 s, centralka_can_20261119_150048.log; CAN transmit disarmed). Used only to name the command in the console |
| `radio_commands air 07` | measured | `8A 07 00` at 996.451 s and 996.674 s after owner marker 39 "nawiew na szybe" (air to windscreen), centralka_konsola_20261119_150048.txt; the car did not react (`350` and `2AD` unchanged 930-1035 s, centralka_can_20261119_150048.log; CAN transmit disarmed). Used only to name the command in the console |
| `radio_commands air 08` | measured | `8A 08 00` 4 times 985.253-986.517 s after owner marker 37 "nawiew na nogi" (air to feet), centralka_konsola_20261119_150048.txt; the car did not react (`350` and `2AD` unchanged 930-1035 s, centralka_can_20261119_150048.log; CAN transmit disarmed). Used only to name the command in the console |
| `radio_commands air 09` | measured | `8A 09 02` at 1012.731 s, 0.9 s after owner marker 44 "wiatrak" (fan button on the radio climate screen), centralka_konsola_20261119_150048.txt; the code name is the screen label "fan"; the car did not react (`350` and `2AD` unchanged 930-1035 s, centralka_can_20261119_150048.log; CAN transmit disarmed). Used only to name the command in the console |
| `radio_commands air 0A` | measured | `8A 0A 02` at 1020.195 s after owner marker 45 "sila nawiewu -" (fan speed -), `8A 0A 01` at 1025.312 s after marker 46 "sila nawiewu +", centralka_konsola_20261119_150048.txt; the car did not react (`350` and `2AD` unchanged 930-1035 s, centralka_can_20261119_150048.log; CAN transmit disarmed). Used only to name the command in the console |
| `radio_commands air 0B` | measured | `8A 0B 00` at 1007.744 s, 0.7 s after owner marker 43 "mono", centralka_konsola_20261119_150048.txt; the car did not react (`350` and `2AD` unchanged 930-1035 s, centralka_can_20261119_150048.log; CAN transmit disarmed). Used only to name the command in the console |
| `radio_commands air 0C` | measured | `8A 0C 01` at 1248.400 s and 1249.805 s in the pass through the radio screen "Function settings" row by row from the top (after owner marker 64, 1223-1263 s), centralka_konsola_20261119_150048.txt; row 17 "Jednostka temperatury" (temperature unit) of that screen (owner photos of 2026-11-19, not published): the 26 different numbers in the order of their first occurrence match the 26 rows one to one. Used only to name the command in the console |
| `radio_commands sound 02` | measured | `84 02 00` at 1107.588 s and 1108.103 s after owner marker 59 "dzwiek przestrzenny" (surround), `84 02 01` at 1113.801 s after marker 60, centralka_konsola_20261119_150048.txt. The meaning of the value byte is not determined |
| `radio_commands sound 03` | measured | `84 03 00` at 1065.525 s after owner marker 53 "tlumienie" (fader), `84 03 01` at 1070.757 s and 1073.646 s after marker 54, centralka_konsola_20261119_150048.txt. The meaning of the value byte is not determined |
| `radio_commands sound 04` | measured | `84 04 00` at 1077.546 s after owner marker 55 "balans", `84 04 01` at 1082.387 s after marker 56, centralka_konsola_20261119_150048.txt. The meaning of the value byte is not determined |
| `radio_commands sound 05` | measured | `84 05 00` at 1054.391 s after owner marker 51 "bas", `84 05 01` at 1057.942 s after marker 52, centralka_konsola_20261119_150048.txt. The meaning of the value byte is not determined |
| `radio_commands sound 06` | measured | `84 06 00` at 1046.355 s after owner marker 49 "wysokie tony" (treble; one more `84 06 00` at 1040.374 s just before the marker), `84 06 01` at 1050.621 s after marker 50, centralka_konsola_20261119_150048.txt. The meaning of the value byte is not determined |
| `radio_commands sound 09` | measured | `84 09 00` at 1097.825 s after owner marker 57 "wybor dzwieku" (sound selection), `84 09 01` at 1101.942 s after marker 58, centralka_konsola_20261119_150048.txt. The meaning of the value byte is not determined |
| `radio_commands sound 0A` | measured | `84 0A 01` at 1120.619 s, 1.2 s after owner marker 61 "glosnosc / loudness", centralka_konsola_20261119_150048.txt. The meaning of the value byte is not determined |
| `xp_protocol BOX_FEATURES=3C` | measured | 3C 9E 80 after every 81 01, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `vehicle_settings ID_BUTTONS=1A9` | measured | screen button frame sent by the aftermarket box, 8 presses with reactions, can_20261110_190421.log |
| `vehicle_settings setting 01` | measured | radio 80 01 01 / 00 -> 15B -> 260 5.7, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) and console log of this device in the car (not published); row 2 "Asystent parkowania" (parking assist) of the radio screen, `80 01 00` at 1223.770 s in centralka_konsola_20261119_150048.txt |
| `vehicle_settings setting 02` | measured | radio 80 02 -> 15B accepted by the BSI both ways, console log of this device in the car (not published); row 3 "Automatyczna tylna wycieraczka przy cofaniu" (rear wiper in reverse) of the radio screen, `80 02 01` at 1228.346 s in centralka_konsola_20261119_150048.txt |
| `vehicle_settings setting 03` | measured | `80 03 01` at 1258.701 s in the pass through the radio screen "Function settings" row by row from the top (after owner marker 64, 1223-1263 s), centralka_konsola_20261119_150048.txt; row 26 "Odblokowanie bagażnika" (boot unlocking; drawn over a second, cut text on the screen) of that screen (owner photos of 2026-11-19, not published): the 26 different numbers in the order of their first occurrence match the 26 rows one to one. No frame known, the firmware only refuses it |
| `vehicle_settings setting 05` | measured | `80 05 00` at 1241.055 s in the pass through the radio screen "Function settings" row by row from the top (after owner marker 64, 1223-1263 s), centralka_konsola_20261119_150048.txt; row 12 "Przełącznik radaru" (radar switch, i.e. parking sensors) of that screen (owner photos of 2026-11-19, not published): the 26 different numbers in the order of their first occurrence match the 26 rows one to one. The 1A9 press for it (`00 7F FF 04 FF 00 10 00`) is measured from the box (can_20261110_190421.log, 1207.859 s, step 51) and was accepted by the car after radio `80 05` on 2026-11-18 (console log of this device in the car, not published) |
| `vehicle_settings setting 06` | measured | radio 80 06 -> 15B accepted both ways, console log of this device in the car (not published); row 4 "Oświetlenie „prowadź mnie do…" (follow me home; cut on the screen) of the radio screen, `80 06 01` / `03` at 1229.951 s in centralka_konsola_20261119_150048.txt |
| `vehicle_settings setting 07` | measured | radio 80 07 -> 15B accepted both ways, console log of this device in the car (not published); row 5 "Oświetlenie powitalne" (welcome lighting) of the radio screen, `80 07 03` / `01` at 1232.506 s in centralka_konsola_20261119_150048.txt |
| `vehicle_settings setting 09` | measured | radio 80 09 -> 15B accepted both ways (byte 1 = 00), console log of this device in the car (not published); row 6 "Ustawienia dźwięku" (sound) of the radio screen, `80 09 01` / `02` at 1234.109 s in centralka_konsola_20261119_150048.txt |
| `vehicle_settings setting 0A` | measured | `80 0A 02` at 1236.047 s, `80 0A 00` at 1236.595 s in the pass through the radio screen "Function settings" row by row from the top (after owner marker 64, 1223-1263 s), centralka_konsola_20261119_150048.txt; row 7 "Jednostki zużycia paliwa" (fuel units) of that screen (owner photos of 2026-11-19, not published): the 26 different numbers in the order of their first occurrence match the 26 rows one to one. No frame known, the firmware only refuses it |
| `vehicle_settings setting 0B` | measured | `80 0B 00` at 1226.142 s (clicked second, between two `80 01`) in the pass through the radio screen "Function settings" row by row from the top (after owner marker 64, 1223-1263 s), centralka_konsola_20261119_150048.txt; row 1 "Ustawienia języka" (language) of that screen (owner photos of 2026-11-19, not published): the 26 different numbers in the order of their first occurrence match the 26 rows one to one. No frame known, the firmware only refuses it |
| `vehicle_settings setting 0C` | measured | radio sends 80 0C 00 from the blind spot row (console log of this device in the car (not published)); the 1A9 bit is a measured box press, can_20261110_190421.log; row 8 "Wykrywanie martwego pola" (blind spot detection) of the radio screen, `80 0C 01` at 1237.213 s in centralka_konsola_20261119_150048.txt |
| `vehicle_settings setting 0D` | measured | `80 0D 01` at 1240.405 s in the pass through the radio screen "Function settings" row by row from the top (after owner marker 64, 1223-1263 s), centralka_konsola_20261119_150048.txt; row 11 "Start/Stop silnika" (engine Start/Stop) of that screen (owner photos of 2026-11-19, not published): the 26 different numbers in the order of their first occurrence match the 26 rows one to one. No frame known (no 1A9 content for it was ever recorded): the firmware only refuses it |
| `vehicle_settings setting 0F` | measured | radio 80 0F -> 15B accepted both ways, console log of this device in the car (not published); row 9 "Otwarcie drzwi" (door opening) of the radio screen, `80 0F 00` at 1237.860 s in centralka_konsola_20261119_150048.txt |
| `vehicle_settings setting 10` | measured | `80 10 00` at 1329.446 s, 8.7 s after owner marker 65 "kalibracja cisnienia opon" (tyre pressure calibration), centralka_konsola_20261119_150048.txt; also `80 10 00` at 1239.570 s in the pass through the radio screen "Function settings" row by row from the top (after owner marker 64, 1223-1263 s), centralka_konsola_20261119_150048.txt; row 10 "Kalibracja czujnika ciśnienia w oponach" (tyre pressure sensor calibration) of that screen (owner photos of 2026-11-19, not published): the 26 different numbers in the order of their first occurrence match the 26 rows one to one. No frame known, the firmware only refuses it |
| `vehicle_settings setting 12` | measured | `80 12 01` at 1247.282 s, `80 12 03` at 1247.899 s in the pass through the radio screen "Function settings" row by row from the top (after owner marker 64, 1223-1263 s), centralka_konsola_20261119_150048.txt; row 16 "Opóźnienie wyłączenia reflekto…" (headlamp delay; cut on the screen) of that screen (owner photos of 2026-11-19, not published): the 26 different numbers in the order of their first occurrence match the 26 rows one to one. No frame known, the firmware only refuses it |
| `vehicle_settings setting 14` | measured | `80 14 01` at 1241.555 s and 1243.924 s in the pass through the radio screen "Function settings" row by row from the top (after owner marker 64, 1223-1263 s), centralka_konsola_20261119_150048.txt; row 13 "System kontroli trakcji" (traction control) of that screen (owner photos of 2026-11-19, not published): the 26 different numbers in the order of their first occurrence match the 26 rows one to one. No frame known, the firmware only refuses it |
| `vehicle_settings setting 15` | measured | `80 15 01` at 1244.458 s in the pass through the radio screen "Function settings" row by row from the top (after owner marker 64, 1223-1263 s), centralka_konsola_20261119_150048.txt; row 14 "System wykrywania zmęczenia" (fatigue detection) of that screen (owner photos of 2026-11-19, not published): the 26 different numbers in the order of their first occurrence match the 26 rows one to one. No frame known, the firmware only refuses it |
| `vehicle_settings setting 16` | measured | `80 16 00` at 1245.065 s and 1246.045 s in the pass through the radio screen "Function settings" row by row from the top (after owner marker 64, 1223-1263 s), centralka_konsola_20261119_150048.txt; row 15 "Kolor wyświetlacza wskaźnikó…" (instrument display colour; cut on the screen) of that screen (owner photos of 2026-11-19, not published): the 26 different numbers in the order of their first occurrence match the 26 rows one to one. No frame known, the firmware only refuses it |
| `vehicle_settings setting 18` | measured | `80 18 01` at 1251.095 s in the pass through the radio screen "Function settings" row by row from the top (after owner marker 64, 1223-1263 s), centralka_konsola_20261119_150048.txt; row 18 "Automatyczne światła" (automatic lights) of that screen (owner photos of 2026-11-19, not published): the 26 different numbers in the order of their first occurrence match the 26 rows one to one. No frame known, the firmware only refuses it |
| `vehicle_settings setting 19` | measured | `80 19 01` at 1251.576 s in the pass through the radio screen "Function settings" row by row from the top (after owner marker 64, 1223-1263 s), centralka_konsola_20261119_150048.txt; row 19 "Automatyczne ryglowanie podczas jazdy" (automatic locking while driving) of that screen (owner photos of 2026-11-19, not published): the 26 different numbers in the order of their first occurrence match the 26 rows one to one. No frame known, the firmware only refuses it |
| `vehicle_settings setting 1A` | measured | `80 1A 01` at 1252.023 s in the pass through the radio screen "Function settings" row by row from the top (after owner marker 64, 1223-1263 s), centralka_konsola_20261119_150048.txt; row 20 "Adaptacyjne reflektory" (adaptive headlamps) of that screen (owner photos of 2026-11-19, not published): the 26 different numbers in the order of their first occurrence match the 26 rows one to one. No frame known, the firmware only refuses it |
| `vehicle_settings setting 1B` | measured | `80 1B 01` at 1252.423 s and 1254.172 s in the pass through the radio screen "Function settings" row by row from the top (after owner marker 64, 1223-1263 s), centralka_konsola_20261119_150048.txt; row 21 "Collision risk Activation" of that screen (owner photos of 2026-11-19, not published): the 26 different numbers in the order of their first occurrence match the 26 rows one to one. No frame known, the firmware only refuses it |
| `vehicle_settings setting 1C` | measured | `80 1C 02` at 1253.123 s, `80 1C 01` at 1256.151 s in the pass through the radio screen "Function settings" row by row from the top (after owner marker 64, 1223-1263 s), centralka_konsola_20261119_150048.txt; row 22 "Collision risk Sensitivity" of that screen (owner photos of 2026-11-19, not published): the 26 different numbers in the order of their first occurrence match the 26 rows one to one. No frame known, the firmware only refuses it |
| `vehicle_settings setting 1D` | measured | `80 1D 01` at 1256.637 s in the pass through the radio screen "Function settings" row by row from the top (after owner marker 64, 1223-1263 s), centralka_konsola_20261119_150048.txt; row 23 "Speed recommendation" of that screen (owner photos of 2026-11-19, not published): the 26 different numbers in the order of their first occurrence match the 26 rows one to one. No frame known, the firmware only refuses it |
| `vehicle_settings setting 1E` | measured | `80 1E 01` at 1257.783 s in the pass through the radio screen "Function settings" row by row from the top (after owner marker 64, 1223-1263 s), centralka_konsola_20261119_150048.txt; row 24 "Directional headlamps" of that screen (owner photos of 2026-11-19, not published): the 26 different numbers in the order of their first occurrence match the 26 rows one to one. No frame known, the firmware only refuses it |
| `vehicle_settings setting 1F` | measured | `80 1F 01` at 1258.234 s in the pass through the radio screen "Function settings" row by row from the top (after owner marker 64, 1223-1263 s), centralka_konsola_20261119_150048.txt; row 25 "Prevention of door mirror folding" of that screen (owner photos of 2026-11-19, not published): the 26 different numbers in the order of their first occurrence match the 26 rows one to one. No frame known, the firmware only refuses it |
| `vehicle_settings language 9->8` | measured | 260 language code 9 (Polish) -> 38 DATA[4] = x8, can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines) |
| `vehicle_settings button 05 b4=04` | measured | 1A9 = 00 7F FF 04 FF 00 10 00, 4 presses, 4 reactions, can_20261110_190421.log |
| `vehicle_settings button 0C b4=20` | measured | 1A9 = 00 7F FF 20 FF 00 10 00, 4 presses, 4 reactions, can_20261110_190421.log |
| `vehicle_settings button frame=0:00 1:7F 2:FF 4:FF 5:00 6:10 7:00` | measured | fixed bytes of every recorded 1A9 press (byte 7 = `10` in all of them), can_20261110_190421.log |
| `tx_allowlist ID_TX_SETTINGS=15B` | measured | 15B sent by the box (can_20261111_190817.log, can_20261111_191045.log (CAN + both UART lines)) and by this device, accepted by the BSI (console log of this device in the car (not published)) |
| `tx_allowlist ID_TX_BUTTONS=1A9` | measured | frame sent by the aftermarket box, can_20261110_190421.log |

## Logic that is not a single constant

The checker covers named constants and tables. These pieces of logic carry bit positions inside
the code and are listed here by hand. "Unverified" marks a bit that was taken from the analysis of
the box firmware or the radio app and never changed in a recording; such a bit only decides what
the radio screen shows, never what is sent to the car.

| Code | What | Status |
|---|---|---|
| `psa_signals.cpp` `decodeSteering`, `decodeTrip`, `decodeOutsideTemp`, `decodeEcoMode` with `xp_signals.cpp` `encodeSteering`, `encodeTrip`, `encodeOutsideTemp`, `encodeEcoMode`; `xp_car.cpp` `buildObstacleFlag` | CAN -> XP byte copies (through the neutral types) and the temperature formula | measured (match counts in the table above) |
| `psa_signals.cpp` `decodeClimate` with `xp_signals.cpp` `encodeClimate` | temperatures (+0x23), fan level, recirculation, A/C, AUTO, rear defrost, air distribution codes 8/5/3 | measured: 26 of 27 frames `21` match; constant bytes `84`, `81`, `40` copied from the recordings. CAN side `350` / `2AD` measured again with owner markers 12-28 (presses on the car's climate panel, `centralka_can_20261119_150048.log`, table [Climate panel](#climate-panel-of-the-car-markers-12-28)). Bit names of frame `21` beyond what changed: unverified (radio app) |
| `psa_signals.cpp` `decodeDoors` with `xp_signals.cpp` `encodeCarState` (DATA[0]) | door bits of frame `38` DATA[0] from CAN `1A1` / `128` | CAN side measured in two independent sessions (2026-11-10 and 2026-11-19: `1A1` byte 4 = `40` driver, `80` passenger, `10` rear left, `20` rear right, `08` boot, `centralka_can_20261119_150048.log` 1337-1388 s); positions in `38`: unverified (radio app), no recorded door change in `38` |
| `psa_signals.cpp` `decodeSettings` (values) with `xp_signals.cpp` `encodeCarState` (DATA[1..5]); reference copy `xp_settings.cpp` `update38` | frame `38` DATA[1..5] from `260`, `227`, `3E1`, `217` | bits that changed in the recordings measured (parking assist, `227` 3.0, `3E1`); all other bits unverified (box firmware). The whole rule reproduces 38 of 38 recorded frames. Language: only Polish is translated (other codes keep the previous value) |
| `psa_signals.cpp` `decodeSettings` (361) with `xp_signals.cpp` `encodeSettingsPresent`; reference copy `xp_settings.cpp` `features361` / `build3C` | frame `3C` from `361` | one recorded value (9 of 9 frames; `361` had the same content through the whole session of 2026-11-19); bit assignment unverified |
| `vehicle_settings.cpp` `build15B`, `fieldMask`, `stateOf` | `15B` = `00`, `260` bytes 2-5 with one field changed, byte 6 & `F4`, `00 00` | measured for 01, 02, 06, 07, 09, 0F (accepted by the BSI). The masking of byte 6 and the zero bytes 7-8 follow PSA-RE (unverified) and keep the recorded form |
| `vehicle_settings.cpp` `buildButton` | `1A9` = `00 7F FF xx FF 00 10 00` with `xx` = `04` (80 05) or `20` (80 0C) | measured (box recording 2026-11-10, accepted by the car from this device 2026-11-18) |
| `vehicle_settings.cpp` `TABLE` columns `361` byte / mask | "car has this function" bits | consistent with the configuration read by the dealer tool in 8 fields; assignment: PSA-RE (unverified) |
| `vehicle_settings.cpp` `TABLE` names | row names of the radio settings screen | measured: all 25 numbers with the rows of the screen "Function settings" (session of 2026-11-19, see the setting rows above) |

### Climate panel of the car (markers 12-28)

Owner markers typed before each press of a button on the car's own climate panel (not on the radio),
`centralka_can_20261119_150048.log`; the change of `350` came 1-4.5 s after the marker. Bytes are
counted from 1, bits 7..0. "Unambiguous" means that only this field changed at that press.

| Marker | Button (owner's words) | Change in `350` (and `2AD`) | Bit or field | Status |
|---|---|---|---|---|
| 12 | auto | byte 6 `15` -> `40` (772.2 s) | byte 6 bits 3-0 = 0 (fan automatic); bits 7-4 1 -> 4 | measured: fan 0 = automatic; byte 6 bits 7-4 value 4 not named |
| 13 | a/c | byte 1 `01` -> `09` (781.9 s) | byte 1 bit 3 = 1: A/C off | measured, unambiguous |
| 14 | max a/c | byte 1 `09` -> `01`, bytes 4 and 5 `09` -> `00` (788.4 s) | A/C on, both temperatures `00` (lowest) | measured: MAX A/C has no own bit in `350` |
| 15 | left temperature + | bytes 1, 4, 5 back to `09 .. 09 09` (799.9 s) | the press ended MAX A/C | not a single step: no new bit |
| 16 | left temperature - | byte 4 `09` -> `08` -> `07` (805.9, 806.4 s) | byte 4 = left temperature | measured, unambiguous |
| 17 | air to face | byte 7 `00` -> `30` (814.3 s) | byte 7 bits 7-4 = 3 (left: face) | measured, unambiguous |
| 18 | air to feet | byte 7 `30` -> `50` (824.3 s) | byte 7 bits 7-4 = 5 (left, after the feet button) | measured, unambiguous |
| 19 | air from bottom to top | byte 7 -> `80`, `50`, `80` (829.3-831.8 s) | byte 7 bits 7-4 = 8 (left, after this button) | measured (the value went back and forth once, probably two presses) |
| 20 | air to face, right | byte 7 `80` -> `83` (842.3 s) | byte 7 bits 3-0 = 3 (right) | measured, unambiguous |
| 21 | air to feet, right | byte 7 `83` -> `85` (848.3 s) | byte 7 bits 3-0 = 5 (right) | measured, unambiguous |
| 22 | air from bottom to top, right | byte 7 `85` -> `88` (855.3 s) | byte 7 bits 3-0 = 8 (right) | measured, unambiguous |
| 23 | air to windscreen | byte 1 `09` -> `15`, byte 6 `40` -> `10`, byte 7 `88` -> `11` (860.8 s) | byte 1 bit 4 = 1 (windscreen demist; back to 0 at 879.5 s), distribution code 1 on both sides; at the same time A/C on (bit 3 = 0) and byte 1 bit 2 = 1 | bit 1.4 and code 1 measured; byte 1 bit 2 and byte 6 `10` changed together with them: meaning not determined |
| 24 | fan speed + | no change of `350` in 5 s (windscreen mode was on) | - | not determined |
| 25 | fan speed - | `350` back to the state before marker 23 (879.5 s) | the press ended windscreen mode | not a single step: fan level bits 3-0 of byte 6 not confirmed in this session (measured earlier only through the box frame `21`) |
| 26 | recirculation | byte 6 `40` -> `30` (886.2 s) | byte 6 bits 7-4 = 3: recirculation | measured, unambiguous |
| 27 | rear window heating | `350` byte 1 bit 5 for 0.4 s (891.2-891.6 s); `2AD` byte 1 `0E` -> `2E` (891.4 s) and it stays | `2AD` byte 1 bit 5 = rear window heating | measured, unambiguous |
| 28 | mono | byte 5 `09` -> `07` (897.8 s) | byte 5 = right temperature, set to the left value | measured: byte 5; MONO has no own bit in `350` |

`2AD` byte 4 changed several times in this part (`54`, `65`, `76`, `87`, `43`, `42`, `32`), mostly
after MAX A/C and the temperature buttons; its meaning is not determined and the firmware does not
read it. The firmware maps distribution code 1 to no air flag (the radio gets only the demist bit for
it); this is not changed, because the stream to the radio must stay identical to the recorded one.

## Summary for the owner

No constant has the status `source: firmware analysis / needs owner decision` (state of
2026-11-19, `python -I tools/check_provenance.py`: 109 constants, measured 108, design 1). Of the 71
constants that had it before that day:

- **38 measured** with the session of 2026-11-19: command `84` and the sound rows `02`-`06`, `09`,
  `0A` (owner markers 49-61); climate buttons `8A 01`-`08`, `0A`, `0B` (markers 33-46) and `09`
  (marker 44, named after the screen label "fan"); `8A 0C` and the 17 settings `80 03`, `05`, `0A`,
  `0B`, `0D`, `12`, `14`, `15`, `16`, `18`, `19`, `1A`-`1F` (the pass through the settings screen row
  by row, matched one to one with the owner's photos of that screen); `80 10` (marker 65);
- **33 removed from the code**: the 16 bits of frame `364` with the whole climate trial (console
  command `climate`, the `364` row of the transmit allow list with its validator and the fast lane of
  the gate); 11 language codes other than Polish; the settings `80 04`, `80 08`, `80 0E` (the radio
  never sent them); the radio frame `C5`; the sound row `84 01`; the Stop and Start content of `1A9`
  (`b7 = 90`, never recorded). Together with them the rows `air364 idle` and `tx_allowlist
  ID_TX_CLIMATE=364` (measured, but without code) left the table. The stream to the radio and the
  outputs of the reference pair are identical byte for byte (golden files).

Still open (logic, not constants, table above): positions of the doors in frame `38`, the bits of
frames `21`, `38` and `3C` that never changed in a recording, the fan level bits of `350` on the CAN
side. They decide only what the radio screen shows.
