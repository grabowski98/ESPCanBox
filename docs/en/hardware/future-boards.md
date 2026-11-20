# Future boards

Prototype v1 proved the idea, but it is a home-etched carrier board with wire leads. A new board is
planned. **It is not designed yet.** Nothing on this page is a promise or a date: it is the list of
lessons and ideas the next board starts from.

The first public release of the project is planned only after the work on the new board is done.

## What the next board should fix

| Prototype v1 | Plan for the next board |
|---|---|
| home etching, wire vias, no Gerber files | a board for a PCB factory, with Gerber and drill files |
| through-hole sockets for ready-made modules | SMD parts, connectors for modules where a module still makes sense |
| harness socket wired with loose leads | a harness connector on the board, with its pin map checked against a real plug |
| SLNT fixed high by R3; transmitting needs a hand-made wire | the selectable lock `JP_S` and the TX jumper `JP_TX` from the [schematic](schematic.md) |
| fixed UART direction | `JP_UART` swap jumpers for harnesses wired the other way |
| 3 outputs (ACC, ILL, REV) | the fourth output already in the schematic |
| no expansion, no I2C, no button | expansion header, I2C, emergency button (all in the schematic) |
| sleep current not measured | sleep current measured and written down |

The schematic of prototype v1 already contains most of the right-hand column. It is the starting
point, not a finished design.

## Ideas under consideration

These come from early planning notes. None of them is tested.

- **A transceiver with a standby mode** (for example TJA1042T/3, same footprint as the TJA1051T/3)
  if the sleep current turns out too high. In standby it does not receive frames normally, so the
  only hardware transmit lock would be the TX jumper.
- **A low-speed fault-tolerant transceiver**, if an oscilloscope confirms that the bus at the radio
  of the reference car is low-speed fault-tolerant. The prototype receives it with a high-speed
  transceiver, which works but is not the matching part.
- **CAN line protection** (a dual TVS diode for CAN) and places for ferrite beads, as drawn in the
  schematic.
- **Power that switches itself off**: a converter and a transceiver that let the board sleep with
  very little current and wake up on bus traffic.
- **A variant with an external Wi-Fi antenna** (ESP32-S3-WROOM-1U), if the range from behind the
  dashboard is too short. Not measured.
- **Two devices from one code base**: a permanent decoder board for the car, and a separate bench
  analyser with module slots (for example a UART listening module) for recording and decoding.

## Your own board

You do not have to wait. The firmware takes its pins from one header file per board. Copy
`firmware/boards/board_template.h` to `firmware/boards/board_<name>.h`, fill in every pin, remove
its `#error` line and build with `-DBOARD=CUSTOM -DBOARD_CUSTOM_HEADER='"board_<name>.h"'`
(example in `firmware/platformio_override.sample.ini`). The build refuses GPIO 35-37 and 19/20,
strapping pins 0/3/45/46 without a written reason, GPIOs that do not exist and one GPIO used twice. See
[Configuration and pins](../reference/config-and-pins.md).

If you design a board, keep the safety rules of the prototype:

- a hardware way to keep the transceiver silent after reset (pull-up on the silent input);
- a TX jumper you can leave open;
- no 120 ohm terminator;
- a fuse and a surge clamp on the 12 V input;
- 12 V outputs off after reset.

Hardware files are shared under CERN-OHL-S-2.0. If you improve the board, a pull
request is welcome: see [Contribute](../make-it-yours/contribute.md).

Next: [Installation checklist](../install-in-car/checklist.md).
