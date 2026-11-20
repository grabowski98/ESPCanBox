# Peugeot 508 SW: wiring at the radio

Everything below was measured on one car (2015, aftermarket Android radio, harness of an
aftermarket CAN box). Other cars, harnesses and model years can differ: **measure your own** with
the method at the end before you connect anything.

## Where the CAN bus is

The CAN pair goes from the car's Quadlock connector (behind the radio) only to the CAN box plug;
the radio itself has no CAN wires.

| Quadlock, 16-pin block | Signal | Status |
|---|---|---|
| pin 10 | **CANH** | measured: continuity CAN box transceiver pin 7 (CANH) -> box socket -> harness -> Quadlock pin 10; confirmed by error-free reception with this assignment |
| pin 13 | **CANL** | measured: confirmed by reception (17 169 frames, no errors, no losses) |
| pin 12 | +12 V, assumed permanent | measured: continuity in the harness only; that it is permanent (live with the key out) is an assumption, the voltage in the car was not measured |
| pin 16 | ground | measured continuity in the harness |

Numbering: Quadlock seen **from the contact side**, 16-pin block at the top, small blocks at the
bottom; columns from the left 1-4, 5-8, 9-12, 13-16, each from top to bottom. The housing has no
readable numbers, so the match with the manufacturer's numbering is **unverified**.
Photo: [images/quadlock_contact_side_pin10_canh.jpg](images/quadlock_contact_side_pin10_canh.jpg)
(arrow = pin 10).

### Physical layer

| Fact | Status |
|---|---|
| bit rate 125 kbit/s | measured: frames received without errors |
| DC voltage to ground with ignition on (multimeter, average): CANH 0.65 V, CANL 4.5 V; the same with and without the device | measured |
| these levels match a **low-speed fault-tolerant** bus (ISO 11898-3), not high-speed (both lines about 2.5 V there) | unverified: needs an oscilloscope (idle and dominant levels) |
| a 3.3 V high-speed transceiver (SN65HVD230, TJA1051) still receives this bus | measured: all recordings in `firmware/test_data/captures` were made this way |
| with the key out the bus is silent; after ignition off full traffic for about 100 s, then 14 s of frame 036 only, then silence | measured |
| CANH-CANL resistance of the aftermarket boxes (unpowered): 48 kOhm and 59 kOhm | measured: the boxes have no terminator. **Do not add a 120 ohm terminator.** |

## CAN box plug (2x8, pitch about 2.0 mm, estimated without a caliper)

Seen **from the wire side, latch at the top**, positions 1-8 from the left. Upper row / lower row.

| Position | Wire colour (one harness) | Signal | Status |
|---|---|---|---|
| upper 1 | black | ground | measured (continuity to Quadlock 16 and to the box ground) |
| lower 1 | yellow | +12 V (assumed permanent) | measured: continuity to Quadlock 12 and to the box fuse; voltage in the car not measured |
| upper 2 | green | CANH (Quadlock 10) | measured |
| lower 2 | green-black | CANL (Quadlock 13) | measured |
| upper 3 | orange | illumination output -> radio plug pin 07 (ILL) | measured: continuity in one harness, colour only in the other |
| lower 3 | red | ACC output -> radio plug pin 15 (ACC IN) | unverified: wire colour and harness topology only, continuity not recorded in either harness. Measured separately: the radio loses ACC when the box is unplugged |
| lower 4 | brown | reverse output -> radio plug pin 14 (REVERSE) | measured: continuity in one harness, colour only in the other |
| upper 4 | pink / white-black "BRAKE" | handbrake, not connected anywhere | measured (loose lead) |
| upper 6, lower 6 | violet, violet-black | speaker of the CAN box | wire label; one of the harnesses only |
| upper 7 | grey-white | data **radio -> box** (radio TX), to the radio "CANBUS" plug, lower row pos. 4 | measured with the recorder: only radio commands (0x80 and up) on this line |
| lower 8 | white | data **box -> radio** (radio RX), to the radio "CANBUS" plug, upper row pos. 4 | measured with the recorder |
| others | empty | | |

Data lines: UART 38400 8N1, 3.3 V logic. Idle voltage with the radio on: lower 8 = 3.3 V, upper 7
= 2.9 V (measured). Swapping the two data wires = the radio neither receives nor answers
(measured).

The radio "CANBUS" plug (2x4) carries only these two data wires (measured: seen in the plug).
That the common ground comes through the radio power plug is the owner's description of the
harness, not a continuity measurement.

## Radio power plug (16-pin, as used by this harness)

Seen from the wire side, latch at the top. Numbers from the radio seller's drawing, matched by
wire colour (**unverified** except where noted).

| Row at the latch, from the left | 16 GND | 15 ACC IN | 14 REVERSE | 13 KEY | 12 FR- | 11 FR+ | 10 FL- | 09 FL+ |
|---|---|---|---|---|---|---|---|---|
| **Second row** | **08 B+** | **07 ILL** | **06 KEY** | **05 antenna / amp** | **04 RL-** | **03 RL+** | **02 RR+** | **01 RR-** |

Measured with a continuity tester from the CAN box plug: 07 ILL (one harness) and 14 REVERSE (the
other harness). 15 ACC IN: wire colour and topology only, not measured. The KEY1 / KEY2
numbering differs between the wire labels and the seller's drawing; it does not matter when the
steering wheel buttons come over CAN.

## Harness topology (one harness)

- Yellow and black: from the Quadlock to the radio power plug, with a branch to the CAN box plug.
- Speaker wires: from the Quadlock straight to the radio power plug.
- CAN pair: from the Quadlock only to the CAN box plug.
- Red, orange, brown (ACC, ILL, REV): from the CAN box plug to the radio power plug, not to the
  Quadlock. The CAN box switches them.
- Two data wires: from the CAN box plug to the radio "CANBUS" plug.

## How to measure your own harness

1. Unplug the harness from the car. Number the positions of every plug as seen from one side
   and write the view down (wire side or contact side, where the latch is). Most pin mix-ups
   come from a mirrored view.
2. Continuity tester ("beeper"): from each wire of the CAN box plug to the Quadlock and to the
   radio plugs. Note colour, both ends and how you found it. A drawing from a seller or a forum
   is only a hint until it beeps.
3. Find the CAN pair: the two wires that go only from the Quadlock to the CAN box plug.
4. In the car, ignition on, multimeter to ground: CANH and CANL voltages. About 2.5 V on both =
   high-speed CAN; one line near 0 V and the other near 5 V = probably low-speed fault-tolerant
   (confirm with an oscilloscope).
5. Unpowered resistance CANH-CANL of the CAN box: about 120 ohm = it has a terminator,
   kilo-ohms = it does not. The car bus is terminated already.
6. Idle voltage of the two data lines with the radio on. Find the direction with a listen-only
   recorder (`extras/sniffer`, environment `recorder`): the line that carries the radio
   commands is the radio TX.
7. Connect the device in parallel, listen-only first.
