# PulseTime for Pebble (.pbw)

PulseTime for Pebble is a **C watchapp on SDK 4.33.1** that plays the current
local time as a vibration pattern, for blind and low-vision users. It ports the
six haptic time systems from `shared/.../encoding/TimeSystems.kt` and
`TerseQuarterEncoder.kt` to pure C (`src/c/encoder.c`), and reproduces every
case in `appstores/shared/fixtures/patterns.json` byte for byte (host test
below). Author: Luke Steuber.

It also ships an optional **background worker** (`worker_src/c/worker.c`) so
the time can be felt from any watchface without opening anything by hand.

Targets: basalt, chalk, diorite, emery, flint, gabbro (every current Pebble
except the 2013 aplite). Verified 2026-08-18: clean build for all six (worker
binary included on every platform), runs in the `basalt`, `diorite`, and
`chalk` emulators (`docs/*.png`).

## How it behaves on the watch

- **Launch = tell time.** The app encodes `localtime()` with the saved settings
  and vibrates immediately. It is a watchapp (not a watchface) so it can be
  bound to **Quick Launch** (hold Up or Down) — that is the intended gesture.
  On a worker (gesture) launch the vibration is started *before* the window,
  AppMessage buffers, and service subscriptions are set up, because that is the
  one path where the wearer is waiting on nothing else.
- **Select** repeats with a fresh time (`vibes_cancel()` first).
- **Up / Down** cycle the six time systems; the choice is persisted, redrawn,
  and played straight away. **Back** exits.
- **Hold Up / Down** cycles the playback speed. **Hold Select** opens Practice.
  Every setting is reachable without the phone, and without a gesture anyone has
  to be told about.

**BACK is left entirely alone.** A long press on it is swallowed by the system's
own back-out before any handler runs — verified on the emery emulator, where the
app simply exits — and subscribing to BACK at all risks a screen the wearer
cannot leave. That is why Practice sits on hold-Select.

### The system cycle has seven rungs, not six

Terse Quarter appears twice, with and without exact minutes, because it
genuinely feels like two different systems and a hidden toggle is a setting
nobody finds:

| Rung | System | Cue |
|---|---|---|
| 1 | Terse Quarter | 1 short pulse |
| 2 | Terse Quarter + exact minutes | 2 |
| 3 | Five-Minute Ladder | 3 |
| 4 | Decimal Place Rhythm | 4 |
| 5 | Binary Clock | 5 |
| 6 | Nearest-Quarter Offset | 6 |
| 7 | Morse Digits | 7 |

`s_system` and `s_exact` stay separate settings — only the *cycle* treats them
as one sequence, and the phone's Clay page still sets either independently.

### Practice

`Hold Select`. A haptic clock is only useful once its patterns are legible, and
"now" is the worst possible teacher: you cannot choose it and it moves. Practice
plays a **chosen** time from a 22-rung ladder ordered easiest to hardest, so the
wearer knows the answer before the pattern starts and can attend to its shape.

This works **without sight**, which matters because the app is for people who
cannot read the screen: you know the answer because you stepped to it. One press
up is one rung up.

The ladder walks bare hours → the quarters → minutes inside a quarter → then the
boundaries that catch every system out: the 12/0 wrap at midnight and noon, and
11:59 → 12:00 → 12:01, where Nearest-Quarter has to roll its landmark into the
next hour. Up/Down step, Select replays, Back returns to the clock.

### Two Quick Launch buttons, two questions

Pebble reports **which button** started the app:

```c
ButtonId launch_button(void);                              // pebble.h
AppQuickLaunchAction launch_get_quick_launch_action(void);  // HOLD | TAP | COMBO
```

So one install bound to both Quick Launch slots can answer two different
questions — hold Up for the hour, hold Down for the minutes. `QL_UP` and
`QL_DOWN` (Clay selects, persist keys 5 and 6) choose what each button plays;
both default to the whole time, so no existing binding changes behaviour.

**Availability, checked in the 4.33.1 headers rather than assumed:**

| Platform | Device | `launch_button` | Speaker | Touch |
|---|---|---|---|---|
| emery | Pebble Time 2 | yes | yes | yes |
| flint | Pebble 2 Duo | yes | yes | — |
| gabbro | Pebble Round 2 | yes | — | yes |
| basalt / chalk / diorite / aplite | classic | `#define launch_button(...) (0)` | — | — |

The frozen platforms are `FROZEN_AT_REVISION: 89` in
`common/tools/pebble_sdk_platform.py`, and their headers define both launch
calls as macros returning `0` (which is `BUTTON_ID_BACK`). The scope settings
therefore never match there and every launch plays the whole time — no `#ifdef`
needed, but the config page says so plainly rather than leaving a dead setting.

A scope is a **projection** of the pattern, not a new haptic vocabulary:
hours-only is exactly the hour phrase of the chosen system, minutes-only is
exactly its minute phrase. Nothing extra has to be learned.

`pt_encode_scoped()` splits at the boundary each encoder records
(`Builder.boundary`). That index has to be recorded rather than found, because
**"the first silence segment" is not the boundary** — past five hours the hour
field carries group gaps of its own. An empty field (five-minute ladder at
`:00`, terse quarter before `:15`) plays one `PT_ZERO_FIELD_ON_MS` (300 ms)
pulse meaning "none"; silence would be indistinguishable from a fault. That
reuses the decimal-place convention where digit zero is already one
double-length pulse.

Once the app is open, **any button press drops back to the whole time**, so a
scoped launch always has a way to the full answer without leaving.

### Chimes: the one path that needs nothing running

`wakeup_schedule()` has the system launch PulseTime at a chosen moment, whether
or not it is running and whatever watchface is showing. The app buzzes and
closes itself. **Nothing is resident in between** — no worker, no process, no
battery drawn while it waits, and the watch's single worker slot stays free.

This is the important distinction from the watchface variant. A watchface can
chime, but only while it is the *foreground app*: open any app, or even the
launcher, and it is unloaded along with its tick subscription. The wakeup path
has no such condition.

It does **not** break the platform rule — Pebble still cannot vibrate from a
background context. What it does is make the app appear without being asked,
which is the part that matters.

| Constant | Value | Why |
|---|---|---|
| `PT_WAKEUP_MAX` | 8 | Pebble's hard per-app limit on scheduled wakeups. |
| `PT_WAKEUP_PROBES` | 96 | Candidate slots examined; quiet hours can eat a long run, so this must exceed the limit comfortably. |

So a recurring chime is **not** "schedule forever" — it is a rolling window,
topped up on every launch. Since each chime *is* a launch, the chain sustains
itself and the wearer never has to open the app. Coverage at any moment:
hourly ≈ 8 hours ahead, half-hourly ≈ 4, quarter-hourly ≈ 2.

Details that are easy to get wrong:

- **The cookie carries the intended hour and minute**, rather than letting the
  woken app read the clock. A wakeup can land a second either side of the
  boundary, and announcing "12:59" when the wearer expected one o'clock is
  exactly the kind of error that destroys trust in a clock you cannot see.
- **A negative `WakeupId` is usually `E_RANGE`** — another app already owns a
  wakeup within a minute of that slot. Skip it and take the next candidate;
  never treat it as fatal.
- **Quiet hours are applied at scheduling time**, not at wake time. Booking a
  wakeup only to ignore it would waste one of the eight slots.
- **The screen reports the number actually scheduled**, e.g. `Chime hourly (8)`,
  not the setting. "Chimes are on" and "a chime will really happen" are
  different claims, and only the second is worth anything.
- `wakeup_schedule` is present on **every** platform including the frozen ones,
  so unlike the Quick Launch scopes and the speaker this works on a Pebble 2 SE
  exactly as it does on a Time 2.

### Sound: the dimension the motor cannot carry

`PBL_SPEAKER` is defined for **emery (Pebble Time 2)** and **flint (Pebble 2
Duo)** only — *not* gabbro, despite it being new hardware. Verify with
`nm build/<platform>/pebble-app.elf | grep -i speaker` after a build: the
speaker path links on emery and flint and nowhere else, so the `#ifdef` is doing
real work rather than compiling the feature out everywhere unnoticed.

The point is not that a clock can beep. Four of the six systems separate their
fields by **relative strength** (`layer` 100 / 72 / 50 / 45) and a Pebble motor
has **no amplitude control at all**, so that whole dimension is lost on the
vibration channel. `pt_layer_midi_note()` gives it back as **pitch**:

| Layer | Note | Used for |
|---|---|---|
| 100 | C5 (72) | hours, binary ones, Morse dashes |
| 72 | G4 (67) | five-minute steps, minute tens |
| 50 | E4 (64) | Morse dots |
| 45 | C4 (60) | leftover minutes, binary zeros |

A spread C major triad, so systems that interleave layers stay consonant in any
order. The confirmation cue sits above all of them at E5 (76), staying as
distinct by ear as it is by touch.

Notes are built from the **segments**, not from the vibe durations, because
`pt_to_vibe` merges silences and discards the layer — precisely the information
the speaker exists to carry. `SPEAKER_MAX_NOTES` is 256 and the worst case here
is 140.

`SOUND` is off / with vibration / instead of vibration, `VOLUME` is 30/65/100 %.
Off by default — a watch that starts making noise by itself in a meeting is a
worse product than a quiet one — and `speaker_is_muted()` is always obeyed.

### Confirmation cues — operable without the screen

The app is for people who cannot see the screen, so no setting may be confirmed
by sight alone. Every change plays a **cue** before the time, in the same vibe
pattern (`vibes_enqueue_custom_pattern` is a no-op while a pattern plays, so two
calls would drop the second):

| Change | Cue | Count |
|---|---|---|
| Time system (Up/Down) | `PT_CUE_SHORT_MS` 60 ms pulses | 1–6, the system's 1-based index |
| Playback speed (hold Up/Down) | `PT_CUE_LONG_MS` 220 ms pulses | 1–3, the speed's 1-based index |
| Exact minutes (hold Select) | none | the extra minute ticks *are* the feedback |

Pulse **length**, not just count, carries which setting changed — a bare count
would make "two short" and "two long" both mean "2". Cue pulses are identical
and evenly spaced (`PT_CUE_GAP_MS` 150 ms), which no time system produces, and
`PT_CUE_LEAD_MS` (500 ms) separates the cue from the time.

`pt_cue_durations()` lives in `src/c/encoder.c`, not in `main.c`, so the host
test covers it and the watchface variant can reuse it.
- A **wrist flick** (`accel_tap_service`) also repeats — but not while a
  pattern is playing, since the motor shakes the watch hard enough to register
  as a tap and would otherwise loop.
- **Flick to tell time** (off by default) runs the background worker described
  below. A double wrist tap from any watchface opens PulseTime, plays the time,
  and closes it again.
- Screen: the time large (Leco), the system name, its short label and speed
  (or "Playing h:mm" while the pattern runs), and two hint lines. Works in
  1-bit on diorite/flint, centres inside the chalk circle, and scales up on
  emery/gabbro (`big` = unobstructed height >= 200).
- Settings (`persist_*` keys 1–11): time system, playback speed, exact minutes,
  flick to tell time, what each Quick Launch button plays, sound mode, volume,
  chime interval, and the quiet-hours window. Defaults: Terse Quarter, Standard,
  off, off, whole time, whole time, sound off, medium, chimes off, 22:00–07:00.

Pebble motors have **no amplitude control**, so the encoder's relative
`layer` (100/72/50/45) is carried for the host test but ignored on the watch.
No PulseTime system depends on strength alone.

## Background worker: "Flick to tell time"

Off by default. Turn it on in the phone's config page.

**What it costs, plainly.** `pebble_worker.h` contains **no vibes API at all**,
so a worker can never buzz on its own. All it can do is watch the
accelerometer and call `worker_launch_app()`. That means PulseTime has to open
on screen for a moment to vibrate, and the worker has to stay resident and
subscribed to the accelerometer all day, which uses battery. A Pebble also runs
**only one worker at a time, across the whole watch** — turning this on may ask
you to take the slot from another app, and another app can take it back later.

**The zero-battery alternative is Quick Launch.** Settings > Quick Launch > Up
(or Down) > PulseTime. Then a long press on that button from any watchface
opens PulseTime and it tells the time immediately, with no worker running and
no background cost at all. Quick Launch is the recommended default; the worker
exists for wearers who want a hands-free gesture and accept the trade.

### The gesture

A **double tap on the same accelerometer axis**. Constants live at the top of
`worker_src/c/worker.c`:

| Constant | Value | Why |
|---|---|---|
| `PT_TAP_MIN_GAP_MS` | 120 | Two taps closer than this are one knock ringing twice, not a gesture. Ignored outright. |
| `PT_TAP_WINDOW_MS` | 1200 | The second tap must land within this of the first, on the **same** axis. Anything else just re-arms on the newest tap. |
| `PT_LAUNCH_COOLDOWN_MS` | 5000 | After a launch, taps are ignored for this long. |

A single tap does nothing; that is deliberate, because a single tap fires
constantly during ordinary arm movement.

The worker holds **no timers**. Every window is measured from a stored
`time_ms()` stamp when the next tap arrives, so an idle worker costs nothing
beyond the accelerometer subscription the tap service already needs. Its
compiled size is 644 bytes per platform.

### Not buzzing at itself

The 5 s cooldown is not enough on its own: a slow Morse Digits pattern runs
well past it, and the motor shakes the watch hard enough to look like a
gesture. So the app sends the worker a `PT_WORKER_MSG_SUPPRESS`
(`app_worker_send_message`) message carrying the pattern length plus
`PT_WORKER_QUIET_MARGIN_MS` (1200 ms) every time it plays, and the worker stays
deaf for exactly that long. The message type is defined in both `src/c/main.c`
and `worker_src/c/worker.c` — **keep the two in sync**.

### Launch reason and auto-exit

`init()` records `launch_reason() == APP_LAUNCH_WORKER`. When that is true the
app plays the time as usual and then calls `window_stack_pop_all(false)` from
the playback-finished timer, so the wearer lands back on their own watchface
without touching anything. Any button press cancels the auto-exit, so a wearer
who wants to change the system mid-gesture keeps the app open. A launch from
the menu, Quick Launch, or the phone never auto-exits — that path is unchanged.

### Saying the truth about the worker

The app never claims the worker started when it did not. `app_worker_launch()`
is asynchronous, so `apply_flick_setting()` records what came back and a repeat
`AppTimer` (`PT_WORKER_CHECK_MS` 1500 ms, `PT_WORKER_CHECK_TRIES` 6) confirms
against `app_worker_is_running()`:

| `AppWorkerResult` | Screen line | Second line |
|---|---|---|
| `SUCCESS` / `ALREADY_RUNNING` | `Flick: on` | — (verified within ~9 s, or it falls back to off) |
| `ASKING_CONFIRMATION` | `Flick: waiting` | "Confirm on the watch to let PulseTime run the worker." |
| `DIFFERENT_APP` | `Flick: blocked` | "Another app's worker is running. A watch allows only one." |
| `NO_WORKER` | `Flick: unavailable` | "This build has no background worker." |
| anything else | `Flick: off` | "The watch refused to start the worker." |

The one-line state (`Flick: on` / `Flick: off`) sits under the system name on
every launch. When there is a second line it takes over the button-hint block
at the bottom so the words are not cut short.

Turning the setting off calls `app_worker_kill()`, and the state line goes back
to `Flick: off`.

### Vibe-pattern conversion (`pt_to_vibe`)

`vibes_enqueue_custom_pattern` wants `ON, OFF, ON, ...` starting with ON, each
slot <= 10000 ms. From the encoder's `on/off` segments: emit `ON = on` for
each pulse; its `off` plus any directly following silence-only segments
(`on == 0`) merge into **one** OFF slot; a trailing OFF is dropped; a
zero-length ON is never emitted. The host harness asserts every pattern is
odd-length, has no zero ON, and no slot above 10000.

## Config page (phone)

The phone side (`src/pkjs/index.js`) hosts a **Clay** page — Time system,
Playback speed, Exact minutes (Terse Quarter only; the other five are
inherently exact), and Flick to tell time — and sends the values over
AppMessage as the keys `SYSTEM` / `SPEED` (storage-id strings) and
`EXACT_MINUTES` / `FLICK_LAUNCH` (0/1). The flick toggle's description states
the costs in full: continuous worker, battery, one worker per watch, the app
briefly opening because Pebble cannot vibrate from the background, and Quick
Launch as the free alternative. The watch
applies them at once, persists them, and plays the new pattern. On `ready` the
phone sends `REQUEST`; the watch answers with its live settings, and after
Up/Down changes on the watch it pushes them too, so the Clay page never shows
a stale choice.

Clay is **vendored** at `src/pkjs/vendor/pebble-clay.js` (v1.0.4, MIT, licence
alongside). `pebble package install pebble-clay` works, but the published
package declares support only for aplite/basalt/chalk/diorite/emery, and
`pebble build` refuses it for **flint and gabbro**; the bundle itself has no
platform-specific code, so the built `dist/js/index.js` is checked in and
required directly. `capabilities: ["configurable"]` is what makes the phone
app show the settings gear.

## Prerequisites

```sh
uv tool install pebble-tool --python 3.14   # match the SDK venv's Python
pebble sdk install latest                    # 4.33.1: toolchain + qemu-pebble
pebble sdk activate 4.33.1
```

## Host test (encoder parity)

```sh
appstores/pebble/pulsetime/tests/check-fixtures.sh
# flattens appstores/shared/fixtures/patterns.json with node, compiles
# src/c/encoder.c + tests/check_fixtures.c with cc, and exits non-zero on any
# mismatch. Four lines, all of which must pass:
#   420/420 match                 encoder output equals the Kotlin fixtures
#   420/420 vibe patterns valid   pt_to_vibe output is odd-length, no zero ON,
#                                 nothing over 10000 ms
#   420/420 cued patterns valid   the same, with the longest system cue (x6)
#                                 and the longest speed cue (x3) prepended
#   ok cue shape                  cue pulse lengths, gaps, totals, and the
#                                 short-vs-long separation
#   840/840 scoped match          patterns-scoped.txt: every case in hours-only
#                                 and minutes-only form, and never silent
```

## Build

```sh
cd appstores/pebble/pulsetime
pebble build
ls -la build/pulsetime.pbw
unzip -l build/pulsetime.pbw | grep pebble-worker.bin   # six lines, one per platform
```

The worker is built because `worker_src/` exists — `wscript` checks for the
directory and adds a `bin_type='worker'` build per platform. Nothing in
`package.json` switches it on. A PBW without `<platform>/pebble-worker.bin` in
every platform folder is a broken release; `stage-release.sh` refuses to stage
one.

Release build with staged artifact and checksum (runs the host test first):

```sh
appstores/pebble/pulsetime/stage-release.sh
# -> appstores/dist/pebble/{pulsetime.pbw,SHA256SUMS.txt}
```

## Emulator preview

```sh
export PEBBLE_QEMU_PATH="$HOME/Library/Application Support/Pebble SDK/SDKs/4.33.1/toolchain/bin/qemu-pebble"
pebble install --emulator basalt      # Pebble Time (colour)
pebble install --emulator diorite     # Pebble 2 SE profile (1-bit)
pebble logs --emulator basalt         # "PulseTime: <system> <speed> ... -> N segments"
pebble emu-button --emulator basalt click select   # repeat
pebble emu-button --emulator basalt click down     # next system
pebble emu-tap --emulator basalt                   # wrist flick
pebble screenshot --no-open --emulator basalt docs/basalt.png
BROWSER=/bin/echo pebble emu-app-config --emulator basalt   # prints the Clay page URL
```

The emulator cannot vibrate; use the log line to confirm playback. `emery`
(Pebble Time 2) is not supported by the QEMU 10.x build; test it on hardware.

### Driving the worker gesture in the emulator

`pebble emu-tap` works, but each invocation opens a fresh connection and takes
seconds, so two calls can never land inside the 1200 ms window. Send both taps
over one connection instead:

```python
# needs the pebble-tool venv python, e.g.
# ~/.local/share/uv/tools/pebble-tool/bin/python taps.py
import time
from pebble_tool.sdk.emulator import ManagedEmulatorTransport
from pebble_tool.commands.emucontrol import send_data_to_qemu
from libpebble2.communication import PebbleConnection
from libpebble2.communication.transports.qemu.protocol import QemuTap

t = ManagedEmulatorTransport('basalt'); t.connect()
p = PebbleConnection(t); p.connect(); p.run_async(); time.sleep(0.5)
for i in range(2):
    if i: time.sleep(0.3)
    send_data_to_qemu(p.transport, QemuTap(axis=QemuTap.Axis.X, direction=1))
time.sleep(1)
```

Turning the setting on without a browser: run `BROWSER=/bin/echo pebble
emu-app-config --emulator basalt` in the background, read the `return_to` port
out of the temporary `~/pebble-tool-emu-app-config-*.html` it writes, then
`curl "http://localhost:<port>/close?%7B%22FLICK_LAUNCH%22%3Atrue%7D"`.

Verified 2026-08-18 on basalt: two same-axis taps 300 ms apart launched the app
from a watchface (`main.c:233> PulseTime: ... -> 17 segments`) and it exited on
its own ~6 s later. A single tap, two taps on different axes, and two same-axis
taps 2000 ms apart all did nothing. With the setting off, the same double tap
did nothing at all.

## Hardware install (Core Devices phone app)

Enable **Developer Connection** in the Pebble app, keep that screen open, make
the target watch the active watch, then:

```sh
pebble login                          # once, same account as the phone app
pebble install --cloudpebble
pebble logs --cloudpebble
```

Then bind PulseTime to Quick Launch: Settings > Quick Launch > Up (or Down)
> PulseTime. That is the zero-battery way to tell the time from any watchface;
turn on "Flick to tell time" in the config page only if the hands-free gesture
is worth a worker running all day.

## Toolchain notes

- pebble-tool must run on the same Python as the SDK venv (3.14 on this Mac).
- QEMU is not bundled with pebble-tool; `pebble sdk install latest` provides
  `toolchain/bin/qemu-pebble` (`PEBBLE_QEMU_PATH`).
- The `ld: ... LOAD segment with RWX permissions` warnings are normal for the
  Pebble toolchain.
