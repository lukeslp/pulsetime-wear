# PulseTime 1.3.0 Signals candidate

By Luke Steuber. For Pebble Time 2, 2 Duo, and Round 2.

Ask for a short vibration phrase describing the next six hours of rain,
Drummer service health, or a custom HTTPS data feed. Three timed weather
windows use sparse or dense patter; opening and closing cues distinguish
dry weather from an unavailable reading.

Learn named examples with visible pulse timing, compare the timeline with
a first-rain-and-peak summary, feel four stock-inspired references, and try
an untimed recognition exercise. Existing time lessons remain available.

Choose a favorite source and assign Favorite signal to a Quick Launch button
for direct access. Existing time shortcuts remain unchanged until selected.
Custom samples can show names and units, with presets for departures in minutes
and CPU, memory, and disk usage in percent. The comparison exercise now contains
eight balanced trials and reports separate timeline and summary scores.
Signal requests now wait for a busy phone-message channel instead of reporting
unavailable immediately; cancellation and the request deadline still apply.

Weather has a three-step setup and uses the phone's location after enabling
it and granting Pebble location access. Detected coordinates are rounded for
the forecast and are not saved as location history. Fixed-place weather and
custom HTTPS sources are under Advanced at the bottom of settings. There is
no Drummer token input. Feed URLs and
optional credentials stay on the phone. Requests are explicit. Old readings
remain labeled after refresh failures and app restarts, and replay uses the
unknown rhythm. Expiry never starts an unsolicited vibration.

Install the included PBW through the Pebble phone app; see GUIDE.md for
controls and setup. This is a local tester candidate. The public store's
1.2.0 release remains separate. Motor playback uses timing only: OS braking
and per-step strength are unavailable through the installed public SDK.
Comfort and recognition still require testing on physical watches.
