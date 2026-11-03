# PulseTime Wake for Pebble (prototype)

A gentle vibration alarm and a work/rest interval timer that work without
looking at the watch. It is a separate watchapp in the PulseTime family, built
on PulseTime's encoder (`src/c/encoder.*`, `display.h`, `reading.h` and
`readout.h` are symlinks into `../pulsetime/src/c/`), so the only times it
states are in PulseTime's default language: Terse Quarter at Standard speed.

Version 0.1.0 is an emulator-tested prototype. It has not been worn, packaged
for release or listed. Nobody has felt its patterns on a real motor yet.

## Why a separate app

- PulseTime's chimes call `wakeup_cancel_all()` and fill all eight of its
  wakeup slots. An alarm inside PulseTime would have to change that scheduler
  in a published app. A separate app has its own eight slots.
- PulseTime's buttons are fully used (Up/Down/Select and the three holds).
  Wake gets its own simple button map.
- A second listing in Tools & Utilities sends alarm and timer browsers to the
  PulseTime family and links back to PulseTime.

## Controls

Launch opens a three-item list; one short pulse means item 1, two mean item 2,
three mean item 3. Up and Down move and repeat the count. Select opens.

**Wake alarm.** Opening it plays the state and then the dial time: a rising
pair (short, long) means on, a falling pair (long, short) means off.

| Button | Action | You feel |
|---|---|---|
| Up / Down | Alarm 15 minutes later / earlier | The new time, after you stop pressing |
| Hold Up / Down | One hour later / earlier | The new time |
| Select | Turn on / off | On: rising pair, then the time. Off: falling pair |
| Hold Select | Repeat the state | Rising or falling pair, then the time |
| Back | Return to the list | — |

The dial has twelve hours, like a wind-up alarm clock. Turning it on picks the
next time the dial matches, which is always less than twelve hours away, so
morning or evening is settled when you turn it on. After that it repeats every
day at that clock time until you turn it off. Set it at bedtime for the morning.

**When it rings** the app opens by itself through Pebble's wakeup service. It
starts with one faint 60 ms tick every 10 seconds and grows: two pulses every
8 seconds from the second minute, three every 6 seconds from the third, then two
long pulses every 4 seconds from 2 min 36 s. It stops by itself after 10
minutes.

| Button | Action | You feel |
|---|---|---|
| Select | Stop | The time now (hours, pause, quarters), then the app closes |
| Up or Down | Snooze 9 minutes | Two ticks, then the app closes |
| Back | Not a stop: the app closes, and the ring comes back within 90 s | The ramp, at the strength it had reached |

While it rings, the app keeps one wakeup booked 45 to 90 seconds ahead. If
anything closes it before Stop or Snooze (Back pressed half asleep, or another
app's wakeup taking the screen), that wakeup reopens the ring where it left off.
The 10-minute limit still counts from the original alarm. Opening the app by
hand cancels a pending resume.

**Intervals.** Four presets: 20 s / 10 s × 8, 30 / 30 × 10, 45 / 15 × 10, and
4 min / 1 min × 6. Up and Down choose (one to four pulses), Select starts.
Three one-second ticks lead in, then:

| Signal | Pattern |
|---|---|
| Work | One 900 ms buzz |
| Rest | Two 300 ms pulses, a pause, then one pulse per work round still to come, grouped in fives |
| Countdown | A 60 ms tick at 3, 2 and 1 seconds before each change (phases of 6 s or longer) |
| Done | Three 600 ms pulses |

Select pauses (one tick) and resumes (the phase signal). Up or Down tells where
you are: the phase signal, then the rounds still to come. Back ends the timer.

**Learn the buzzes** lists twelve signals in a fixed order: alarm on, alarm
off, snoozed, could not set, the four wake stages, work, rest with three
rounds to go, countdown, done. Up and Down move and play the new row; Select
repeats it.

## Build and test

```sh
bash appstores/pebble/pulsetime-wake/tests/check-logic.sh   # host: schedule math and patterns
pebble build                                                # SDK 4.33.1, all six targets
```

The host test checks every dial time against every minute of the day, the
daily repeat, the E_RANGE fallback order, that no cue equals any Terse Quarter
time phrase, that the ramp only grows, and that no interval signal overlaps
the next one in any preset. It prints every pattern as an ON/OFF table.

The app logs each vibration it starts as `VIBE <label> slots=<n> ms=<total>`
followed by the duration list, and each alarm decision as `WAKE ...`, so an
emulator run (which has no motor) still records exactly what would be felt.

## Known limits

- Physical feel is unverified: whether the first tick is too faint to notice
  or the last stage too harsh, and whether the rising and falling pairs are
  told apart, needs a wrist on each motor (Time 2, 2 Duo, 2 SE, Round 2).
- Another app's wakeup within a minute of the alarm moves it: one minute later,
  then one earlier, then two later or earlier. PulseTime's quarter-hour chimes
  will usually cause this, so with chimes on a 6:30 alarm rings at 6:31, after
  the chime. The screen shows the booked time; nothing is felt about the shift.
- Pebble Quiet Time behavior with a wakeup-launched vibration is unverified.
- Haptic hours have no AM/PM; the twelve-hour rule above settles it instead.
- The language is fixed to Terse Quarter; PulseTime's saved language is not
  shared between apps.
- The menu icon is PulseTime's.
