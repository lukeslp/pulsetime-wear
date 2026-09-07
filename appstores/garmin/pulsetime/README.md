# PulseTime for Garmin (Venu 2S)

A Connect IQ **device app** with a glance and one published complication, which
plays the current local time as a vibration pattern. It is the Garmin port of
PulseTime's six haptic time systems
(see the repository README); the encoders are a line-for-line Monkey C port of
`shared/src/main/java/com/lukesteuber/pulsetime/shared/encoding/`, and are
locked to the same 420-case fixture set as the Pebble port
(`appstores/shared/fixtures/patterns.json`).

License: MIT. A small open-source project for friends and anyone it helps.

## How it feels

1. Reach PulseTime from any of three places: the **PulseTime complication** on
   your watch face, the **PulseTime glance**, or the app list. The glance and
   the complication both show the app name and the current time.
2. Tap it. The full view opens and the watch **vibrates the time immediately**
   in the selected system.
3. Tap the screen or press Start/Enter to **repeat**. Swipe up/down or press
   Down/Up to **switch time systems**. The app saves your choice. Back exits, and
   any pattern still queued stops.

The screen shows large time digits, the active system, and the repeat hint.
The hint also reports when the watch's vibration setting is off.

## The complication

PulseTime publishes one complication so a watch face can carry it:

| | |
|---|---|
| Long label | PulseTime |
| Short label | Pulse |
| Face It text | PULSE |
| Access | `public` - any watch face may use it |
| Value | the current local time, `h:mm` or `HH:mm`, following the watch's 12/24-hour setting |
| Icon | `complication_icon.svg`, the three pulse bars without the launcher disc |

**Using a compatible watch face.** Look for **PulseTime** in the face's data-slot
editor. The simulator confirms publication; appearance in the Venu 2S editor
and tap-to-launch still need a hardware check. If the face offers it, tapping
the complication should open PulseTime and play through the same `onShow` path
as opening the app. The app list remains available if the face cannot host it.

**What it cannot do.** Connect IQ has no way to vibrate from a background
process, so the complication is a launcher, not a background haptic: the app has
to come to the foreground for the motor to run. For the same reason PulseTime
ships **no `Toybox.Background` service** - a temporal event could refresh the
number on the face but could never buzz, so it would only cost battery. The
published value is therefore refreshed whenever the app runs (every open and
every replay), and between runs the face keeps showing the last value it was
given, or the `--:--` placeholder from the resource before the first run. Read
the complication as a labelled button that happens to remember the last time it
spoke, not as a live clock.

## Splitting patterns at the 8-profile limit

`Toybox.Attention.vibrate()` takes at most **eight** `VibeProfile`s per call.
A full pattern is up to 32 segments (about 64 profiles), so `PulsePlayer`:

- converts each encoded `[on, off, layer]` triple to profiles: an ON pulse
  becomes `VibeProfile(duty, on)`, every silence becomes `VibeProfile(0, ms)`;
  consecutive silences merge into one profile and trailing silence is dropped;
- splits the profile list into chunks of at most eight, cutting **only where a
  silence profile ends the chunk** (falling back to a cut where the next chunk
  begins with a silence, and to a hard cut at eight only if there is no
  silence at all inside the window - the current encoders never produce that);
- plays chunk N and arms a one-shot `Timer.Timer` for exactly the chunk's total
  length (sum of profile lengths) to play chunk N+1. The chunk queue and the
  per-chunk durations are built up front; the timer callback only indexes into
  them.

Because every ON pulse is followed by a silence in all six systems, chunks are
`ON OFF ON OFF ON OFF ON OFF` and every seam lands inside a gap the pattern
already had. Timer jitter on the watch can only lengthen a pause by a few
milliseconds; it can never truncate a pulse. The harness verifies these
invariants on all 420 fixture patterns (chunk sizes, exact reassembly, seam in
silence, and durations).

Strength maps to duty cycle: Soft / Standard / Strong = base duty **45 / 70 /
100**. With **Layered strength** on, each pulse's duty is
`round(base x layer)` where layer is the encoder's relative strength
(100 / 72 / 50 / 45 percent); otherwise every pulse uses the base duty.
The player clamps duty to 1..100.

## Settings (Garmin Connect / Connect IQ app settings)

| Property | Values | Default |
|---|---|---|
| `system` | Terse Quarter, Five-Minute Ladder, Decimal Place Rhythm, Binary Clock, Nearest-Quarter Offset, Morse Digits | Terse Quarter |
| `speed` | Slow (x1.25), Standard (x1.0), Fast (x0.78) | Standard |
| `strength` | Soft, Standard, Strong (duty 45 / 70 / 100) | Standard |
| `layered` | boolean - layered strength | on |
| `exactMinutes` | boolean - leftover-minute ticks; applies to Terse Quarter only (the other five are inherently exact) | off |

All properties are read with null guards and clamping in
`PulseTimeView.reloadSettings()`; `AppBase.onSettingsChanged` reloads and
redraws. Up/Down in the app also writes `system` back through
`Application.Properties.setValue`.

## Layout

```
appstores/garmin/
  .gitignore                  dist/ is build output
  scripts/build-venu2s.sh     builds dist/pulsetime-venu2s.prg (+ --harness)
  pulsetime/
    manifest.xml              type="watch-app", venu2s, minSdkVersion 4.2.0,
                              uses-permission ComplicationPublisher
    monkey.jungle
    resources/complications   complications.xml - the one published complication
    resources/drawables       launcher_icon.png (61x61) + complication_icon.svg
                              (39x39), both generated, + drawables.xml
    resources/strings         strings.xml
    resources/settings        properties.xml + settings.xml
    scripts/render-icon.py    regenerates both icons (Pillow for the PNG)
    source/PulseTimeEncoder.mc  pure encoders (no UI / Attention imports)
    source/PulsePlayer.mc       VibeProfile conversion, chunker, Timer
    source/PulseTimeApp.mc      AppBase: getInitialView / getGlanceView / onSettingsChanged
    source/PulseTimeView.mc     full view: vibrate on show, draw time + hints
    source/PulseTimeDelegate.mc select = replay, up/down = cycle system, back = exit
    source/PulseTimeGlance.mc   glance: "PulseTime" + current time
    source/PulseTimeComplication.mc  publishes the current time to the complication
    tests/gen-fixtures.mjs      writes tests/harness/source/Fixtures.mc from the shared JSON
    tests/harness/              tiny watch app that runs all 420 cases and prints PASS n/420
```

## App type, API floor, and the four rules the compiler enforces

The project was a `type="widget"` app before it published a complication, and is
now a `type="watch-app"` **device app**. Both types compile and both publish
successfully in the simulator, so the compiler did not force the change; the
documented contract did. `Toybox.Complications` says only "device apps and audio
content providers can publish complication information", and the Venu 2S device
profile has no `widget` app type at all - `compiler.json` lists
`audioContentProvider`, `background`, `datafield`, `glance`, `watchApp` and
`watchFace`, because System 5 devices retired widgets as a separate class.
Staying inside the documented set costs nothing here: the glance is untouched
(`getGlanceView` is unchanged and still runs), the app is the only product on a
CIQ 5.0 watch, and the one visible difference is that it lives in the app list
rather than a widget carousel the Venu 2S no longer has.

`minSdkVersion` moved from `3.2.0` to `4.2.0`, the level at which
`Toybox.Complications` arrived. That drops nothing: `venu2s` is the only product
and it is CIQ 5.0. `PulseTimeComplication.publish` still guards on
`Toybox has :Complications` and `Complications has :updateComplication`, so old
firmware on the same watch degrades to a no-op instead of an error.

Four rules the resource compiler enforces, each of which is a build error:

- publishing needs `<iq:uses-permission id="ComplicationPublisher"/>` in the
  manifest (reading other apps' complications would need `ComplicationSubscriber`
  as well - PulseTime only publishes, so it does not ask for it);
- the complication icon must be an **SVG**, not a bitmap;
- it must be a **different resource id** from the launcher icon;
- `value` takes a number or a **string resource reference**, never a literal -
  hence `@Strings.ComplicationPlaceholder`.

## Build

```sh
SDK="$(tr -d '\r\n' < "$HOME/Library/Application Support/Garmin/ConnectIQ/current-sdk.cfg")"
"$SDK/bin/monkeyc" -d venu2s -f appstores/garmin/pulsetime/monkey.jungle \
  -o appstores/garmin/dist/pulsetime-venu2s.prg \
  -y "$HOME/.config/ambient-time/garmin/developer_key.der"
# or
appstores/garmin/scripts/build-venu2s.sh            # add --harness for the test app
```

Sideload: copy `dist/pulsetime-venu2s.prg` to the watch's `GARMIN/APPS/`
folder over USB (or use the Ambient Time `sideload-venu2s.py` helper). Edit
settings in Garmin Connect once the app is on the watch.

## Test harness

```sh
cd appstores/garmin/pulsetime
node tests/gen-fixtures.mjs                              # regenerate Fixtures.mc after fixture changes
../scripts/build-venu2s.sh --harness
"$SDK/bin/connectiq" &                                    # simulator, if not already running
timeout 90 "$SDK/bin/monkeydo" ../dist/pulsetime-harness-venu2s.prg venu2s
# -> PASS 420/420 (chunker ok on all 420)
```

The harness pulls `PulseTimeEncoder.mc` and `PulsePlayer.mc` straight from
`source/` through its jungle `sourcePath` (per-file entries are valid jungle
paths, so no copy or symlink is needed). It runs six cases per 50 ms timer tick
because doing all 420 in one call trips the Connect IQ watchdog.

Float behaviour: Kotlin multiplies `Long * Float` in single precision and
truncates. Monkey C `Float` is also single precision, and the port uses
`(ms * mult).toNumber()` (truncation, never rounding). The fixtures confirm the
two agree on every case (for example 150 x 0.78 -> 116, 200 x 0.78 -> 156).

## Limits and remaining checks

- **An app, not a watch face.** `Attention.vibrate` is unavailable to watch
  faces on Connect IQ, so PulseTime cannot buzz from the face itself. The
  complication is the shortest path: one tap on the face launches the app, and
  the app buzzes on show.
- **No background haptic, by design.** Connect IQ cannot vibrate from a
  background process, so there is no `Toybox.Background` service and the
  complication's value only refreshes while the app is running.
- **Strength is duty cycle.** Garmin exposes vibration strength only as a
  0-100 duty cycle; the mapping to Wear OS amplitudes is approximate and
  motor-dependent. Perception of layered strength still needs a wrist check;
  no system depends on amplitude alone.
- **No mid-chunk cancel.** The platform has no vibrate-cancel call. Stopping
  (Back, or a replay) drops the queued chunks and stops the timer, but the
  chunk already handed to the motor (at most eight profiles, well under two
  seconds) finishes on its own.
- **Sideloaded; wrist checks pending.** The device evidence records a Venu 2S
  sideload, and the harness passes in the simulator. Whether its motor resolves
  58 ms pulses at 45 percent
  duty distinctly is a physical-device question, as with every PulseTime port.
  The complication is in the same position: `updateComplication` was verified to
  succeed at runtime in the simulator, but that the complication appears in the
  Venu 2S watch-face editor, and that tapping it launches the app and buzzes,
  has not been seen on hardware.
- Only `venu2s` is in the manifest. Other duty-cycle-capable devices should
  work but are untested and unlisted; Forerunners ignore duty cycle entirely.
