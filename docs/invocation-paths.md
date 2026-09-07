# How PulseTime gets invoked, per platform

PulseTime aims to make a time check reachable with one or two gestures. Its
Wear OS tile, Garmin complication, and Pebble shortcuts open the app to play
the answer. PulseTime Face can play directly while it is on screen.

The platform rules below come from the Connect IQ 9.2.0 documentation and
Pebble SDK 4.33.1 headers checked on 2026-08-18. Current Pebble 1.2.0 interaction
coverage is in the [validation record](../appstores/pebble/pulsetime/VALIDATION.md).

## Apple Watch

Apple's Taptic Time is the built-in reference for this kind of clock.
PulseTime has no watchOS build.

## Wear OS

- **Tile** → `TellTimeRelayActivity`, a blank activity that vibrates and
  finishes. One swipe to the tile carousel, one tap. The relay exists precisely
  because a tile cannot vibrate by itself.
- In-app **Tell time** for training and for people who prefer the list.
- Complications and hourly chimes remain later work.

## Garmin (Venu 2S, `appstores/garmin/pulsetime/`)

| Path | Gestures | Notes |
|---|---|---|
| Glance loop | swipe up, tap | The default. The app vibrates on `onShow`. |
| Published complication on a watch face | one tap | Built. The complication is a **launcher**: a face calls `Complications.exitTo(id)` and the system foregrounds PulseTime, which vibrates. Requires API 4.2.0; the Venu 2S is CIQ 5.0. |
| App list | more | Fallback. |

Hard limits:

- `Toybox.Attention.vibrate()` is available to Watch App, Widget, Glance, Data
  Field and audio providers, excluding watch faces. A Connect IQ watch face
  cannot buzz, so PulseTime cannot live on the face itself.
- `Toybox.Background` services cannot touch `Attention`, and
  `Background.requestApplicationWake()` shows a confirmation dialog. There is no
  silent background haptic.
- `vibrate()` accepts **at most 8 `VibeProfile`s per call**, so the player
  splits patterns at silences and chains them with a timer (see the port
  README). Strength is duty cycle (0–100), not amplitude.
- **Widgets are gone on System 5.** `Devices/venu2s/compiler.json` lists no
  `widget` app type at all (only audioContentProvider, background, datafield,
  glance, watchApp, watchFace); `type="widget"` survives as a legacy alias.
  Publishing is documented for device apps, so PulseTime is `type="watch-app"`
  and keeps its glance via `getGlanceView`.
- Publishing a complication needs `<iq:uses-permission id="ComplicationPublisher"/>`,
  an **SVG** icon distinct from the launcher icon, and a `value` that is a
  number or string-resource reference. Each violation is a compiler error,
  not a warning. `updateComplication` must not be called from `AppBase.onStart`:
  that also runs in the glance code space, which carries only `(:glance)`
  symbols.

## Pebble (Time 2, 2 SE, and round models)

| Path | Gestures | Notes |
|---|---|---|
| Quick Launch (long-press Up or Down) | one press | No resident worker. Both buttons default to whole time; separate replies require Time 2, 2 Duo, or Round 2. |
| Watchface (`appstores/pebble/pulsetime-face/`) | double-tap | Already running; plays without another app launch. Gestures and chimes stop while another app or the launcher is open. |
| Watchapp chimes via `wakeup_schedule()` | none | The system launches the app, it plays the scheduled time, then closes. No PulseTime process stays resident between chimes. |
| Background worker + wrist gesture | one double-tap | The worker cannot vibrate (`pebble_worker.h` has no vibes API); it calls `worker_launch_app()`, and the app vibrates and auto-exits when `launch_reason() == APP_LAUNCH_WORKER`. Off by default. |
| App list | more | Fallback. |

**Quick Launch carries a button id.** `launch_button()` returns which button
started the app, and `launch_get_quick_launch_action()` distinguishes HOLD from
TAP from COMBO, so one install can answer more than one question: PulseTime uses
hold-Up and hold-Down for independently chosen replies, initially whole time.
The SDK calls are available only on
**emery (Pebble Time 2), flint (Pebble 2 Duo) and gabbro (Pebble Round 2)**; the
frozen platforms define both as macros returning `0`, so the feature degrades to
"always the whole time" with no conditional compilation.

Hard limits:

- **One worker per watch, system-wide.** `app_worker_launch()` returns
  `APP_WORKER_RESULT_DIFFERENT_APP` when another app owns the worker, and
  `APP_WORKER_RESULT_ASKING_CONFIRMATION` while the wearer is being asked to
  switch. The flick setting starts off; Watch status reports whether the worker
  is running, waiting for confirmation, or blocked by another app.
- `vibes_enqueue_custom_pattern()` takes alternating on/off durations (≤10 s
  each) and plays the whole pattern in one call; a second call while a vibe is
  playing is a no-op, and `vibes_cancel()` stops it. **No amplitude control**,
  so layered strength does not exist on Pebble. No time system may
  depend on amplitude alone.
- **The motor re-triggers the tap sensor.** A vibrating watch shakes hard
  enough to read as a fresh gesture, so a gesture-launched haptic app will
  relaunch itself in a loop unless the playing app explicitly tells the worker
  to ignore taps for the pattern's duration (PulseTime sends a suppress message
  carrying the pattern length plus a margin). Any future gesture trigger on any
  platform inherits this problem.
- Unlike Garmin, a Pebble **watchface can vibrate** and receives accelerometer
  taps. The PulseTime watchface is therefore the only current "no additional app launch"
  path in the current PulseTime builds. It requires the
  wearer's active face and a second store listing. **First built 2026-08-19**
  (`appstores/pebble/pulsetime-face/`, uuid `f9ee597e-…`), sharing the
  watchapp's encoder by symlink. It is the only path where the time is felt with
  no additional app launch. The wakeup chimes below still start the app for a few
  seconds, they just do it without being asked.
- **A watchface only runs while it is the foreground app.** Being the *selected*
  face is not enough: opening any app, or even the launcher, unloads it and its
  tick and tap subscriptions go with it. On the emery emulator, with
  PulseTime Face still selected but the launcher open, crossing the hour fired
  no chime, where the same crossing on the face did. It resumes by itself on the
  way back, so the practical scope is "answers whenever the wearer is on their
  watchface".
- **`wakeup_schedule()` is the one path that needs nothing resident**, and is
  what PulseTime's chimes use. Max **8** events per app, and no event within
  **one minute** of another already scheduled (`E_RANGE`, and the collision can
  be with *another app's* wakeup), so a recurring chime is a rolling window
  topped up on every launch. Since each chime is itself a launch, the chain
  sustains itself. It does not keep a process resident or take
  the watch's single worker slot. Available on **every** supported Pebble
  platform, including Pebble 2 SE and Time 2. Battery impact has not been measured.
  `pebble_worker.h` does also
  expose `tick_timer_service_subscribe()`, so a worker was the alternative, at
  the price of running all day for the same result.
- **Carry the intended time in the wakeup cookie.** A wakeup can land a second
  either side of the boundary, and a clock you cannot see announcing "12:59"
  when you expected one o'clock is worse than no chime at all.
- **A speaker changes the answer on two watches.** `PBL_SPEAKER` is defined for
  **emery (Pebble Time 2)** and **flint (Pebble 2 Duo)**. Where it
  exists, `speaker_play_notes()` gives back the relative-strength dimension the
  motor cannot express, as pitch. The worker still cannot use it:
  `pebble_worker.h` has no speaker API either, so this changes nothing about the
  foreground rule.

## Consequences for the product

1. Invocation speed and pattern recognition both determine whether a time check
   is useful. The [haptic comparison](../appstores/pebble/pulsetime/HAPTICS.md)
   separates encoder timing from physical recognition, which remains untested.
2. The Pebble worker runs continuously; Quick Launch needs no resident worker.
   Quick Launch is the default recommendation. Battery differences remain unmeasured.
3. Store shape follows: both the Connect IQ store and the Pebble appstore list
   **one app per listing**, so the watchface is a separate submission from the
   watchapp, with its own artwork, copy, and review.
4. **Quick Launch carries a button id and an action kind**, so on Pebble one
   install can serve more than one intent on Time 2, 2 Duo, and Round 2.
   Both scopes start at whole time; 2 SE and older targets always launch that way.
   No other platform currently supported by PulseTime reports which
   affordance started the app.
