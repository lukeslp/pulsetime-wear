# PulseTime Signals wrist check

This sheet is for Luke Steuber's 1.3.0 candidate on Time 2, 2 Duo, or Round 2.
It records physical evidence separately from emulator results. No results
have been collected yet. Test one watch at a time; do not transfer a result
from one model to another.

Candidate PBW SHA-256:
`343768595ce589079fae18af612208066f0b3dc7ba555731f46ebf70bbaa7f0c`

## Record the setup

- Watch model and firmware:
- Date and tester initials:
- Candidate hash or filename:
- Wrist, strap fit, and system vibration strength:
- Seated or walking:
- Prior practice with PulseTime:

Install the PBW through the Pebble phone app. Hold Select from the clock,
choose Signals, then Playback. Select plays two taps. Confirm they are
perceptible at a comfortable system strength before continuing. Stop if the
pattern is uncomfortable. Keep the same strength for a comparison block.
This candidate has timing control, not per-step strength or OS motor braking.

## Learn before scoring

Open Learn signals. Up and Down move through dry, early rain, late rain,
unknown, healthy, busy, degraded, and down. Select replays or stops.

For weather, identify the opening tap, three equal windows, and closing pulse.
Each window lasts 1.4 seconds and represents two forecast hours. Dry has
quiet windows between explicit cues; unknown is long-short-long. Hold Up
switches between timeline and summary. Summary counts the first wet window,
then gives the strongest rain category. Practice each example until the
tester can explain its meaning; record how much practice was needed.

## Weather recognition

Use Try without looking for 12 trials. Listen by touch before looking at the
answer choices. Record the first answer and replay count. Up/Down select an
answer; hold Select reveals it, and Select starts another trial. The screen
and answer selection still require vision or a helper: this is not a claim
of independent blind onboarding. The app chooses randomly, so record the
actual example distribution rather than assuming it is balanced.

| Trial | First answer | Revealed meaning | Replays | Comfortable? |
|---|---|---|---|---|
| 1–12: use one row per trial | | | | |

Pay particular attention to dry/unknown confusions and early/late reversals.
Any dry/unknown confusion requires revisiting those cues before release.
Report counts by meaning, not only an overall percentage. Twelve trials are
an initial design check, not proof of general usability.

## Compare timeline and summary

With a helper operating Learn signals out of view, present the same dry,
early-rain, and late-rain examples in both modes. Alternate which mode comes
first. For each presentation, ask “When is the first wet window?” and “What
is the peak rain category?” Record the answer before revealing the screen.
The app's randomized exercise does not test summary mode.

| Example | Mode first | Timeline answer / replays | Summary answer / replays | Preferred / why |
|---|---|---|---|---|
| Dry | | | | |
| Early rain | | | | |
| Late rain | | | | |

Do not use the live forecast as the answer key: it can change between requests.

## Service states and controls

Have the helper present each of healthy, busy, degraded, down, and unknown
twice in a shuffled order using Learn signals. Record intended state, first
answer, replays, and discomfort. Keep busy/degraded and down/unknown mistakes
separate. Distinct duration arrays do not prove distinguishable sensations.

On a live source, check Select stops immediately and Back leaves without
continuing vibration. Let a Drummer reading expire: the page should show Old
data without vibrating; replay should give unknown. Reopen the app with the
phone disconnected and request that source: the old reading should remain
labeled, never become a current healthy result. Restore the connection after
the check. No production service needs to be stopped.

Record any missed opening/closing cue, merged taps, excessive length, layout
issue, or accidental long press. Try a short repeat while walking only after
the seated check is comfortable. State the conditions actually tested.

## Decision

- What was reliably understood on this particular watch?
- Which meanings were confused, and in which mode?
- Did stopping, expiry, and reconnection behave as described?
- What needs changing before another wrist check?

Keep failed and incomplete trials. A tester preference does not establish
accuracy, and a single successful wearer does not validate all three motors.
Attach completed results to this document or a dated companion record; leave
the other models explicitly untested.
