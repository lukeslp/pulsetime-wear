# PulseTime for Pebble (.pbw)

PulseTime for Pebble is a **C watchapp on SDK 4.33.1** that plays the current
local time as a vibration pattern, for blind and low-vision users. It ports the
six haptic time systems from `shared/.../encoding/TimeSystems.kt` and
`TerseQuarterEncoder.kt` to pure C (`src/c/encoder.c`), and reproduces every
case in `appstores/shared/fixtures/patterns.json` byte for byte (host test
below). Author: Luke Steuber.

Targets: basalt, chalk, diorite, emery, flint, gabbro (every current Pebble
except the 2013 aplite). Verified 2026-08-18: clean build for all six, runs in
the `basalt`, `diorite`, and `chalk` emulators (`docs/*.png`).

## How it behaves on the watch

- **Launch = tell time.** The app encodes `localtime()` with the saved settings
  and vibrates immediately. It is a watchapp (not a watchface) so it can be
  bound to **Quick Launch** (hold Up or Down) — that is the intended gesture.
- **Select** repeats with a fresh time (`vibes_cancel()` first).
- **Up / Down** cycle the six time systems; the choice is persisted, redrawn,
  and played straight away. **Back** exits.
- A **wrist flick** (`accel_tap_service`) also repeats.
- Screen: the time large (Leco), the system name, its short label and speed
  (or "Playing h:mm" while the pattern runs), and two hint lines. Works in
  1-bit on diorite/flint, centres inside the chalk circle, and scales up on
  emery/gabbro (`big` = unobstructed height >= 200).
- Settings (`persist_*` keys 1–3): time system, playback speed, exact minutes.
  Defaults: Terse Quarter, Standard, off.

Pebble motors have **no amplitude control**, so the encoder's relative
`layer` (100/72/50/45) is carried for the host test but ignored on the watch.
No PulseTime system depends on strength alone.

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
inherently exact) — and sends the values over AppMessage as the keys
`SYSTEM` / `SPEED` (storage-id strings) and `EXACT_MINUTES` (0/1). The watch
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
# src/c/encoder.c + tests/check_fixtures.c with cc, prints "420/420 match"
# and exits non-zero on any mismatch.
```

## Build

```sh
cd appstores/pebble/pulsetime
pebble build
ls -la build/pulsetime.pbw
```

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

## Hardware install (Core Devices phone app)

Enable **Developer Connection** in the Pebble app, keep that screen open, make
the target watch the active watch, then:

```sh
pebble login                          # once, same account as the phone app
pebble install --cloudpebble
pebble logs --cloudpebble
```

Then bind PulseTime to Quick Launch: Settings > Quick Launch > Up (or Down)
> PulseTime.

## Toolchain notes

- pebble-tool must run on the same Python as the SDK venv (3.14 on this Mac).
- QEMU is not bundled with pebble-tool; `pebble sdk install latest` provides
  `toolchain/bin/qemu-pebble` (`PEBBLE_QEMU_PATH`).
- The `ld: ... LOAD segment with RWX permissions` warnings are normal for the
  Pebble toolchain.
