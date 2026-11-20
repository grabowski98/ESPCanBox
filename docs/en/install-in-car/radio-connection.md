# Radio connection

This page shows how to find and connect the wires between the device and the radio. Between an
aftermarket CAN box and an Android radio there are usually two kinds of wires:

- **two data wires** (a serial UART line, one for each direction), and
- **switched 12 V outputs** of the box that tell the radio about the car: ACC (switch on), ILL
  (illumination, lights on) and REV (reverse gear).

In the decoder mode the device takes the place of the box and drives all of them. In the "beside
the box" mode it only listens.

Decoder mode works only with a radio set to the Simple Soft (XP) CAN box protocol; it was measured
on one radio. With any other radio protocol use the "beside the box" mode. The `listen_generic_wifi`
build sends nothing to a radio. Build options: [Map the radio](../make-it-yours/radio-mapping.md).
The project is not affiliated with Simple Soft or with any radio maker.

## The data line (UART)

| Property | Value | Status |
|---|---|---|
| format | 38400 baud, 8 data bits, no parity, 1 stop bit (8N1) | measured: recorder on both lines, `firmware/test_data/captures/can_20261111_*.log` |
| logic level | 3.3 V | measured: idle voltage with the radio on, CAN box line 3.3 V, radio line 2.9 V |
| ground | the small radio data plug carries only the two data wires (measured: seen in the plug); the common ground comes through the radio power plug | unverified: the author's description of the harness, no continuity measurement |

The two wires are named from the radio's point of view in this documentation:

- **radio TX**: the radio transmits, the box (or the device) receives;
- **radio RX**: the box (or the device) transmits, the radio receives.

On prototype v1 the wire pads J4 are, from the J5 side: **radio RX, radio TX, ACC, ILL, REV**.
"Radio RX" is the pad the device transmits on. The board has series resistors in both data lines.

### Find the direction

1. Map the two data wires with a continuity tester, from the CAN box plug to the radio data plug.
2. Radio on, multimeter to ground: note the idle voltage of each wire. On the reference car the
   box line idled at 3.3 V and the radio line at 2.9 V; a small difference is a hint, not proof.
3. Find out which wire carries the radio's commands. That wire is radio TX.
    - **Sure way:** a listen-only recording of both lines with the recorder (`extras/sniffer`,
      environment `recorder`). It needs a second ESP32-S3 board and a CAN module; see
      `extras/sniffer/README.md`.
    - **With the device** (build `listen` or `listen_wifi`): connect pad J4-2 "radio TX" to one of
      the two wires. This pad is only an input, so either wire is safe. Radio on, type `radio`.
      `[radio] the radio has not spoken yet` means no valid frame arrived on this wire. The device
      alone cannot tell the two lines apart for sure: a valid frame from the box also raises
      `received frames=`.

> [!WARNING]
> **Swapped data wires: no communication**
>
> If the two data wires are swapped, the radio neither receives nor answers (measured on the
> reference car). Before you connect the device's transmit pin, make sure it goes to the radio
> RX line, never to the radio TX line: two outputs on one wire fight each other.

### Example: Peugeot 508 SW (reference car)

CAN box plug 2x8, seen from the wire side, latch at the top, positions 1-8 from the left.

| Wire | Signal | Status |
|---|---|---|
| upper 7 (grey-white) | radio TX, to the radio data plug lower row pos. 4 | measured: recorder, only radio commands on this line |
| lower 8 (white) | radio RX, to the radio data plug upper row pos. 4 | measured: recorder and continuity tester |

More of this harness: [Peugeot 508 SW](../reference/vehicles/peugeot-508.md). What the
frames mean: [XP radio protocol](../reference/xp-protocol.md).

## The 12 V outputs: ACC, ILL, REV

On prototype v1 each output is a P-MOSFET high-side switch driven by a transistor from a GPIO pin.
A high level on the pin puts 12 V on the output. At start the program sets all outputs low
**before** it switches the pins to outputs, so there is no pulse. After a driver error at start the
outputs stay off.

| Output | When it is on (decoder mode) | Source in the car profile | Status on the reference car |
|---|---|---|---|
| ACC | ignition on; off 2.5 s after the ignition goes off, or about 3.5 s after the ignition frame stops (1 s to call it off, then 2.5 s) | ignition frame (`0F6` byte 1 on PSA AEE2010: `80` off, `88` on, `8E` engine running) | measured: the switching moment matches the CAN box within 40 ms on the recordings (host tests) |
| ILL | lights on, and only with the ignition on | lights frame (`036` 4.5 on PSA AEE2010) | measured: bit 4.5 follows the lights in the recordings; what the CAN box did on its ILL wire was **not measured** |
| REV | reverse gear engaged | ignition/body frame (`0F6` on PSA AEE2010) | measured: frame `0F6` follows the reverse gear in the recordings (`can_20261111_191045.log`) |
| spare (OUT 4) | never switched by the firmware | none | on the reference harness this position is the unused "BRAKE" lead |

The output pins are set in the board header (`boards/board_proto_v1.h`); see
[Configuration and pins](../reference/config-and-pins.md).

> [!WARNING]
> **Not bench-tested as a set**
>
> On prototype v1 there is no separate bench report for the ACC, ILL and REV outputs
> (unverified). Before the car: power the board from a bench supply, put a small load (for
> example a 12 V bulb or a resistor) on each output, and check with the meter that all outputs
> are at 0 V after start, in both modes.

### Example: Peugeot 508 SW (reference car) {#example-peugeot-508-sw-reference-car_1}

| CAN box plug | Signal | Goes to | Status |
|---|---|---|---|
| lower 3 (red) | ACC output | radio power plug pin 15 (ACC IN) | unverified: wire colour and harness topology; continuity not recorded |
| upper 3 (orange) | ILL output | radio power plug pin 07 (ILL) | measured: continuity in one harness, colour only in the other |
| lower 4 (brown) | REV output | radio power plug pin 14 (REVERSE) | measured: continuity in one harness, colour only in the other |

These three wires go from the CAN box plug to the radio power plug, not to the car connector:
the box switches them. The pin numbers of the radio plug are from the radio seller's drawing,
matched by colour (unverified except the ILL and REVERSE rows above).

Before you use the decoder mode, beep the ACC wire from the CAN box plug to pin 15 of the radio
power plug. Do not rely on the colour.

## Which wires to connect in each mode

> [!CAUTION]
> **Never two sources on one wire**
>
> In the decoder mode the CAN box must be **unplugged**. With the box and the device both
> connected, two transmitters drive one UART wire and two ACC outputs drive one wire: the
> outputs short each other.

| Wire | Beside the box | Decoder |
|---|---|---|
| ground, +12 V, CANH, CANL | connect | connect |
| radio TX (radio -> box) | connect to the device RX (listen) | connect to the device RX |
| radio RX (box -> radio) | **leave unconnected** (the device transmit pin is not used in this mode) | connect to the device TX |
| ACC, ILL, REV | **leave unconnected** (the outputs are always off in this mode) | connect, CAN box unplugged |

In the "beside the box" mode the outputs stay off and the UART transmit pin is not assigned, so
nothing would happen on these wires anyway. Leaving them unconnected means a wrong build cannot
cause a conflict with the box.

## Check on the radio

1. Decoder mode, CAN box unplugged, ignition off.
2. Ignition on: the radio starts. The console `stat` line shows `acc=1`.
3. `radio` in the console: `received frames=` rises. Some radios send a malformed `C5` frame; each one counts as one bad checksum and three garbage bytes, so `bad checksum=` rising slowly with `garbage=` exactly three times as large is normal. Any other ratio means a wiring or baud-rate problem.
4. Lights on: the radio dims its screen (if it is set to follow ILL). `stat` shows `lights=1`.
5. Reverse gear: the radio shows the camera. `stat` shows `reverse=1`.
6. Ignition off: the radio switches off about 2.5 s later (plus its own shutdown delay, if set
   in the radio).

If a step fails, switch the ignition off and plug the CAN box back in; then see
[Troubleshooting](../troubleshooting/index.md). The console commands are described in
[USB console](../reference/console.md).

Next: [First drive](first-drive.md).
