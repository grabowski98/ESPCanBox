# Sniffer and recorder (listen-only tools)

Two small ESP32-S3 programs used to learn the reference car before the main firmware existed.
They are kept because they are the simplest way to look at an unknown car: one transceiver
module, three wires, no transmit path at all.

| Environment | Program | What it records |
|---|---|---|
| `sniffer` | `src/sniffer/main.cpp` | every CAN frame |
| `recorder` | `src/recorder/main.cpp` | every CAN frame plus the two UART lines between an aftermarket CAN box and the radio, on one clock |

Both print one line per event over the native USB port, in candump `-L` style, which SavvyCAN
and the host tests of the main firmware read:

```
(12.345678) can0 0E1#B0F000FCFC1EC200     CAN frame
(12.346900) uartA 2E20021401C8            burst of UART bytes (recorder only)
# stat rx=... lost=... bus_err=...        comment / statistics (every second)
```

A UART burst ends after 4 ms of silence on its line. Its time stamp is the time the first byte
was read (about 1-2 ms accuracy).

## Safety

- The TWAI controller runs in `LISTEN_ONLY` mode: it sends no frames, no ACK bits and no error
  frames.
- The `TX` input of the CAN transceiver module is **not wired**. With TXD floating the
  transceiver keeps its driver recessive (internal pull-up), so the hardware cannot transmit
  even if the program were wrong.
- The recorder opens both UARTs with no TX pin. It cannot drive the radio lines.
- No 120 ohm terminator: the car bus is already terminated. Remove the terminator resistor from
  the transceiver module if it has one.

## Hardware

| Part | Notes |
|---|---|
| ESP32-S3 dev board, module ESP32-S3-WROOM-1-N16R8 | any S3 board with native USB works; pins in `include/board_pins.h` |
| 3.3 V CAN transceiver module (SN65HVD230 or similar) | for a high-speed bus. A low-speed fault-tolerant bus usually still receives with it (the reference car did), but check the physical layer first |
| 2 series resistors (recorder only) | see below |

Wiring (sniffer and recorder):

| Signal | From | To |
|---|---|---|
| CAN | module `CANH`, `CANL` | the two CAN wires of the car, in parallel to the existing device |
| CAN RX | module `RX` / `CRX` | GPIO4 |
| CAN TX | module `TX` / `CTX` | **not connected** |
| power | module `3V3`, `GND` | board `3V3`, `GND` |
| ground | board `GND` | car ground |

Recorder only, data lines between the CAN box and the radio:

| Signal | To |
|---|---|
| line A | through a series resistor to GPIO11 |
| line B | through a series resistor to GPIO10 |

Measure the idle voltage of each line first (multimeter, radio on):

- about 3.3 V: one series resistor 2.2-10 kOhm per line;
- about 5 V: divider, 10 kOhm from the line to the pin and 18-22 kOhm from the pin to ground
  (about 3.2-3.4 V at the pin);
- anything else: do not connect, find out first.

Use a laptop on battery or powered from the car, and connect the ESP32 ground to the car ground.

## Build and upload

```
uvx platformio run -d extras/sniffer                          # build both
uvx platformio run -d extras/sniffer -e sniffer -t upload     # upload the sniffer
uvx platformio run -d extras/sniffer -e recorder -t upload    # upload the recorder
```

The CAN bit rate is set in the source (`CAN_TIMING`, 125 kbit/s). If `bus_err` grows and no
frames arrive, the rate is wrong.

## Recording

```
uv run --with pyserial python -I extras/sniffer/record_serial.py COM7 --scenario extras/sniffer/scenarios/example_buttons.txt
```

Press Enter at the start of every step of the scenario: the script writes a comment line
`# STEP n <time> <text>` into the capture. `1`-`9` + Enter write `# MARK n <time>`, `q` + Enter
stops. With `--baud-uart 19200` the script sends the recorder command `baud 19200` once at start.

Recorder commands over USB (type them in any serial terminal):

| Command | Effect |
|---|---|
| `scan` | measures the shortest pulse on both lines for 1.5 s and prints the bit rate and the share of time the line is high |
| `baud 38400` | sets the receive rate of both lines (default 38400) |

## Before sharing a capture

CAN captures of PSA cars contain the VIN (frames `336`, `3B6`, `2B6` on the reference car).
Before you publish any capture, run from the repository root:

```
python -I tools/sanitize_capture.py raw.log shared.log      # zeroes the data of 336, 3B6, 2B6
python -I tools/sanitize_capture.py --check shared.log
python -I tools/scan_private.py shared.log
```

Comment lines written during a recording (step texts, markers) are copied unchanged: read them
before sharing.
