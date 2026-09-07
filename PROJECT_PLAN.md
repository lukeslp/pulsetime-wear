# PulseTime project plan

## Objective

A haptic clock for blind and low-vision wearers on Wear OS, Pebble, and Garmin.
Six time languages share the same encoding fixtures. Each platform offers
controls suited to its watch; Pebble also has an optional watchface.

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

## Wearable ports

| ID | Item | Status |
|----|------|--------|
| W1 | Reference encoder port + fixtures pinned to Kotlin (`PortFixturesTest`) | done |
| W2 | Garmin Venu 2S device app with glance, 8-profile chunker, settings | built, harness 420/420, sideloaded |
| W3 | Pebble watchapp, phone settings, host checks | 1.2.0 built for all six targets and installed on Time 2 and 2 SE; standalone mirror matches |
| W4 | Garmin complication → launch on tap; Pebble worker flick-launch | built, both sideloaded/installed; wrist verification open |
| W4b | Pebble PulseTime Face | 1.2.0 built for all six targets; round playback and return verified in emulators; earlier face installed on Time 2 |
| W6 | Eyes-free operation: confirmation cues, speed and practice on the wrist | done, verified on emulator + Time 2 |
| W7 | Hours-only / minutes-only per Quick Launch button (`launch_button()`) | done, 840 scoped fixtures; wrist check open because emulators cannot trigger a real Quick Launch |
| W8 | Speaker output on emery/flint: relative strength becomes pitch | done; wrist check open |
| W10 | Guided setup, bright/dark clock layouts, synchronized single digits | 1.2.0 merged; 90,720 reading cases and 57,600 clock-layout cases pass; wearer checks underway |
| W9 | Watchapp chimes via `wakeup_schedule()` (8 events max, 1-minute exclusion window) | built; rolling 8-slot window refills on every launch and skips quiet hours; no resident process between chimes |
| W5 | Store submissions (Connect IQ, Pebble appstore) | pending; three listings: Garmin app, Pebble watchapp, Pebble watchface; submit the Pebble app before the optional face |

## Outstanding

1. Finish Time 2 and 2 SE wearer checks: digit readability, vibration recognition,
   Quick Launch replies, sound on Time 2, and gesture false triggers over a day.
   Check round hardware when available. See [device evidence](DEVICE_EVIDENCE.md).
2. Share the Pebble tester package with the wearer guide, then prepare the app
   and face store listings. See [sharing notes](appstores/pebble/pulsetime/SHARING.md).
3. Complete haptic and TalkBack checks on Pixel Watch 2, then collect current
   Wear screenshots for the signed phone and Wear OS closed-test bundles.
