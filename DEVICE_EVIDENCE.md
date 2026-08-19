# Device evidence — PulseTime Wear

## Store-staged (local)

| Artifact | Path | Built |
|----------|------|-------|
| Debug APK | `app/build/outputs/apk/debug/app-debug.apk` | 2026-07-16 — `assembleDebug` green |
| Phone release AAB | `dist/play/pulsetime-phone-0.3.0-10000003.aab` | 2026-07-19: clean-built and signed with the protected upload key |
| Wear release AAB | `dist/play/pulsetime-wear-0.3.0-20000003.aab` | 2026-07-19: clean-built and signed with the same protected upload key |
| Release validation | `scripts/build-play-release.sh` | Clean tests, release lint, signatures, certificate match, and SHA-256 checksums passed |
| Bundle validation | bundletool 1.18.3 | Both bundles valid; phone version code 10000003, Wear version code 20000003, package `com.lukesteuber.pulsetime.wear` |

## Device-tested phone companion

| Field | Value |
|-------|-------|
| Model | Google Pixel 10 (`frankel`) |
| Package | `com.lukesteuber.pulsetime.wear` v0.3.0, version code 10000003 |
| Install | 2026-07-19: bundletool installed the device-specific APK set generated from the signed Play AAB |
| Launch | Foreground activity verified as `com.lukesteuber.pulsetime.mobile.MainActivity` |
| Accessibility inspection | Primary cards expose descriptive text in logical order and measure at least 123 dp high; the retry control measures 48 dp high |
| Evidence | `play/assets/phone/phone-home.png`, `phone-time-systems.png`, `phone-terse-quarter.png`, `phone-watch-settings.png` |

## Device-tested Pulse Lab

| Field | Value |
|-------|-------|
| Model | Google Pixel 10 (`frankel`) |
| Package | `com.lukesteuber.pulsetime.hapticlab` v0.1.0 |
| Install | 2026-07-18: debug APK installed alongside PulseTime |
| Vibration | General signal and shared PulseTime time-encoder waveforms appeared in `VibratorManagerService` under the separate Pulse Lab package |
| Coverage | Six time systems, customization, blind recognition trials, six general signals, and structured feedback sharing |
| Evidence | `hapticlab/device-evidence/home-full.png`, `time-systems.png`, `market-detail.png`, `market-playing.png` |

## Device-tested (Pixel Watch 2)

**Status: installed — haptic / TalkBack QA pending human.**

The watch was not reachable during the 2026-07-19 release refresh. Its earlier debug install remains documented below, but current Wear screenshots and release-build checks must be completed before rollout.

| Field | Value |
|-------|-------|
| Model | Google Pixel Watch 2 (`aurora`) |
| Wear OS | 15 |
| Package | `com.lukesteuber.pulsetime.wear.debug` v0.1.0-debug |
| Install | 2026-07-16 22:04 — wireless pair `192.168.0.146` + sideload |
| Serial (mDNS) | `adb-38041RTJWW36NH-VNJaEW (2)._adb-tls-connect._tcp` |

```sh
adb pair <watch-ip>:<pairing-port>
./scripts/install-watch.sh
```

Human checklist:

- [x] `getprop ro.product.model` → Google Pixel Watch 2 / aurora
- [ ] Tell time matches wall clock (±1s) for a known time (e.g. 3:20 → 3 + pause + 1)
- [ ] Retap cancels in-flight pattern
- [ ] Calibration steps feel monotonically stronger
- [ ] TalkBack: Tell time ≤2 swipes; training completable eyes-free
- [ ] Tile tap plays time without hunting a small target

Play Console login, upload, tester management, and rollout remain **human** gates.

## Garmin Venu 2S — PulseTime app (2026-08-18)

- [x] `appstores/garmin/dist/pulsetime-venu2s.prg` sideloaded over MTP
  (object id 16777987); imports when the watch disconnects.
- [ ] Wrist: glance → tap plays the time; 3:20 → 3 + pause + 1 (Terse
  Quarter); seams between 8-profile chunks imperceptible; Soft/Standard/Strong
  duty differences felt; tap/press repeats; Up/Down cycles.

## Pebble Time 2 — PulseTime watchapp (2026-08-18)

- [x] `pulsetime.pbw` installed over the CloudPebble developer connection
  ("App install succeeded"), final build sha256 952aaa01….
- [ ] Wrist: launch vibrates immediately; Select repeats; Up/Down cycles;
  flick repeats; Quick Launch binding; config page reachable from the phone
  app; Pebble 2 SE install.

## W4 invocation paths (2026-08-18)

- [x] Garmin: app type changed `widget` → `watch-app` (System 5 retired
  widgets; `compiler.json` for venu2s lists no widget type), `minSdkVersion`
  4.2.0, `ComplicationPublisher` permission, one published complication
  (`resources/complications/complications.xml`, SVG icon) updated with the
  current time on every open. Re-sideloaded (object id 16777988).
- [ ] Venu 2S: does the watch-face editor offer the PulseTime complication,
  and does tapping it launch the app and buzz? `updateComplication` succeeding
  in the simulator is as far as off-device evidence goes.
- [x] Pebble: background worker installed on the Time 2; double-tap → launch →
  vibrate → auto-exit verified in the basalt emulator.
- [ ] Time 2: enable "Flick to tell time", then check false-trigger rate during
  ordinary movement and whether the round trip reads as "no app". Tighten
  `PT_TAP_WINDOW_MS` toward 800 ms if twitchy.

