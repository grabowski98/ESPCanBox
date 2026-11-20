# What you need

You can start small. Each level below adds to the one before it.

## Level 0: only a computer

Enough to build the firmware, run the tests and try the web panel without any board.

- A computer with Windows, Linux or macOS and an internet connection for the first build.
- [uv](https://docs.astral.sh/uv/), a small tool runner. It fetches PlatformIO, the compiler for
  the tests and Python packages on demand.
- Install by hand only:
    - [Python 3](https://www.python.org/downloads/), for the panel mock and the helper scripts in
      `firmware/tools/` and `tools/` (they use only the standard library). On Windows tick
      **Add python.exe to PATH** in the installer. On Linux and macOS the command is `python3`.
    - On Windows, for the tests on your computer: **Git Bash** (comes with
      [Git for Windows](https://git-scm.com/download/win)). The test scripts are bash scripts.
- A web browser.

With only a computer you can do steps 1-3 of [Flash the firmware](../quick-start/flash.md) and
use the panel mock on the page [First boot](../quick-start/first-boot.md).

## Level 1: the ESP32-S3 board on your desk

- An **ESP32-S3 development board with 16 MB flash and 8 MB PSRAM** (module
  ESP32-S3-WROOM-1-N16R8). Look for **N16R8** in the module name, on its metal cover or in the
  shop listing. The project uses the Waveshare ESP32-S3-DEV-KIT-N16R8. A board with less flash
  is not supported as it is. Another N16R8 board may work if its pins match, but it was not
  tested.
- A USB cable that carries data (not a charge-only cable). The console runs over the native USB
  of the ESP32-S3.

With only the board you can flash, use the console and the web panel. The pin rules of the
firmware are on the page [Configuration and pins](../reference/config-and-pins.md).

## Level 2: the full device

The carrier board connects the ESP32-S3 board to the car. The main parts of
[prototype v1](../hardware/prototype-pcb.md):

| Part | Used on the prototype | What it does |
|---|---|---|
| CAN transceiver module | Adafruit CAN Pal (TJA1051T/3), terminator switch OFF | connects to CANH / CANL |
| I2S amplifier module | MAX98357A | drives the speaker for the parking tone |
| speaker | 4-8 Ω, 2-3 W (the author used an 8 Ω / 0.5 W speaker from a CAN box kit and limits the volume in the firmware) | parking tone |
| 12 V to 5 V converter | Traco TSR 1-2450 | power from the car |
| input protection | polymer fuse, reverse-polarity diode, SMBJ18A surge clamp | protects the board on car 12 V |
| three P-MOSFET switches | AO3407 with BC847 drivers | 12 V outputs ACC, ILL, REV for the radio |

The full list with values is on the page [Parts list](../hardware/bom.md). The board is a
home-etched two-layer PCB; there are no files for a PCB factory yet.

## Tools

- Multimeter with a continuity beeper. You use it to find pins in the harness and to check the
  board before power-up.
- Soldering iron (the prototype has SMD parts in 0805, SOT-23 and SMA/SMB packages).
- Bench power supply with a current limit, set to about 100 mA for the first power-up.
- Strongly recommended for a new car: an oscilloscope or a logic analyser to see the bus type
  and speed before you connect. See [Find your bus](../make-it-yours/find-your-bus.md).

## The car and the radio

| What | Status |
|---|---|
| Peugeot 508 SW 2015 (PSA AEE2010), profile `psa_aee2010` | measured on one car; see [Peugeot 508 SW](../reference/vehicles/peugeot-508.md) |
| any other car, profile `generic` | listen-only: records CAN frames, sends nothing to the radio and keeps the 12 V outputs off |
| Android radio on the FYT platform set to the CAN profile "XP Simple Soft -> Peugeot 508 -> Type 2", protocol `XP` | measured with one radio (Junsun V3 Plus); see [XP protocol](../reference/xp-protocol.md) |

Other PSA cars of the same generation probably use many of the same frames, but nobody has
measured them with this project. Treat them as **unverified**.

## Skills

You do not need to be a programmer. You need to:

- type commands in a terminal (copy and paste is fine);
- use a multimeter and read a pinout;
- solder, if you build the carrier board.

Writing a profile for a new car means editing a small C++ table; the page
[Write a profile](../make-it-yours/write-a-profile.md) walks you through it.

## Next

[Flash the firmware](../quick-start/flash.md).
