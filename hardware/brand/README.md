# Logo and icon for the silkscreen of a board

One-colour versions of the project logo and icon, with the dots merged into solid shapes so that
they survive a silkscreen or a copper layer. Author: grabowski98, licence CC BY 4.0. The rules for
the marks are in `docs/en/contributing/brand.md`.

## Files

| File | Content |
|---|---|
| `pcb-logo.svg` | logo, vector master |
| `pcb-icon.svg` | icon, 2 × 4 pins, vector master |
| `pcb-icon-2x8.svg` | icon, 2 × 8 pins, vector master |
| `pcb-logo-08mm.png` ... `pcb-logo-20mm.png` | logo, 1 bit, 1200 dpi, the width is in the file name |
| `pcb-icon-04mm.png` ... `pcb-icon-08mm.png` | icon, 1 bit, 1200 dpi (`04`, `05`, `06` = 2 × 4 pins; `08` = 2 × 8 pins) |

Import a PNG into the PCB program at 1200 dpi, or import the SVG and scale it to the width you
need.

## Minimum widths

The numbers are the smallest feature (line or gap) of the mark at that width, calculated from the
grid of the mark. They assume that the board house needs at least 0.15 mm for a silkscreen
feature; check the rules of your board house.

| Mark | Width | Smallest feature | Usable |
|---|---|---|---|
| logo | 8 mm | 0.140 mm | no, below 0.15 mm |
| logo | 10 mm | 0.175 mm | yes |
| logo | 12 mm | 0.211 mm | yes |
| logo | 15 mm | 0.263 mm | yes |
| logo | 20 mm | 0.351 mm | yes |
| icon 2 × 4 | 4 mm | 0.250 mm | yes |
| icon 2 × 4 | 5 mm | 0.312 mm | yes |
| icon 2 × 4 | 6 mm | 0.375 mm | yes |
| icon 2 × 8 | 8 mm | 0.250 mm | yes |

- Logo: the calculated limit is 8.5 mm wide; use **10 mm or more**.
- Icon 2 × 4: the calculated limit is 2.4 mm; the smallest file here is 4 mm.
- Icon 2 × 8: the calculated limit is 4.8 mm; the smallest file here is 8 mm.

None of these sizes has been ordered or etched yet: the numbers are calculated, not measured on
a board.
