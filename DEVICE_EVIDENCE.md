# Device evidence — PulseTime Wear

## Store-staged (local)

| Artifact | Path | Built |
|----------|------|-------|
| Debug APK | `app/build/outputs/apk/debug/app-debug.apk` | 2026-07-16 — `assembleDebug` green |
| Phone release AAB | `dist/play/pulsetime-phone-0.3.0-10000003.aab` | 2026-07-18: signed with the protected upload key |
| Wear release AAB | `dist/play/pulsetime-wear-0.3.0-20000003.aab` | 2026-07-18: signed with the same protected upload key |
| Release validation | `scripts/build-play-release.sh` | Clean tests, release lint, signatures, certificate match, and SHA-256 checksums passed |

## Device-tested phone companion

| Field | Value |
|-------|-------|
| Model | Google Pixel 9a (`tegu`) |
| Package | `com.lukesteuber.pulsetime.wear.debug` v0.3.0-debug |
| Install | 2026-07-18: companion debug APK installed over the earlier watch build |
| Evidence | `play/assets/phone/phone-home.png`, `phone-time-systems.png` |

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
