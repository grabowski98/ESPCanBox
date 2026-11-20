# Porting checklist

The whole way from "a car nobody has measured" to "the device works in place of the CAN box", on
one page. Go from top to bottom. Each row says when the step is done. Do not skip a row because
another car "probably" works the same: every fact about your car needs a status, **measured** or
**unverified**.

Write your results into a copy of `vehicles/_template/README.md` as you go. Commands with
`tools/` run from the folder named in the row: the repository folder or `firmware`.

> [!CAUTION]
> **Nothing on this list transmits on the car bus**
>
> Every build used here only listens on the car CAN bus. Your own profile cannot be built with
> transmission at all. Transmitting for a new car is a separate project with its own rules:
> [Transmitting on the CAN bus](../safety/transmit.md).

## 1. The bus

| Step | Done when | Page |
|---|---|---|
| Find the CANH and CANL wires at the radio | continuity measured from the connector to a known point, and frames received on them | [Find your bus](find-your-bus.md) |
| Measure the line voltages to ground, ignition on | CANH and CANL voltages written down | [Find your bus](find-your-bus.md) |
| Decide high-speed or low-speed fault-tolerant | conclusion written as a conclusion; oscilloscope check noted as done or still to do | [CAN connection](../install-in-car/can-connection.md) |
| Measure the terminator of the old CAN box | resistance CANH-CANL, unpowered, written down | [Find your bus](find-your-bus.md#check-the-terminator-of-the-old-box) |
| Build and flash `listen_generic_wifi` with your first guess of the bit rate | console shows `# profile: car=generic radio=none` | [Flash the firmware](../quick-start/flash.md) |
| Find the bit rate | frames received without errors at one of 50, 100, 125, 250, 500 or 1000 kbit/s | [Find your bus](find-your-bus.md) |
| Check the bus goes quiet with the key out | time from key out to silence, from a recording | [Find your bus, step 4](find-your-bus.md#step-4-when-is-the-bus-awake) |

## 2. Recordings

| Step | Done when | Page |
|---|---|---|
| Plan the steps | written plan: one action per step, each repeated three times | [Record the bus](record.md) |
| Record with markers | file with one marker per step; lost-frame count checked | [Record the bus](record.md) |
| Find the frames that carry the VIN | IDs written down, or "not found" | [Share your car](contribute.md) |
| Clean the recording (from the repository folder) | `python -I tools/sanitize_capture.py --check` reports 0 problems, with your VIN frame IDs in `--ids` | [Share your car](contribute.md) |

## 3. Signals

Decode only what you need. In order of usefulness:

| Signal | Done when | Used for |
|---|---|---|
| Ignition and engine state | byte and values for off / on / engine running, seen in three repeats; [frame period](decode.md#how-often-a-frame-comes) known (must be under 1 s) | ACC to the radio |
| Reverse gear | byte.bit, seen three times | reverse output (camera) |
| Lights | byte.bit; [frame period](decode.md#how-often-a-frame-comes) known (must be under 1 s) | illumination output |
| Steering wheel buttons | one byte.bit pattern per button, none present at rest | keys to the radio |
| Knobs | counter byte, direction per detent | next / previous |
| Parking sensors | status, zones 0 to 7 per sensor, sound request, interval if the car sends one; [frame period](decode.md#how-often-a-frame-comes) (must be under 400 ms) | beeper, sensor picture on the radio |
| Doors, boot, bonnet (optional) | one bit per door, seen open and closed for each door | door picture on the radio |
| Outside temperature, climate, economy mode, settings, trip computer (optional) | byte and scale for each value, checked against the car's own display | the matching radio screens |

Each signal goes into the vehicle page with its frame, `byte.bit`, values, status and the
recording step that showed it. [Decode frames](decode.md) shows how.

## 4. Profile

| Step | Done when | Page |
|---|---|---|
| Copy `firmware/lib/car_example` to `firmware/lib/car_<name>` and rename | files, object, namespace and `library.json` name renamed | [Write a car profile](write-a-profile.md) |
| Fill in only measured signals | every unknown pointer is `nullptr`, every unknown table empty | [Write a car profile](write-a-profile.md) |
| Byte numbers converted | your notes (byte 1 = first) turned into code indexes (`data[0]`, `byteIndex 0`) | [Write a car profile](write-a-profile.md) |
| `powerOutputs` decided | `true` only with a reliable `decodeBody` | [Write a car profile](write-a-profile.md) |
| Build `listen_mycar` | `SUCCESS`, `car_<name>` in the dependency list | [Write a car profile](write-a-profile.md) |

## 5. Tests on the PC

| Step | Done when | Page |
|---|---|---|
| Host test on your cleaned recording | output read line by line against your markers, then stored in `firmware/test_host/expected/` | [Test your profile](test-your-profile.md) |
| All host tests (from `firmware`) | `All tests match the references.` | [Host tests](../testing/host-tests.md) |
| Layering check (from `firmware`) | `python -I tools/check_layering.py` ends with `Layering OK.` | [Test your profile](test-your-profile.md) |
| Radio codes checked | each of your buttons gives the code you want; default remappings (`keyRemapDefaults`) only the ones you want, with `keyRemapProtocol` naming the protocol whose codes they are. With a radio protocol of your own (`-DRADIO_PROTOCOL=CUSTOM`): copy `firmware/test_host/radio_example.cpp` to a test of your own, link your `protocol_<name>.cpp` instead of `protocol_example.cpp` (the `radio_example` lines in `firmware/tools/run_host_tests.sh` show the build), and check that the radio line shows only your framing, no XP frame | [Map the radio](radio-mapping.md) |

## 6. Bench and car

| Step | Done when | Page |
|---|---|---|
| Bench test with your build | start-up lines show your profile and `# can=ok`; 12 V outputs off in the listen build | [Bench test](../quick-start/bench-test.md) |
| In the car beside the CAN box (`listen_mycar`) | your buttons appear with `keys`, ignition and reverse in the console, parking beeps from the device speaker; nothing sent to the radio | [First drive](../install-in-car/first-drive.md) |
| Decoder mode (`decoder_mycar`, CAN box unplugged) | radio switches on and off with the ignition; buttons, reverse camera and sensor picture work | [Radio connection](../install-in-car/radio-connection.md) |
| Notes of what does not work | listed in "Not known yet" of your vehicle page | [Share your car](contribute.md) |

## 7. Share

| Step | Done when | Page |
|---|---|---|
| Vehicle page | `vehicles/<make>-<model>-<year>/README.md` with a status on every row | [Share your car](contribute.md) |
| Private data check (from the repository folder) | `python -I tools/scan_private.py .` with 0 errors | [Share your car](contribute.md) |
| Pull request or issue | opened with the "New vehicle" template | [Share your car](contribute.md) |

A vehicle page alone, without a firmware profile, is a useful contribution too.

Next: [Share your car](contribute.md).
