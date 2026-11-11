# PulseTime platform status

## Pebble 1.2.0

| Package | Ready | Still to check |
|---|---|---|
| [PulseTime Public Beta](https://apps.repebble.com/1624684ac5dd46ac85eee2a6) | Published as version 1.2.0; clean builds for all six targets; installed on Time 2 and 2 SE; lessons, settings, and large synchronized digits checked in emulators | On-wrist readability, vibration recognition, Quick Launch replies, and gesture false triggers |
| PulseTime Face | Clean builds for all six targets; shared encoder and readout; round-screen playback and return checked in emulators | Physical 1.2.0 install, double-tap reliability, and battery use |
| Standalone imports | App and face repositories contain matching build inputs and pass independent builds | Source repositories remain private; separate watchface store listing pending |

The [wearer guide](appstores/pebble/pulsetime/GUIDE.md) covers installation and
daily controls. The [validation record](appstores/pebble/pulsetime/VALIDATION.md)
records 90,720 synchronized-reading cases, 57,600 clock-layout cases, and the
unchanged haptic fixtures. See [sharing notes](appstores/pebble/pulsetime/SHARING.md)
for the tester package.

## Wear OS and Android

| Stage | Status | Evidence |
|-------|--------|----------|
| scaffolded | done | Gradle Wear Compose app, standalone, minSdk 30 |
| built | done | `./gradlew :app:assembleDebug`; APK at `app/build/outputs/apk/debug/` |
| validated | done | 2026-07-19 clean release tests and lint green; both signed AABs pass checksum, certificate, and bundletool validation |
| companion | built | Android companion APK with six system guides and Data Layer watch playback |
| phone haptic lab | built; earlier device checks recorded | Current source is 0.2.0 with six time languages, twelve fictional signals, and feedback sharing; device evidence below records the earlier 0.1.0 build on Pixel 10 |
| device-tested | installed; wearer checks pending | Pixel Watch 2 has an earlier debug build; haptic and TalkBack checks remain. See `DEVICE_EVIDENCE.md` |
| store-staged | signed | Phone and Wear AABs under `dist/play/`; unique version codes, one upload certificate, listing copy, Play graphics, phone screenshots, App content answers, and privacy policy prepared |
| closed-test | human gate | Add real Wear screenshots, upload both form-factor bundles, add testers, and start rollout in Play Console |

Do not submit to Play from automation.

## Earlier Garmin and Pebble checks

These records describe the earlier builds. Current Pebble 1.2.0 coverage is above.

| Port | Stage | Evidence |
|------|-------|----------|
| Garmin device app + glance + complication (`appstores/garmin/pulsetime`) | built, encoder validated, sideloaded | `monkeyc` clean; simulator harness `PASS 420/420` incl. chunker invariants; PRG sideloaded to the Venu 2S over MTP (imports on disconnect); complication publishes in the simulator but face-editor hosting and tap-to-launch are unverified off-device; wrist QA pending |
| Pebble watchapp (`appstores/pebble/pulsetime`) | built, encoder validated, device-installed | host C test `420/420 match` + `840/840 scoped match` + cue and pitch checks; basalt/diorite/chalk/emery emulators; the former standalone mirror `ambient-time/pebble-pulsetime` is archived, so this directory is the only source |
| Pebble chimes (`wakeup_schedule`, in the watchapp) | built · verified with the app closed | at 19:30:00 on the emery emulator with PulseTime closed and a watchface on screen, the system launched it, it played `scope=full`, rescheduled itself to 19:45 (8/8 slots) and exited with `Still allocated <0B>`; present on every platform including the frozen ones, so it behaves identically on a Pebble 2 SE; installed on the Time 2 and confirmed on hardware (`appstores/pebble/pulsetime/docs/time2-live.png`) |
| Pebble watchface (`appstores/pebble/pulsetime-face`) | built, encoder shared, device-installed, import-ready | shares the watchapp's encoder by symlink, so one host test covers both; emery emulator confirms tap discrimination (single armed, cross-axis rejected, same-axis 718 ms plays), hourly chime striking `scope=hours` at 11:00, and quiet hours suppressing it; installed on the Pebble Time 2; the former root mirror `ambient-time/pebble-pulsetime-face` is archived, so this directory is the only source; wrist QA pending |
| Store at the time of these earlier checks | not submitted then | The current Pebble app publication is recorded above. The Garmin app and separate Pebble watchface listings remain pending. |
