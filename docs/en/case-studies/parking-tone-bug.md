# The stuck parking tone

This is the story of a fault on the reference car (Peugeot 508 SW, 2015) with an aftermarket
Android radio and its CAN box. It is told with the wrong turns, because they are the useful part.
**The cause was never found.** What was found is how to trigger it, what clears it, and that one
early conclusion was wrong.

All observations were made by the author in the car, unless a recording is named. "Measured" here
means seen on the car; most trials were repeated only a few times.

## Background

The new radio did not beep for the parking sensors. To get the beeps back, the car was set in the
dealer diagnostic tool to play them through the switch module under the steering wheel. After that
the parking tone sometimes got stuck. A second problem, with the remote key, was also seen with
the box installed; when it started is not recorded.

## The symptoms

**The tone that does not stop.** With an obstacle in front of the car, switching the ignition on
or off sometimes left the continuous parking tone sounding:

- the sensor display on the radio cleared, but the tone stayed;
- it stayed above 10 km/h, with the sensors switched off, with the ignition off, with the car
  locked, for about 10-15 minutes;
- fastening the seat belt interrupted it for a moment;
- disconnecting the battery stopped it;
- the sensors themselves worked.

**The remote key.** After 1-2 hours of parking the remote did not unlock the car. After opening
with the mechanical key it worked again.

## Attempt 1: take the box out

With the CAN box unplugged, the tone did not get stuck and the remote worked (measured: a few
trials). The conclusion at the time: **the box causes both problems.**

This conclusion was too fast. It rested on a small number of trials, and part of it was later
proved wrong (attempt 6).

## Attempt 2: the standby pin

The box has one CAN transceiver with a standby input. The idea: maybe the box puts its transceiver
into a state that disturbs the bus. The standby pin was cut from its pad and tied to ground, so the
transceiver always stays in normal mode. See [Inside an aftermarket CAN box](inside-a-can-box.md).

Result (measured: one trial each):

- the tone still got stuck, reproducibly: obstacle in front, quick reverse, engine off;
- it now stopped about **1 minute** after the ignition was switched off, instead of 10-15 minutes;
- the remote still failed after 15 minutes of parking.

The modification changed the length of the stuck tone, not the fault.

## Attempt 3: unplug the box while the tone is stuck

Tone stuck, ignition off, box unplugged: **the tone went on.** So the box may trigger the fault,
but the car itself holds the stuck state, not the box.

Later the same with the engine running: unplugging the box did not stop a stuck tone either. It
stopped only after the box was plugged in again and the engine was restarted.

## Attempt 4: narrowing down the trigger

A firmware update of the box gave a lucky observation. While the box sat in its bootloader (off the
bus), the author drove towards an obstacle, and the tone got stuck **with the ignition on**. No
switch-off was needed.

More trials followed. Most rows below are single trials, so they show what can happen, not how
often:

| Trial | Result |
|---|---|
| Several approaches to an obstacle, front at the closest zone, engine off, engine on | stuck after the restart |
| Switch off and on with an obstacle in the zone of **intermittent** beeps | never stuck |
| Box unplugged during intermittent beeps | beeps stopped normally |
| Box unplugged during the **continuous** tone, then drive away | tone stayed |
| Box plugged back in, drive away | tone stopped |
| Box unplugged **before** the tone, then approach and leave | tone stopped normally |
| Box connected, engine off during the continuous tone | once stopped at once, once stayed |
| Engine off within about 1 s of reaching the continuous tone | stayed |
| Morning start with an obstacle in front (recalled by the author, not a planned trial) | the tone started and stayed |

What this showed:

- only the **continuous** tone (the closest zone) gets stuck, never the intermittent beeps;
- a change of ignition state during the continuous tone can trigger it, both switching off and
  starting; not every time, and the rule behind it was not found;
- the box disappearing from the bus during the continuous tone also triggers it.

## Attempt 5: what clears it

Two ways were found (measured):

- **A new continuous tone that ends normally.** With a stuck tone, drive to the closest zone for
  about 2 seconds and back off: the tone stops. Repeated twice in one session.
- **Waiting.** In one trial with the ignition off the tone stopped by itself after about 1-1.5
  minutes, at the moment the car switched the accessories off.

## Attempt 6: the tone got stuck without any box

The box had been unplugged for about an hour and the radio was off. The author approached an
obstacle, backed off, approached the continuous tone again and switched off quickly. **The tone
stayed.** It stopped by itself after about 1 minute.

This overturned the conclusion of attempt 1 for the tone. **The car can get the tone stuck on its
own**, with nothing connected at the radio. Whether the box makes it more frequent is not known:
the trials without the box were too few to say.

## Attempt 7: look at the bus

A listen-only sniffer recorded the bus at the radio during five switch-offs in the continuous
tone. In two of them the tone stayed.

In **all five**, at the moment of switch-off, the parking module sent frame `0E1` **without** a
sound request (`D8 00 3F FC FC FC C2 00`), also in the two trials where the tone stayed (measured:
sniffer recordings).

So the stuck tone is not requested by any frame on this bus. It is held further inside the car:
the body control unit (BSI) or the module under the steering wheel. A public description (PSA-RE)
says the BSI tells that module which sound to play with frame `102` on a different bus, sent only
when the sound changes (unverified: PSA-RE `LS.CAR`). That bus does not reach the radio connector,
so this project cannot see it.

The tone stayed only in trials where the engine had run for more than 30 seconds before the
switch-off. Five trials are too few to call that a rule.

## The remote key

The remote problem followed a different path:

- without the box, the remote worked, also on the day the tone got stuck without the box;
- with the box and the standby modification, it failed after 15 minutes of parking;
- with an older firmware on the same box (standby modification still in place), the remote worked
  after 16 minutes and after about
  30 minutes of parking. During the 16 minutes a sniffer showed the bus and the box completely
  silent (measured: recording during parking);
- with a different CAN box (Raise), the remote worked after every parking, up to 90 minutes and
  then overnight.

On the day the remote started to work again, three things had changed at once: the box firmware,
the battery charge after a longer drive, and a sniffer on the bus. The author's guess that the
battery charge mattered is **unverified**. The cause is not known.

## Where it stands

- **Cause: unknown**, for both the tone and the remote.
- The setting that plays parking beeps through the module under the steering wheel was switched off
  again in the dealer tool. Whether the tone can still get stuck in this state was not tested.
- This device plays its own parking beeps from frame `0E1` through its own speaker. Its beeper goes
  silent 400 ms after the last `0E1` frame (host test `beeper_scale`), so it cannot hold a tone
  after the car goes quiet. It does not fix the car's own fault.

## What we learned

- **A few trials are not a proof.** "The box causes it" rested on a handful of trials and was half
  wrong.
- **Change one thing at a time.** The remote "fix" came with three changes at once, so nobody knows
  which one helped.
- **Record the bus before you blame a frame.** The recording showed in one evening that the tone
  was not requested on the bus at the radio.
- **Write down what you did not test.** "Unplugging the box with the engine running" was a gap in
  the first notes, and it changed the picture when it was finally tried.
