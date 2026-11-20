# Map the radio

Your car profile says **which CAN bits are which button**. The radio protocol says **which code
the radio gets for that button**. Between the two sits a list of logical button names, `KeyId`.
This page explains the three parts and how to change what a button does in the radio, with or
without rebuilding.

## Three tables

```text
CAN frame ──► car profile ──► logical button ──► radio protocol ──► key code ──► remapping ──► radio
              keys / knobs      KeyId             keyCode()          e.g. 14       map FROM TO
```

| Part | File | Changes with |
|---|---|---|
| CAN bits -> `KeyId` | your profile, tables `keys` and `knobs` ([Write a car profile](write-a-profile.md)) | the car |
| `KeyId` -> key code | the radio protocol, `firmware/lib/radio_xp/protocol_xp.cpp` (`xp::keyCode`) | the radio protocol |
| key code -> other key code | remapping table, at run time, stored in the device settings | your taste |

The car profile knows no radio codes, and the radio protocol knows no CAN IDs.
`tools/check_layering.py` checks this.

## Radio protocols available

| Build flag | Protocol | What it does |
|---|---|---|
| `-DRADIO_PROTOCOL=XP` (default) | `xp` | the protocol of the Simple Soft CAN box ("XP") to an Android radio on the FYT platform, UART 38400 8N1. Only commands seen on the wire are used: [XP protocol](../reference/xp-protocol.md) |
| `-DRADIO_PROTOCOL=NONE` | `none` | no radio: the UART is not started, no key has a code, nothing is ever sent |
| `-DRADIO_PROTOCOL=CUSTOM` | yours | your own protocol in `firmware/lib/radio_<name>`, given with `RADIO_PROTOCOL_CUSTOM_HEADER` and `RADIO_PROTOCOL_CUSTOM_OBJECT` (template: `firmware/lib/radio_example`). Listen only on CAN: a transmitting build stops with an error |

`listen_generic_wifi` uses `none`. Your own profile with the `listen_mycar` environment uses `xp`,
but in "beside the box" mode nothing is sent to the radio anyway.

> [!NOTE]
> **A radio protocol of your own**
>
> Everything specific to one radio is in one `const` structure, `RadioProtocol`
> (`firmware/lib/core/radio_protocol.h`): the UART speed, the framing of a frame into bytes
> (`encodeFrame`), the parser of the bytes from the radio (`rxParser`), the acknowledgement byte, the
> gap between frames, the frames for power, keys, parking sensors, obstacle and version, the
> encoders of the car data for the radio screen, the decoding of the radio's own frames, and two
> tables: keys with a latch (`latchKeys`) and state frames with a rate limit (`rateLimits`). If your
> radio has one "car data" frame number with a page or sub-type in `DATA[0]`, set `stateKeyBytes = 1`,
> so that the pages do not overwrite each other in the session. The common part
> (`firmware/lib/core/radio_session.h`: queue, sending on change, repeats, gap) and the decoder know no
> frame number of any protocol, so your protocol gets its own framing on the radio line; the host test
> `radio_example` checks this with the template `firmware/lib/radio_example` (an invented radio,
> compiled only by the host tests). Copy the template to `lib/radio_<name>`, rename the files and the
> object, replace the framing and the frames with your radio's, and build with
> `-DRADIO_PROTOCOL=CUSTOM` (example section in `firmware/platformio_override.sample.ini`).
> Commands from the radio (`decodeCommand`) are only reported: unless the protocol sets
> `commandsUseVsetNumbers` (only XP does), the decoder does not interpret setting, climate or sound
> numbers and prints `[radio] command ... not supported`. A `CUSTOM` protocol does not build with CAN
> transmit, because changing vehicle settings follows the XP setting numbers. The default key remaps
> of a car profile are used only with the protocol named in its `keyRemapProtocol` (the reference
> profile: `xp`). Still XP-specific, whatever the protocol: the web panel (frame names and VIN frames
> of the PSA profile, the radio source shown with XP source codes, the "frame 38" settings line).
> A new protocol must be written from your own measurements of the radio link, not from decompiled
> apps or box firmware.

## Logical buttons and XP codes

The names describe the button of the reference car (Peugeot 508 SW 2015). For your car, use the
name with the closest meaning. The XP code is what the radio receives in key frame `20`
(DATA[0] = code, DATA[1] = 1 pressed, 2 held, 0 released). This is a UART frame: its data bytes
are counted from DATA[0], as on the [XP radio protocol](../reference/xp-protocol.md) page. CAN bytes on
the other pages are counted from byte 1.

| `KeyId` | Button on the reference car | XP code |
|---|---|---|
| `VOLUME_UP` | steering wheel volume + | `14` |
| `VOLUME_DOWN` | steering wheel volume - | `15` |
| `MUTE` | both volume buttons together | `16` |
| `PHONE` | steering wheel phone button | `30` |
| `LIST` | steering wheel LIST | `50` |
| `WHEEL_KNOB_PRESS` | steering wheel knob pressed | `07` |
| `SOURCE` | SRC | `21` |
| `TRIP` | trip computer button on the wiper stalk | `23` |
| `NAV` | panel: NAV | `9E` |
| `MEDIA` | panel: MEDIA | `A0` |
| `SCROLL_LEFT` | panel: seek left | `97` |
| `ZOOM_IN` | panel: zoom + | `9C` |
| `SCROLL_RIGHT` | panel: seek right | `98` |
| `TEL` | panel: TEL | `9F` |
| `SETUP` | panel: SETUP | `99` |
| `DRIVE` | panel: DRIVE | `9B` |
| `MOON` | panel: screen off (moon) | `A4` |
| `ZOOM_OUT` | panel: zoom - | `9D` |
| `MIC_MUTE` | panel: microphone mute | `A1` |
| `WEB` | panel: WEB | `A3` |
| `PANEL_EXTRA` | a panel button, not identified | `22` |
| `PANEL_KNOB_PRESS` | panel power knob pressed | `16` (the same code as `MUTE`) |
| `NEXT` / `PREV` | steering wheel knob, one detent | `17` / `18` |
| `PANEL_KNOB_UP` / `PANEL_KNOB_DOWN` | panel knob, one detent right / left | `81` / `82` |

Status: **measured**. The codes are what the Simple Soft CAN box sent to the radio for each
button, recorded together with the CAN bus on one clock (recordings `can_20261111_190817.log` and
`can_20261111_191045.log` in `firmware/test_data/captures/`; source of each constant in
`radios/xp-simple-soft/provenance.md`). The button that sends frame `0A2` (`SOURCE`) is described
in two ways in the notes of the reference car ("SRC" and "the button between SRC and LIST"); it
needs to be checked again in the car.

If your car has a button with no matching name, a new name can be added to `enum class KeyId` in
`firmware/lib/core/vehicle_signals.h`, **at the end of the list only**, so that the numbers of the
existing names stay the same. It also needs a code in the radio protocol, or it is never sent.

## Default remappings come from the car profile

Just before a key frame goes to the radio, the firmware applies a small remapping table
(`firmware/lib/radio_xp/xp_keys_custom.h`). Its starting entries come from the car profile
(fields `keyRemapDefaults` and `keyRemapDefaultCount`, see [Write a car profile](write-a-profile.md)).
The reference profile `psa_aee2010` has two, chosen for the reference car and its radio:

| From | To | Why (measured: the author's test in the car, one radio) |
|---|---|---|
| `9F` (`TEL`) | `9B` | opens the phone screen; `9F` did nothing without a connected phone |
| `21` (`SOURCE`) | `1F` | voice assistant (Siri in CarPlay) |

They are the owner's preferences, not facts about the bus: another car may not have these buttons.
The `generic` and `example` profiles have no default entries, and neither has a profile copied from
the template, so every button reaches the radio with its own code. The template test shows it:
`[key] code=21 state=1` (SRC is sent as `21`).

To get a default remapping in your own profile, add a row to its `keyRemapDefaults` table. To drop
one at run time, use `map 21 21` on the USB console, or the key remapping section of the web panel.

The moon button (`A4`) is handled apart from the table: it is in the latch table of the XP protocol
(`RadioProtocol::latchKeys`), so the first press sends only "pressed" (the radio blanks its screen),
the second only "released", and at ignition off a latched key gets "released". A remapping for `A4`
has no effect. Another protocol lists its own latch keys, or none.

## Change a mapping without rebuilding

USB console commands ([USB console](../reference/console.md)):

| Command | What it does |
|---|---|
| `map` | lists the table, for example `[key] swap 9F -> 9B` |
| `map FROM TO` | button code `FROM` sends `TO` to the radio, both in hex: `map 9B 1F`. `map 9B 9B` removes it |
| `map reset` | back to the default entries of the car profile (none in `generic`) |
| `keys` | switches the printing of every button event on or off (`[key] printing=1` / `0`) |

The table holds at most 16 entries and is stored in the device settings, so it survives a
restart. The web panel edits the same table.

`FROM` is the code that the radio protocol gives the button (the third column above), not a CAN
value.

## Find out what a code does in your radio

In a decoder build, you can send any key code to the radio from the USB console, without touching
the car:

| Command | What it sends |
|---|---|
| `key 1F` | code `1F` pressed, then released |
| `kd A4` | only "pressed" |
| `ku A4` | only "released" |

In a listen-only build the console answers `: not sent (listen mode)` and nothing goes out.

What some codes did in the radio of the reference car, tried with `key` (measured: the author's test,
one radio, CAN profile "XP Simple Soft -> Peugeot 508 -> Type 2"):

| Code | Reaction |
|---|---|
| `08` | source change |
| `1F` | voice assistant (Siri in CarPlay) |
| `9A` | screen brightness |
| `9B` | phone |
| `A5` | climate control screen |
| `A6` | back |
| `B5` | FM / AM |
| `B9`, `BF` | both: tuning down by 0.05 MHz (two codes with the same note; to be checked again) |
| `B1`, `91` to `96` | no reaction |

Another radio, or another CAN profile in the radio's settings, may react differently. Try each
code on your own radio and write down what it did.

## What else goes to the radio with your profile

With the XP protocol and a profile other than the reference one, the radio gets only these frames
(measured: host test of the template profile, `firmware/test_host/expected/example_car.txt`):

| XP command | Content | From your profile |
|---|---|---|
| `EA` | power state (ACC) | `decodeBody` (ignition) |
| `20` | key | `keys`, `knobs` |
| `32` | parking sensors picture | `decodeParking` |
| `3E` | obstacle flag | `decodeParking` |

The car data for the radio screen come from the optional functions of your profile:

- `38` doors and setting values: `decodeDoors`, `decodeSettings`;
- `3C` which setting rows to show: `decodeSettings`;
- `33`, `34`, `35` trip computer pages: `decodeTrip`;
- `36` outside temperature: `decodeOutsideTemp`;
- `44` economy mode: `decodeEcoMode`;
- `21` climate control state: `decodeClimate`;
- `29` steering angle: `decodeSteering`.

A frame whose function is `nullptr` in your profile is never sent. The XP encoders are in
`firmware/lib/radio_xp/xp_signals.h`. Changing vehicle settings from the radio (command `80`) works
only with the reference profile.

Next: [Porting checklist](porting-checklist.md).
