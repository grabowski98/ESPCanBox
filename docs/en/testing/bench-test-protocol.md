# Bench test protocol

A short check of a built board **on the desk, not in the car**. Do it after you build a board and
after every firmware version you want to put in a car. It covers the parts the
[host tests](host-tests.md) cannot reach: the CAN driver, the UART, the 12 V outputs, Wi-Fi and
updates.

Print this page or copy the tables and fill in the last column: `pass`, `fail` or what you saw.

> [!CAUTION]
> **Do not connect the board to a car during this protocol**
>
> Every step runs on the desk. Keep the CAN terminals free, or connected only to a bench bus you
> built yourself. Do not arm transmission (`cantx on`) at any step.

## What you need

- the assembled board and a USB-C data cable;
- a PC with the toolchain from [Flash the firmware](../quick-start/flash.md);
- a serial terminal at 115200 baud (the PlatformIO monitor is fine);
- a multimeter;
- for the 12 V steps: a bench supply set to 12 V with a current limit (start low, for example
  0.3 A);
- for the panel steps: a phone or laptop with Wi-Fi;
- optional: a 3.3 V USB-UART adapter, and a CAN bus with at least one other working node.

## Record sheet

| Item | Value |
|---|---|
| Board (name, revision) | |
| Firmware version (from `ver`) | |
| Build time (from `ver`) | |
| Tested by | |
| Overall result | |

## 1. Flash over USB and start

Flash the default environment `listen` (listen only, no network, no transmit code):

```bash
cd firmware
uvx platformio run -e listen -t upload
uvx platformio device monitor -b 115200
```

| # | Step | Expected | Result |
|---|---|---|---|
| 1.1 | Upload finishes | PlatformIO ends with `SUCCESS` | |
| 1.2 | Start lines on USB | a line with `LISTEN ONLY. Mode: alongside the box (sends nothing).` | |
| 1.3 | Profile line | `# profile: car=psa_aee2010 radio=xp` | |
| 1.4 | Driver line | `# can=ok audio=ok radio=ok` (any `FAIL` = stop, see [Troubleshooting](../troubleshooting/index.md#the-device-reports-fail-at-start)) | |
| 1.5 | Type `ver` | `[ver] <product> <version> listen, built <date time>` | |
| 1.6 | Type `help` | the list of commands | |
| 1.7 | Type `test` | 1.5 s of beeping from the speaker | |

## 2. Status and reception

Type `stat`. The first lines look like this:

```text
[stat] mode=alongside the box can=ok audio=ok radio=ok
[stat] rx=0 lost=0 errors=0 bus=quiet volume=40% ...
```

| # | Step | Expected | Result |
|---|---|---|---|
| 2.1 | `stat` with nothing on CAN | `rx=0 lost=0 errors=0 bus=quiet`, LED off | |
| 2.2 | Last line of `stat` | `[stat] can-tx=none sent=0 acked=0 failed=0 refused=0` | |
| 2.3 | Paste `0E1#B0F000FCFC1EC200` and press Enter | the logic takes it like a frame from the car; the `[park]` line of the next `stat` changes | |
| 2.4 | Optional, with a CAN bus that has another working node at the right bit rate: connect CANH and CANL, wait 10 s, `stat` | `rx` grows, `lost=0`, `errors=0`, `bus=active`, LED green | |
| 2.5 | Optional: type `dump`, then `dump` again | frames printed as `(s.ms) can0 ID#DATA`, then stop | |

> [!NOTE]
> **A listen-only device never acknowledges a frame**
>
> On a bench bus with only one sender and this device, nobody acknowledges the frames and the
> sender repeats them with errors. You need a second node that acknowledges, as in the car.

## 3. UART to the radio is off in `listen`

| # | Step | Expected | Result |
|---|---|---|---|
| 3.1 | Type `radio` | first line contains `transmit=no` | |
| 3.2 | Type `key A5` | `[key] trial code A5: not sent (listen mode)` | |
| 3.3 | Optional: adapter RX on the pad that goes to the radio (on prototype v1: J4 "radio RX"), 38400 8N1, GND common | no bytes at all, also after 3.2 | |

In `listen` the firmware does not assign a pin to the UART output, so the line cannot be driven
(from the code, `firmware/src/radio_uart.cpp`).

## 4. The 12 V outputs stay off in `listen`

> [!WARNING]
> **Use a current-limited supply**
>
> Connect the bench supply to the +12 V and GND terminals with the current limit set. If the
> current jumps to the limit, switch off and look for a short before you go on.

| # | Step | Expected | Result |
|---|---|---|---|
| 4.1 | 12 V on, USB still connected | supply current stable, start lines as in step 1 | |
| 4.2 | Measure ACC, ILL and REV outputs to GND | 0 V on each | |
| 4.3 | Paste an ignition-on frame `0F6#885C1E23225D5C20` | 0 V on each output, whatever `stat` shows | |
| 4.4 | `stat` | a `supply=` value close to the bench voltage minus the input diode drop | |

In `listen` the main loop always switches the outputs off (from the code, `firmware/src/main.cpp`).

> [!NOTE]
> **Outputs in `decoder` mode**
>
> A bench report of the outputs **switching on** in `decoder` does not exist yet. If you test
> it, flash `decoder`, paste the ignition frame from 4.3 and write down which outputs come on
> and for how long. Never do this with an aftermarket CAN box connected to the same outputs.

## 5. Transmission is not armed

| # | Step | Expected | Result |
|---|---|---|---|
| 5.1 | In `listen`: type `cantx` | `[cantx] built=0: no CAN transmit in this build (CAN listen only)` | |
| 5.2 | Type `cantx on` | `[cantx] REFUSED: this program cannot transmit on CAN` | |

Only if you build the transmit environment on purpose (read [Transmitting on the CAN bus](../safety/transmit.md) first).

> [!CAUTION]
> **decoder_tx_wifi is a decoder environment**
>
> It can switch on the 12 V outputs and send on the UART to the radio. Connect nothing to the
> outputs and no CAN box during these steps.

Flash it:

```bash
uvx platformio run -e decoder_tx_wifi -t upload
```

| # | Step | Expected | Result |
|---|---|---|---|
| 5.3 | Flash `decoder_tx_wifi`; start line | `TRANSMIT 15B: built, armed=0` | |
| 5.4 | Next start line | `# CAN controller: LISTEN ONLY (transmit disarmed)` | |
| 5.5 | Type `cantx` | `[cantx] built=1 armed=0 after_restart=0 controller=listen ...` | |
| 5.6 | Type `set 01 01` | a `[vset]` line with `REFUSED`; nothing sent | |

Flash `listen_wifi` again before section 6.

## 6. Web panel in two languages

Flash `listen_wifi`:

```bash
uvx platformio run -e listen_wifi -t upload
```

Keep USB connected: with a computer on USB the device keeps Wi-Fi on even without bus traffic.

| # | Step | Expected | Result |
|---|---|---|---|
| 6.1 | Type `wifi` | `[wifi] device network: <name>, address http://10.50.8.1`; no password in this output | |
| 6.2 | Type `wifi showpass` | the device password. Do not paste this line into any log or issue | |
| 6.3 | Join the device network from the phone, open `http://10.50.8.1` | the panel loads and asks for the password | |
| 6.4 | Log in | status cards fill in; CAN counters as in `stat` | |
| 6.5 | Language selector: English | all labels in English | |
| 6.6 | Language selector: Polski | all labels in Polish, no English leftovers, no raw keys like `ota.ask.ok` | |
| 6.7 | Reload the page | the chosen language stays (it is stored in this browser only) | |
| 6.8 | Any other language | a note that the translation is machine-made and needs review | |
| 6.9 | Console card: type `stat` | the same output as on USB | |

## 7. Firmware update over Wi-Fi

Build a new image of the same environment (any rebuild changes the build time):

```bash
uvx platformio run -e listen_wifi
```

The file to upload is `firmware/.pio/build/listen_wifi/firmware.bin`, not a merged image.

| # | Step | Expected | Result |
|---|---|---|---|
| 7.1 | Panel: update, choose the file | version, build time and mode of the file are shown before sending | |
| 7.2 | Send | progress, restart, the panel comes back; "Done" after about 20 s | |
| 7.3 | `ver` on USB | the new build time | |
| 7.4 | Panel: press **Roll back to the previous one** | the previous build time in `ver` | |
| 7.5 | Choose a file that is not firmware (any other file) | refused before sending: no marker of this device | |
| 7.6 | Choose a `decoder_wifi` image | a separate warning about the mode change; cancel it | |
| 7.7 | After 7.2 and 7.4: start lines on USB | `# can=ok audio=ok radio=ok` and a `# start:` line with the reason | |

> [!WARNING]
> **A new project name blocks updates from older images**
>
> The update checks the product name inside the image. An image with a different product
> name is refused (`wrong_product`). After the project name changes, the first flash of the new
> name must go over USB.

## 8. After the protocol

- Flash the environment you want to use in the car (usually `listen_wifi` first).
- Keep the filled sheet with the firmware version. Attach it to a pull request if you changed
  anything in `firmware/src/` or `firmware/boards/`.

Next: before the car, go through the [Installation checklist](../install-in-car/checklist.md).
