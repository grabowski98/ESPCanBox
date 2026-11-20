# Flash the firmware

This page takes you from a fresh computer to a board that runs the firmware and opens its web
panel. Steps 1-3 work without a board.

> [!NOTE]
> **Status of this firmware tree**
>
> This firmware compiles and passes the tests on a PC (all environments build, the host tests
> match their references). Version 2.0.0 was bench-tested on an ESP32-S3-WROOM-1-N16R8 board
> ([bench test protocol](../testing/bench-test-protocol.md)) and tried in one car on 2026-11-20.

**Which window to type in.** On Windows type every command of this page in **PowerShell** (or
Windows Terminal). Git Bash is not needed to flash; it is needed for the tests on your computer and for the
pages of [Make it yours](../make-it-yours/porting-checklist.md). On Linux
and macOS use the normal terminal.

## Quickest route: a ready image in the browser {#ready-image-in-the-browser}

This is the recommended way for the first install. It needs no PlatformIO, no Python package
for flashing and normally no extra driver: only Chrome or Edge and the image file. It also works where
`pip install esptool` fails (on a very new Python version the installation can stop with a
build error of a dependency).

1. Get the image. Ready images are on the project page under **Releases**, one pair per
   environment without CAN transmit (`listen`, `decoder`, `listen_wifi`, `decoder_wifi`,
   `listen_generic_wifi`; read [Which environment](#which-environment)). A transmit image is
   never offered ready. Pick the file by what you do:

    | File | Address | Use it for |
    |---|---|---|
    | `<environment>-<version>-merged.bin` | `0x0` | first install; erases the whole flash, including the stored settings (the device then generates a new Wi-Fi password) |
    | `<environment>-<version>.bin` | `0x10000` | update of a device that already runs this firmware; keeps the stored settings |

    Your own build (steps 1-3 below) gives the application image
    `.pio/build/<environment>/firmware.bin`: like the file without `-merged`, it goes to
    `0x10000` and only updates a device that already runs this firmware.

2. Plug the board into the computer with a data USB cable, into the native USB socket of the
   ESP32-S3 (see [Connect the board](#4-connect-the-board)).
3. Open <https://espressif.github.io/esptool-js/> in Chrome or Edge.
4. Click **Connect** and choose the port of the board in the list the browser shows.
5. Type the address from the table into the address field, choose the image file next to it and
   click **Program**. No buttons on the module are needed: the board resets by itself.
6. Expected: the log on the page ends with `Hash of data verified.` If the new program does not
   start, press RESET or plug the cable in again.

The console on that page only shows what the device prints; you cannot type commands there. To
type commands, install the small serial terminal of Python once and start it (on Linux and macOS
type `python3` instead of `python`):

```text
python -m pip install pyserial
python -m serial.tools.miniterm - 115200
```

The `-` makes it list the ports; choose the board. Your typing is not shown: type a command
blind and end it with Enter. Leave the terminal with Ctrl+].

After a first install, `ver` shows the version. A build with network then opens its own Wi-Fi
network `<project name>-XXXX` with the panel at `http://10.50.8.1`, for at most four devices at
the same time. Continue with [First boot](first-boot.md#2-read-the-password-from-the-usb-console)
to read the password.

<details class="note" markdown="1">
<summary>Alternative: esptool on the command line</summary>

If `esptool` installs on your computer, it writes the same files with the same addresses. Use one
of the two write commands:

```text
python -m pip install esptool
python -m esptool --chip esp32s3 write-flash 0x0 <environment>-<version>-merged.bin
python -m esptool --chip esp32s3 write-flash 0x10000 <environment>-<version>.bin
```

</details>

**Build it yourself.** The numbered steps below build the firmware from the source code with
PlatformIO and upload it over USB. Use them when there is no ready image for what you need, or
when you change the code.

## 1. Install uv

The firmware is built with [PlatformIO](https://platformio.org/). You do not install it yourself:
`uvx platformio ...` downloads and runs it through [uv](https://docs.astral.sh/uv/), a small tool
runner.

### Windows

1. Open PowerShell and run:

    ```powershell
    winget install --id=astral-sh.uv -e
    ```

2. Close PowerShell and open it again, so it finds `uv`.

Not needed to flash; needed for the tests on your computer and for "Make it yours":
[Git for Windows](https://git-scm.com/download/win). It brings **Git Bash**, which runs the test
scripts and the commands of those pages.

### Linux

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
```

To use the USB port without root, add yourself to the serial group (`dialout` on Debian and
Ubuntu, `uucp` on Arch) and install the PlatformIO udev rules as described in the
[PlatformIO documentation](https://docs.platformio.org/en/latest/core/installation/udev-rules.html).
Log out and in again.

### macOS

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
```

or, with Homebrew, `brew install uv`.

### Check it

Open a new terminal and run:

```text
uvx platformio --version
```

Expected: one line with the PlatformIO version, for example `PlatformIO Core, version 6.2.0`.
The first call downloads PlatformIO and takes a moment; later calls start at once.
If you see "command not found" or "not recognized", close the terminal, open a new one and try
again.

If you prefer an editor, the PlatformIO extension for VS Code works too: open the `firmware`
folder and pick the environment in the status bar. The commands below are what the extension
runs for you.

## 2. Get the code

1. Open the project page (the repository where you found this guide) and choose
   **Code → Download ZIP**. If you use Git, `git clone` works too.
2. Unpack the ZIP. On Windows: right-click it → **Extract All**.
3. Open the unpacked folder that shows `firmware`, `docs` and `README.md` side by side. After
   unpacking it is often one level deeper, inside a folder of the same name.
4. Open a terminal in the `firmware` folder inside it:
    - **Windows:** open `firmware` in File Explorer, click the address bar, type `powershell` and
      press Enter. (On Windows 11 a right-click on an empty spot → **Open in Terminal** works too.)
    - **Linux and macOS:** open a terminal, type `cd ` (with a space), drag the `firmware` folder
      into the window and press Enter.

All commands below are run in this terminal, in the `firmware` folder.

## 3. Build

```text
uvx platformio run -e listen_wifi
```

This program only listens to the car and has the web panel. Without the panel, use `-e listen`
here and in step 5.

The first build downloads the ESP32 platform (`espressif32@8.0.0`, pinned in `platformio.ini`) and
the compiler. That is a large download and can take many minutes; do not stop it. Later builds
reuse it.

Expected: at the end PlatformIO prints `SUCCESS`. The program is in
`.pio/build/listen_wifi/firmware.bin`. If you see `Error` instead, read the first error line: a
network error means the download broke, so run the command again.

### Which environment

An **environment** is a named set of build options in `firmware/platformio.ini`. It decides what
the device is allowed to do. On the desk `listen_wifi` is enough. For another car than the
reference car, use `listen_generic_wifi` later.

<details class="note" markdown="1">
<summary>All six environments, and building them all</summary>

Never change the mode by editing `src/config.h`; pick the environment instead.

| Environment | Mode | Wi-Fi panel and updates over Wi-Fi | Transmits on the car bus | Use it for |
|---|---|---|---|---|
| `listen` (default) | beside the box | no | no (no transmit code in the program) | first flash, bench, listening next to the CAN box |
| `decoder` | decoder | no | no | replacing the CAN box, without network |
| `listen_wifi` | beside the box | yes | no | listening with the web panel |
| `decoder_wifi` | decoder | yes | no | replacing the CAN box, with the web panel |
| `decoder_tx_wifi` | decoder | yes | **only frame `15B`, after `cantx on` and a restart** | personalisation changes from the radio screen; read [Transmitting on the CAN bus](../safety/transmit.md) first |
| `listen_generic_wifi` | beside the box | yes | no | another car: profile `generic`, radio `none`, records the bus and sends nothing to the radio |

"Beside the box" is the listen-only mode: the device works next to the CAN box and sends
nothing. The console calls the same mode `alongside the box`.

A bare `uvx platformio run` (without `-e`) builds `listen`, the safe mode without network.

To build all six environments at once, run `make build` in the top directory (needs `make`,
which Git Bash does not include; the plain `uvx platformio run -e ...` commands always work).

Optional settings that stay on your computer go into `firmware/platformio_override.ini` (copy
`platformio_override.sample.ini`). The sample shows a custom board (`listen_mine`) and a custom
car profile (`listen_mycar`). The trial frame `1A9` exists only if you add
`-DCAN_TX_EXPERIMENTAL=1` to a transmit build there, on purpose.

</details>

> [!WARNING]
> **Decoder mode only without the CAN box**
>
> Flash a `decoder*` environment only when the aftermarket CAN box is unplugged, or on the
> bench. Two devices on the radio data line and two sources of the ACC wire short each other's
> outputs.

## 4. Connect the board

1. Plug the board into the computer with a USB cable that carries data (not a charge-only
   cable). On boards with two USB sockets use the one wired to the ESP32-S3 itself (often
   labelled `USB`, not `UART`): this firmware talks over the native USB of the chip.
2. List the ports:

    ```text
    uvx platformio device list
    ```

Expected: a new entry appears after you plug in the board. Windows 10 and 11 normally need no
extra driver for this port.

| System | The port looks like |
|---|---|
| Windows | `COM5` (any number) |
| Linux | `/dev/ttyACM0` |
| macOS | `/dev/cu.usbmodem1101` (any number) |

Nothing new in the list? Try another cable. If it is still missing, start the bootloader by hand,
see [If the upload does not start](#if-the-upload-does-not-start-bootloader-mode).

## 5. Upload

```text
uvx platformio run -e listen_wifi -t upload
```

PlatformIO finds the port by itself. With several boards connected, name it:
`uvx platformio run -e listen_wifi -t upload --upload-port COM5`.

Expected: the upload ends with `[SUCCESS]`.

- "could not open port" or "Access is denied": the port is in use. Close the monitor (Ctrl+C) in
  the other window and run the upload again.
- No port found: try another cable, then the bootloader mode below.

An upload over USB keeps the stored settings (volume, key table, Wi-Fi password).

### If the upload does not start: bootloader mode

Normally the upload tool resets the ESP32-S3 into its built-in bootloader over USB. If the port
does not appear, or the upload stops with a connection error (for example after a program that
crashes at start-up), start the bootloader by hand:

1. Hold the **BOOT** button (on some boards marked `0` or `IO0`).
2. Press and release **RESET** (`RST` or `EN`), or plug in the USB cable while you hold BOOT.
3. Release BOOT. The port appears again; run the upload command.
4. After the upload press RESET once, so the new program starts.

These are buttons of the ESP32-S3 development board, not of the carrier board.

## 6. Watch the console

```text
uvx platformio device monitor
```

The speed (115200) comes from `platformio.ini`. Press RESET on the board to see the start-up
lines. They look like this (the first line starts with the project name; `listen` prints
`network=no`, `listen_wifi` `network=yes`):

```text
# <project name>. CAN 125 kbit/s LISTEN ONLY. Mode: alongside the box (sends nothing).
# profile: car=psa_aee2010 radio=xp
# version: <product> 2.0.0, built <date and time>, network=yes, volume=40%
# can=ok audio=ok radio=ok
# start: <reset reason>
```

RESET also disconnects the USB port for a moment. If the monitor stops or you miss the start-up
lines, start the monitor again and type `ver`.

> [!NOTE]
> **Your typing is not shown**
>
> The letters you type do not appear on the screen. That is normal: the firmware does not echo
> them. Type the command blind and press Enter; the answer appears on a new line.

Type `help` and Enter for the list of commands, `ver` for the version, `stat` for the state.
Leave the monitor with Ctrl+C.

A `FAIL` in the `can=... audio=... radio=...` line means a driver did not start. The device then
stays in its safe state (12 V outputs off, nothing sent) and the LED on the board turns red. It
does not restart in a loop.

## Erasing everything

`uvx platformio run -e listen -t erase` erases the whole flash, including the stored settings.
The next network start generates a new Wi-Fi password. Upload a program again afterwards.

## Next

- [First boot](first-boot.md): read the password and open the web panel.
- [Bench test](bench-test.md): check the board before the car.
