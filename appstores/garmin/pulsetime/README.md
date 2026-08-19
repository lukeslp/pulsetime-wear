# PulseTime for Garmin (Venu 2S)

A Connect IQ **widget** with a glance that plays the current local time as a
vibration pattern. It is the Garmin port of PulseTime's six haptic time systems
(see the repository README); the encoders are a line-for-line Monkey C port of
`shared/src/main/java/com/lukesteuber/pulsetime/shared/encoding/`, and are
locked to the same 420-case fixture set as the Pebble port
(`appstores/shared/fixtures/patterns.json`).

Author: Luke Steuber. License: MIT.

## How it feels

1. In the glance loop, the **PulseTime** glance shows the app name and the
   current time.
2. Tap it (or open PulseTime from the app list). The full view opens and the
   watch **vibrates the time immediately** in the selected system.
3. Tap the screen or press Start/Enter to **repeat**. Swipe up/down or press
   Down/Up to **switch time systems** (the choice is saved). Back exits, and
   any pattern still queued stops.

The screen shows the time large, the active system, and the repeat hint. If
vibration is switched off in the watch settings, the hint says so.

## The 8-profile limit and why the seams are inaudible

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
Duty is clamped to 1..100.

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
    manifest.xml              type="widget", venu2s, minSdkVersion 3.2.0
    monkey.jungle
    resources/drawables       launcher_icon.png (61x61, generated) + drawables.xml
    resources/strings         strings.xml
    resources/settings        properties.xml + settings.xml
    scripts/render-icon.py    regenerates the launcher icon (Pillow)
    source/PulseTimeEncoder.mc  pure encoders (no UI / Attention imports)
    source/PulsePlayer.mc       VibeProfile conversion, chunker, Timer
    source/PulseTimeApp.mc      AppBase: getInitialView / getGlanceView / onSettingsChanged
    source/PulseTimeView.mc     full view: vibrate on show, draw time + hints
    source/PulseTimeDelegate.mc select = replay, up/down = cycle system, back = exit
    source/PulseTimeGlance.mc   glance: "PulseTime" + current time
    tests/gen-fixtures.mjs      writes tests/harness/source/Fixtures.mc from the shared JSON
    tests/harness/              tiny watch app that runs all 420 cases and prints PASS n/420
```

Glances need Connect IQ API 3.1+, so `minSdkVersion="3.2.0"` (the same floor
the Ambient Time faces use) is sufficient; the Venu 2S itself is CIQ 5.0.

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
folder over USB (or use the Ambient Time `sideload-venu2s.py` helper). Settings
are edited from Garmin Connect once the app is on the watch.

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

## Limitations (honest)

- **Widget, not a watch face.** `Attention.vibrate` is unavailable to watch
  faces on Connect IQ, so PulseTime cannot buzz from the face; the glance loop
  plus one tap is the shortest path.
- **Strength is duty cycle.** Garmin exposes vibration strength only as a
  0-100 duty cycle; the mapping to Wear OS amplitudes is approximate and
  motor-dependent. Layered strength is still perceptible on the Venu 2S motor,
  but no system depends on amplitude alone.
- **No mid-chunk cancel.** The platform has no vibrate-cancel call. Stopping
  (Back, or a replay) drops the queued chunks and stops the timer, but the
  chunk already handed to the motor (at most eight profiles, well under two
  seconds) finishes on its own.
- **No device evidence yet.** Compile is clean and the harness passes in the
  simulator; whether the Venu 2S motor resolves 58 ms pulses at 45 percent
  duty distinctly is a physical-device question, as with every PulseTime port.
- Only `venu2s` is in the manifest. Other duty-cycle-capable devices should
  work but are untested and unlisted; Forerunners ignore duty cycle entirely.
