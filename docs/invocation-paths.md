# How PulseTime gets invoked, per platform

The product question is always the same: **how few gestures from a cold wrist to
feeling the time?** The answer is constrained by one rule that holds on every
platform we ship to — *the vibration motor belongs to a foreground process*. No
watch we target lets a background service buzz on its own. Every "no app to
open" design below is therefore a way to make a foreground process appear and
vibrate fast enough that the wearer never perceives an app.

Verified 2026-08-18 against the Connect IQ 9.2.0 SDK docs and the Pebble SDK
4.33.1 headers. Re-verify before trusting; do not re-derive from memory.

## Apple Watch — the reference point we are measured against

Taptic Time (Settings → Clock → Taptic Time; cover the face with two fingers) is
a **first-party system feature**. A third-party app cannot register a gesture
that plays haptics with no app running: `WKInterfaceDevice.play(_:)` needs the
app foregrounded, and complications launch the app rather than emitting haptics
themselves. So on Apple the honest comparison is "system feature vs our app",
not "our app vs theirs". This is context for expectations, not a shipping
target — PulseTime has no watchOS build.

## Wear OS (the shipping app)

- **Tile** → `TellTimeRelayActivity`, a blank activity that vibrates and
  finishes. One swipe to the tile carousel, one tap. The relay exists precisely
  because a tile cannot vibrate by itself.
- In-app **Tell time** for training and for people who prefer the list.
- Complications and hourly chimes remain later work.

## Garmin (Venu 2S — `appstores/garmin/pulsetime/`)

| Path | Gestures | Notes |
|---|---|---|
| Glance loop | swipe up, tap | The default. The app vibrates on `onShow`. |
| Published complication on a watch face | one tap | Built. The complication is a **launcher**: a face calls `Complications.exitTo(id)` and the system foregrounds PulseTime, which vibrates. Requires API 4.2.0; the Venu 2S is CIQ 5.0. |
| App list | more | Fallback. |

Hard limits:

- `Toybox.Attention.vibrate()` is available to Watch App, Widget, Glance, Data
  Field and audio providers — **not to watch faces**. A Connect IQ watch face
  cannot buzz, so PulseTime cannot live on the face itself.
- `Toybox.Background` services cannot touch `Attention`, and
  `Background.requestApplicationWake()` shows a confirmation dialog. There is no
  silent background haptic.
- `vibrate()` accepts **at most 8 `VibeProfile`s per call**, which is why
  patterns are chunked at silences and chained with a timer (see the port
  README). Strength is duty cycle (0–100), not amplitude.
- **Widgets are gone on System 5.** `Devices/venu2s/compiler.json` lists no
  `widget` app type at all (only audioContentProvider, background, datafield,
  glance, watchApp, watchFace); `type="widget"` survives as a legacy alias.
  Publishing is documented for device apps, so PulseTime is `type="watch-app"`
  and keeps its glance via `getGlanceView`.
- Publishing a complication needs `<iq:uses-permission id="ComplicationPublisher"/>`,
  an **SVG** icon distinct from the launcher icon, and a `value` that is a
  number or string-resource reference — each of those is a hard compiler error,
  not a warning. `updateComplication` must not be called from `AppBase.onStart`:
  that also runs in the glance code space, which carries only `(:glance)`
  symbols.

## Pebble (Time 2, 2 SE — `appstores/pebble/pulsetime/`)

| Path | Gestures | Notes |
|---|---|---|
| Quick Launch (long-press Up or Down) | one press | Zero background cost. The best default. |
| Background worker + wrist gesture | one double-tap | The worker cannot vibrate (`pebble_worker.h` has no vibes API); it calls `worker_launch_app()`, and the app vibrates and auto-exits when `launch_reason() == APP_LAUNCH_WORKER`. Off by default. |
| App list | more | Fallback. |

Hard limits:

- **One worker per watch, system-wide.** `app_worker_launch()` returns
  `APP_WORKER_RESULT_DIFFERENT_APP` when another app owns the worker, and
  `APP_WORKER_RESULT_ASKING_CONFIRMATION` while the wearer is being asked to
  switch. Any UI that offers the flick gesture must surface those honestly, and
  the setting has to default to off — taking the watch's only worker slot is
  not ours to assume.
- `vibes_enqueue_custom_pattern()` takes alternating on/off durations (≤10 s
  each) and plays the whole pattern in one call; a second call while a vibe is
  playing is a no-op, and `vibes_cancel()` stops it. **No amplitude control**,
  so layered strength does not exist on Pebble — which is why no time system may
  depend on amplitude alone.
- **The motor re-triggers the tap sensor.** A vibrating watch shakes hard
  enough to read as a fresh gesture, so a gesture-launched haptic app will
  relaunch itself in a loop unless the playing app explicitly tells the worker
  to stay deaf for the pattern's duration (PulseTime sends a suppress message
  carrying the pattern length plus a margin). Any future gesture trigger on any
  platform inherits this problem.
- Unlike Garmin, a Pebble **watchface can vibrate** and receives accelerometer
  taps. A PulseTime watchface variant is therefore the only true "no app at all"
  haptic clock available to us on any platform — at the cost of being the
  wearer's active face, and of a second store listing. Not built; recorded here
  so the option is not rediscovered from scratch.

## Consequences for the product

1. Latency, not capability, is the design problem. Every path is
   "foreground something, then buzz"; the work is making that invisible.
2. Battery is a real trade: the Pebble worker runs continuously, Quick Launch
   costs nothing. Offer both, default to the free one.
3. Store shape follows: both the Connect IQ store and the Pebble appstore list
   **one app per listing**, so a watchface variant or a second gesture app is a
   separate submission with its own artwork, copy, and review.
