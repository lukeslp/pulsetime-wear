# PulseTime — project plan (MVP v0.1)

## Objective

Standalone Wear OS haptic clock for blind/low-vision users. Six selectable
haptic time systems, Tile-first invocation, training + calibration. No Ambient Time
coupling.

## Architecture

- `:app` Wear Compose app + Tile and remote playback service
- `:mobile` accessible Android companion for learning, practice, and watch demos
- `:shared` versioned playback protocol and six-system education catalog
- Pure encoder registry with six time systems (JVM unit tests)
- `AndroidHapticPlayer` via `VibratorManager` / amplitude waveform
- `PulseSettingsRepository` (DataStore): system, amplitude layers, speed, precision, training
- Surfaces: Tile → `TellTimeRelayActivity` (blank); in-app Tell time / Training / Calibration

## Milestone status

| ID | Item | Status |
|----|------|--------|
| M0 | Gradle scaffold | done |
| M1 | Encoding + tests | done |
| M2 | Haptic player + cancel | done |
| M3 | Tile + Tell time | done |
| M4 | Calibration + training | done |
| M5 | Device evidence + AAB stage | AAB staged; Pixel Watch 2 installed; haptic and TalkBack QA pending human review |
| M6 | Companion learning and remote watch demos | built; paired-device QA pending |

## Wearable ports (2026-08-18)

| ID | Item | Status |
|----|------|--------|
| W1 | Reference encoder port + fixtures pinned to Kotlin (`PortFixturesTest`) | done |
| W2 | Garmin Venu 2S device app with glance, 8-profile chunker, settings | built, harness 420/420, sideloaded |
| W3 | Pebble watchapp, Clay config, host test | built, 420/420, on the Time 2 |
| W4 | Garmin complication → launch on tap; Pebble worker flick-launch | built, both sideloaded/installed; wrist verification open |
| W4b | Pebble PulseTime *watchface* variant (the only true no-app haptic path) | **built**, encoder shared by symlink, tap + chime + quiet hours verified on the emery emulator, installed on the Time 2 |
| W6 | Eyes-free operation: confirmation cues, speed and practice on the wrist | done, verified on emulator + Time 2 |
| W7 | Hours-only / minutes-only per Quick Launch button (`launch_button()`) | done, 840 scoped fixtures; **wrist check open** — no emulator can trigger a real Quick Launch |
| W8 | Speaker output on emery/flint: relative strength becomes pitch | done; wrist check open |
| W9 | `wakeup_schedule()` chimes in the **watchapp** — the only path that fires with nothing resident and regardless of which watchface is showing (8 events max, 1-minute exclusion window) | **built**, rolling 8-slot window topped up on every launch, quiet hours applied at scheduling time; works on every platform including the frozen ones |
| W5 | Store submissions (Connect IQ, Pebble appstore) | human gate — now **three** listings: Garmin app, Pebble watchapp, Pebble watchface |

## Outstanding

0. Pebble wrist QA, which is now the gate on almost everything: Quick Launch
   scopes (unreachable from any emulator), the watchface's false-trigger rate
   over a real day, speaker legibility, and the Pebble 2 SE degradation path.
   See `DEVICE_EVIDENCE.md`.
1. Human haptic and TalkBack QA on the installed Pixel Watch 2
2. Human Play internal-track upload (replace debug signing with upload keystore)
