# Choosing a haptic language on Pebble

Start with **Terse Quarter, Standard speed, whole-time playback**. Learn the
hour pulses, the pause, and the quarter pulses before adding exact minutes.
At 3:32, that default says 3:30. It deliberately groups 3:30–3:44 together;
the screen still shows the current minute.

For exact minutes, **try Decimal Place Rhythm next**. Its hours → minute
tens → minute ones follows familiar place value, and its average playback is
slightly shorter than the two other counting choices. This is a teaching
recommendation, not a demonstrated perceptual advantage. A wearer who already
knows Morse or binary may prefer that existing vocabulary.

## What the encoder measures

Run from the repository root:

```sh
bash appstores/pebble/pulsetime/tests/evaluate-patterns.sh
```

The audit enumerates every minute of a 24-hour day at Standard speed, converts
each pattern to the motor's actual ON/OFF duration list, and counts pulses,
playback time, and unique waveforms. Timing ends with the last motor pulse;
it excludes trailing silence, app launch time, and confirmation cues.

| Language | Min seconds | Mean seconds | Max seconds | Pulses | Unique patterns |
|---|---:|---:|---:|---:|---:|
| Terse Quarter | 0.20 | 4.05 | 7.70 | 1–15 | 48 |
| Terse Quarter + exact | 0.20 | 5.83 | 10.73 | 1–29 | 720 |
| Five-Minute Ladder | 0.24 | 5.93 | 11.45 | 1–27 | 720 |
| Decimal Place Rhythm | 1.58 | 5.69 | 10.41 | 3–26 | 720 |
| Binary Clock | 2.94 | 3.43 | 3.88 | 10 | 720 |
| Nearest-Quarter Offset | 1.08 | 5.85 | 10.02 | 2–25 | 720 |
| Morse Digits | 4.34 | 6.07 | 8.84 | 15–20 | 720 |

Every exact language has a unique duration sequence for each minute of a
12-hour cycle, even after removing amplitude information. Each repeats twice
per day. Terse Quarter has 48 intentional quarter-hour groups, each occurring
30 times per day. **None encodes AM/PM**, including when the display is set to
24-hour time. These checks establish mathematical distinction, not that a
person can reliably feel each distinction on their watch.

## What to compare by touch

| Language | Useful property | Learning or perception cost |
|---|---|---|
| Terse Quarter | Two counting fields; no tiny minute ticks by default | Quarter precision only; counting 12 hours takes attention |
| Terse Quarter + exact | Keeps the familiar hour and quarter structure | Up to 14 short remainder ticks and 29 pulses in all |
| Five-Minute Ladder | Counts familiar groups of five with leftover minutes | Up to 11 five-minute pulses; the longest worst-case playback |
| Decimal Place Rhythm | Familiar tens and ones; exact at every minute | Zero is a double-length pulse; field gaps and grouping gaps need practice |
| Binary Clock | Fixed 10 pulses; shortest mean and worst case among exact choices | Requires tracking bit positions and summing values; short/long errors change time substantially |
| Nearest-Quarter Offset | At most seven offset minutes to count | Adds a before/after marker and a landmark; the hour phrase can name the next hour |
| Morse Digits | Reuses Morse numeral knowledge | 15–20 pulses and multiple digit boundaries; learning Morse only for this is extra work |

Pebble motors expose duration but not adjustable strength. Small timing
differences, the fit of a watch strap, motor behavior, and individual sensation
therefore matter. Short pulses range down to 70 ms at Standard speed and
54 ms at Fast in Nearest-Quarter Offset. Slow lengthens the whole pattern by
25%; Fast shortens it by approximately 22%. Neither is proven best for every
motor or wearer.

### Minutes-only zero is a known weak distinction

In Terse Quarter at Standard speed, an empty minute phrase produces one
**300 ms** pulse. One quarter produces one **320 ms** pulse. The 20 ms gap is
small and must not be treated as an established tactile distinction. Slow or
Fast scales both; neither introduces a separate marker.

This affects Down on the clock and a minutes-only Quick Launch. With exact
minutes off, the empty phrase covers :00–:14 and the one-quarter phrase covers
:15–:29. With exact minutes on, :00 and :15 still have that close pair.
Whole-time playback at :00 ends after the hours, while :15 adds a separated
quarter pulse. **Use Select for the whole time when unsure, and keep Quick
Launch on whole time until both cases have been compared on the wrist.**

The vibration timings remain unchanged so an already learned language stays
consistent across watchapp, watchface, and the other PulseTime platforms.
Any future zero-marker change needs a deliberate migration across that shared
contract and evidence from wearers, including on Pebble 2 SE.

## A short learning sequence

1. Open the watchapp, hold Select for the menu, and choose **Learn pattern**. Use Down to move
   through the guide, Up to return, Select to play or stop, and Back to leave.
2. Start with the known 3:32 example. In the default language its hour phrase
   has three pulses and its minute phrase has two quarter pulses: it reports
   3:30. Replay the whole pattern and locate the long pause.
3. In **Practice**, compare 3:00, 3:15, 3:30, and 3:45. At 3:00 the full
   pattern stops after three hour pulses. At 3:30 it adds two quarter pulses.
4. Only then compare exact choices using the same known examples. Learn each
   language's zero convention and boundary cases before using a partial phrase.
5. Choose a normal daily language. Keep the whole-time Select action as the
   direct recovery path when a partial or interrupted pattern is unclear.

The phone guide provides the same controls in text that can be read with a
screen reader. A numeric haptic cue can identify a menu position once learned;
it cannot explain a new menu or announce arbitrary prose by itself. A sighted
screen walkthrough alone does not establish independent blind onboarding.

## Physical comparison still needed

Test the **Time 2 and Pebble 2 SE separately**, with the same strap fit and
chosen examples. Use a round watch when available; emulator screenshots can
validate its visible layout but cannot evaluate its motor.

Begin with known examples and unlimited replay. Then hide the answer and
present examples in mixed order: 1:00, 3:00, 3:15, 3:30, 3:32, 3:44, 11:59,
12:00, and 12:01. Include minutes-only :00 and :15 as an explicit comparison,
not as a prerequisite that a user must pass to use whole-time playback.

Record the decoded time, whether the intended precision was understood,
number of replays, missed/merged pulses, and confusion between fields. Compare
Standard with Slow first; try Fast only after recognition is comfortable.
For settings, verify that the wearer can return to the clock and recover their
chosen language without reading the screen. For sound, separately check mute
and switching between modes during playback on Time 2.

Do not select a universal winner from these source measurements. Prefer the
language the wearer reads consistently with the least effort. Preserve the
observed disagreements between devices and wearers when changing defaults.
