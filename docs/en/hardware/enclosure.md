# Enclosure

A two-part 3D-printed case for the prototype board. Files are in
`hardware/prototype-v1/enclosure/`.

| Part | State |
|---|---|
| bottom part | printed, the board fits |
| top part | not checked as a print (the screw ears were reinforced after the first version) |

The board outline and the holes come from the PCB file. The heights above and below the board and
the board thickness were measured with calipers on the built prototype. The position of the USB
socket and of the wire cut-outs is still an assumption: see [the list below](#measured-and-assumed-dimensions).

## Files

| File | Content |
|---|---|
| `enclosure_bottom.stl` | bottom part: floor, posts for the board, walls up to 5 mm above the board |
| `enclosure_top.stl` | top part, saved **roof down** (the way it is printed) |
| `preview.png` | quick preview of both parts |
| `gen_enclosure.py` | generator; all dimensions are at the top of the file |

Outside size: 107.8 x 84.8 x 45.0 mm, or 121.8 x 98.8 mm with the screw ears.

## How it is built

- The board sits on three posts under its M3 holes and on one support in the corner near the REV
  pad, with 14.5 mm of room under the board. The tallest capacitor under the board is 13 mm.
- The case splits 5 mm above the board. A 2 mm tongue of the top part enters the bottom part and
  keeps both aligned.
- All cut-outs lie on the split line, so you lay the wires in from above instead of threading them:
    - left wall at J5 (CANH, CANL, GND, +12V): 22 mm wide;
    - left wall at J4 (wires to the radio, also the speaker wire): 37 mm;
    - wall at the USB side of the ESP32 board: 27 x 12 mm, room for a plug with its housing.
- Four screws in the corner ears join the halves. Each ear of the top part is a solid corner joined
  with both walls over 14 mm and 14 mm up (a plain ring had too little contact with the wall).

## Screws

| Where | Screw | Hole in the print |
|---|---|---|
| board to posts, 3 pcs | M3 x 6 to 8 mm | 2.8 mm, the screw cuts its own thread |
| top to bottom, 4 pcs | M3 x 10 to 12 mm, head up to 6.5 mm | 2.8 mm in the bottom, 3.4 mm in the top |

## Printing

- Both parts without supports: the bottom on its floor, the top on its roof (the file is already
  rotated).
- 0.2 mm layers, 3 perimeters, 20 % infill.
- The top part has one 27 mm bridge over the USB cut-out.

> [!WARNING]
> **Do not print it in PLA**
>
> Use PETG or ABS/ASA. PLA softens in a car in summer: the cabin gets above 60 °C.

## Measured and assumed dimensions

Board thickness: just under 2 mm (measured), 2.0 mm in the generator.

| Parameter in `gen_enclosure.py` | Value | Status |
|---|---|---|
| `UNDER` | 14.5 mm | measured: tallest capacitor under the board 13 mm (calipers) |
| `ABOVE` | 24.5 mm | measured: top of the screw terminal block 22 mm above the board (calipers) |
| `USB_Z` | 5 to 17 mm above the board | assumed: height of the USB sockets of the ESP32 board |
| `USB_X` | 70 to 97 mm from the left edge | assumed: position of the USB sockets along the edge |
| `NOTCH_Z` | 1.5 to 9 mm above the board | assumed: height at which the wires leave the terminals |
| `NOTCH_J4` | 36 to 73 mm from the top edge | assumed: room for the harness socket wired to the radio pads |

Also check that the J5 terminals do not stick out more than 1.5 mm past the board edge: that is the
clearance to the wall.

If your ESP32 board or your parts differ, change the values at the top of `gen_enclosure.py` and
generate the STL files again.

## Not included

- Mounting in the car (brackets, cable ties).
- Room for the speaker: the speaker stays outside on its wire.
- A window for the status LED.

## Generate the STL files

The generator needs Python with `manifold3d`, `numpy` and `matplotlib`. With
[uv](https://docs.astral.sh/uv/) installed, from the repository root:

```
cd hardware/prototype-v1/enclosure
uv run --with manifold3d --with numpy --with matplotlib python -I gen_enclosure.py
```

The STL files in the repository were produced by this script.

Next: [Finding and checking wires](wiring.md).
