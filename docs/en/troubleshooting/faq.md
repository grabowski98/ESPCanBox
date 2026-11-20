# FAQ

Short answers to the questions people ask most often. The details are behind the links.

## Does the device transmit on my car's CAN bus?

Not by default. Every environment except `decoder_tx_wifi` contains no CAN transmit code at all,
and the controller runs in listen-only mode. `decoder_tx_wifi` transmits only after you arm it with
`cantx on` and restart, and only the frames on the allow list of the car profile: on the reference
car frame `15B`, checked by ID and length only. Frame `1A9` exists only in a build with
`-DCAN_TX_EXPERIMENTAL=1`. Details: [Transmitting on the CAN bus](../safety/transmit.md).

## Will it work with my car?

Only the Peugeot 508 SW of 2015 (PSA AEE2010) has a measured profile. On any other car you start
with the `generic` profile in `listen_generic_wifi`: it decodes nothing, sends nothing to the
radio and never switches the 12 V outputs. You record your bus and write your own profile:
[Make it yours](../make-it-yours/find-your-bus.md).

## Is the bus at the radio high-speed CAN?

On the reference car it runs at 125 kbit/s (measured). Its voltages, CANH 0.65 V and CANL 4.5 V
on average with ignition on, look like **low-speed fault-tolerant** CAN, not high-speed CAN, where
both lines sit near 2.5 V. This is a conclusion from a multimeter; an oscilloscope is needed to
confirm it. A high-speed 3.3 V transceiver receives this bus without errors (measured).

## Can I control the climate from the radio?

No. On the reference car single `364` frames from the device did not change the climate
(measured: several presses, 4-byte and 6-byte variants). The frames were acknowledged on the bus,
but the temperature did not change. The other direction, the climate state shown on the radio, is
implemented: 26 of 27 frames matched what the CAN box sent in a recording (host test). The
firmware has no climate frame to send: the radio climate buttons only print a console line.

## Can I change vehicle settings from the radio screen?

Only with the transmit build, armed. The body control unit (BSI) accepted `15B` requests for six
items in both directions: parking assist, rear wiper in reverse, follow-me-home lighting time,
welcome lighting time, sound, door opening (measured: the change appeared in frame `260`). What
each change does in the car was not checked.

## Do I have to remove my CAN box?

No. In the `listen` modes the device works **beside** the box: it listens to the car and to the
radio line and sends nothing. In the `decoder` modes it works **instead of** the box: unplug the box
first, because two senders on one radio line and two ACC sources short the outputs.

## Will it drain my battery?

Not known. The current of the device in sleep is not measured, and deep sleep is not tested on the
hardware yet. Wi-Fi draws about 100 mA (figure from a code comment, not a measurement of this board), which is why it switches off when the
car sleeps.

## Which environment should I flash first?

`listen` (the default of a plain `uvx platformio run -t upload` in `firmware/`): listen only, no
network. Then `listen_wifi`
for the web panel. Move to a `decoder` environment only when you have checked that the decoded
signals are right. See [Flash the firmware](../quick-start/flash.md).

## Are firmware images signed?

No. Anybody who knows the device password can upload any image over Wi-Fi. The update checks the
file against mistakes (product marker, mode, transmit marker), not against forgery. Keep the
password private. See [Security model](../safety/security.md).

## Why are some words in Polish?

The project started in Polish. A few things keep their original words because tools or files
depend on them: step markers in the recordings (`# KROK` = step), some words in the frozen test
references, KiCad net names and copper texts of prototype v1. Old Polish console commands still
work as aliases.

## Is this a product? Who supports it?

No. It is a hobby project, provided as it is, without warranty. It is not made for anything that
affects driving safety. The project is not affiliated with Peugeot, Stellantis, Simple Soft,
Raise, Junsun or FYT; those names only say what was tested.

## How do I ask for help?

Open an issue in the repository and include:

1. the output of `ver` and `stat` (and `radio` for radio problems);
2. your car, radio and environment name;
3. what you did, what you expected, what happened;
4. a short recording, **anonymised** with `tools/sanitize_capture.py`, run in the top directory
   of the repository, not in `firmware/` (see [Security model](../safety/security.md#before-you-share-anything)
   and [Car recordings](../testing/captures.md)).

Never paste the output of `wifi showpass`. Answers come when the author or another contributor has
time.

Next: problems that really happened, with checks, are in [Troubleshooting](index.md).
