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
- Phone setup: manual weather location, optional authenticated URLs, clear
  explanation of what leaves the phone. Credentials stay on the phone.
- Release: build all three targets, independently verify the source archive,
  exercise real companion requests and failures, capture native layouts,
  package truthful release notes and installation instructions. Physical
  recognition and comfort must be tested on named watches.

## Strength playback: unresolved upstream export

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
`99e67d5344e111688de8ca6abbe0b2fc1c7a2f478667fcec5a97e9ec5bc6d7e9`.
The local tester kit is under
`appstores/dist/pebble/signals-1.3.0/99e67d5344e1/` with its source archive,
checksum manifest, and 18 native frames per target.

- SDK 4.33.1 builds exactly emery, flint, and gabbro, including their workers.
- All three isolated native emulator runs used the packaged companion to
  fetch real weather for an explicit New York City test location. They
  produced current readings and exercised lessons, summary, reference,
  playback, trial, and help pages. Small-display help and round-frame layouts
  were visually inspected and corrected. These are not physical motor checks.
- 9,600 signal combinations passed bounds, deterministic rendering, field
  alignment, strict parsing, and expiry checks under address/undefined sanitizers.
  All five state rhythms remain distinct after removing amplitude information.
- Existing 420 full and 840 scoped time fixtures passed; 90,720 visual timing
  cases passed. Phone tests cover opt-in, upstream failures, late replies,
  private settings, and stale configuration forms.
- Six bridge tests cover classification, timestamp-preserving adapters,
  auth, and unknown routes. Executing the bridge's real collector on Drummer
  returned healthy for both configured gateways. No persistent bridge or
  public proxy route has been installed yet.
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

Remaining: private bridge deployment and end-to-end Drummer/custom requests,
physical comfort and recognition on named new watches, and supported amplitude
playback. Drummer's infrastructure checkout has unrelated uncommitted edits
and differs from live Caddy; any deployment must preserve both sets of changes.
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
field and a verified unpublished response. No store write was performed.
