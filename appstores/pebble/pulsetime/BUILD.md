# PulseTime for Pebble (.pbw)

PulseTime for Pebble is a **C watchapp on SDK 4.33.1** that plays the current
local time as a vibration pattern, for blind and low-vision users. It ports the
six haptic time systems from `shared/.../encoding/TimeSystems.kt` and
`TerseQuarterEncoder.kt` to pure C (`src/c/encoder.c`), and reproduces every
case in `appstores/shared/fixtures/patterns.json` byte for byte (host test
below).

It also ships an optional **background worker** (`worker_src/c/worker.c`) so
the time can be felt from any watchface without opening anything by hand.

Targets: basalt, chalk, diorite, emery, flint, gabbro (every current Pebble
except the 2013 aplite). Start with the [wearer guide](GUIDE.md).
See [1.2.0 validation](VALIDATION.md) for current build and emulator evidence;
the dated checks below retain their original scope.

## Synchronized digits, version 1.2.0

During a lesson, Practice, a time check, or a settings preview, one geometry-drawn
numeral fills the screen. A short label identifies hours, quarters, minute digits,
or another encoded unit. The same annotated encoder drives both the motor and
visual deadlines. Existing waveform fixtures remain unchanged.

The numeral describes the encoded group rather than counting each pulse.
Two-digit group values appear one digit at a time, with the full value and digit
position below. Binary displays each bit and its weight; nearest-quarter keeps
its landmark and before/after meaning. Confirmation cues have their own labels.
The overlay preserves Bright/Dark contrast, holds through gaps without flashing,
and returns to the current screen on completion, stop, or navigation. Lessons
and Practice request the watch's normal temporary backlight.

```sh
bash appstores/pebble/pulsetime/tests/check-reading.sh
```

This covers 90,720 day/speed/scope combinations, known examples, empty minute
fields, Morse, binary, and twelve-hour boundaries. Every visual transition lands
on an encoded pulse boundary. Physical display latency is a separate check.

## Guided clock, introduced in 1.1.0

A first launch from the launcher or phone opens **Learn pattern**. Quick Launch,
worker, and chime launches keep their immediate time reply. Finish the guide
with Down on its last page to remember completion; Back leaves it available
for the next normal launch. Existing language, speed, and optional settings
are preserved.

Hold **Select** for a named menu: Learn pattern, Practice, Language, Speed,
Display, Quick Launch, and Watch status. Select applies a setting; Back returns
to the previous menu, then the clock. Hold Up/Down remains the language shortcut.
On the clock, Select asks for the whole time, Up for hours, and Down for minutes.
Press the same reply button during playback to stop it. Pressing a different
reply button starts that phrase. Learning and menu navigation stop previews;
scheduled chimes wait for the next scheduled slot while those screens are open.

The guide teaches the saved language at **3:32**: feel hours, minutes, then the
whole answer. In approximate Terse Quarter this means 3:30–3:44. A separate
zero lesson makes the ambiguous minutes-only zero cue explicit. See
[the measured language comparison](HAPTICS.md) before selecting an exact mode.
No emulator can establish whether a pattern is comfortable or reliably felt.

**Bright / bold** is the initial display: black, scalable digits on white with
hours above minutes. Dark inverts both colors. Row layout puts the time on one
line. These display choices do not change the haptic language. Both the app and
face share the digit renderer. The watch controls physical backlight brightness;
PulseTime only requests the normal temporary light on a deliberate time check.

Quick Launch assignments belong to Pebble's Settings. PulseTime cannot assign
an operating-system shortcut automatically. Follow the guide: Settings > Quick
Launch > Up > PulseTime; repeat for Down if wanted. Time 2, 2 Duo, and Round 2
report the launch button, so the app's Quick Launch menu can select each reply.
Pebble 2 SE and the original Time Round always launch with the whole time;
their Up/Down replies work normally once the app is open.

Display persistence uses keys 13–14; guide completion uses key 12. Existing
keys 1–11 and message-key ordering remain unchanged. AppMessage buffers are
512 bytes to hold the complete settings dictionary. Display-only changes save
quietly; haptic changes preview the new choice, and Practice keeps its known time.

Validation commands:

```sh
bash appstores/pebble/pulsetime/tests/check-fixtures.sh
bash appstores/pebble/pulsetime/tests/evaluate-patterns.sh
node appstores/pebble/pulsetime/tests/check-phone.mjs
bash appstores/pebble/pulsetime-face/tests/check-display.sh
```

## CloudPebble import

This directory is canonical. The private
[`lukeslp/pebble-pulsetime`](https://github.com/lukeslp/pebble-pulsetime)
repository is its self-contained, root-level CloudPebble mirror. Preserve UUID
`e69d82c1-73f1-4b29-a50f-06d17edd13b2` across both copies. This package has
`watchface: false`, so it is submitted to the Pebble Appstore as an **app**, in
a separate listing from PulseTime Face.

## How it behaves on the watch

- **Quick Launch = tell time.** A normal first launch offers the guide, where
  Select tells the current time on the introduction and setup pages.
  The lesson pages play their labeled examples. Later launches encode `localtime()`
  with the saved settings and vibrate immediately. It is a watchapp (not a watchface) so it can be
  bound to **Quick Launch** (hold Up or Down).
  A worker launch starts vibration before setting up the window, AppMessage
  buffers, and service subscriptions to reduce the delay after a gesture.
- **Select** plays the whole time with a fresh reading (`vibes_cancel()` first).
- **Up** plays only the hour; **Down** plays only the minutes. These ordinary
  presses never alter a learned language. **Back** exits.
- **Hold Up / Down** deliberately cycles the haptic language. **Hold Select**
  opens the learning and settings menu. Speed is available there and on the phone.

The clock leaves **Back** to the system. On the emery emulator, holding Back
exits before an app handler can run. Hold Select opens the menu; Back then
leaves a submenu and returns to the clock.

### Seven language choices

Terse Quarter appears twice so both quarter-time and exact-minute playback are
easy to find:

| Rung | System | Cue |
|---|---|---|
| 1 | Terse Quarter | 1 short pulse |
| 2 | Terse Quarter + exact minutes | 2 |
| 3 | Five-Minute Ladder | 3 |
| 4 | Decimal Place Rhythm | 4 |
| 5 | Binary Clock | 5 |
| 6 | Nearest-Quarter Offset | 6 |
| 7 | Morse Digits | 7 |

`s_system` and `s_exact` stay separate settings; only the *cycle* treats them
as one sequence, and the phone's Clay page still sets either independently.

### Practice

Choose `Hold Select > Practice` to replay known times. Its 22-step sequence
starts with bare hours and moves to quarters, exact minutes, and hour boundaries.
The fixed answer lets the wearer concentrate on the pattern.

Up moves one rung through the fixed ladder. Practice plays the target pattern
but does not announce its time separately. The irregular sequence must be
learned or read with assistance first; independent blind onboarding has not
been validated. The phone guide provides screen-reader-readable instructions.

The ladder walks bare hours → the quarters → minutes inside a quarter → then the
boundaries that catch every system out: the 12/0 wrap at midnight and noon, and
11:59 → 12:00 → 12:01, where Nearest-Quarter has to roll its landmark into the
next hour. Up/Down step, Select replays or stops, Back returns to the menu.

### Two Quick Launch buttons, two questions

Pebble reports **which button** started the app:

```c
ButtonId launch_button(void);                              // pebble.h
AppQuickLaunchAction launch_get_quick_launch_action(void);  // HOLD | TAP | COMBO
```

So one install bound to both Quick Launch slots can answer two different
questions: hold Up for the hour, hold Down for the minutes. `QL_UP` and
`QL_DOWN` (Clay selects, persist keys 5 and 6) choose what each button plays;
both default to the whole time, so no existing binding changes behaviour.

**Availability in the 4.33.1 headers:**

| Platform | Device | `launch_button` | Speaker | Touch |
|---|---|---|---|---|
| emery | Pebble Time 2 | yes | yes | yes |
| flint | Pebble 2 Duo | yes | yes | no |
| gabbro | Pebble Round 2 | yes | no | yes |
| basalt / chalk / diorite / aplite | classic | `#define launch_button(...) (0)` | no | no |

The frozen platforms are `FROZEN_AT_REVISION: 89` in
`common/tools/pebble_sdk_platform.py`, and their headers define both launch
calls as macros returning `0` (which is `BUTTON_ID_BACK`). The scope settings
therefore never match there and every launch plays the whole time. The phone
settings explain this limit; the implementation needs no `#ifdef`.

A scope selects a field from the existing pattern. Hours-only plays the chosen
system's hour phrase; minutes-only plays its minute phrase.

`pt_encode_scoped()` splits at the boundary each encoder records
(`Builder.boundary`). That index has to be recorded rather than found, because
**"the first silence segment" is not the boundary**: past five hours the hour
field carries group gaps of its own. An empty field (five-minute ladder at
`:00`, terse quarter before `:15`) plays one `PT_ZERO_FIELD_ON_MS` (300 ms)
pulse meaning "none"; silence would be indistinguishable from a fault. That
reuses the decimal-place convention where digit zero is already one
double-length pulse.

On the clock, **Select gives the whole current time**, Up gives the hour,
and Down gives the minutes. A scoped launch therefore always has a direct path
back to the full answer without leaving.

### Chimes: the one path that needs nothing running

`wakeup_schedule()` has the system launch PulseTime at a chosen moment, whether
or not it is running and whatever watchface is showing. The app buzzes and
closes itself. **No PulseTime process is resident in between**, and the watch's single
worker slot stays free. Battery impact has not been measured.

The watchface variant chimes only while it is on screen. Opening the launcher
or another app unloads it. Scheduled watchapp chimes work across those changes
because the system launches PulseTime briefly to play the answer.

| Constant | Value | Why |
|---|---|---|
| `PT_WAKEUP_MAX` | 8 | Pebble's hard per-app limit on scheduled wakeups. |
| `PT_WAKEUP_PROBES` | 96 | Candidate slots examined; quiet hours can eat a long run, so this must exceed the limit comfortably. |

Recurring chimes use a rolling window that refills on every launch, including
each scheduled chime. Coverage at any moment:
hourly ≈ 8 hours ahead, half-hourly ≈ 4, quarter-hourly ≈ 2.

Details that are easy to get wrong:

- **The cookie carries the intended hour and minute**, rather than letting the
  woken app read the clock. A wakeup can land a second either side of the
  boundary, and announcing "12:59" when the wearer expected one o'clock is
  exactly the kind of error that destroys trust in a clock you cannot see.
- **A negative `WakeupId` is usually `E_RANGE`**: another app already owns a
  wakeup within a minute of that slot. Skip it and take the next candidate;
  never treat it as fatal.
- **The scheduler skips quiet hours** to preserve the eight available slots.
- **The screen reports the number actually scheduled**, e.g. `Chime hourly (8)`,
  not the setting. "Chimes are on" and "a chime will really happen" are
  different claims, and only the second is worth anything.
- `wakeup_schedule` is present on **every** platform including the frozen ones,
  so unlike the Quick Launch scopes and the speaker this works on a Pebble 2 SE
  exactly as it does on a Time 2.

### Sound: the dimension the motor cannot carry

`PBL_SPEAKER` is defined for **emery (Pebble Time 2)** and **flint (Pebble 2
Duo)** only. Gabbro has no speaker support. Verify with
`nm build/<platform>/pebble-app.elf | grep -i speaker` after a build: the
speaker path links on emery and flint and nowhere else, so the `#ifdef` is doing
real work rather than compiling the feature out everywhere unnoticed.

Four of the six systems carry **relative strength** (`layer` 100 / 72 / 50 / 45),
which a Pebble motor cannot reproduce. `pt_layer_midi_note()` maps those layers
to **pitch**:

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
`pt_to_vibe` merges silences and discards the layer, the information
the speaker exists to carry. `SPEAKER_MAX_NOTES` is 256 and the worst case here
is 140.

`SOUND` is off / with vibration / instead of vibration, `VOLUME` is 30/65/100 %.
Sound starts off. Playback always respects `speaker_is_muted()`.

### Confirmation cues and their limits

Language and speed changes play a **cue** before the time, in the same vibe
pattern (`vibes_enqueue_custom_pattern` is a no-op while a pattern plays, so two
calls would drop the second):

| Change | Cue | Count |
|---|---|---|
| Haptic language (hold Up/Down or Language menu) | `PT_CUE_SHORT_MS` 60 ms pulses | 1–7, the language rung's 1-based index |
| Speed menu | Long pulses | 1–3: Slow, Standard, Fast |

Display changes save silently. Menu and guide navigation do not announce their
labels by touch. Quick Launch scope changes preview the selected time phrase.
These cues support familiar controls; they do not establish independent blind use.

Cue pulses are identical and evenly spaced (`PT_CUE_GAP_MS` 150 ms), which no
time system produces, and `PT_CUE_LEAD_MS` (500 ms) separates the cue from the
time. The encoder's long speed cue remains fixture-tested for release
compatibility, but the on-watch Speed menu emits it before previewing a new speed.

`pt_cue_durations()` lives in `src/c/encoder.c`, not in `main.c`, so the host
test covers it and the watchface variant can reuse it.
- A **wrist flick** (`accel_tap_service`) also repeats, except while a
  pattern is playing, since the motor shakes the watch hard enough to register
  as a tap and would otherwise loop.
- **Flick to tell time** (off by default) runs the background worker described
  below. A double wrist tap from any watchface opens PulseTime, plays the time,
  and closes it again.
- Screen: scalable time digits, a precise/quarter status, and explicit Select
  and menu hints. Optional worker and chime details live in Watch status.
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
**only one worker at a time, across the whole watch**. Turning this on may ask
you to take the slot from another app, and another app can take it back later.

**Quick Launch needs no background worker.** Settings > Quick Launch > Up
(or Down) > PulseTime. Then a long press on that button from any watchface
opens PulseTime and it tells the time immediately, with no worker running and
no accelerometer subscription between checks. Quick Launch is the recommended default; the worker
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

The worker holds **no timers**. It measures each window from a stored
`time_ms()` stamp when the next tap arrives. The worker remains resident
with an accelerometer subscription; its battery impact has not been measured. Its
compiled size is 644 bytes per platform.

### Not buzzing at itself

The 5 s cooldown is not enough on its own: a slow Morse Digits pattern runs
well past it, and the motor shakes the watch hard enough to look like a
gesture. So the app sends the worker a `PT_WORKER_MSG_SUPPRESS`
(`app_worker_send_message`) message carrying the pattern length plus
`PT_WORKER_QUIET_MARGIN_MS` (1200 ms) every time it plays, and the worker ignores
taps for exactly that long. The message type is defined in both `src/c/main.c`
and `worker_src/c/worker.c`; **keep the two in sync**.

### Launch reason and auto-exit

`init()` records `launch_reason() == APP_LAUNCH_WORKER`. When that is true the
app plays the time as usual and then calls `window_stack_pop_all(false)` from
the playback-finished timer, so the wearer lands back on their own watchface
without touching anything. Any button press cancels the auto-exit, so a wearer
who wants to change the system mid-gesture keeps the app open. A launch from
the menu, Quick Launch, or the phone never auto-exits.

### Confirming worker state

The app never claims the worker started when it did not. `app_worker_launch()`
is asynchronous, so `apply_flick_setting()` records what came back and a repeat
`AppTimer` (`PT_WORKER_CHECK_MS` 1500 ms, `PT_WORKER_CHECK_TRIES` 6) confirms
against `app_worker_is_running()`:

| `AppWorkerResult` | Screen line | Second line |
|---|---|---|
| `SUCCESS` / `ALREADY_RUNNING` | `Flick: on` | Verified within ~9 s, or it falls back to off |
| `ASKING_CONFIRMATION` | `Flick: waiting` | "Confirm on the watch to let PulseTime run the worker." |
| `DIFFERENT_APP` | `Flick: blocked` | "Another app's worker is running. A watch allows only one." |
| `NO_WORKER` | `Flick: unavailable` | "This build has no background worker." |
| anything else | `Flick: off` | "The watch refused to start the worker." |

The state is available under Watch status. The phone settings explain worker
requirements and how to resolve a blocked or unavailable worker.

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

The phone side (`src/pkjs/index.js`) hosts a **Clay** page. It begins by naming
the simple default, then groups Time system, Playback speed, Exact minutes, and
Quick Launch scopes as advanced choices; sound, chimes, and Flick to tell time
are explicitly optional. It sends values over
AppMessage as the keys `SYSTEM` / `SPEED` (storage-id strings) and
`EXACT_MINUTES` / `FLICK_LAUNCH` (0/1). The flick toggle's description states
the costs in full: continuous worker, battery, one worker per watch, the app
briefly opening because Pebble cannot vibrate from the background, and Quick
Launch as the alternative with no resident worker. The watch
applies and persists them at once. Haptic changes preview the new pattern;
display-only changes remain silent. On `ready` the
phone sends `REQUEST`; the watch answers with its live settings, and after a
deliberate hold-Up/hold-Down language change it pushes them too, so the Clay
page can reflect those changes after successful delivery. Reconnect and reopen
settings if the phone and watch disagree.

Clay is **vendored** at `src/pkjs/vendor/pebble-clay.js` (v1.0.4, MIT, licence
alongside). `pebble package install pebble-clay` works, but the published
package declares support only for aplite/basalt/chalk/diorite/emery, and
`pebble build` refuses it for **flint and gabbro**; the bundle itself has no
platform-specific code, so the built `dist/js/index.js` is checked in and
required directly. `capabilities: ["configurable"]` is what makes the phone
app show the settings gear.

## Prerequisites

```sh
uv tool install pebble-tool==5.0.39 --python 3.14   # match the SDK venv's Python
pebble sdk install 4.33.1                    # 4.33.1: toolchain + qemu-pebble
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

The worker builds because `worker_src/` exists: `wscript` checks for the
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
pebble emu-button --emulator basalt click up       # hours only
pebble emu-button --emulator basalt click down     # minutes only
pebble emu-button --emulator basalt --duration 1000 click down  # next language
pebble emu-tap --emulator basalt                   # wrist flick
pebble screenshot --no-open --emulator basalt docs/basalt.png
BROWSER=/bin/echo pebble emu-app-config --emulator basalt   # prints the Clay page URL
```

The emulator cannot establish tactile perception; use logs to verify requested
playback. SDK 4.33.1 includes current emery and gabbro emulator boards. Test the
actual motor on hardware before making perception claims.

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
> PulseTime. That tells the time from any watchface without a resident worker;
turn on "Flick to tell time" in the config page only if the hands-free gesture
is worth a worker running all day.

## Toolchain notes

- pebble-tool must run on the same Python as the SDK venv (3.14 on this Mac).
- QEMU is not bundled with pebble-tool; `pebble sdk install 4.33.1` provides
  `toolchain/bin/qemu-pebble` (`PEBBLE_QEMU_PATH`).
- The `ld: ... LOAD segment with RWX permissions` warnings are normal for the
  Pebble toolchain.
