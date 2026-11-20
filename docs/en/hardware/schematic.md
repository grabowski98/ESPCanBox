# Schematic

This page tells you where the schematic of prototype v1 is, how to open it and which design
details are worth knowing. The files are in `hardware/prototype-v1/`:

| File | Content |
|---|---|
| `pdf/schematic.pdf` | all sheets as PDF: open this if you do not use KiCad |
| `kicad/centralka.kicad_pro` | KiCad project |
| `kicad/*.kicad_sch` | schematic sheets |
| `kicad/centralka.kicad_sym` | symbol library of the project |

The KiCad file and library names (`centralka*`) are the working name of the prototype, from before
the project was named. They are kept so the project files keep their links.

The electrical rules check (ERC) of KiCad reports 0 violations.

> [!NOTE]
> **KiCad 10 only**
>
> The KiCad files were saved by a KiCad 10 development build (file format version 20250610,
> shown as "9.99"). They open in KiCad 10. **Older KiCad versions cannot open them.** Everybody
> else can use the PDF.

The schematic and the PCB were produced by generator scripts that are not part of the repository.
Edit the KiCad files directly.

## The schematic is fuller than the board

The schematic describes the **full** carrier board. The prototype PCB builds a reduced subset of
it. Drawn in the schematic but not on the prototype PCB:

- expansion connector (`EXP1`-`EXP6`) and I2C connector;
- emergency button on GPIO3;
- fourth 12 V output (GPIO21);
- spare 3.3 V regulator (AMS1117-3.3, marked "do not fit");
- selection jumpers `JP_S`, `JP_UART`, `JP_3V3`;
- the 2x8 main connector and the 4-way auxiliary connector (on the prototype they are replaced by
  screw terminal J5 and wire pads J4).

Read [Prototype PCB](prototype-pcb.md) for what the etched board really has.

## Sheets

The sheet files keep their original Polish names. They are identifiers shared between schematic
and PCB, so they were not renamed. The notes and titles on the sheets are in English.

| Sheet file | Name means | Content |
|---|---|---|
| `centralka.kicad_sch` | (project) | top sheet with the block sheets |
| `zasilanie.kicad_sch` | power | 12 V input, fuse F1, reverse-polarity diode D1, TVS D2, DC/DC module U1, diode D3 from USB 5 V, battery divider, 3.3 V source jumper, test points |
| `can.kicad_sch` | CAN | CAN Pal socket, the two transmit locks, 0 ohm resistors for ferrite beads, optional line protection D4; an alternative footprint for an SN65HVD230 module |
| `procesor.kicad_sch` | processor | the two 1x22 sockets of the ESP32-S3 dev board, emergency button |
| `dzwiek.kicad_sch` | sound | MAX98357A socket, SD pull-down, gain jumper |
| `uart.kicad_sch` | UART | the serial link to the radio, swap jumpers |
| `wyjscia.kicad_sch` | outputs | 12 V outputs ACC, ILL, REV and a spare one |
| `zlacza.kicad_sch` | connectors | main 2x8 connector, auxiliary connector, I2C, expansion header, mounting holes |

Other Polish words you will meet: `masa` = ground, `przewody z wiazki` = harness wires,
`sruba M3` = M3 screw, `PUNKTY POMIAROWE` = test points, `ROZSZERZENIA` = expansions.

## Design notes worth knowing

### CAN and the two transmit locks

The schematic has two hardware locks against transmitting:

- `JP_TX` (2-pin) in the TX line from GPIO5 to the module. Open = the module's TX input floats and
  the transceiver stays recessive (it does not drive the bus).
- `JP_S` (3-pin) on the SLNT input. Position 1-2 ties SLNT to 3.3 V: the transmitter is off, the
  receiver works. Position 2-3 gives SLNT to GPIO6, so the firmware sets the mode (high = listen
  only).

A 10 kOhm pull-up (R3) keeps SLNT high after a reset. This matters because the TJA1051 has an
internal pull-down on this pin: an unconnected SLNT means normal mode with the transmitter on
(NXP TJA1051 datasheet, section 7.2.2).

There is **no 120 ohm terminator** on the board, and the switch on the CAN Pal must be OFF: the car
bus is terminated already.

The alternative SN65HVD230 module has no silent input. With it, the only locks are `JP_TX` and the
listen-only mode of the controller.

### Power

- F1 (about 1.1 A, 30 V or more), then D1, then D2. D2 sits after the fuse, so a long surge trips F1.
- D3 separates the DC/DC module from the 5 V rail when the board runs from the USB of the dev board.
- Divider 470k / 100k to GPIO7: 16 V gives 2.8 V at the ADC, current about 21 uA.

### 12 V outputs

Each output is a P-MOSFET high-side switch driven by an NPN transistor. After a reset a 100 kOhm
resistor keeps the NPN off, so the output is off. A 12 V Zener protects the MOSFET gate. The switch
works from about 5 V, so ACC is meant to hold while the engine cranks. On the reference car the
body control unit (BSI) reported 7.16 V at cranking (measured: DiagBox reading of the BSI). Whether
the outputs really hold during cranking: not measured.

### UART

By design: 1 kOhm in the transmit line, 4.7 kOhm in the receive line (safe also with a 5 V level
from the radio). On the prototype both values were measured, but which one sits on which line was
not checked with a meter yet. The `JP_UART` solder jumpers swap transmit and receive, for a harness wired the other way.

### Sound

The amplifier's SD input is pulled low by 10 kOhm: silence until the firmware starts. Gain jumper
`JP_GAIN`: open = 9 dB, to ground = 12 dB, to 5 V = 6 dB (unverified: from the Adafruit module
description; check on a clone).

## Pins

The schematic and `firmware/boards/board_proto_v1.h` use the same pins. The header file is the
binding list: [Hardware overview](overview.md#pins).

Next: [Prototype PCB](prototype-pcb.md).
