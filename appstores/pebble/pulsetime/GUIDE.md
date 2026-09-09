# Your first time check

The **1.3.0 Signals candidate** is for **Time 2, 2 Duo, and Round 2**.
The public store link below still refers to the published 1.2.0 time app.
Signals is installed from its tester PBW until a release is published.

## Feel weather or a data source

Hold Select from the clock, choose **Signals**, then choose Weather,
Drummer, or Custom signal. Select replays or stops. Hold Select refreshes;
Up and Down switch source. Back returns to the menu. Hold Down shows details.

In the phone app, open PulseTime's settings and follow **Weather · Set up in
three steps**. Enable weather, save, and allow location for the Pebble phone
app if prompted. Saving checks location without fetching weather. Reopen
settings to see whether location is ready or how to fix denied access.
Then choose Weather on the watch. Time and lessons need no location access.

Each weather request gets one phone position and sends approximate coordinates
to [Open-Meteo](https://open-meteo.com/). No detected-location history is saved.
Weather covers rain, not snowfall. Turn weather off to stop location requests;
the phone's settings control Pebble's system permission. Existing fixed-place
installs keep their previous behavior until this new setup is saved.

**Advanced · Fixed place and data feeds**, at the bottom of settings, contains
optional manual coordinates and HTTPS sources. Custom feeds may use a bearer
token stored on the phone. The Drummer token input has been removed; already
configured private connections keep their existing credential. Time and
weather require no feed URL or token.

One short opening tap means weather, two mean Drummer, three mean custom.
Weather then plays three equal windows for the next two, four, and six hours:
quiet for dry, sparse patter for light rain, denser patter for heavier rain.
A closing pulse ends the answer. Long-short-long means unknown. Quiet alone
is never a successful answer. Hold Up compares the timeline with a summary:
count the first wet window, then feel the strongest predicted rain.
Each window lasts 1.4 seconds. The three strips beneath the forecast show
the pulse timing, and a moving mark follows playback. Summary names the
first wet window and peak rain; it does not predict an exact onset time.

Use **Learn signals** for known examples and **Try without looking** for an
untimed four-choice weather exercise. It still needs visual setup and answer
selection. **Feel references** offers four stock-inspired timing patterns;
it does not reproduce OS braking or per-step strength. Set overall vibration
strength in the watch's system settings and use **Playback** to feel two taps.

Readings expire. An old or unavailable response plays unknown; a network
failure retains the last reading as **Old data**, including after reopening
the app. Expiry updates the screen without starting a vibration. A network
failure does not mean Drummer is down. Drummer's down rhythm describes failed
selected services when its observer is reachable. This prototype's patterns
still need comfort and recognition checks on actual watches.

Start with **PulseTime**, the app. It has lessons, practice, and buttons for
asking the time. **PulseTime Face** is an optional second package: choose it as
your watchface for a large clock and double-tap playback. The app and face save
their own settings, so set your preferred language and display in each.

## Install or update

Install [**PulseTime Public Beta**](https://apps.repebble.com/1624684ac5dd46ac85eee2a6) from the Pebble app store.
It appears as **PulseTime** in your watch's app list. If you use more than one
Pebble, select the watch you want to update in the phone app first. Installing
an update keeps your saved settings.

If you have the offline tester kit, you can instead save `pulsetime.pbw` on your
phone and open or share it with the Pebble app paired to your watch.

The optional **PulseTime Face** is a separate tester package; it does not yet
have a public store listing. Open `pulsetime-face.pbw` from the kit the same
way, then choose it as your watchface. Each package has its own settings gear
in the phone app. The clock and lessons work offline after installation.

Version **1.2.0** supports Time / Time Steel, Time Round, 2 / 2 SE, Time 2,
2 Duo, and Round 2. It does not support the original 2013 Pebble or Pebble Steel.

## Learn one answer

1. Open PulseTime from the watch's app list. Its first normal launch opens
   **Learn pattern**. Select on the introduction tells the current time.
2. Press Down for the **3:32 hours** lesson. Down again reaches the minute
   phrase, then Down once more reaches the whole answer. Select plays the
   current example; press it again to stop. Down advances, Up returns, and
   Back leaves the guide.
3. Keep **Terse Quarter / Standard** initially. At 3:32, three hour pulses,
   a pause, and two quarter pulses mean **about 3:30**. This answer covers
   3:30–3:44; the visual clock still shows the current minute.
4. Compare the guide's **:00** and **:15** minute phrases. They can feel alike.
   Use the whole-time answer if uncertain; this comparison is not a test you
   must pass before using the clock.
5. Press Down on the final **Ready** page to finish. Hold Select from the clock
   to revisit **Learn pattern** or choose **Practice** with known times.

Each lesson plays its labeled example. The guide's introduction and setup
pages play the current time. Haptic hours use 1–12 with no AM/PM marker, even when
the visual clock uses 24-hour time.

## Follow the vibration with your eyes

While an answer plays, **one very large numeral** replaces the lesson or clock.
The label tells you what it means. In the default 3:32 lesson, a large **3** stays
through the three hour pulses, then a large **2** stays through the two quarter
pulses. `QUARTERS` and `2 x 15 min` explain why that second numeral means :30.
With Decimal Place Rhythm, the same example reads **3 → 3 → 2**: hours, minute
tens, minute ones.

Each numeral stays visible for its vibration group. A two-digit
value such as twelve appears as **1**, then **2**; the caption keeps `12` visible
and identifies which digit you are seeing. Binary shows each 0/1 with its weight.
Nearest-quarter labels retain the landmark and before/after direction.

The display stays steady through pauses. Bright/Dark preferences keep maximum
black/white contrast. Your usual buttons still stop, replay, or navigate; the
lesson or clock returns when playback ends. The watchface shows the same readout
when it plays a pattern, then returns to its clock.

## Daily buttons

These controls apply on the app's clock screen:

| Button | Action |
|---|---|
| Select | Whole current time |
| Up | Hours only |
| Down | Minutes only |
| Same reply button during playback | Stop |
| Different reply button during playback | Replace with that reply |
| Hold Select | Learning and settings menu |
| Hold Up / Down | Change haptic language, with a numbered cue |
| Back | Leave the app |

In menus, Up/Down move, Select chooses, and Back returns. Display changes save
quietly; language and speed changes preview their pattern. The phone's settings
also provide sound, chimes, quiet hours, and the optional wrist gesture.

For exact minutes, try **Decimal Place Rhythm** after learning the default.
It counts hours, minute tens, then minute ones. Choose **Slow** if pulses blur
together. Compare on your own watch; neither choice is a proven winner for every
wearer. The [language comparison](HAPTICS.md) explains all seven choices and
their tradeoffs.

## Tap the watch

There is no special touch-sensitive spot. PulseTime detects a physical knock or
flick through the watch's motion sensor. Try holding your wrist still and giving
the bezel or case two light fingertip knocks, about half a second apart. Use
the same spot both times; the double-tap detector expects the same movement axis.

The current controls differ depending on what is open:

| Where you are | Gesture |
|---|---|
| PulseTime app's main clock | One detected tap or flick reads the whole time |
| PulseTime Face is on screen | Two taps read the time; enabled by default |
| Another watchface | Two taps launch PulseTime only if you enable "Flick to tell time" in the app's phone settings; off by default |

Wait until the vibration finishes, then another second before trying again.
Use Select in lessons and menus. If the gesture is unreliable on your wrist,
the app's buttons and Quick Launch give you another way to ask.

## Set up the side buttons

In the watch's **Settings > Quick Launch > Up**, choose **PulseTime**. Repeat
for Down if wanted. These assignments belong to the watch; the app cannot make
them automatically. Hold a bound button from your watchface to ask the time.
Both replies initially give the whole time.

| Watch | Different replies for Up and Down launches |
|---|---|
| Time 2, 2 Duo, Round 2 | Hold Select in PulseTime, choose Quick Launch, then choose each reply |
| 2 / 2 SE, Time / Time Steel, original Time Round | Either launch gives the whole time; Up/Down give separate fields once the app is open |

Keep whole-time launches until partial phrases feel reliable. In particular,
Terse Quarter's minutes-only zero and one-quarter pulses differ by just 20 ms
at Standard speed. Whole-time playback avoids relying on that pair alone.

## Make the time easier to see

Choose **Hold Select > Display > Bright / bold** for large black digits on white,
with hours above minutes. **Bright / row** uses one line. Dark versions invert
the colors. These choices work on rectangular and round screens and leave the
haptic language unchanged. The watch controls physical backlight brightness;
the app only requests its normal temporary light during a deliberate check.

For the face, use **PulseTime Face's phone settings**. Its system buttons keep
their normal watch functions. Double-tap the active face to play the time. If
another app or the launcher is open, the face's gestures and chimes stop until
you return. Use the watchapp's scheduled chimes if they need to work across
other apps. Avoid enabling both chime sources for the same interval.

## When something is unclear

- **Lost your place?** Back out to the app's clock and press Select for a fresh
  whole-time answer. Hold Select to reopen learning and settings.
- **Phone and watch settings disagree?** A missed update from the watch can
  leave older values in the app's phone form. Check every option before saving,
  especially the language, speed, and exact-minute choice: saving can restore
  the older values. Then verify the language on the watch and replay a known
  lesson. Reopening the phone form alone does not guarantee fresh settings.
- **No sound on 2 SE or a round watch?** Those targets use vibration. Optional
  pattern tones require Time 2 or 2 Duo and respect the watch's speaker mute.
- **Learning without the screen?** Use this guide and the phone instructions
  with a screen reader. Menu navigation does not speak its labels, and Practice
  does not separately announce its target time. First-use assistance may be
  needed; independent blind onboarding has not been validated.

The [validation record](VALIDATION.md) covers builds, encoder checks, and
emulator layouts. Version 1.2.0 has installed successfully on Time 2 and 2 SE.
Wearer checks of readability and pattern recognition are underway; round
hardware still needs a separate check.

## Why I built PulseTime

A friend who is visually impaired asked me to build a way to feel the time.
That request became PulseTime. I want the clock to be useful to my friend and
anyone else it helps.

It also led me to the idea of vibration "fingerprints," or **haptic QRs**:
learned patterns for information beyond time, such as a server's status.
Pulse Lab, a separate phone app, has fictional examples and a local pattern
builder for exploring rhythms. **Live server monitoring and automatic mappings
from outside data to vibration are not implemented.** These Pebble packages
tell the time.

## Share your feedback

This is a public beta. Send feedback and ideas to
[luke@lukesteuber.com](mailto:luke@lukesteuber.com). Include your watch model,
pattern and speed, the time you expected, and what you felt. Confusing controls
and hard-to-read labels are useful reports too.

For future vibration fingerprints, what would you want to recognize by touch?
A remote endpoint's status is one idea; those connections are not implemented.
