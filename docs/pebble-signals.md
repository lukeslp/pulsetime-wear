# PulseTime signals for new Pebbles

The goal is a usable tactile data glance on Pebble Time 2 (emery), 2 Duo
(flint), and Round 2 (gabbro). Luke Steuber's existing time lessons remain
available. Hold Select for the menu, then choose Signals.

## Required outcome

- Weather: one bounded, replayable phrase with three consecutive two-hour
  windows. Fixed patter encodes predicted rain amount, with clear opening and
  closing cues. Known dry and unavailable are different complete phrases.
- Drummer: healthy, busy, degraded, down, and unknown have distinct rhythms.
  Serving correctly under load is busy, not failure. A failed phone request
  means unknown, never confirmed down. An independent observer is necessary
  to establish a host outage.
- Custom: an HTTPS endpoint can supply a bounded categorical value or three
  ordinal samples for sensor, transit, build, tide, or other data. Remote
  endpoints supply data, never executable code or unbounded motor commands.
- Learn: known examples, timeline versus summary comparison, stock-inspired
  references, calibration, replay, stop, and an untimed recognition exercise.
- Interaction: explicit requests only; no surprise background data alerts.
  Leaving a page cancels playback and invalidates pending replies. A late
  response may not start vibration or replace a newer answer.
- Freshness: validate source timestamps and expiry. Show last-known data as
  old and play the unknown motif when it expires. No successful fetch may
  silently turn stale upstream data into a current reading.
- Phone setup: a weather/location walkthrough, opt-in phone location, and
  permission recovery instructions. Fixed locations and authenticated URLs
  belong under Advanced. Credentials stay on the phone; no Drummer token input.
- Release: build all three targets, independently verify the source archive,
  exercise real companion requests and failures, capture native layouts,
  package truthful release notes and installation instructions. Physical
  recognition and comfort must be tested on named watches.

## Optional strength playback: unresolved upstream export

Observed on September 9, 2026: PebbleOS v4.33.2, v4.36.0 and current main
declare and implement `vibes_enqueue_custom_pattern_with_amplitudes`, but
`tools/generate_native_sdk/exported_symbols.json` omits it. Installed SDK
4.33.1 lacks both its declaration and link symbol on all three targets.
The changelog's claim of public support is therefore insufficient.

The signal model retains durations and amplitudes. The production rhythm
renderer is independently useful, but is not evidence of amplitude support.
Completion of strength playback requires a supported export/SDK and a real
watch check. Do not invent a jump-table offset or patch a physical watch to
bypass its application boundary. Prepare an upstream export patch and a
reproducible diagnostic while finishing independent application work.

## Verification ledger

Implementation, live transport, native builds, emulator interaction, archive
reproduction, physical perception, and store publication are separate checks.
This document records requirements, not completion. Public release is not
authorized merely by creating a build; prepare the exact candidate first.

### Candidate verified September 9, 2026

Version 1.3.0 PBW SHA-256:
`f0447deb7dca0b50fe77f987e32eb3d50afcb9a849cce976a9de12c09d7d5228`.
The local tester kit is under
`appstores/dist/pebble/signals-1.3.0/f0447deb7dca/` with its source archive,
guide, custom-feed contract, candidate release notes, checksum manifest, and
30 native interaction frames per target (plus five emulator navigation captures).

- SDK 4.33.1 builds exactly emery, flint, and gabbro, including their workers.
- Favorite signal is opt-in for either Quick Launch button; existing time
  defaults are preserved. The watch menu can also request the selected source.
  All three targets passed weather and custom favorite-menu checks. The SDK's
  emery emulator has no system Settings app, so assigning the OS shortcut and
  checking its cold-start phone handshake remain physical Time 2 checks.
- Custom samples now carry bounded labels and unit text. Departures and
  resource-use presets preserve timestamps and validate measurement ranges;
  parser and phone tests cover both. Native lessons display both examples on
  all three targets. No live transit provider is connected by these presets.
- The recognition exercise shuffles eight trials, one of each weather meaning
  in each mode, and reports separate scores and replay counts. Every native
  run completed it; always selecting dry yielded the expected 1/4 in each
  mode. This validates scoring and balance, not tactile recognition.
- All three isolated native emulator runs used the packaged companion to
  fetch real weather for an explicit New York City test location. They
  produced current readings and exercised lessons, summary, reference,
  playback, trial, and help pages. Small-display help and round-frame layouts
  were visually inspected and corrected. These are not physical motor checks.
  A second run verified authenticated Drummer and custom-channel requests on
  all three targets. The custom channel used the same private bridge as a
  contract check; no unrelated hosted sensor integration is being claimed.
  The refined candidate adds named lessons, a pulse timing strip, and a
  distinct first-window/peak summary. Each target passed actual idle expiry
  without unsolicited playback, unknown replay, a missing-source response,
  and persisted last-known history after restarting the app. A timer callback
  rechecks wall-clock freshness to handle early boundary delivery on Flint.
- Phone location is now declared in capabilities and requested through
  PebbleKit JS after saving the weather walkthrough. Each requested forecast
  gets one position, rounded to two decimal places; detected coordinates are
  not persisted. Existing manual-location installs retain their behavior until
  the new setup is saved. Fixed place and HTTPS feeds are at the bottom under
  Advanced. The Drummer token input is removed; existing private configuration
  remains usable, and the bridge's authorization has not been weakened.
  Tests cover denial, unavailable/stale positions, timeout, cancellation,
  disabling during setup, stale callbacks, approximate coordinates, and
  location-to-weather handoff. The actual Clay page rendered at 390px without
  horizontal overflow or script errors. Native live-weather checks used a
  fixed test location; they do not prove a phone OS permission prompt or GPS.
  The documented location API is described in the official
  [PebbleKit JS guide](https://developer.rebble.io/guides/communication/using-pebblekit-js/).
- 9,600 signal combinations passed bounds, deterministic rendering, field
  alignment, strict parsing, and expiry checks under address/undefined sanitizers.
  All five state rhythms remain distinct after removing amplitude information.
- Existing 420 full and 840 scoped time fixtures passed; 90,720 visual timing
  cases passed. Phone tests cover opt-in, upstream failures, late replies,
  private settings, and stale configuration forms.
- Six bridge tests cover classification, timestamp-preserving adapters,
  auth, and unknown routes. Executing the bridge's real collector on Drummer
  returned healthy for both configured gateways. The private bridge is now
  installed behind `https://api.dr.eamer.dev/pulsetime-signals/v1/status`;
  unauthenticated requests return 401 and authenticated requests return 200.
  Deployed `server.py` SHA-256 is
  `a5d2026040b209e1c5e6f65dc14a43721788f71ffc497150e4c677d94a80784f`.
- The portable source archive builds independently. All 10 app, worker,
  resource, and companion payloads match after excluding SDK CRC, timestamp,
  and verified GNU build-ID bytes. Resources and JavaScript match byte for
  byte. The rebuilt PBW's archive hash is intentionally not called identical.
- Open-Meteo's rain and showers are sums for the preceding hour. Tests check
  this boundary and partial-hour overlap. Thresholds are 0.1, 0.5, and 2.5
  mm/hour averaged within each two-hour field; they are prototype categories,
  not an onset prediction. Source: [Open-Meteo definitions](https://open-meteo.com/en/docs).

Run `tests/check-native-signals.py <target>` with pebble-tool's Python from
the Pebble app directory, then `python3 tests/package-signals.py`. Packaging
rejects emulator evidence that does not match the exact candidate PBW.

Remaining: physical comfort and recognition on named new watches. Timing-only
playback is the supported product path; amplitude is an optional future
enhancement. Drummer's unrelated infrastructure edits and existing
source/live differences were preserved during the additive bridge deployment.
The proposed upstream export patch and read-only diagnostic are in
[pebble-amplitude-export](pebble-amplitude-export/README.md).

### Store automation verified

The store has authenticated dashboard APIs, not only catalog APIs. A read-only
request to `GET /api/v1/developer/me` at `appstore-api.repebble.com` returned 200
and mapped this UUID to existing app `1624684ac5dd46ac85eee2a6`.
The installed pebble-tool 5.0.39 implements app creation and multipart release
uploads at `POST /api/dashboard/apps` and
`POST /api/dashboard/apps/{id}/releases`, using a Firebase ID token.

Do not use `pebble publish` for staging based on its help text: both upload
methods hardcode `isPublished=true`, even when its flag is false. New-app
creation also hardcodes `visible=true`. The current upstream
[publish implementation](https://github.com/coredevices/pebble-tool/blob/main/pebble_tool/commands/publish.py)
has the same behavior. A draft path requires an explicit false publication
field and a verified unpublished response. No Signals release was uploaded.

### Store icon refreshed September 9, 2026

Luke requested a store icon. The existing 144px pulse artwork was re-uploaded
through the official developer site's session-authenticated multipart PATCH
at `/api/dashboard/apps/1624684ac5dd46ac85eee2a6` using `iconLarge`.
The dashboard's Firebase session endpoint accepted the existing CLI account;
the developer site's API is distinct from the earlier unsuccessful API-host
dashboard GET. Title, description, visibility, small icon, release records,
platforms, companion records, and screenshot asset contents were preserved.

The public API now references large icon `9703adb9-c030-4c06-990a-8aaa829185e8.png`.
Its public CDN returns HTTP 200; SHA-256 is
`64e5abdb963a62164981616a67dc5975100d18ea374df6278f0c6bdd245d6565`.
The old and new served images are byte-identical and contain the full pulse
mark, so an initially suspected damaged image was not confirmed. Re-upload
refreshes the asset reference; it does not establish the cause of a missing
icon in a client. The public web page still referenced the old URL immediately
after the change. No browser/client rendering check was possible while the
Mac was locked. Published time app 1.2.0 remains unchanged.
