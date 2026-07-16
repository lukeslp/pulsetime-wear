# Device evidence — PulseTime Wear

## Store-staged (local)

| Artifact | Path | Built |
|----------|------|-------|
| Debug APK | `app/build/outputs/apk/debug/app-debug.apk` | 2026-07-16 — `assembleDebug` green |
| Release AAB | `app/build/outputs/bundle/release/app-release.aab` | 2026-07-16 — `bundleRelease` green (debug signing for local stage only) |
| Unit tests | `./gradlew :app:testDebugUnitTest` | 20 tests, green |

## Device-tested (Pixel Watch 2)

**Status: blocked — no Wear device in `adb devices` during this session.**

Phones may appear when USB-connected; the watch needs its own Wireless debugging session:

```sh
adb pair <watch-ip>:<pairing-port>
adb connect <watch-ip>:<port>
./scripts/install-watch.sh
```

After install, record here:

- [ ] `adb -s <serial> shell getprop ro.product.model` → Pixel Watch 2 / aurora
- [ ] Tell time matches wall clock (±1s) for a known time (e.g. 3:20 → 3 + pause + 1)
- [ ] Retap cancels in-flight pattern
- [ ] Calibration steps feel monotonically stronger
- [ ] TalkBack: Tell time ≤2 swipes; training completable eyes-free
- [ ] Tile tap plays time without hunting a small target

Play Console upload remains a **human** gate (do not auto-publish).
