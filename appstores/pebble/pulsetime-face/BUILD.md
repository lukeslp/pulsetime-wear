# PulseTime Face (.pbw)

**The haptic clock as a watchface.** Double-tap the active face to feel the
time without launching the separate watchapp. Start with the
[wearer guide](../pulsetime/GUIDE.md) to choose a package and learn the controls.

## Synchronized digits, version 1.2.0

A deliberate double-tap, chime, or pattern preview temporarily replaces the clock
with one very large numeral and its encoded unit. The face shares `reading.h`
and `readout.h` with the app, as well as the encoder and digit renderer. Numerals
follow the unchanged vibration timings; completion restores the normal face.
Dark and Bright preferences retain full contrast. The face leaves system
buttons to Pebble.

## Large time, introduced in 1.1.0

Bright bold starts with black digits on white, hours above minutes. The phone's
PulseTime Face settings offer a classic row and a dark inverse palette. All text
uses full contrast on color and monochrome displays. Display choices save quietly
and preserve the language, speed, sound, and chime settings. Physical backlight
brightness remains a watch setting.

The face shares the watchapp's digit renderer and encoder through symlinks;
CloudPebble mirrors keep ordinary copies. The phone settings include a worked lesson,
explain that haptic hours use 1–12 even with a 24-hour visual clock, and direct
wearers to the separate watchapp for guided practice and side-button control.
The watchface does not intercept system buttons.

```sh
bash appstores/pebble/pulsetime-face/tests/check-display.sh
```

The renderer test covers all minutes in both clock formats, stacked and row
layouts, every supported screen size, circular clipping, and smaller available
bounds. See emulator screenshots and interaction checks in the
[release validation](../pulsetime/VALIDATION.md). Physical readability and tactile
perception still need testing on the wearer's watch.

## CloudPebble import

This directory is canonical. The private
[`lukeslp/pebble-pulsetime-face`](https://github.com/lukeslp/pebble-pulsetime-face)
repository is its self-contained, root-level CloudPebble mirror. The mirror
materializes the shared encoder symlinks so CloudPebble can build it without the
sibling watchapp directory. Preserve UUID
`f9ee597e-a160-43a8-b08e-6af03007487b` across both copies.

For chimes that work across watchfaces and other apps, use the watchapp's
`wakeup_schedule()` option. It launches the app briefly to play the time. See
`../pulsetime/BUILD.md`.

Every other route foregrounds something first:

| Platform | Path | What has to happen |
|---|---|---|
| Apple Watch | Taptic Time | first-party only; no third-party equivalent exists |
| Wear OS | tile → relay activity | a blank activity opens purely so it can vibrate |
| Garmin | complication → app | `Complications.exitTo()` foregrounds PulseTime |
| Pebble watchapp | Quick Launch / worker | `pebble_worker.h` has **no vibes API at all** |
| Pebble watchapp chimes | `wakeup_schedule()` | nothing resident, but the app is launched for a few seconds |
| **Pebble watchface** | **nothing** | **it is already running, and it can vibrate** |

A Pebble watchface can vibrate and receive accelerometer taps while it is
on screen. A Connect IQ watch face cannot call `Attention.vibrate()`.

## Choosing the face

- **Double-tap and feel the time.** The face plays the answer without opening another app.
- **Chimes**, without scheduling anything. Note the watchapp can chime too, via
  `wakeup_schedule()`, and its version keeps working when the wearer is in
  another app. Use the watchapp for chimes that need to work across apps.
- **No background worker**, so no worker slot taken from another app (a Pebble
  runs exactly one, watch-wide) and no second process resident all day.
- **It has to be your active watchface.** The face is an optional second
  package with its own settings and store listing.

## It only works while it is the running app

The face must be **on screen**. Pebble runs
one app at a time, and a watchface is that app only while nothing else is open.
Opening any app, or even the launcher, unloads the face: `deinit` runs, the tick
subscription and the tap subscription go with it, and neither chimes nor
double-tap work again until the wearer backs out.

Verified on the emery emulator, 2026-08-19, with hourly chime on and no quiet
period:

| State | Crossing the hour | Result |
|---|---|---|
| Face on screen | 14:00 | chime fired (`scope=hours`) |
| Face still the **selected** face, launcher open | 15:00 | **nothing** |

The face answers only while it is visible. Opening another app or the launcher
stops its gestures and chimes. Returning to the face resumes them automatically.

For chimes across apps, the watchapp uses scheduled wakeups:

- **`wakeup_schedule()`** launches the app at a set time *even when it is not
  running*. Max **8** scheduled events per app, and no event may be scheduled
  within **one minute** of another already-scheduled one (`E_RANGE`), so hourly
  chimes mean keeping a rolling window topped up. No PulseTime process remains resident
  between chimes; battery impact has not been measured. **Built in the watchapp**; see
  `../pulsetime/BUILD.md`. If chimes are what you want, that is the one to use.

## No buttons

Up and Down belong to the timeline, Select opens the launcher, Back does
nothing. A watchface gets none of them. Input here is **accelerometer taps and
the phone's Clay page**. Every setting lives on
the config page, unlike the watchapp where most are reachable on the wrist.

## The gesture

A **double tap on the same axis**, the same detector the watchapp's worker uses
and for the same reasons:

| Constant | Value | Why |
|---|---|---|
| `PT_TAP_MIN_GAP_MS` | 120 | Two taps closer than this are one knock ringing twice. |
| `PT_TAP_WINDOW_MS` | 1200 | The second tap must land within this, on the **same** axis. |
| `PT_TAP_QUIET_MARGIN_MS` | 1200 | Added to the pattern length; the face ignores taps for that long. |

The quiet window is not optional: **the motor shakes the watch hard enough to
read as a fresh tap**, so without it the face answers itself in a loop.

The DEBUG log records each tap's axis and gap. Use wrist observations to judge
whether the 1200 ms window catches deliberate gestures without frequent false
triggers:

```
PulseTimeFace: tap axis=1 (armed)
PulseTimeFace: tap axis=1 prev=1 gap=718ms
```

## Chimes

`off` / `on the hour` / `every half hour` / `every quarter hour`.

**On the hour it strikes just the hour** (`PT_SCOPE_HOURS`). At a quarter or
half hour it plays the whole time.

**Quiet hours** (default 22:00–07:00) silence chimes only. A deliberate
double-tap always answers. Setting both ends
to the same hour disables the quiet period.

## Shared encoder

`src/c/encoder.c` and `src/c/encoder.h` are **symlinks** into
`../pulsetime/src/c/`. The
canonical app and face compile the same source. Standalone mirrors need a
parity check when their materialized copies change. `stage-release.sh` runs the
watchapp's `tests/check-fixtures.sh`, refuses to build on a broken symlink, and
then checks `nm` for `pt_encode_readable` in every platform's ELF, because a
silently-unlinked encoder is the one failure a symlink build could hide.

## Sound

Inherited from the watchapp: `PBL_SPEAKER` is emery (Pebble Time 2) and flint
(Pebble 2 Duo) only, and `pt_layer_midi_note()` turns the encoder's relative
strength into pitch. A Pebble motor cannot reproduce strength differences.
Sound starts off and always respects `speaker_is_muted()`.

## Build

```sh
cd appstores/pebble/pulsetime-face
pebble build
ls -la build/pulsetime-face.pbw
nm build/emery/pebble-app.elf | grep pt_encode_readable   # shared encoder linked
```

Release build with staged artifact and checksum:

```sh
appstores/pebble/pulsetime-face/stage-release.sh
# -> appstores/dist/pebble/{pulsetime-face.pbw,SHA256SUMS.txt}
```

## Emulator preview

After installation, select PulseTime Face in the emulator and confirm it is
visible. Installation alone may leave the previous app on screen. For double
taps, use the [single-connection example](../pulsetime/BUILD.md#driving-the-worker-gesture-in-the-emulator)
with `emery` as the target. Separate CLI calls can miss the 1,200 ms window.

```sh
export PEBBLE_QEMU_PATH="$HOME/Library/Application Support/Pebble SDK/SDKs/4.33.1/toolchain/bin/qemu-pebble"
pebble install --emulator emery
pebble logs --emulator emery

# Settings, without needing the phone. send-app-message wants NUMERIC keys;
# read them from build/js/message_keys.json (CHIME 10004, QUIET_FROM 10005,
# QUIET_TO 10006, ...).
pebble send-app-message --emulator emery --string 10004=hour --uint 10005=0 10006=0
pebble emu-set-time --emulator emery 10:59:45     # then wait for the tick
```

Verified 2026-08-19 on the emery emulator: clean build for all six platforms;
the face renders time, date, system and prompt; a single tap arms and does not
play; two taps on different axes do not play; two same-axis taps 718 ms apart
play the time; with chime off, crossing 11:00 does nothing; with hourly chime
and no quiet period, 11:00 plays `scope=hours`; with quiet hours 0–23, 13:00
plays nothing.
