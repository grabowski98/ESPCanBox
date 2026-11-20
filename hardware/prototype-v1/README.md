# Prototype v1: double-sided board for home etching

A carrier board for ready-made modules: ESP32-S3 dev board, CAN transceiver module, I2S
amplifier module, plus the 12 V input, the UART to the radio and three switched 12 V outputs.
Read [STATUS.md](STATUS.md) first: it says what is verified and how the transmit path is wired.

Board 100 x 77 mm. Tracks 0.8 mm, clearance 0.4 mm. **7 wire vias**: 5 join the bottom ground
with the top ground, 2 are the ends of one 5 V jumper on the top side.

## Files

| File | Content |
|---|---|
| `kicad/` | schematic (KiCad 10), symbol library `centralka.kicad_sym` |
| `kicad/proto/centralka_proto.kicad_pcb` | PCB with tracks and ground fill |
| `pdf/schematic.pdf` | schematic, all sheets |
| `print/1_bottom_copper_toner_transfer.pdf` | bottom copper, **not mirrored** |
| `print/2_top_copper_mirrored_toner_transfer.pdf` | top copper, **mirrored** |
| `print/3_assembly_top_view.pdf`, `4_assembly_bottom_view.pdf` | part placement for drilling and soldering (2:1) |
| `images/assembly_map_bottom_view.png` | what to solder where, polarity (red pad), seen from the solder side |
| `images/drill_map_bottom_view.png` | holes coloured by drill size, seen from the solder side |
| `images/esp32_socket_test_map_bottom_view.png` | multimeter checks of the ESP32 sockets before inserting the modules |
| `images/ground_wire_vias.png` | where the ground wire vias go |
| `images/preview_*`, `images/render_*` | previews and 3D renders |
| `bom.csv` | parts list |
| `enclosure/` | 3D-printed enclosure |

The KiCad file, sheet and library names (`centralka*`) are the working name of the prototype, from
before the project was named. They are kept so the project files keep their links.

### Legends of the images (they are in Polish)

- `assembly_map_bottom_view.png`: "MONTAZ - WIDOK ZE SPODU" = assembly, bottom view (track side,
  J5 top right). Colours: yellow resistors 0805, blue capacitors (T = tantalum), pink diodes
  SMA/SMB, green PTC fuses 1812, violet SOT-23 transistors. **Red pad** = diode cathode (band),
  tantalum/electrolytic plus, transistor pin 1. Grey = part inserted from the top, soldered on the
  bottom only. Blue ring = wire via (7). Green frame: Q2, Q4, Q6 = AO3407 (marking "3407").
  Transistor pins: BC847 1 B, 2 E, 3 C; AO3407 1 G, 2 S, 3 D. Order: 1. resistors and 0805
  capacitors, 2. transistors, diodes, PTC, tantalums, 3. electrolytics C1, C4 (tall), 4. the 7
  wire vias, 5. TRACO, J5, Zener diodes, 6. sockets; JP1 stays empty. "(gora)" = part on the top.
- `drill_map_bottom_view.png`: "Wiertla" = drills: 0.8 mm x13 (wire vias, Zener diodes), 1.0 mm
  x63 (sockets, TRACO, JP1), 1.2 mm x5 (J4 wires), 1.3 mm x4 (J5 terminals), 3.2 mm x3 (M3 holes).
  Bottom view, the side you drill from.
- `esp32_socket_test_map_bottom_view.png`: multimeter checks of the empty sockets, solder side.
  Black GND (M1 pin 22, M2 pins 1, 21, 22), orange +3V3 (M1 pins 1, 2 -> VCC of the CAN module),
  red +5V (M1 pin 21 -> plus of C4), blue dashed = signal to its destination, white = unused pin.
  1. Continuity tester, NO power: no neighbouring pins beep (except M1 1-2 and M2 21-22); black
     pins beep to GND of J5; coloured pins beep to the ring of their colour. Pins 7 and 11 have
     100k / 10k to GND: no beep is right (ohmmeter about 85k / 10k).
  2. 12 V, 100 mA limit, NO ESP32, meter minus on GND of J5: M1 pin 21 4.6-4.8 V; M1 pins 1, 2
     0 V (3.3 V comes from the ESP32 board); M1 pin 7 about 1.9-2.0 V; current a few mA.
  3. ESP32 in the sockets (USB on the pin 22 side): M1 pins 1, 2 and VCC of the CAN module 3.3 V;
     current tens of mA; if the supply hits the limit, switch off at once.
  4. CAN module (terminator OFF) and amplifier, then the firmware.
- `ground_wire_vias.png`: red = the 5 ground vias (wire, soldered on both sides); blue = the 5 V
  wire jumper. "RX radia (dolny 8)" = radio RX (harness lower 8), "TX radia (gorny 7)" = radio TX
  (harness upper 7).

## Connectors

- **J5, screw terminals 5.08 mm (2x KF301-2P), from the top:** CANH, CANL, GND, +12V. Power and
  CAN wires from the car harness (or a bench supply).
- **J4, wire pads** (1.2 mm hole, 3 mm pad), from the top: **radio RX, radio TX, ACC, ILL, REV.**
  "From the top" = from the J5 side. "Radio RX" is the line the board transmits on (the radio
  receives it). Swapping the two data wires = the radio neither receives nor answers.
- The 2x8 harness socket is not on the board (its dimensions were not measured). On the
  reference car a 2x8 socket desoldered from an old CAN box is wired to J5 / J4 with short leads.
  Pin map of that harness: [vehicles/peugeot-508-sw-2015/wiring.md](../../vehicles/peugeot-508-sw-2015/wiring.md).

Module pin order (check against the print on your module before etching):

- CAN Pal: from the bottom 1 CANL, 2 CANH, 3 SLNT, 4 TX, 5 RX, 6 GND, 7 VCC (3.3 V);
- MAX98357A: from the top 1 LRC, 2 BCLK, 3 DIN, 4 GAIN, 5 SD, 6 GND, 7 VIN (5 V).

Amplifier gain: GAIN input left open (module default). Speaker: screwed to the terminal of the
amplifier module. The amplifier output is bridged: neither speaker wire may touch ground.

## Making the board

1. Print sheets 1 and 2 at 100 % scale (no "fit to page"; the frame lets you check the scale
   with a ruler). Bottom **not mirrored**, top **mirrored**: the toner always faces the copper.
   **Check: the texts "SPOD v1" (bottom) and "GORA v1" (top) are mirrored on the paper and
   readable on the copper.** Assembly sheets 3 and 4 are at 2:1, only to look at.
2. Drill the three M3 holes first (top left next to J5, top right corner, bottom right corner) and
   align both sides on them. The top ground keeps 1 mm from every hole, so an offset of the top
   side up to about 1 mm does not short a socket pin.
3. After etching inspect with a magnifier. The smallest gap between copper of different nets is
   0.4 mm, in about 17 places. The bottom ground fill keeps 0.6 mm from the pads.
4. Drilling (hole centres are small dots): 0.8 mm x13 (7 vias, 3 Zener diodes), 1.0 mm x63
   (sockets, TRACO, JP1), 1.2 mm x5 (J4), 1.3 mm x4 (J5), 3.2 mm x3 (M3). Without a 0.8 mm drill
   use 1.0 mm (annular ring about 0.65 mm). **Do not drill the sockets larger than 1.0 mm**: along
   a row the ring is only 0.52 mm.
5. Soldering order: SMD parts on the bottom -> 7 wire vias (wire through the hole, solder both
   sides; two are 1.6 mm from the ESP32 header, so before the sockets) -> TRACO, terminals, Zener
   diodes -> sockets last.
6. **Through-hole pads are soldered on the bottom only.** The rings around the holes on the top
   side are dead copper.
7. The top ground joins the bottom only through the 5 ground wire vias. Without them parts of the
   bottom ground float (for example the ground of the converter, of the CAN module and of the
   output transistors).
8. The ground pins of the module sockets (M1-M4) join the fill with solid copper (no thermal
   relief): solder a little longer. The socket pads are 0.5 mm apart: check neighbouring pins for
   shorts after soldering.
9. C1 and C4 (SMD 8x10) are 10 mm tall and sit on the bottom: the board needs standoffs of at
   least 12 mm (the enclosure has them).

## First power-up (no modules)

1. Multimeter: no short +12V-GND, +5V-GND, +3V3-GND (pads near J5, C4, C6).
2. 12 V bench supply with current limit about 100 mA on J5 (terminal 4 = +12V, 3 = GND): about
   4.6-4.8 V on C4 (5 V through diode D3).
3. Only then the ESP32 board, then the modules.

## If a part is missing

- No 12 V Zener (D5, D7, D9): the board works without them; they only protect the MOSFET gates.
- No polymer fuses: a wire bridge in place of F1-F4 **on the bench only**, with a fuse in the +12 V
  lead of the supply.
- No SMBJ18A (D2): may be left out on the bench; **fit it before the car**.

## Before the car

- D2 fitted, C1 rated 35 V or more.
- JP1 open (listen-only hardware), unless you build the transmit firmware on purpose.
- No 120 ohm terminator anywhere: the car bus is terminated already. The CAN Pal terminator
  switch OFF.
- Check without power: no short between neighbouring socket pins and between +12 V and ground;
  CANH-CANL tens of kOhm.
