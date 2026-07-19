# PulseTime (Wear OS)

Eyes-free haptic clock for blind and low-vision Wear OS users. Tap the
**PulseTime** tile (or **Tell time** in the app) to feel the current local time
as a short vibration pattern.

**Author:** Luke Steuber · **License:** MIT

This project is standalone. It is not part of Ambient Time and shares no
package IDs, App Groups, or product coupling.

## Time systems

PulseTime now includes six selectable haptic time languages:

- **Terse Quarter** — hours, quarters, and optional leftover-minute ticks
- **Five-Minute Ladder** — hours, five-minute steps, and leftover minutes
- **Decimal Place Rhythm** — hours, minute tens, and minute ones
- **Binary Clock** — four hour bits followed by six minute bits
- **Nearest-Quarter Offset** — a quarter-hour landmark plus before/after minutes
- **Morse Digits** — displayed digits as standard Morse numerals

The original Terse Quarter system remains the default for existing installs.
Settings also control playback speed and whether relative-strength layers are
used alongside duration and rhythm. No system depends on amplitude alone.

## Android companion

The `:mobile` companion teaches every system on its own page. Each page includes
a pulse legend, worked examples, watch demonstrations, and a replayable test.
Separate pages explain playback speed, layered strength, and exact minutes.

When a compatible watch is reachable, the phone sends an immediate versioned
playback request through the Wear OS Data Layer. The watch validates the request,
generates the pattern locally, starts the vibration, and acknowledges playback.
Demonstrations never change saved watch settings and are never queued offline.

### Terse Quarter

1. **Hours** 1–12 — 200 ms ON / 280 ms OFF
2. **700 ms** silence
3. **Quarters** 0–3 — 320 ms ON / 280 ms OFF
4. Optional **exact minutes** (after training) — 80 ms ticks, separated by 120 ms, for the remainder inside the quarter

Example: **3:20** → 3 hour pulses, pause, 1 quarter pulse.

Base amplitude is 180/255 × your calibration scale.

## Requirements

- JDK 21 (toolchain) / project compiles with Java 17 bytecode  
- Android SDK with Wear OS (compileSdk 36, minSdk 30, targetSdk 35)  
- A physical watch for haptic QA — **emulator vibration is not sufficient**

Gradle must run with JDK 21, and the Android SDK must be available through
`ANDROID_HOME` or `sdk.dir` in the ignored `local.properties` file. JDK 26 is
not supported by this project's Kotlin/Gradle toolchain.

## Build

```sh
./gradlew :app:testDebugUnitTest
./gradlew :app:assembleDebug
./gradlew :shared:testDebugUnitTest :mobile:assembleDebug
./gradlew :app:bundleRelease   # AAB for internal Play track (human publish)
```

Debug APK:

`app/build/outputs/apk/debug/app-debug.apk`  
(application id `com.lukesteuber.pulsetime.wear.debug`)

Companion debug APK:

`mobile/build/outputs/apk/debug/mobile-debug.apk`

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

## Later work

Complications, hourly ambient chimes, phone sync, watchOS
(Core Haptics — only if meaningfully better than Apple Taptic Time).
