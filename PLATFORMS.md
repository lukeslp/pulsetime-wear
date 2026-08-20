# PulseTime — platforms track (Wear OS)

| Stage | Status | Evidence |
|-------|--------|----------|
| scaffolded | done | Gradle Wear Compose app, standalone, minSdk 30 |
| built | done | `./gradlew :app:assembleDebug` — APK at `app/build/outputs/apk/debug/` |
| validated | done | 2026-07-19 clean release tests and lint green; both signed AABs pass checksum, certificate, and bundletool validation |
| companion | built | Android companion APK with six system guides and Data Layer watch playback |
| phone haptic lab | device-tested | Separate phone-only app with all six time systems, customization, blind-test signal optimization, structured feedback, and six general-purpose demos; installed and vibration playback verified on Pixel 9a |
| device-tested | installed; QA pending | Pixel Watch 2 is installed; human haptic and TalkBack QA remain — see `DEVICE_EVIDENCE.md` |
| store-staged | signed | Phone and Wear AABs under `dist/play/`; unique version codes, one upload certificate, listing copy, Play graphics, phone screenshots, App content answers, and privacy policy prepared |
| closed-test | human gate | Add real Wear screenshots, upload both form-factor bundles, add testers, and start rollout in Play Console |

Do not submit to Play from automation.

## Garmin Venu 2S and Pebble (added 2026-08-18)

| Port | Stage | Evidence |
|------|-------|----------|
| Garmin device app + glance + complication (`appstores/garmin/pulsetime`) | built · encoder validated · sideloaded | `monkeyc` clean; simulator harness `PASS 420/420` incl. chunker invariants; PRG sideloaded to the Venu 2S over MTP (imports on disconnect); complication publishes in the simulator but face-editor hosting and tap-to-launch are unverified off-device; wrist QA pending |
| Pebble watchapp (`appstores/pebble/pulsetime`) | built · encoder validated · device-installed | host C test `420/420 match` + `840/840 scoped match` + cue and pitch checks; basalt/diorite/chalk/emery emulators; installed on the Pebble Time 2 via CloudPebble; wrist QA and Pebble 2 SE pending |
| Pebble chimes (`wakeup_schedule`, in the watchapp) | built · verified with the app closed | at 19:30:00 on the emery emulator with PulseTime closed and a watchface on screen, the system launched it, it played `scope=full`, rescheduled itself to 19:45 (8/8 slots) and exited with `Still allocated <0B>`; present on every platform including the frozen ones, so it behaves identically on a Pebble 2 SE; installed on the Time 2 and confirmed on hardware (`appstores/pebble/pulsetime/docs/time2-live.png`) |
| Pebble watchface (`appstores/pebble/pulsetime-face`) | built · encoder shared · device-installed | shares the watchapp's encoder by symlink, so one host test covers both; emery emulator confirms tap discrimination (single armed, cross-axis rejected, same-axis 718 ms plays), hourly chime striking `scope=hours` at 11:00, and quiet hours suppressing it; installed on the Pebble Time 2; wrist QA pending |
| Store | not submitted | Connect IQ store and Pebble appstore are per-app human submissions. The watchapp and the watchface are **two separate listings** — neither store bundles apps. |

