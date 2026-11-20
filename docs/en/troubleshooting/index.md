# Troubleshooting

This page helps when the device does not do what you expect. It lists only problems that really
happened on the reference car or the bench, and what the code does.

1. Connect USB and open the console in the top directory of the repository:
   `uvx platformio device monitor -d firmware` (details: [USB console](../reference/console.md)).
2. Type `stat`. A good result with the ignition on contains `can=ok audio=ok radio=ok` and
   `lost=0 errors=0 bus=active`. The colour of the LED also helps:
   [Status LED](../reference/config-and-pins.md#status-led).
3. Find your symptom:
    - `FAIL` in the start lines, red LED: [The device reports FAIL at start](#the-device-reports-fail-at-start);
    - `rx=0`, `bus=quiet`: [No CAN frames](#no-can-frames);
    - `lost` or `errors` above 0: [Frames lost or bus errors](#frames-lost-or-bus-errors);
    - the radio stays off: [The radio does not turn on (no ACC)](#the-radio-does-not-turn-on-no-acc);
    - no buttons or car data on the radio: [The radio does not react to buttons or shows no car data](#the-radio-does-not-react-to-buttons-or-shows-no-car-data);
    - the panel does not open: [The web panel does not open](#the-web-panel-does-not-open);
    - the remote key: [The remote key does not unlock after parking](#the-remote-key-does-not-unlock-after-parking);
    - a tone that does not stop: [The parking tone keeps sounding](#the-parking-tone-keeps-sounding).

If your problem is not here, see [How to ask for help](faq.md#how-do-i-ask-for-help).

## The device reports FAIL at start

The start lines end with `# can=... audio=... radio=...`. If any of them says `FAIL`:

- the LED stays red;
- the 12 V outputs stay off and nothing is sent to the radio;
- the device does **not** restart by itself, so the radio and the car do not get ACC pulses.

This is the safe state of the firmware (from the code, `firmware/src/main.cpp`). Check the module named
`FAIL`: `can` = the CAN controller or its pins, `audio` = the amplifier module, `radio` = the UART.
Then check the pins of your board file (`firmware/boards/`) against your wiring.

## No CAN frames

`stat` shows `rx=0` and `bus=quiet`, the LED is off.

| Check | Why |
|---|---|
| Is the car awake? | On the reference car the bus is silent with the key out. After switch-off there is full traffic for about 100 s, then frame `036` alone for 14 s, then silence until the car is opened (measured: recording during parking). No frames with the car asleep is normal. |
| CANH and CANL on the right wires | On the reference car: Quadlock pin 10 = CANH, pin 13 = CANL (measured: voltages and reception without errors). On another car, measure first: [Find your bus](../make-it-yours/find-your-bus.md). |
| Bit rate of the profile | The firmware does not detect the bit rate. The `generic` profile uses 125 kbit/s by default (`firmware/lib/car_generic/profile_generic.cpp`). Set the measured rate of your car. The first start line shows the rate in use. |
| Transmit jumper on prototype v1 | Seen once on the author's board: in the car, with JP1 (TX line to the CAN module) closed, the board received no frames; with JP1 open reception came back. A continuity check found no short between the TX and RX tracks. Cause unknown. JP1 open = no transmission possible: keep it open for everyday listening, close it only to transmit on purpose ([Transmitting on the CAN bus](../safety/transmit.md)). |
| A bench bus | A listen-only device does not acknowledge frames. On a bench you need another node that does. |

## Frames lost or bus errors

`stat` shows `lost` or `errors` above zero, or the console prints a line like
`[can] controller: state=... tec=N rec=N`.

- The controller state is read once per second; a line is printed only when it changes. While
  the state is not "running" the LED is red. The 12 V outputs keep following the ignition from
  the last fresh frames (from the code, `firmware/src/main.cpp`).
- Writes to flash memory stop CAN reception for a few milliseconds. That is why the firmware
  delays settings writes while reversing or with active parking sensors, and why a firmware update
  is refused or aborted at those times.
- **Recording in the browser panel** loses frames when its tab is not visible (measured: tens of
  thousands of frames lost when the author switched tabs during one recording). Use
  `firmware/tools/record.py` instead; it marks gaps as `# lost N frames`.

For reference: the recordings of the reference car were made with zero lost frames and zero bus
errors (measured: listen-only sniffer and this firmware, 125 kbit/s).

## The radio does not turn on (no ACC)

| Check | Why |
|---|---|
| Which environment? | `listen` and `listen_wifi` never switch the 12 V outputs. Only `decoder`, `decoder_wifi` and `decoder_tx_wifi` drive ACC, illumination and reverse. |
| Which profile? | The `generic` profile never switches the 12 V outputs. |
| Ignition frames | In decoder mode ACC follows the ignition frame of the profile (on the reference car `0F6` byte 1: `88` ignition on, `8E` engine running). `stat` prints `ignition=... acc=...`. |
| Is a CAN box still connected? | Never run decoder mode with the aftermarket CAN box connected: two ACC sources on one wire short the outputs. |

On the reference car the radio got its ACC from the CAN box, not from the car wiring: when the box
was unplugged with the engine running, the radio logged an ACC loss (measured: radio log). If you
remove the box, something else must supply ACC.

> [!NOTE]
> **12 V outputs of prototype v1**
>
> There is no separate bench report of the ACC, ILL and REV outputs switching on. Check them
> with the [bench test protocol](../testing/bench-test-protocol.md) before you rely on them.

## The radio does not react to buttons or shows no car data

| Check | Why |
|---|---|
| UART wires swapped | On the reference car, with the two data wires swapped, the radio did not receive and did not answer (measured: in the car). The device sends on the wire the box used for sending, and listens on the other one. |
| Mode | In `listen` the UART output has no pin. `radio` shows `transmit=no`; `key A5` answers `not sent (listen mode)`. |
| Has the radio spoken? | `radio` prints `the radio has not spoken yet` when nothing valid came from the radio. Check the wire from the radio, 38400 8N1, 3.3 V levels, common ground. |
| No ACC | Without ACC the device drops button and sensor frames for the radio on purpose. `radio` counts them under `dropped without ACC`. |
| Radio profile | The radio must be set to the protocol the firmware speaks (XP / Simple Soft profile for the reference radio). |

## The web panel does not open

| Check | Why |
|---|---|
| Environment | Only `*_wifi` environments have Wi-Fi. `listen` and `decoder` contain no network code. |
| Is Wi-Fi on now? | Wi-Fi switches off when the car sleeps. The rules are below the table. |
| Address | `http://10.50.8.1`, plain `http`, not `https`. The `.local` name may not resolve on every system; use the address. |
| VPN on the PC | On the author's PC a VPN with a "kill switch" blocked all traffic to the device network, while the system showed "connected". Disconnect the VPN or allow local network traffic. |
| PC does not rejoin | After a USB upload resets the board, the author's Windows PC did not rejoin the device network by itself. Reconnect by hand. |
| Password | Read it on USB with `wifi showpass`. It is the Wi-Fi key and the panel password. |
| Switched off | `wifi off` is remembered across restarts. After repeated crash restarts the network is held off. `wifi` on USB tells which; `wifi on` enables it again. |
| During an update | While a firmware file is being written, the panel does not answer other requests. |

Wi-Fi runs while (from the code, `firmware/lib/core/net_policy.h` and `firmware/src/config.h`):

- the car is awake (frames on the bus, or ACC to the radio), or
- a computer is connected to USB.

After the bus goes quiet, Wi-Fi stays on only for a logged-in client that asked something in the
last 60 s. That lasts at most 10 minutes, and only while the supply behind the input diode is at
least 11.0 V (about 11.8 V at the battery), when the board measures it. After a start with a
silent bus, the first 60 s count as such a request.

From your home network (after `wifi sta`) the panel is read-only until you type `wifi lan on` on
USB. Transmit controls work only from the device's own network, never through the home network.

## The remote key does not unlock after parking

On the reference car this happened with one aftermarket CAN box, not with another one and not
without a box. The cause was not found. The full story: [The stuck parking tone](../case-studies/parking-tone-bug.md#the-remote-key).

What is known about this device:

- it goes to deep sleep 30 s after the bus goes quiet, if the radio has no ACC, nothing holds Wi-Fi
  and no computer is on USB, and wakes on the first CAN frame (from the code; **not tested on the
  hardware**);
- its current in sleep is **not measured**.

If the remote stops working with the device installed: unplug the device, park for the same time
and try again. Write down the parking time, the battery state and what was connected.

## The parking tone keeps sounding

See [The stuck parking tone](../case-studies/parking-tone-bug.md). The device's own beeper stops
400 ms after the last parking sensor frame (host test `beeper_scale`); the stuck tone described
there came from the car, not from this device.

Next: short answers to common questions are in the [FAQ](faq.md).
