# Panel tour

This page walks through the web panel one screen at a time. Each picture shows a real state of the
panel; the text under it says what you see and why it matters. What every button does is on
[Web panel guide](panel-guide.md); the requests behind the cards are on [Panel API](panel-api.md).

All pictures come from the mock device (`firmware/tools/mock_device.py`) on a computer, never from
a car. The car states are short synthetic recordings of the frames the device decodes; the CAN tab
replays an anonymized recording from `firmware/test_data/captures/`. Pictures marked "radio that
answers" use the mock option `--demo-radio`, which fills the radio fields with the example values of
[Panel API](panel-api.md). The mock also runs with `--screenshot`: one fixed build date and hash, the
network name `ESPCanBox-A1B2` and console lines as the firmware prints them, and a 12 V reading that
fits the car state (13.42 V with the engine running, 11.63 V with the ignition on; the bench picture
has none, as on USB power). Click a picture to open it at full size.

## Log in

[![Login screen with the language picker](../../assets/screenshots/panel-login.png)](../../assets/screenshots/panel-login.png)

The first screen. The password is the key of the device Wi-Fi network; the language picker works
before you log in.

[![Login refused: wrong password](../../assets/screenshots/panel-login-wrong-password.png)](../../assets/screenshots/panel-login-wrong-password.png)

A wrong password gives a red line under the button. The password field is cleared for the next try.

[![Login with 'Remember on this device' ticked](../../assets/screenshots/panel-login-remember.png)](../../assets/screenshots/panel-login-remember.png)

"Remember on this device" saves the password in this browser, so the phone in the car does not ask
every time. Tick it only on your own device: the panel works over unencrypted HTTP.

[![Login lockout after five wrong passwords from the home network](../../assets/screenshots/panel-login-locked-out.png)](../../assets/screenshots/panel-login-locked-out.png)

From the home network, five different wrong passwords within a minute lock the login. The red banner
counts down; the lock grows with each new series.

## Dashboard and the three kinds of firmware

[![Dashboard, listen-only build, engine running, front sensors beeping](../../assets/screenshots/panel-dashboard.png)](../../assets/screenshots/panel-dashboard.png)

The home screen of the listen-only build. The header says the mode ("beside the box") and
"CAN: listen only": this firmware cannot send anything on the car bus.

[![Dashboard in the light scheme](../../assets/screenshots/panel-dashboard-light.png)](../../assets/screenshots/panel-dashboard-light.png)

The panel follows the light or dark scheme of the phone or computer.

[![Dashboard of the decoder build](../../assets/screenshots/panel-dashboard-decoder.png)](../../assets/screenshots/panel-dashboard-decoder.png)

In the decoder build the mode badge turns orange: the device talks to the radio itself and replaces
the factory CAN box.

[![Decoder build with a radio that answers: radio card filled](../../assets/screenshots/panel-dashboard-radio-linked.png)](../../assets/screenshots/panel-dashboard-radio-linked.png)

When the radio answers, the Radio card shows its power state, volume, source and clock (radio that
answers).

## Car states

[![No CAN traffic: every car value says no frame, the bus dot is amber](../../assets/screenshots/panel-dashboard-bus-silent.png)](../../assets/screenshots/panel-dashboard-bus-silent.png)

Nothing on the bus: the bus dot next to "Car" turns amber and every car value says "no frame". This
is what you see on the bench, with the wires not yet connected or with the car asleep; on USB power
the 12 V row says so.

[![Ignition off: ACC has gone off, the car still talks on the bus](../../assets/screenshots/panel-dashboard-ignition-off.png)](../../assets/screenshots/panel-dashboard-ignition-off.png)

Ignition off: the ignition byte reads `80` and ACC drops 2.5 s later. The bus stays awake for a while
after the key is out, so the dot is still green.

[![Ignition on, engine not running](../../assets/screenshots/panel-dashboard-ignition-on.png)](../../assets/screenshots/panel-dashboard-ignition-on.png)

Ignition on without the engine: byte `88`, ACC on. The radio gets its ACC from this state. The 12 V
row shows the battery behind the diode of the device.

[![Engine running, sensors waiting](../../assets/screenshots/panel-dashboard-engine-running.png)](../../assets/screenshots/panel-dashboard-engine-running.png)

Engine running: byte `8E`, the chip says "engine running" and the 12 V row rises with the alternator.

[![Lights on: the Lights chip is lit](../../assets/screenshots/panel-dashboard-lights-on.png)](../../assets/screenshots/panel-dashboard-lights-on.png)

Lights on: the Lights chip follows the lighting frame `036` while the ignition is on.

[![Driver door and boot open](../../assets/screenshots/panel-dashboard-doors-open.png)](../../assets/screenshots/panel-dashboard-doors-open.png)

Open doors turn blue on the top view and are listed by name.

[![Vehicle settings row: last radio command refused by a build without transmission](../../assets/screenshots/panel-dashboard-vehicle-settings.png)](../../assets/screenshots/panel-dashboard-vehicle-settings.png)

The Vehicle settings row shows the last `260` frame, the state sent to the radio and the last
setting the radio asked for. A build without CAN transmission refuses it and says so (radio that
answers).

## Parking sensors

[![Reverse gear: rear sensors active and beeping](../../assets/screenshots/panel-dashboard-park-rear.png)](../../assets/screenshots/panel-dashboard-park-rear.png)

Reverse gear: the rear zones light up from far (green) to close (yellow) and the device beeps at
the interval the car asks for.

[![Front and rear sensors active at the same time](../../assets/screenshots/panel-dashboard-park-front-rear.png)](../../assets/screenshots/panel-dashboard-park-front-rear.png)

Front and rear active together; the sound follows the side the car names in the frame.

[![Obstacle very close: continuous tone](../../assets/screenshots/panel-dashboard-park-continuous.png)](../../assets/screenshots/panel-dashboard-park-continuous.png)

A zone at 0 is red and the interval 0 means a continuous tone.

[![Parking sensors switched off with the car button](../../assets/screenshots/panel-dashboard-park-off.png)](../../assets/screenshots/panel-dashboard-park-off.png)

Sensors switched off in the car: both sides read "off" and the device stays silent.

[![Test beep running from the parking sensors card](../../assets/screenshots/panel-dashboard-test-beep.png)](../../assets/screenshots/panel-dashboard-test-beep.png)

The test button beeps for 1.5 s, so you can set the volume without an obstacle.

[![Beep volume below 15 %: kept only until restart](../../assets/screenshots/panel-dashboard-volume-low.png)](../../assets/screenshots/panel-dashboard-volume-low.png)

Below 15 % the volume is not stored: a very low value saved by mistake would mute the sensors.

## CAN transmission (decoder_tx_wifi)

[![Build with CAN transmission, disarmed](../../assets/screenshots/panel-dashboard-decoder-tx.png)](../../assets/screenshots/panel-dashboard-decoder-tx.png)

Only this build can send on the car bus, and only after it is armed. The CAN bus card shows the
warning, the gate state and the two buttons.

[![Arming CAN transmission: the dialog with its warnings](../../assets/screenshots/panel-dashboard-cantx-arm-dialog.png)](../../assets/screenshots/panel-dashboard-cantx-arm-dialog.png)

Arming asks once more and says what will be sent, how it was tested and how to disarm.

[![Arming stored: it takes effect after a restart](../../assets/screenshots/panel-dashboard-cantx-arm-stored.png)](../../assets/screenshots/panel-dashboard-cantx-arm-stored.png)

Arming is stored but the device keeps listening until it restarts: the row says "after restart:
armed".

[![Armed after the restart](../../assets/screenshots/panel-dashboard-cantx-armed.png)](../../assets/screenshots/panel-dashboard-cantx-armed.png)

After the restart the header badge reads "CAN: armed" and the gate is ready.

[![Transmit build opened from the home network](../../assets/screenshots/panel-dashboard-cantx-home-network.png)](../../assets/screenshots/panel-dashboard-cantx-home-network.png)

From the home network the arming buttons are disabled: arming only works on the device's own
Wi-Fi or over USB.

## CAN tab

[![CAN tab: frame table and live preview of a recording](../../assets/screenshots/panel-can.png)](../../assets/screenshots/panel-can.png)

One row per identifier with period, count, age and the last data; changed bytes flash yellow.

[![CAN tab filtered to one identifier](../../assets/screenshots/panel-can-filter.png)](../../assets/screenshots/panel-can-filter.png)

Type an identifier, or tap a row, and the live preview shows only that frame.

[![CAN preview paused; frames that carry the VIN show dots](../../assets/screenshots/panel-can-paused.png)](../../assets/screenshots/panel-can-paused.png)

Pause freezes the table and the preview so you can read them; the button turns into Resume. Frames
that carry the VIN (`336` in the preview) show dots while "mask VIN" is ticked.

[![Recording CAN frames in the browser](../../assets/screenshots/panel-can-recording.png)](../../assets/screenshots/panel-can-recording.png)

Record collects frames in the browser and saves them as a `.log` file in the sniffer format.

## Radio tab

[![Radio tab in the listen-only build](../../assets/screenshots/panel-radio.png)](../../assets/screenshots/panel-radio.png)

Beside the factory box the device only listens to the radio line, so the radio fields stay empty
and sending is off.

[![Radio tab of the decoder build with a radio that answers](../../assets/screenshots/panel-radio-decoder.png)](../../assets/screenshots/panel-radio-decoder.png)

In the decoder build the UART link counters show frames both ways and the acknowledgements of each
side. Bad checksums, lost frames and full slots turn red: here two bad checksums (radio that
answers).

[![Key test in the listen-only build](../../assets/screenshots/panel-radio-key-test-beside.png)](../../assets/screenshots/panel-radio-key-test-beside.png)

The key test sends a key code to the radio; beside the box it is only written to the console.

[![Key test in the decoder build: sent to the radio](../../assets/screenshots/panel-radio-key-test-decoder.png)](../../assets/screenshots/panel-radio-key-test-decoder.png)

In the decoder build the same key goes to the radio (radio that answers).

[![Key remap editor with the last pressed key](../../assets/screenshots/panel-radio-keymap.png)](../../assets/screenshots/panel-radio-keymap.png)

The remap table swaps a car key for another radio function. "Add to the table" takes the key you
pressed last (radio that answers).

[![Key remap editor with a new, unsaved row](../../assets/screenshots/panel-radio-keymap-edit.png)](../../assets/screenshots/panel-radio-keymap-edit.png)

An edited table is marked "unsaved changes" until you press Save.

[![Back to the built-in key table: confirmation](../../assets/screenshots/panel-radio-keymap-reset-dialog.png)](../../assets/screenshots/panel-radio-keymap-reset-dialog.png)

Going back to the built-in table asks first.

[![Key table saved in the device](../../assets/screenshots/panel-radio-keymap-saved.png)](../../assets/screenshots/panel-radio-keymap-saved.png)

Saved tables are written when the car does not need the device.

## Console

[![Console after the commands ver and stat](../../assets/screenshots/panel-console.png)](../../assets/screenshots/panel-console.png)

The same console as on USB, with buttons for the common commands. Lines keep the device time in
seconds.

[![Unknown command: the console lists the commands it takes](../../assets/screenshots/panel-console-unknown-command.png)](../../assets/screenshots/panel-console-unknown-command.png)

An unknown command prints the list of commands the panel accepts. A panel line holds at most 200
characters, so the end of the list (the example frame) is cut off here; the USB console shows all of
it.

[![A command that only the USB console accepts](../../assets/screenshots/panel-console-usb-only.png)](../../assets/screenshots/panel-console-usb-only.png)

Some commands, such as `set NN VV`, work only from USB; the panel says so instead of "invalid data".

[![Wi-Fi commands are refused in the panel](../../assets/screenshots/panel-console-wifi-refused.png)](../../assets/screenshots/panel-console-wifi-refused.png)

`wifi` commands handle passwords, so the panel does not send them at all.

[![The cantx command in a transmit build](../../assets/screenshots/panel-console-cantx-status.png)](../../assets/screenshots/panel-console-cantx-status.png)

`cantx` prints the gate: built, armed, controller mode, enabled items and counters.

[![Vehicle setting item that the firmware refuses for good](../../assets/screenshots/panel-console-vset-refused.png)](../../assets/screenshots/panel-console-vset-refused.png)

Only items with a measured frame can be enabled; others are refused by the firmware itself.

[![Console filtered to the parking sensor lines](../../assets/screenshots/panel-console-filter.png)](../../assets/screenshots/panel-console-filter.png)

The filter shows only the lines that contain the text, here the parking sensor reports.

[![From the home network the panel is read-only](../../assets/screenshots/panel-console-home-network-read-only.png)](../../assets/screenshots/panel-console-home-network-read-only.png)

From the home network, without `wifi lan on`, every command is refused (here `vol 50`) and a yellow
banner says why.

## System tab

[![System tab](../../assets/screenshots/panel-system.png)](../../assets/screenshots/panel-system.png)

Versions, load, memory and Wi-Fi at the top, the home network and the password below. The settings
backup and the firmware update further down have their own sections on this page.

[![Home network and device password forms](../../assets/screenshots/panel-system-network-password.png)](../../assets/screenshots/panel-system-network-password.png)

The two forms at full size: the home network (optional) and the password of the device.

[![System tab in the light scheme](../../assets/screenshots/panel-system-light.png)](../../assets/screenshots/panel-system-light.png)

The same tab in the light scheme.

[![Device, load and Wi-Fi cards](../../assets/screenshots/panel-system-device-cards.png)](../../assets/screenshots/panel-system-device-cards.png)

Free memory and the stack headroom are worth a look after an update or with a new feature.

[![Opened from the home network after 'wifi lan on': changes allowed](../../assets/screenshots/panel-system-home-network-admin.png)](../../assets/screenshots/panel-system-home-network-admin.png)

After `wifi lan on` over USB, the Wi-Fi card says that changes from the home network are allowed.

[![Home network form: the panel checks the name before sending](../../assets/screenshots/panel-system-home-network-error.png)](../../assets/screenshots/panel-system-home-network-error.png)

The panel checks the home network form before it sends anything; the warning above it explains
why the home network is less safe.

[![Password change: the two new passwords differ](../../assets/screenshots/panel-system-password-mismatch.png)](../../assets/screenshots/panel-system-password-mismatch.png)

The new password has to be typed twice.

[![Password change: confirmation](../../assets/screenshots/panel-system-password-dialog.png)](../../assets/screenshots/panel-system-password-dialog.png)

The password is also the Wi-Fi key, so the dialog warns that every device must reconnect.

[![Password changed](../../assets/screenshots/panel-system-password-changed.png)](../../assets/screenshots/panel-system-password-changed.png)

After the change the device network restarts with the new key.

[![Language picker with the six panel languages](../../assets/screenshots/panel-system-language-list.png)](../../assets/screenshots/panel-system-language-list.png)

Six languages; the choice is stored in the device, so every phone gets the same one. The list is
shown open here for the picture.

[![Panel switched to German, with the machine translation note](../../assets/screenshots/panel-system-language-de.png)](../../assets/screenshots/panel-system-language-de.png)

A machine translated language shows a note at the bottom of the System tab.

## Settings backup

[![Settings backup card](../../assets/screenshots/panel-system-settings-backup.png)](../../assets/screenshots/panel-system-settings-backup.png)

The backup file holds the language, the beep volume and the key table, never passwords or networks.

[![Settings exported to a file](../../assets/screenshots/panel-system-backup-exported.png)](../../assets/screenshots/panel-system-backup-exported.png)

Save to file names the file with the date and time.

[![Importing a settings file: confirmation](../../assets/screenshots/panel-system-backup-import-dialog.png)](../../assets/screenshots/panel-system-backup-import-dialog.png)

Loading a file asks first and says which fields are replaced.

[![Settings imported](../../assets/screenshots/panel-system-backup-imported.png)](../../assets/screenshots/panel-system-backup-imported.png)

The device stores the values when the car does not need it.

[![Import refused by the device](../../assets/screenshots/panel-system-backup-import-rejected.png)](../../assets/screenshots/panel-system-backup-import-rejected.png)

The device checks the whole file before it changes anything; here the volume is below the lowest
stored value.

[![Import of a file that is not a settings backup](../../assets/screenshots/panel-system-backup-import-bad-file.png)](../../assets/screenshots/panel-system-backup-import-bad-file.png)

A file that is not a JSON object is refused in the browser.

## Firmware update

[![Firmware update wizard before a file is chosen](../../assets/screenshots/panel-ota-empty.png)](../../assets/screenshots/panel-ota-empty.png)

Three steps: choose the file, let the panel check it, flash and wait. The top of the card shows
both firmware slots.

[![Wizard step 2: the file is not a firmware image](../../assets/screenshots/panel-ota-not-firmware.png)](../../assets/screenshots/panel-ota-not-firmware.png)

A file without the ESP32 image header is stopped at step 2, so a `-merged.bin` or a photo never
reaches the device.

[![Wizard step 2: firmware of another product](../../assets/screenshots/panel-ota-wrong-product.png)](../../assets/screenshots/panel-ota-wrong-product.png)

An ESP32 image without this project's marker is refused too.

[![Wizard step 2: file too short](../../assets/screenshots/panel-ota-too-short.png)](../../assets/screenshots/panel-ota-too-short.png)

A cut download is too short and is refused.

[![Wizard: file checked, same mode, ready to send](../../assets/screenshots/panel-ota-ready.png)](../../assets/screenshots/panel-ota-ready.png)

A file of the same kind as the running one goes straight to step 3.

[![Wizard: a decoder file for a listen-only device needs an explicit tick](../../assets/screenshots/panel-system-firmware-wizard.png)](../../assets/screenshots/panel-system-firmware-wizard.png)

A different operating mode is shown in red and needs its own tick before the button unlocks.

[![Wizard: a file that can transmit on CAN needs its own tick](../../assets/screenshots/panel-ota-cantx-ack.png)](../../assets/screenshots/panel-ota-cantx-ack.png)

The same for a file that can transmit on the car bus.

[![Wizard: a file without the network panel](../../assets/screenshots/panel-ota-nonet-ack.png)](../../assets/screenshots/panel-ota-nonet-ack.png)

A file without the network panel means the next update needs a USB cable.

[![Wizard step 3: last confirmation before sending](../../assets/screenshots/panel-ota-confirm-dialog.png)](../../assets/screenshots/panel-ota-confirm-dialog.png)

The last question repeats the version and the mode and asks you to flash while parked.

[![New firmware running, waiting to be confirmed](../../assets/screenshots/panel-ota-pending.png)](../../assets/screenshots/panel-ota-pending.png)

After the restart the new firmware runs on trial; it confirms itself after about 20 s of normal work.

[![Update finished and confirmed; rollback offered](../../assets/screenshots/panel-ota-done.png)](../../assets/screenshots/panel-ota-done.png)

Done: the other slot now holds the previous firmware and "Roll back to the previous one" is active.

[![Rollback to the previous firmware: confirmation](../../assets/screenshots/panel-ota-rollback-dialog.png)](../../assets/screenshots/panel-ota-rollback-dialog.png)

Rolling back names the version, build and hash it will start.

## Restart and lost connection

[![Restart of the device: confirmation](../../assets/screenshots/panel-system-restart-dialog.png)](../../assets/screenshots/panel-system-restart-dialog.png)

In the decoder build a restart drops the radio ACC for a moment, so the panel asks first.

[![Restart scheduled: the panel waits for the device](../../assets/screenshots/panel-system-restart-pending.png)](../../assets/screenshots/panel-system-restart-pending.png)

The panel counts the seconds until the device answers again.

[![Device back after the restart: the console marks it](../../assets/screenshots/panel-system-restarted.png)](../../assets/screenshots/panel-system-restarted.png)

Back again: the console marks the restart with a line of its own, and the start-up lines of the new
boot follow with the device time from zero.

[![No connection to the device](../../assets/screenshots/panel-offline.png)](../../assets/screenshots/panel-offline.png)

When the device stops answering, a red banner appears and the panel keeps the last values until
the connection returns.

## On a phone or tablet

[![Dashboard on a phone](../../assets/screenshots/panel-phone-dashboard.png)](../../assets/screenshots/panel-phone-dashboard.png) [![Continuous tone on a phone](../../assets/screenshots/panel-phone-dashboard-park-continuous.png)](../../assets/screenshots/panel-phone-dashboard-park-continuous.png)

[![CAN tab on a phone](../../assets/screenshots/panel-phone-can.png)](../../assets/screenshots/panel-phone-can.png) [![Radio tab on a phone](../../assets/screenshots/panel-phone-radio.png)](../../assets/screenshots/panel-phone-radio.png)

[![Console on a phone](../../assets/screenshots/panel-phone-console.png)](../../assets/screenshots/panel-phone-console.png) [![System tab on a phone](../../assets/screenshots/panel-phone-system.png)](../../assets/screenshots/panel-phone-system.png)

[![Arming dialog on a phone](../../assets/screenshots/panel-phone-dashboard-cantx-arm-dialog.png)](../../assets/screenshots/panel-phone-dashboard-cantx-arm-dialog.png) [![Firmware check on a phone](../../assets/screenshots/panel-phone-ota-ready.png)](../../assets/screenshots/panel-phone-ota-ready.png)

[![Login on a phone](../../assets/screenshots/panel-phone-login.png)](../../assets/screenshots/panel-phone-login.png) [![No connection on a phone](../../assets/screenshots/panel-phone-offline.png)](../../assets/screenshots/panel-phone-offline.png)

The tabs move to the bottom of the screen and the cards stack in one column. The same panel opens
in the browser of the radio.

[![Dashboard on a tablet: reverse gear, rear sensors beeping](../../assets/screenshots/panel-tablet-dashboard.png)](../../assets/screenshots/panel-tablet-dashboard.png)

On a tablet the cards stand in two columns.

## Make the pictures again

Every picture on this page comes from one command, run in the repository root:

```sh
uvx --with playwright==1.65.2 --with pillow==13.0.0 python -I tools/make_screenshots.py
```

It needs only [uv](https://docs.astral.sh/uv/) and Microsoft Edge or Google Chrome. The scenes are
listed in `tools/screenshots.json`; see [Screenshots](../contributing/screenshots.md).
