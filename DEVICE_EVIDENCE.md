# PulseTime device evidence

## Current Pebble build: 1.2.0

The watchapp installed successfully on **Time 2 and Pebble 2 SE** through
Developer Connection. Both received the same committed-source package, with
SHA-256 `84b74b820c12476fe9ce0a49378558a5dd989d84b40c8570d5256aaf36befb6e`.
Luke is testing the large digits alongside the vibration patterns.

Both app and face build for all six targets. Emulator checks cover the large
playback digits, labels, round-screen clipping, and return to the previous
screen. The [1.2.0 validation record](appstores/pebble/pulsetime/VALIDATION.md)
contains the screenshots and test counts. Physical readability, tactile
recognition, and Quick Launch checks remain open. The 1.2.0 face has emulator
coverage; its physical install remains pending.

The dated records below describe earlier versions and retain their original
observations. Use the [wearer guide](appstores/pebble/pulsetime/GUIDE.md) for
current controls.

## Store-staged (local)

| Artifact | Path | Built |
|----------|------|-------|
| Debug APK | `app/build/outputs/apk/debug/app-debug.apk` | 2026-07-16: `assembleDebug` green |
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

**Status: installed; haptic / TalkBack QA pending human.**

The watch was not reachable during the 2026-07-19 release refresh. Its earlier debug install remains documented below, but current Wear screenshots and release-build checks must be completed before rollout.

| Field | Value |
|-------|-------|
| Model | Google Pixel Watch 2 (`aurora`) |
| Wear OS | 15 |
| Package | `com.lukesteuber.pulsetime.wear.debug` v0.1.0-debug |
| Install | 2026-07-16 22:04: wireless debugging pair on the local network + sideload |
| Serial (mDNS) | omitted; use `adb devices` to read the paired watch's serial |

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

## Garmin Venu 2S: PulseTime app (2026-08-18)

- [x] `appstores/garmin/dist/pulsetime-venu2s.prg` sideloaded over MTP
  (object id 16777987); imports when the watch disconnects.
- [ ] Wrist: glance → tap plays the time; 3:20 → 3 + pause + 1 (Terse
  Quarter); seams between 8-profile chunks imperceptible; Soft/Standard/Strong
  duty differences felt; tap/press repeats; Up/Down cycles.

## Pebble Time 2: PulseTime watchapp (2026-08-18)

- [x] `pulsetime.pbw` installed over the CloudPebble developer connection
  ("App install succeeded"), final build sha256 952aaa01….
- [ ] Wrist: launch vibrates immediately; Select plays the whole time; Up plays
  hours; Down plays minutes; hold Up/Down changes language; flick repeats;
  Quick Launch binding; config page reachable from the phone app; Pebble 2 SE
  install.

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


## Earlier controls, scopes, sound, practice (2026-08-19)

The short-press system / long-press speed mapping below documents the earlier
build. It was deliberately superseded by the streamlined mapping verified on
2026-08-21: ordinary buttons now answer stable questions and cannot change the
learned language accidentally.

Emulator evidence, all on the emery (Pebble Time 2) profile unless noted:

- [x] One short Up/Down press fires only the system handler, one long press only
  the speed handler; the two do not double-fire (basalt).
- [x] Rung 2 of the system cycle logs `exact=1` with cue ×2; rung 3 is the
  five-minute ladder with cue ×3.
- [x] Hold-Select opens Practice at 1:00; Up steps to 2:00; Back returns.
- [x] Hold-**Back** is unusable: the system's own back-out swallows it and the
  app simply exits. BACK is left entirely unsubscribed.
- [x] `840/840 scoped match` in the Kotlin harness and the Pebble C harness; an
  exhaustive Kotlin sweep (6 systems × 3 speeds × 2 scopes × all 1440 minutes)
  confirms no scoped pattern is ever silent.
- [x] Speaker code links on emery and flint only, confirmed with `nm` on the
  built ELFs, not gabbro, despite it being new hardware.
- [ ] **Wrist, Time 2:** bind both Quick Launch buttons and confirm hold-Up
  gives hours and hold-Down gives minutes. This is the one part of the scope
  feature no emulator can exercise: `launch_button()` needs a real Quick Launch.
- [ ] **Wrist, Time 2:** sound on. Is pitch actually easier to read than the
  motor for the five-minute ladder's three layers? Is the volume ladder
  (30/65/100 %) usefully spaced?
- [ ] **Pebble 2 SE (diorite):** install and confirm the graceful degradation:
  scopes and sound absent, everything else identical.

## Pebble Time 2: PulseTime Face watchface (2026-08-19)

- [x] `pulsetime-face.pbw` installed over the CloudPebble developer connection.
      **Installing a watchface makes it selectable; choosing it replaces the
      wearer's current face.**
- [x] Emery emulator: face renders time, date, system, and prompt; a single tap
  arms without playing; two taps on different axes do not play; two same-axis
  taps 718 ms apart play the time.
- [x] Emery emulator: with chime off, crossing 11:00 does nothing. With hourly
  chime and no quiet period, 11:00 plays `scope=hours`, striking the hour.
  With quiet hours 0–23, 13:00 plays nothing.
- [ ] **Wrist:** false-trigger rate during ordinary movement over a full day.
  This is the number that decides whether the face is usable, and no emulator
  can produce it. Tighten `PT_TAP_WINDOW_MS` toward 800 ms if twitchy; the
  DEBUG tap log prints the axis and gap of every tap for exactly this.
- [ ] **Wrist:** battery cost of the always-on tap subscription over a day,
  against the watchapp + worker for comparison.
- [ ] **Wrist:** is an hourly chime welcome or irritating in practice, and are
  the default quiet hours (22:00–07:00) the right ones?
- [x] **The face only runs while it is the foreground app.** Being the selected
  face is not enough. With PulseTime Face selected but the launcher open,
  crossing 15:00 fired no chime; the same crossing with the face on screen fired
  at 14:00. It resumes by itself on the way back. Chimes that survive this need
  `wakeup_schedule()` in the watchapp (W9) - see `docs/invocation-paths.md`.

## Background chimes via wakeup_schedule (2026-08-19)

The appeal of a haptic clock is that it can tell you the time without being
asked. On Pebble that is `wakeup_schedule()` and nothing else: a watchface only
runs while it is the foreground app, and no background context can vibrate.

Emery emulator, quarter-hourly chimes, PulseTime **closed** and the watchface on
screen (its own chime set to hourly, so it could not be the source at :30):

```
[19:16:02] PulseTime: next chime 19:30
[19:16:02] PulseTime: chime=quarter scheduled 8/8 wakeups
[19:16:35] Heap Usage for App <PulseTime>: ... Still allocated <0B>   <- app exits
[19:30:00] Heap Usage for App <PulseTime                              <- system launches it
[19:30:00] PulseTime: five_minute_ladder standard full exact=0 19:30 -> 16 seg
[19:30:00] PulseTime: next chime 19:45
[19:30:00] PulseTime: chime=quarter scheduled 8/8 wakeups             <- chain re-arms
[19:30:07] Heap Usage for App <PulseTime>: ... Still allocated <0B>   <- exits clean
```

- [x] Fires at the exact boundary with nothing of ours running.
- [x] Plays the whole time at :30 (`scope=full`); the hour strike is `scope=hours`.
- [x] Re-arms its own rolling window, so the wearer never has to open the app.
- [x] Exits with `Still allocated <0B>`: no leak across repeated wakeups.
- [x] 8/8 slots accepted; no `E_RANGE` collisions with other apps on this watch.
- [ ] **Wrist, Time 2:** does the brief app appearance read as intrusive, or does
  it pass unnoticed? This is the one thing the log cannot answer.
- [ ] **Wrist:** confirm the chain survives a watch power cycle
  (`notify_if_missed` is set, so missed chimes should be reported on power-up).
- [ ] **Pebble 2 SE (diorite):** `wakeup_schedule` is present on every platform,
  so chimes should behave identically there, the only major feature that does.

Note: `pebble emu-set-time` cannot be used to test this. pypkjs (the emulated
phone) re-syncs the watch clock back to host time, so the jump silently reverts.
Use quarter-hourly chimes and wait for a real boundary instead.

## Pebble Time 2: on-hardware confirmation (2026-08-19, later)

- [x] Chime build (`ae64854`) and the updated watchface installed over the
  CloudPebble developer connection after it came back.
- [x] `docs/time2-live.png`: the real emery watch running the current app:
  20:18, Terse Quarter, "Simple · Standard", "Flick: off", and the new hint
  lines "Sel play · Up/Dn sys" / "Hold: speed, practice". No chime line, which
  is how the screen reports chimes off, so the default is intact on the device.
- Transport note: **CloudPebble allows one connection at a time.** Running
  `pebble logs --cloudpebble` and then `pebble install`/`send-app-message`
  disconnects the logger ("Disconnected."), so device logs and device commands
  cannot be interleaved the way they can on the emulator. Capture logs in one
  session and drive the watch in another.
- A system modal (e.g. "Fully Charged" on the charger) sits on top of the app
  and `pebble screenshot` captures the modal, not the app. It needs a physical
  button press to dismiss; there is no remote way past it.

## Streamlined Pebble controls (2026-08-21)

- [x] All persisted keys and defaults remain unchanged. A pre-existing emulator
  install reopened with its saved Binary Clock / Slow selection intact.
- [x] Basalt emulator: Up logs `scope=hours`, Down logs `scope=minutes`, and
  Select logs `scope=full`; none changes the selected language.
- [x] Basalt emulator: a one-second Down hold advances the language and logs a
  system cue before the full-time phrase; the ordinary Down click does not also
  fire.
- [x] Basalt screenshot refreshed at `appstores/pebble/pulsetime/docs/basalt.png`
  with the starting Terse Quarter / Standard configuration and the revised
  `Sel all · Up hours` / `Dn mins · Hold more` hints.
- [x] Both PBWs clean-build for basalt, chalk, diorite, emery, flint, and
  gabbro. The app retains its worker binaries on all six platforms.
- [x] Encoder parity remains exact: 420/420 full patterns, 420/420 valid vibe
  and cued patterns, and 840/840 scoped patterns.
- [x] **Pebble 2 SE:** imported the staged `pulsetime.pbw` through the RePebble
  app on its paired Pixel 9a. The phone protocol log records `BlobResponse`
  success, `AppRunStateStart` for `e69d82c1-73f1-4b29-a50f-06d17edd13b2`, and
  the watch returning `terse_quarter`, `standard`, and exact-off. RePebble's
  hardware screenshot shows the 144×168 watch running PulseTime with the new
  `Sel all · Up hours` / `Dn mins · Hold more` hints and no clipping.
- [x] **Pebble Time 2:** recovered and rechecked on 2026-09-02. CloudPebble ping
  succeeds, the screenshot transport returns the expected 200×228 display, and
  reinstalling the staged PBW reports `App install succeeded`. An explicit
  `AppRunStateRequest` returns PulseTime's UUID
  (`e69d82c1-73f1-4b29-a50f-06d17edd13b2`) as the running app. After dismissing
  the system diagnostic overlay, `appstores/pebble/pulsetime/docs/time2-streamlined-live.png`
  captures the current app on real hardware: Terse Quarter, Simple / Standard,
  Flick off, and all four short/hold control labels fit without clipping.
- [ ] **Wrist feel:** on both watches, confirm Select = full time, Up = hours,
  Down = minutes, and a deliberate hold cycles language without an accidental
  short-press response.
