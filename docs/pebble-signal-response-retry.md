# Bounded signal response delivery

By Luke Steuber. October 3, 2026.

## Scope and decision

The phone companion currently discards a fetched signal response after one
failed AppMessage delivery. Preserve the same response bytes and retry transient
delivery failures at most four times, 250 ms apart. Stop before the reading
expires or 18 seconds after the phone received the original request, whichever
comes first. The watch's existing request deadline remains unchanged.

Only the phone delivery queue and its host tests change. Do not repeat HTTP or
location requests, change learned vibration timings, alter the time-only face
or standalone mirrors, or publish the Signals candidate.

## Acceptance checks

- A failed delivery followed by success sends identical bytes without fetching
  the source again.
- Synchronous transport errors and asynchronous failure callbacks are bounded.
- Cancellation and superseding requests discard old queued replies.
- Duplicate or late transport callbacks cannot release a newer in-flight send.
- Reading expiry and the original phone-side deadline stop retries.
- Existing signal, location, phone configuration, C request lifecycle, time
  fixture, reading and display tests continue to pass.

## Evidence boundary

Measured before the change: a synthetic AppMessage failure produced one attempt,
zero scheduled retries and zero successful deliveries. Existing host tests pass.
Planned: the regression checks above. Physical delivery, perception and store
publication are outside this change. The 1.3.0 candidate remains distinct from
the published 1.2.0 time app.
