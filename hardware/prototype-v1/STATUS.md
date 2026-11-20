# Prototype v1: status

**Proof of concept.** One board was etched at home, assembled and is used in the reference car
(Peugeot 508 SW). It is published so others can see a working layout and reproduce it at their
own risk. It is not a product and has no production files.

## What exists

| Item | State |
|---|---|
| Schematic (`kicad/*.kicad_sch`, `pdf/schematic.pdf`) | complete, ERC 0 violations. Describes the **full** carrier board; the prototype PCB builds a reduced subset (below) |
| PCB (`kicad/proto/centralka_proto.kicad_pcb`) | 100 x 77 mm, two layers, made for toner transfer: 0.8 mm tracks, 0.4 mm clearance, 7 wire vias. DRC: 0 copper errors, 0 unconnected items (only silkscreen and courtyard overlap warnings) |
| Print files (`print/*.pdf`) | copper of both sides for toner transfer (top mirrored), assembly views (2:1, only to look at) |
| BOM (`bom.csv`) | from the prototype PCB |
| Enclosure (`enclosure/`) | bottom part printed and fits; top part not checked as a print |
| Gerber / drill files | **none** (not made for a PCB fab) |
| STEP model | **none** |

## File format

KiCad files were saved by a KiCad 10 development build (file format version 20250610, "9.99").
They open in KiCad 10; **older KiCad versions cannot open them.** The PDF files are for everyone
else.

The KiCad sheet files keep their original (Polish) file names, net labels and PCB silkscreen
texts (`SPOD v1` = bottom, `GORA v1` = top, `masa` = ground, `przewody z wiazki` = harness
wires, `sruba M3` = M3 screw). They are identifiers shared between schematic and PCB, and the
copper texts are part of the toner-transfer artwork. The notes and titles on the schematic sheets
are in English.

| Sheet file | Content |
|---|---|
| `centralka.kicad_sch` | top sheet |
| `zasilanie.kicad_sch` | power: 12 V input, fuse, TVS, DC/DC, battery voltage divider |
| `can.kicad_sch` | CAN module socket, transmit locks |
| `procesor.kicad_sch` | ESP32-S3 module sockets, emergency button |
| `dzwiek.kicad_sch` | amplifier module socket |
| `uart.kicad_sch` | UART to the radio |
| `wyjscia.kicad_sch` | 12 V outputs ACC, ILL, REV, spare |
| `zlacza.kicad_sch` | connectors |

The schematic and the PCB were produced by generator scripts that are not part of this
repository. Edit the KiCad files directly.

## What the prototype PCB has

12 V input with fuse, reverse-polarity diode and DC/DC converter to 5 V; ESP32-S3 dev board on two
1x22 sockets; CAN transceiver module (Adafruit CAN Pal, TJA1051T/3); I2S amplifier module
(MAX98357A); battery voltage measurement; UART to the radio; switched 12 V outputs ACC,
illumination (ILL) and reverse (REV).

Not on the prototype PCB (only in the schematic): expansion connector, I2C, emergency button,
fourth output, spare 3.3 V regulator, selection jumpers JP_S / JP_UART / JP_3V3.

## Transmit path: read before building

The schematic has two hardware transmit locks (JP_TX open, SLNT tied high through JP_S). The
prototype PCB as drawn has:

- **SLNT** of the CAN module pulled up to 3.3 V by R3 (10k): the TJA1051 stays in silent mode,
  its transmitter is off, the receiver works normally;
- **JP1** (TX line from GPIO5 to the module) as a 2-pin header: **leave it open** for a
  listen-only device. With JP1 open the module TXD floats and stays recessive;
- GPIO6 (which the firmware uses to drive SLNT) **not connected** to the module.

The author's board was later modified for transmit experiments: JP1 closed and a wire from SLNT
(M3 pin 3) to GPIO6 (M1 pin 6), R3 kept. **On such a board only the firmware decides whether
the device transmits** (TWAI listen-only mode and GPIO6 high = listen only). There is no hardware
lock any more. Do not make this modification unless you build the separate transmit firmware on
purpose and accept the risk (see the firmware documentation).

Observed once and not explained: in the car, with JP1 closed, the board received no CAN frames;
with JP1 open reception came back. A continuity check found no short between the TX and RX
tracks. Cause unknown.

## Verified on the built board

- Measured with a multimeter before inserting the modules: all resistor and capacitor values,
  continuous ground without shorts to tracks, no short between supply rails, TRACO pins, every
  pin of the ESP32 sockets (neighbours, ground, 3.3 V, 5 V, signals per
  `images/esp32_socket_test_map_bottom_view.png`), RX-TX.
- Started from USB only (ESP32 + CAN module + amplifier, no 12 V): the firmware reports
  `can=ok audio=ok radio=ok`; the `test` console command plays the tone from the speaker.
- In the car: CAN reception, UART to the radio, 12 V supply. These are reported in the vehicle
  page and the firmware documentation; there is no separate bench report for the 12 V outputs
  ACC / ILL / REV in this directory (**unverified** here).

## Parts that were missing on the first build

D2 (SMBJ18A) and the Zener diodes D5, D7, D9 were not fitted at first. **Before the car: fit D2
and use C1 rated 35 V or more.** The Zener diodes are recommended. Several parts of the built
board came from a dismantled aftermarket CAN box (P-MOSFETs, output diodes, input fuse).

## Next

A board for a PCB fab (SMD, Gerber files, connectors for modules) is planned, not designed.
