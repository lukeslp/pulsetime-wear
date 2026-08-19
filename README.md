# PulseTime (Wear OS)

Eyes-free haptic clock for blind and low-vision Wear OS users. Tap the
**PulseTime** tile (or **Tell time** in the app) to feel the current local time
as a short vibration pattern.

**Author:** Luke Steuber · **License:** MIT

This project is standalone. It is not part of Ambient Time and shares no
package IDs, App Groups, or product coupling.

## Time systems

PulseTime now includes six selectable haptic time languages:

- **Terse Quarter:** hours, quarters, and optional leftover-minute ticks
- **Five-Minute Ladder:** hours, five-minute steps, and leftover minutes
- **Decimal Place Rhythm:** hours, minute tens, and minute ones
- **Binary Clock:** four hour bits followed by six minute bits
- **Nearest-Quarter Offset:** a quarter-hour landmark plus before/after minutes
- **Morse Digits:** displayed digits as standard Morse numerals

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

## Pulse Lab phone testing

The separate `:hapticlab` app runs the same six PulseTime encoders through the
phone's vibrator. It includes practice times, speed and strength controls,
exact-minute options, explanations, and untimed blind trials.

The signal library contains twelve fictional examples. They cover markets,
weather, messages, calendar reminders, transit, exercise pace, home events,
software status, and live games. Every example follows the same tactile phrase:
topic, payload, optional modifier, and optional action. Several examples also
compare a short headline with the full phrase.

The perception lab reports the phone's haptic capabilities, checks comfortable
strength, and compares direction, urgency, and field-boundary patterns. A
personal pattern builder lets testers arrange short pulses, long pulses,
strength levels, and pauses. Pulse Lab records answers, confidence, replays,
test context, and untimed total round duration for the current session.

The feedback page lets blind and sighted testers describe what they felt, what
they thought it meant, comfort, TalkBack, and navigation. Feedback leaves the
app only when the tester opens Android's share sheet and chooses a destination.
The app explains its testing limits, safety boundaries, and phrase grammar on
the **How Pulse Lab works** page.

Pulse Lab uses the independent package `com.lukesteuber.pulsetime.hapticlab`,
so it can be installed beside both PulseTime apps. It needs no watch, pairing,
network access, account, location, contacts, or notification permissions.

### Terse Quarter

1. **Hours** 1–12: 200 ms ON / 280 ms OFF
2. **700 ms** silence
3. **Quarters** 0–3: 320 ms ON / 280 ms OFF
4. Optional **exact minutes** after training: 80 ms ticks, separated by 120 ms, for the remainder inside the quarter

Example: **3:20** → 3 hour pulses, pause, 1 quarter pulse.

Base amplitude is 180/255 × your calibration scale.

## Requirements

- JDK 21 (toolchain) / project compiles with Java 17 bytecode  
- Android SDK with Wear OS (compileSdk 36, minSdk 30, targetSdk 35)  
- A physical watch for haptic QA. **Emulator vibration is not sufficient.**

Gradle must run with JDK 21, and the Android SDK must be available through
`ANDROID_HOME` or `sdk.dir` in the ignored `local.properties` file. JDK 26 is
not supported by this project's Kotlin/Gradle toolchain.

## Build

```sh
./gradlew :app:testDebugUnitTest
./gradlew :app:assembleDebug
./gradlew :shared:testDebugUnitTest :mobile:assembleDebug
./gradlew :hapticlab:testDebugUnitTest :hapticlab:lintDebug :hapticlab:assembleDebug
scripts/build-play-release.sh  # signed phone + Wear AABs for closed testing
```

Debug APK:

`app/build/outputs/apk/debug/app-debug.apk`  
(application id `com.lukesteuber.pulsetime.wear.debug`)

Companion debug APK:

`mobile/build/outputs/apk/debug/mobile-debug.apk`

Pulse Lab debug APK:

`hapticlab/build/outputs/apk/debug/hapticlab-debug.apk`

Release AAB:

- `dist/play/pulsetime-phone-0.3.0-10000003.aab`
- `dist/play/pulsetime-wear-0.3.0-20000003.aab`
(application id `com.lukesteuber.pulsetime.wear`; human Play upload)

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

- `VIBRATE`: haptic patterns
- Tile provider binding (system)  

No network. No location.

## TalkBack checklist

- [ ] Every control has a spoken label  
- [ ] **Tell time** reachable within two swipes from launch  
- [ ] Training completable without sight  
- [ ] Tile content description announces the tell-time action  

## Garmin and Pebble ports (2026-08-18)

Play is blocked for now, so PulseTime also ships natively where the stores are
open. Both ports run the same six systems from the same fixtures:

- `appstores/garmin/pulsetime/` — a Connect IQ **widget** for the Venu 2S
  (Attention.vibrate is unavailable to watch faces and takes at most eight
  VibeProfiles per call, so patterns are chunked at silences and chained with
  a timer; strength = duty cycle). Glance → tap → feel the time; tap/press
  repeats, swipe/Up/Down cycles systems; settings in the Connect IQ phone
  app. `appstores/garmin/pulsetime/README.md`.
- `appstores/pebble/pulsetime/` — a C **watchapp** for basalt/chalk/diorite/
  emery/flint/gabbro (`vibes_enqueue_custom_pattern`, no amplitude, so layered
  strength does not exist there). Bind it to Quick Launch; Select repeats,
  Up/Down cycles systems, a wrist flick repeats; Clay config page for system,
  speed, exact minutes. `appstores/pebble/pulsetime/BUILD.md`.
- `appstores/shared/pulsetime-patterns.mjs` is a reference port that writes
  `appstores/shared/fixtures/patterns.{json,txt}` (420 cases);
  `shared/src/test/.../PortFixturesTest.kt` proves those fixtures equal the
  Kotlin encoders, and each port's harness reproduces them exactly (Pebble:
  host C test; Garmin: simulator harness).

Neither platform can vibrate from a background process
(`docs/invocation-paths.md` records the verified per-platform rules and the
gesture paths that follow from them). The nearest "no app to open" paths — a Garmin complication that launches the widget on tap, and a
Pebble background worker that launches the app on a wrist flick (plus an
optional PulseTime watchface) — are the next additions.

## Platforms track

`scaffolded` → `built` → `validated` → `device-tested` → `store-staged` → **stop**  
(Play upload/publish is a human gate.)

## Later work

Complications, hourly ambient chimes, phone sync, watchOS
(Core Haptics, only if meaningfully better than Apple Taptic Time).
