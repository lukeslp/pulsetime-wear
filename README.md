# PulseTime (Wear OS)

Eyes-free haptic clock for blind and low-vision Wear OS users. Tap the
**PulseTime** tile (or **Tell time** in the app) to feel the current local time
as a short vibration pattern.

**Author:** Luke Steuber · **License:** MIT

This project is standalone. It is not part of Ambient Time and shares no
package IDs, App Groups, or product coupling.

## Encoding (terse-quarter)

1. **Hours** 1–12 — 200 ms ON / 150 ms OFF  
2. **600 ms** silence  
3. **Quarters** 0–3 — 150 ms ON / 100 ms OFF  
4. Optional **exact minutes** (after training) — short ticks for the remainder inside the quarter

Example: **3:20** → 3 hour pulses, pause, 1 quarter pulse.

Base amplitude is 180/255 × your calibration scale.

## Requirements

- JDK 21 (toolchain) / project compiles with Java 17 bytecode  
- Android SDK with Wear OS (compileSdk 36, minSdk 30, targetSdk 35)  
- A physical watch for haptic QA — **emulator vibration is not sufficient**

## Build

```sh
./gradlew :app:testDebugUnitTest
./gradlew :app:assembleDebug
./gradlew :app:bundleRelease   # AAB for internal Play track (human publish)
```

Debug APK:

`app/build/outputs/apk/debug/app-debug.apk`  
(application id `com.lukesteuber.pulsetime.wear.debug`)

Release AAB:

`app/build/outputs/bundle/release/app-release.aab`  
(application id `com.lukesteuber.pulsetime.wear`)

## Install on Pixel Watch 2 (adb)

Pair the watch for wireless debugging, then:

```sh
adb devices
adb -s <watch-serial> install -r app/build/outputs/apk/debug/app-debug.apk

# Or, once a Wear device is connected:
bash scripts/install-watch.sh
# bash scripts/install-watch.sh <watch-serial>
```

Open **PulseTime**, run **Calibration**, complete **Training**, add the
**PulseTime** tile from the tile carousel, tap the tile to feel the time.

Retap while a pattern is playing to **cancel**.

## Permissions

- `VIBRATE` — haptic patterns  
- Tile provider binding (system)  

No network. No location.

## TalkBack checklist

- [ ] Every control has a spoken label  
- [ ] **Tell time** reachable within two swipes from launch  
- [ ] Training completable without sight  
- [ ] Tile content description announces the tell-time action  

## Platforms track

`scaffolded` → `built` → `validated` → `device-tested` → `store-staged` → **stop**  
(Play upload/publish is a human gate.)

## Phase 2 (not in this MVP)

Digits encoding, complications, hourly ambient chimes, phone sync, watchOS
(Core Haptics — only if meaningfully better than Apple Taptic Time).
