# Device evidence — PulseTime Wear

## Store-staged (local)

| Artifact | Path | Built |
|----------|------|-------|
| Debug APK | `app/build/outputs/apk/debug/app-debug.apk` | 2026-07-16 — `assembleDebug` green |
| Release AAB | `app/build/outputs/bundle/release/app-release.aab` | 2026-07-16 — `bundleRelease` green (debug signing for local stage only) |
| Unit tests | `./gradlew :app:testDebugUnitTest` | 31 tests, green (2026-07-18; JDK 21 + Android SDK configured) |

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

Play Console upload remains a **human** gate (do not auto-publish).
