# Pebble 1.2.0: synchronized large digits

Date: 2026-09-06. Both packages retain their UUIDs, existing settings, and haptic
waveforms. During playback, one high-contrast numeral fills the screen, with its
encoded unit and any necessary digit-position or binary-weight caption.

## Measured for 1.2.0

- SDK 4.33.1 / CLI 5.0.39 builds the app and face for all six targets. The app
  remains below 26 KB static RAM on every target; the face remains below 16 KB.
- All 420 encoder, 420 motor, 420 cued, and 840 scoped fixtures still match.
- `tests/check-reading.sh` covers 90,720 day/speed/scope combinations. Every
  visual transition begins on an actual encoded pulse boundary. Golden examples
  cover all seven language choices, empty fields, midnight, binary weights,
  Morse digit positions, and serialized two-digit values.
- The phone contract passes. All 57,600 existing clock-renderer cases pass,
  plus giant-digit and caption bounds on rectangular, circular, and reduced areas.
- Both staging scripts run the readout and renderer checks before packaging.
  The face checks every platform binary for the annotated encoder.

## Observed for 1.2.0

On the Time 2 emulator, the known 3:32 whole-time lesson shows **3 / HOURS**,
then **2 / QUARTERS**. The log records that change at 1,860 ms, matching the
encoded boundary. The lesson returns after playback or Select-to-stop.

The 2 SE emulator exercised the guide and Practice, including repeated changes
that replace an active answer. The twelve-hour example shows **1**, then **2**
at 2,880 ms; the caption identifies `12` and the current digit. Select stops
playback and restores Practice. Time Round retains both the giant quarter digit
and its caption inside the circle. Round 2's face opens the readout after a
double-tap and restores the clock afterward. The emulator phone simulator logged
nonfatal packet-footer decoding warnings during some captures; watch-side
playback, transitions, and cancellation continued.

Screenshots: [Time 2 hours](docs/readout/time2-hours.png),
[Time 2 quarters](docs/readout/time2-quarters.png),
[2 SE first digit](docs/readout/2se-twelve-first.png),
[2 SE second digit](docs/readout/2se-twelve-second.png),
[2 SE stopped](docs/readout/2se-practice-stopped.png),
[round quarters](docs/readout/round-quarters.png),
[round stopped](docs/readout/round-stopped.png),
[Round 2 face playing](docs/readout/round2-face-playing.png),
[Round 2 face restored](docs/readout/round2-face-return.png).

## Installed on physical watches

Time 2 and 2 SE both accepted the same final **1.2.0 watchapp** through Developer
Connection. Each transfer reported `App install succeeded`. The package comes
from committed source `289d45e`, merged into the canonical repository as
`dea2d51`; the merged tree matches the release source exactly.

The Time 2's initial screenshot showed Pebble's Ping screen. The readout images
above come from emulators. Luke is testing readability, display-to-motor timing,
and pattern recognition on both watches. Round-watch hardware checks and a
physical install of the 1.2.0 face remain pending.

| Package | SHA-256 |
|---|---|
| `pulsetime.pbw` | `84b74b820c12476fe9ce0a49378558a5dd989d84b40c8570d5256aaf36befb6e` |
| `pulsetime-face.pbw` | `203426e5cf0aee3153ea03049e5c8b37bd9ef0fc466f910b29a7dedad83ba654` |

The large display describes each encoded group; it does not count every pulse.
Two-digit group values change on pulse boundaries partway through the same
phrase, while the caption keeps the whole value visible. The learned vibration
timings remain unchanged. Neither package has a store release yet.

---

# Pebble 1.1.0 validation

Date: 2026-09-04. Source ownership: this repository's `appstores/pebble/`
directories; CloudPebble mirrors materialize the shared encoder and display
header. UUIDs and all prior setting keys are preserved.

## Measured

- Pebble Tool 5.0.39, SDK 4.33.1. Platforms doctor returned ready; its only
  warning was the optional missing `platforms.yml`.
- Clean native builds for both packages on basalt, chalk, diorite, emery,
  flint, and gabbro. Only the toolchain's existing RWX linker warnings remain.
- 420 complete encoder fixtures, 420 motor conversions, 420 cued conversions,
  cue shape checks, and 840 scoped fixtures pass without changing the encoder.
- Full-day rhythm audit: 48 intended quarter buckets; 720 distinct duration
  sequences for each exact language. See [HAPTICS.md](HAPTICS.md).
- Phone contract test: every enum as numeric/string values, invalid-value
  rejection, defaults, exact toggle, theme/style mirroring, and stable message
  keys pass.
- Shared renderer: 57,600 time/layout cases pass, including 12/24-hour
  conversion, inverse colors, circular bounds, and smaller available areas.
- ZIP integrity, v1.1.0 metadata, UUIDs, and six platform binaries verified in
  each PBW. The app includes six worker binaries; the face has none.

## Observed in emulators

| Target | Watchapp | Watchface |
|---|---|---|
| emery / Time 2 | Guided field playback; large time; menu; complete settings round trip; on-watch language and display changes | Installed, selected, large bright face |
| diorite / Pebble 2 SE | Guide, known hour/minute playback, guide completion, Back, menu, large time; older-button explanation | Installed, large monochrome face |
| chalk / Time Round | Guide, hour lesson, Back, menu, large time within circle | Installed, large face within circle |
| gabbro / Round 2 | Guide, known hour/minute/zero/quarter playback, setup pages, guide completion, large time | Installed, selected, large face within circle |

Time 2's live settings check used the full dictionary, including long language
and scope strings, quiet hours, display settings, and a scheduled chime choice.
Every value returned correctly. Choosing Decimal on the watch reported and
persisted `decimal_place`; choosing Dark / bold reported both display fields.
The test then restored the simple defaults and disabled test chimes.

Playback logs confirmed the saved Terse Quarter lesson:

```text
hours   3:32 -> 3 segments, 5 motor slots
minutes 3:32 -> 2 segments, 3 motor slots
full    3:32 -> 6 segments, 9 motor slots
minutes 3:00 -> 1 segment, 1 motor slot
minutes 3:15 -> 1 segment, 1 motor slot
```

The ten-page guide includes both zero and one-quarter comparisons. Select on
the introduction/setup pages tells the current time, so learning never blocks
the primary action. A final screenshot check caught and corrected a wrapped
heading on the 144-pixel 2 SE display.

Screenshots: [2 SE guide](docs/guide-2se.png),
[Time 2 menu](docs/settings-time2.png), [Time 2 clock](docs/large-time2.png),
[2 SE clock](docs/large-2se.png), [Round clock](docs/large-round.png),
[Round 2 clock](docs/large-round2.png). Watchfaces:
[Time 2](../pulsetime-face/docs/bold-time2.png),
[2 SE](../pulsetime-face/docs/bold-2se.png),
[Round](../pulsetime-face/docs/bold-round.png),
[Round 2](../pulsetime-face/docs/bold-round2.png).

Test environment recovery: old persisted emulator flash caused launch failures;
it was backed up before restoring the SDK test images. A bulk screenshot run
timed out, so missing states were checked with individual screenshot commands.
On newer emulator firmware, installing a face required selecting it in the
Watchfaces menu before capturing it.

## Physical checks still needed

This is emulator evidence, not a new physical-watch test. Time 2 and 2 SE wrist
tests must judge readable size, pulse comfort, short-pulse perception, and the
300 ms zero versus 320 ms quarter distinction. No mathematical uniqueness
result proves those signals are distinguishable by touch. Quick Launch's
operating-system assignments remain a wearer setup step; the 2 SE cannot
report which button launched the app. A Pebble cannot provide spoken guidance
on hardware without a speaker; phone instructions support learning there.

No store submission is claimed. Release packages are staged locally after
validation; developer-account upload remains a human action.
