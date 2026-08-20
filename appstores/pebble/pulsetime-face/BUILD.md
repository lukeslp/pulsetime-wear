# PulseTime Face (.pbw)

**The haptic clock as a watchface.** This is the only "no app at all" haptic
time path available on any watch PulseTime ships to. Author: Luke Steuber.

Every other route foregrounds something first:

| Platform | Path | What has to happen |
|---|---|---|
| Apple Watch | Taptic Time | first-party only; no third-party equivalent exists |
| Wear OS | tile → relay activity | a blank activity opens purely so it can vibrate |
| Garmin | complication → app | `Complications.exitTo()` foregrounds PulseTime |
| Pebble watchapp | Quick Launch / worker | `pebble_worker.h` has **no vibes API at all** |
| **Pebble watchface** | **nothing** | **it is already running, and it can vibrate** |

A Connect IQ watch face is barred from `Attention.vibrate()`. A Pebble
watchface is not, and it receives accelerometer taps. That single asymmetry is
the whole reason this project exists.

## What it buys, and what it costs

- **Double-tap and feel the time.** No launch, no latency, nothing to exit.
- **Chimes.** A watchapp can *never* do this — Pebble has no background
  vibration — so passive time awareness exists only here.
- **No background worker**, so no worker slot taken from another app (a Pebble
  runs exactly one, watch-wide) and no second process resident all day.
- **It has to be your watchface.** That is the entire price, and it is why this
  ships as a second store listing rather than replacing the watchapp.

## It only works while it is the running app

**Selected is not enough — it has to be what is currently running.** Pebble runs
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

So the honest scope of this face is: it answers whenever the wearer is looking
at their watchface, which is nearly always, and it goes quiet the moment they
are doing something else on the watch. It resumes by itself on the way back —
there is nothing to restart.

**If chimes need to survive that, the watchface is the wrong mechanism.** Two
things do work independently of which face is showing, both belonging to the
watchapp rather than here:

- **`wakeup_schedule()`** — launches the app at a set time *even when it is not
  running*. Max **8** scheduled events per app, and no event may be scheduled
  within **one minute** of another already-scheduled one (`E_RANGE`), so hourly
  chimes mean keeping a rolling window topped up. Costs nothing at all while
  idle: no resident process. **Built in the watchapp** — see
  `../pulsetime/BUILD.md`. If chimes are what you want, that is the one to use.
- **The background worker** — `pebble_worker.h` does expose
  `tick_timer_service_subscribe()`, so the existing worker could fire on the
  hour and `worker_launch_app()`. But it holds the watch's single worker slot
  and runs all day, which `wakeup_schedule()` does not.

## No buttons

Up and Down belong to the timeline, Select opens the launcher, Back does
nothing. A watchface gets none of them. Input here is **accelerometer taps and
the phone's Clay page**, and nothing else — which is why every setting lives on
the config page, unlike the watchapp where most are reachable on the wrist.

## The gesture

A **double tap on the same axis**, the same detector the watchapp's worker uses
and for the same reasons:

| Constant | Value | Why |
|---|---|---|
| `PT_TAP_MIN_GAP_MS` | 120 | Two taps closer than this are one knock ringing twice. |
| `PT_TAP_WINDOW_MS` | 1200 | The second tap must land within this, on the **same** axis. |
| `PT_TAP_QUIET_MARGIN_MS` | 1200 | Added to the pattern length; the face stays deaf that long. |

The quiet window is not optional: **the motor shakes the watch hard enough to
read as a fresh tap**, so without it the face answers itself in a loop.

Every tap is logged at DEBUG with its axis and gap, because tuning this is
entirely an on-wrist question — the only way to know whether 1200 ms is too
tight or too loose is to see the gaps a real arm produces:

```
PulseTimeFace: tap axis=1 (armed)
PulseTimeFace: tap axis=1 prev=1 gap=718ms
```

## Chimes

`off` / `on the hour` / `every half hour` / `every quarter hour`.

**On the hour it strikes just the hour** (`PT_SCOPE_HOURS`) — at `:00` the
minute field is empty anyway, and a bare hour is what a chiming clock has always
meant. At a quarter or a half it plays the whole time, because there the minutes
are the point.

**Quiet hours** (default 22:00–07:00) silence chimes only. A deliberate
double-tap always answers, including at three in the morning: that is exactly
when a wearer most wants the time without turning on a light. Setting both ends
to the same hour disables the quiet period.

## Shared encoder

`src/c/encoder.c` and `src/c/encoder.h` are **symlinks** into
`../pulsetime/src/c/`. One encoder, one host test, one set of fixtures — the
watchapp and the watchface can never drift apart. `stage-release.sh` runs the
watchapp's `tests/check-fixtures.sh`, refuses to build on a broken symlink, and
then checks `nm` for `pt_encode_scoped` in every platform's ELF, because a
silently-unlinked encoder is the one failure a symlink build could hide.

## Sound

Inherited from the watchapp: `PBL_SPEAKER` is emery (Pebble Time 2) and flint
(Pebble 2 Duo) only, and `pt_layer_midi_note()` turns the encoder's relative
strength — which a Pebble motor cannot express at all — into pitch. Off by
default; `speaker_is_muted()` always obeyed.

## Build

```sh
cd appstores/pebble/pulsetime-face
pebble build
ls -la build/pulsetime-face.pbw
nm build/emery/pebble-app.elf | grep pt_encode_scoped   # shared encoder linked
```

Release build with staged artifact and checksum:

```sh
appstores/pebble/pulsetime-face/stage-release.sh
# -> appstores/dist/pebble/{pulsetime-face.pbw,SHA256SUMS.txt}
```

## Emulator preview

```sh
export PEBBLE_QEMU_PATH="$HOME/Library/Application Support/Pebble SDK/SDKs/4.33.1/toolchain/bin/qemu-pebble"
pebble install --emulator emery
pebble logs --emulator emery

# Two taps in a row. Do NOT sleep between them: each `pebble` invocation costs
# most of a second, and a sleep pushes the gap past PT_TAP_WINDOW_MS.
pebble emu-tap --emulator emery --direction y+; pebble emu-tap --emulator emery --direction y+

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
