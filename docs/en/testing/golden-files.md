# Golden files

A golden file is a stored, checked output. The test produces the same output again and compares
it byte for byte. Any difference means the behaviour changed, even by one millisecond.

The project has two kinds, in three directories:

| Directory | What is in it | Written by |
|---|---|---|
| `firmware/test_host/expected/` | one text file per host test: the whole program output (stdout and stderr) with a header `### test arguments` and a footer `### code=N` | `run_host_tests.sh --record-golden` |
| `firmware/test_host/golden/` | the stream test: what the device would send and do, for each recording and variant | `run_host_tests.sh --record-golden` |
| `firmware/test_host/golden_060/` | the same stream, recorded once from an early firmware version (0.6.0) | **never recorded again** |

## The stream test

`golden_stream` replays a recording in steps of 1 ms, in the same order as the main loop of the
firmware, in decoder mode. For each recording it writes three files:

| File | Content |
|---|---|
| `<capture>.radio_tx.txt` | every frame the device would send to the radio: time in ms, then the bytes in hex |
| `<capture>.outputs.txt` | every change of bus activity, of the 12 V outputs (ACC, illumination, reverse) and of the beeper, with time |
| `<capture>.console.txt` | the console lines the logic would print, with time |

Some words inside these files are Polish (for example `magistrala` = bus). They are part of
the frozen references and stay as they are.

It runs on the five recordings of `firmware/test_data/captures/` and on four variants of one of
them:

| Variant | What it simulates |
|---|---|
| `--stall 10,50` | every 10 s the main loop stops for 50 ms |
| `--stall 10,500` | every 10 s the main loop stops for 500 ms |
| `--drop 0E1` | every second parking sensor frame is lost |
| `--drop 0F6` | every second ignition frame is lost |

## Three passes

The script runs the stream test three times:

1. **Reference 0.6.0.** A frozen copy of the early program glue (`test_host/ref_glue_060.h`). It
   must give `golden_060/` exactly. This proves the test tool itself did not change.
2. **Current `Decoder` without frames `38` and `3C`.** The current code with the two features
   added later switched off. It must also give `golden_060/` exactly. This proves the rework
   changed only those two frames.
3. **Current `Decoder`.** What the device does now. It must give `golden/` exactly.

If you change the radio stream on purpose, pass 3 changes and `golden/` must be recorded again.
Passes 1 and 2 must still match: if they do not, you changed something you did not intend to.

A fourth output, `.build_host/golden_box_report.txt`, counts how many frames of the real CAN box
have a matching frame from the device. It is information only and does not fail the test.

## When to record again

Record references (`bash tools/run_host_tests.sh --record-golden`) only when:

- you added a test, or
- you changed behaviour on purpose and can say why.

Then:

1. Look at the difference in Git, file by file.
2. Write in the pull request what changed and why, for example: "frame `38` now goes out 10 ms
   later because it waits in the queue".
3. Never edit a golden file by hand.

> [!WARNING]
> **Do not touch `golden_060/` and `ref_glue_060.h`**
>
> They are the fixed point all later changes are compared with. The script never records
> `golden_060/` again; do not do it by hand either.
