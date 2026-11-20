# USB console

This page says how to connect to the text console of the device and what to type. The commands
you need most often are `stat` (the state of the device), `vol N` (beep volume) and `test` (a test
beep). The web panel has a copy of the same console (Console tab); some commands work only over
USB, see [Where a command may come from](#where-a-command-may-come-from).

## Connect to the console

You need uv and the code of the repository, see [Flash the firmware](../quick-start/flash.md).

1. Connect the device to the computer with a USB cable. Use the native USB port of the ESP32-S3
   module: on boards with two sockets it is often labelled `USB`, not `UART`.
2. Open a terminal in the top directory of the repository (on Windows: PowerShell or Git Bash).
3. Start the monitor. The speed, 115200 baud, comes from `platformio.ini`:

    ```bash
    uvx platformio device monitor -d firmware
    ```

4. Press Enter, type `ver` and press Enter. Expected: a line that starts with `[ver]`, for
   example `[ver] <product> 2.0.0-beta.1 listen, built Nov 18 2026 01:33:28`.
5. Leave the monitor with Ctrl+C.

If the monitor cannot open the port, another program uses it (a second monitor, an upload). Close
that program and try again. If no port is found at all, list the ports with
`uvx platformio device list` and check the cable.

The device accepts `\r`, `\n` or both at the end of a line. A command line can be at most 127
characters long. Longer text is cut off.

> [!NOTE]
> **No computer, no waiting**
>
> The device does not wait for the computer. When nothing reads the USB port, its text is simply
> lost. The program never stops because of the console.

## What the device prints at start

Right after a start the device prints a few comment lines, for example (program without CAN
transmission):

```text
# <project name>. CAN 125 kbit/s LISTEN ONLY. Mode: alongside the box (sends nothing).
# profile: car=psa_aee2010 radio=xp
# version: <product> 2.0.0-beta.1, built Nov 18 2026 01:33:28, network=yes, volume=40%
# can=ok audio=ok radio=ok
# start: power on
```

- The first line gives the CAN speed from the car profile and the operating mode
  (`alongside the box (sends nothing)` or `DECODER (sends to the radio, drives ACC)`).
- One mode, three spellings: the console prints `alongside the box`, the panel says "beside the
  box", and `ver` prints `listen`. This page calls it "beside the box".
- In the program that can transmit (environment `decoder_tx_wifi`) the first line says
  `TRANSMIT 15B: built, armed=0` or `armed=1`, and a second line says whether the CAN controller
  runs in listen-only mode or in normal mode. See [Transmitting on the CAN bus](../safety/transmit.md).
- `can=FAIL`, `audio=FAIL` or `radio=FAIL` means a driver did not start. The device then stays in
  the safe state: 12 V outputs off, nothing sent, red LED. See
  [Troubleshooting](../troubleshooting/index.md#the-device-reports-fail-at-start).
- Other possible lines: `# settings unavailable (NVS): defaults, changes will not be stored`,
  `# stored key swap table is invalid: the fixed entries apply`,
  `# CAN transmit arming stored by another program: cleared`.

The `<project name>` and `<product>` parts come from `project.json` and change when the project
gets its final name.

## Commands

The command names are English. Some old command names still work as aliases, see
[Old command names](#old-command-names).

### State and information

| Command | What it does |
|---|---|
| `stat` | Mode, driver state, CAN counters, ignition, ACC, reverse, lights, the last parking sensor frame and the CAN transmit gate. Details below. |
| `radio` | What came from the radio over UART: frame counters, errors, the last volume, time and source. |
| `ver` | Product, version, mode and build time, for example `[ver] <product> 2.0.0-beta.1 listen, built Nov 18 2026 01:33:28`. |
| `help` | The list of commands. Any unknown command prints `Unknown command: ...` and the same list. |

`stat` prints lines like these:

```text
[stat] mode=alongside the box can=ok audio=ok radio=ok
[stat] rx=160131 lost=0 errors=0 bus=active volume=40% supply=12.43V (after the diode)
[stat] ignition=1 (0x8E) acc=1 reverse=0 lights=1
[park] rear=5 front=4 sound=1 front interval=0x18 zones rear 777 front 537
[stat] can-tx=none sent=0 acked=0 failed=0 refused=0
```

- `rx` = frames received, `lost` = frames lost because the receive queue was full, `errors` =
  bus errors seen by the CAN controller, `bus` = `active` when a frame came within the last second.
- `supply` exists only on a board with a voltage divider. It is measured **behind** the input
  diode, so it reads about 0.8 V (0.7-0.9 V) less than the battery (the diode drop is not measured exactly).
  Below 6 V the line says `supply=no 12V (x.xxV)`: you are powering the board from USB only.
- `ignition` shows the raw byte in brackets. On the reference car `0x80` = off, `0x88` = on,
  `0x8E` = engine running (measured: CAN recordings in `firmware/test_data/captures/`).
- In a program without transmission the last line is always `can-tx=none` with zeros.

`radio` prints, among others, `[radio] dropped without ACC=N no slot for a state frame=N too long to frame=N`.
The first counter grows in normal use (buttons and sensors while the radio is off). The second and
third should stay at 0 (the third counts frames the radio protocol could not frame into its buffer).

### Sound

| Command | What it does |
|---|---|
| `vol N` | Beep volume, `N` = 0 to 100 percent. The device limits it to its upper limit (80 %, `AUDIO_VOLUME_MAX` in `firmware/src/config.h`). |
| `test` | 1.5 s of test beeping, without the car. |

A volume below 15 % works only until the next restart. After a restart the volume is at least
15 % (`AUDIO_VOLUME_MIN_SAVED`). Reason: in decoder mode the device speaker may be the only
parking sensor sound in the car, so one `vol 0` must not mute it for good.

### Buttons and the radio

These commands test what the radio does with a button code. They need decoder mode: in the
beside-the-box mode the device sends nothing to the radio, and the reply ends with
`: not sent (listen mode)`.

| Command | What it does |
|---|---|
| `key CODE` | Sends the button code `CODE` (hex, for example `key A5`) to the radio as "pressed" and then "released". |
| `kd CODE` | Sends only "pressed". |
| `ku CODE` | Sends only "released". Together with `kd` this tests buttons that the radio treats differently when held. |
| `keys` | Switches the printing of button events on or off. Every call flips it: `[key] printing=1`, then `[key] printing=0`. |
| `map` | Lists the button remapping table: `[key] swap 9F -> 9B` (a car button with code `9F` sends `9B` to the radio). |
| `map FROM TO` | Adds a remapping, both codes in hex: `map 9B 1F`. `map 9B 9B` removes it. Stored in flash. At most 16 entries (`[key] swap table full`). |
| `map reset` | Back to the default remappings of the car profile (none in `generic`). Stored in flash. |
| `rxlog` | Switches the printing of frames from the radio on or off (`[radio_rx] ...`). |
| `doorswap` | Swaps left and right doors in the frame for the radio, until the next restart. |

The codes and what they do on the reference radio are listed in
[XP protocol](xp-protocol.md) and in [Panel API](panel-api.md#10-tables-for-the-panel).

### Panel language and settings backup

| Command | What it does |
|---|---|
| `lang` | Shows the stored panel language and the languages built into this image: `[lang] panel language=pl (stored in the device); available: de,en,es,pl,ru,uk`. |
| `lang CODE` | Stores the panel language, for example `lang pl`. Only a code from the `available` list. Stored in flash. |
| `lang default` | Forgets the stored language: the panel opens in English. |
| `settings` | Prints the settings backup as one JSON line without passwords: `[settings] {"v":1,"lang":"pl","vol":40,...}`. |
| `settings import JSON` | Imports any subset of the backup fields, for example `settings import {"vol":40}`. Everything is checked first; a bad field changes nothing. `net` and `lan` are never imported. Stored in flash. |

Fields, limits and examples: [Settings](settings.md#backup-and-restore).

### CAN frames

| Command | What it does |
|---|---|
| `dump` | Switches the raw frame dump on or off (`# CAN frame dump=1`). The frames go **only to USB**, not to the panel: at about 218 frames per second on the reference car (measured) they would push everything else out of the panel. |
| `ID#DATA` | A frame typed by hand, for example `0E1#B0F000FCFC1EC200`. The whole sniffer line `(12.345) can0 0E1#B0F0...` works too. |

> [!NOTE]
> **A typed frame never reaches the car**
>
> A frame you type goes only into the internal logic of the device, as if it had come from the
> car. It is never transmitted on the CAN bus. Use it to test the decoding on the bench.
>
> A frame typed in the web panel does not count as bus traffic: it does not keep the device or
> its Wi-Fi awake.

### Vehicle settings (read only)

| Command | What it does |
|---|---|
| `vset` | Prints the vehicle settings table: the last frames `260` and `361`, the age of `260`, the mask, the frame `260` seen at start and the bits changed since then, then one row per item of the radio screen (number of the radio command `80`, name, state read from `260`, basis, mask, result of the `15B` frame logic). |

`vset` only reads. It transmits nothing. What the items mean is in
[Peugeot 508 SW](vehicles/peugeot-508.md).

### CAN transmission

These commands matter only in the program built with environment `decoder_tx_wifi`. In every
other program they answer `[cantx] built=0: no CAN transmit in this build (CAN listen only)` or
`[cantx] REFUSED: this program cannot transmit on CAN`.

> [!CAUTION]
> **Transmits on the car bus**
>
> `cantx on` arms a program that can send frames to the car. Read
> [Transmitting on the CAN bus](../safety/transmit.md) before you use any command in this table.
> The steps to switch it on are in [Switch transmitting on step by step](../safety/transmit.md#switch-transmitting-on-step-by-step).

| Command | What it does | Allowed from |
|---|---|---|
| `cantx` | State: built or not, armed now, armed after a restart, controller mode, mask, gate state, counters, TEC / REC (transmit and receive error counters), reason of a latch. | anywhere |
| `cantx on` | Stores the arming. It takes effect **after a restart** of the device and only for this exact program (the build string is stored). Flashing any other program disarms. | USB, panel on the device network |
| `cantx off` | Disarms immediately and stores it. The controller returns to listen-only at the next start. | USB, panel on the device network |
| `cantx reset` | Clears the latch of the transmit gate after an error. | USB only |
| `vset on NN`, `vset off NN` | Turns item `NN` (hex) of the settings mask on or off. Stored in flash. | USB, panel on the device network |
| `set NN VV` | The same as the radio command `80 NN VV`, for tests without the radio. Same checks, same gate. | USB only |

Notes:

- `vset on NN` accepts only items with a known frame: `01`, `02`, `06`, `07`, `09`, `0F`
  (frame `15B`) and `05`, `0C` (frame `1A9`). Other items answer
  `[vset] item NN cannot be enabled: fixed refusal in the program (no measurement or no frame)`.
- The default build sends only frame `15B`, checked by ID and length only. Frame `1A9` is on the
  allow list only in a build with `-DCAN_TX_EXPERIMENTAL=1`; in the default build the gate refuses
  it even when the item is in the mask. No build sends anything for the radio climate commands
  (`8A`): they only print a `[climate]` line.
- Changes that write flash (`cantx on`, the mask) are skipped while the car needs the device
  (reverse gear or active parking sensors): `[cantx] skipped: the car needs the device ...`. Try again
  when parked. `cantx off` always takes effect at once.
- Numbers `NN` and `VV` are hex, one or two digits, without `0x`. A wrong form prints
  `[cantx] bad syntax. Commands: cantx | cantx on | cantx off | cantx reset | vset on NN | vset off NN | set NN VV`.

### Wi-Fi (USB only)

These commands exist only in the programs with network (`listen_wifi`, `decoder_wifi`,
`decoder_tx_wifi`, `listen_generic_wifi`). They work **only on the USB console**: some of them
show or set passwords, and USB means someone has the device in their hands. From the panel the
answer is `[wifi] USB console only` (the panel itself refuses them earlier).

| Command | What it does |
|---|---|
| `wifi` | State: name and address of the device network, connected clients, home network and its address, the `.local` name. No password. |
| `wifi showpass` | Prints the device password (the key of its Wi-Fi network and the panel login). Printed to USB only, never to the panel. |
| `wifi pass NEW` | Sets a new device password: 10 to 63 printable characters, no spaces. The network restarts. |
| `wifi newpass` | Picks a new random password. The old one stops working on all phones and computers. Show it with `wifi showpass`. |
| `wifi sta NAME|PASSWORD` | Saves the home network. The line is split at the **first** `|`. The password is required, 8 to 63 characters or 64 hex digits: open networks are not supported. |
| `wifi sta off` | Forgets the home network. |
| `wifi lan on`, `wifi lan off` | Allows or blocks changes from the home network (default: blocked, the panel there is read only). Stored. |
| `wifi on`, `wifi off` | Switches the network on or off. Stored across restarts. `wifi on` also clears the counters of failed starts and crash restarts. |

Example: `wifi sta MyHome|secret-pass-123`. The vertical bar `|` is part of the command: it
separates the network name from its password.

> [!WARNING]
> **Keep the password out of shared logs**
>
> Do not keep the `[wifi] password:` line in a log you publish or send to someone.

During a firmware upload, `wifi off`, `wifi pass`, `wifi sta` and `wifi newpass` save the setting,
but Wi-Fi stays as it is until the upload ends.

## Where a command may come from

| Source | Allowed |
|---|---|
| USB console | everything |
| web panel, phone on the **device network** | everything except `wifi ...`, `cantx reset` and `set NN VV` |
| web panel through the **home network** | reading commands; changes only after `wifi lan on` on USB; `cantx on/off` and `vset on/off NN` never |

While reverse gear is engaged or the parking sensors are active:

- the panel refuses `test`, `vol N`, `map FROM TO`, `map reset`, `lang CODE` and `settings import`
  (`[www] skipped: the car needs the device`);
- USB runs `test`, `vol N`, `map FROM TO` and `map reset` at once and writes them to flash later.
  It says so with `[audio] volume write deferred: the car needs the device`, later
  `[audio] deferred volume write 40%: done`. The same for the key table (`[key] ...`);
- USB refuses `lang CODE` and `settings import` (`... try again shortly`).

Reason: a flash write stops CAN reception for a few milliseconds, and a test beep would drown the
sensors.

## Old command names

The commands used to have Polish names. These still work and do exactly the same:

| Old name | Current name |
|---|---|
| `ust` | `vset` (also `ust on NN`, `ust off NN`) |
| `wifi haslo` | `wifi showpass` |
| `wifi nowehaslo` | `wifi newpass` |

The test `firmware/test_host/console_alias.cpp` checks the `ust` alias.

## The line format

This part is for people who write tools that read the console. The format is fixed
(see [Versioning](versioning.md)). The console prints three kinds of lines:

| Line | Example | Meaning |
|---|---|---|
| CAN frame | `(734.501) can0 0E1#B0F000FCFC1EC200` | a raw frame, only after the `dump` command. Time in seconds since the device started, ID in three hex digits, data in hex |
| comment | `# version: ...` | starts with `#`. Start messages, notes, errors of the ESP-IDF libraries (`# idf: ...`) |
| tagged line | `[stat] rx=160131 lost=0 ...` | starts with a tag in square brackets that says which part of the program speaks |

The tags you can meet: `[stat]`, `[radio]`, `[ver]`, `[park]`, `[car]`, `[key]`, `[audio]`,
`[vset]`, `[cantx]`, `[climate]`, `[sound]`, `[radio_rx]`, `[can]`, `[wifi]`, `[www]`, `[ota]`,
`[sleep]`.

The recording format of the `dump` command (`(s.mmm) can0 ID#DATA`) is the same as in the
recordings in `firmware/test_data/captures/` and in the file the panel downloads.

Every command and every line on this page is checked against `firmware/src/console_cmds.cpp`,
`firmware/src/net/net.cpp` and `firmware/lib/core/can_tx_arm.cpp`. If this page and the code
disagree, the code is right: please open an issue.

## See also

- [Panel guide](panel-guide.md): the Console tab.
- [Panel API](panel-api.md): `POST /api/cmd` sends a console line from the panel.
- [Config and pins](config-and-pins.md): build environments.
- Something does not work: [Troubleshooting](../troubleshooting/index.md).
