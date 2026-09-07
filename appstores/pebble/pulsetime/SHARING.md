# Share PulseTime for Pebble

Install or share [**PulseTime Public Beta**](https://apps.repebble.com/1624684ac5dd46ac85eee2a6).
The store listing includes first-use steps and known beta limits. The app
appears as **PulseTime** on the watch.

## Short introduction

A friend who is visually impaired asked me to build a way to feel the time.
That request became PulseTime. Start with a simple hour-and-quarter rhythm,
then learn exact minutes at your own pace. During
lessons and time checks, huge black-and-white digits follow the vibration so
you can connect what you see with what you feel.

PulseTime 1.2.0 supports rectangular and round Pebbles, including Time 2 and
2 SE. The app has guided practice and side-button replies. The optional
PulseTime Face lets you double-tap its clock to hear or feel the pattern;
sound requires Time 2 or 2 Duo. Both work offline after installation.

Created by Luke Steuber. MIT licensed.

## Beyond the clock: an idea to explore

Building PulseTime made me wonder about giving other information a vibration
"fingerprint." I think of these as **haptic QRs**: learned patterns you could
recognize by touch, perhaps to check whether a server is healthy or down.

Pulse Lab, a separate phone app, has fictional examples and a local pattern
builder for exploring rhythms. **Live server monitoring and automatic mappings
from outside data to vibration are not implemented.** The Pebble packages in
this kit tell the time.

## Send these files together

| File | Purpose |
|---|---|
| `pulsetime.pbw` | Start here: the app with lessons, settings, and time checks |
| `pulsetime-face.pbw` | Optional clock face with double-tap playback |
| `README.html` | Offline introduction, screenshots, and the complete wearer guide |
| `GUIDE.md` | The same wearer guide in plain text |
| `HAPTICS.md` | Language comparison, including the close zero/quarter distinction |
| `SHA256SUMS.txt` | Checksums for the two watch packages |

The prepared archive is `pulsetime-pebble-1.2.0-test-kit.zip` beside the staged
PBWs in `appstores/dist/pebble/`. Extract it before opening `README.html` or
sending a PBW to the paired Pebble phone app. Updating preserves saved settings;
the app and face keep separate preferences.

## What testers should know

The default rhythm reports the start of the current quarter. At 3:32, three
hour pulses and two quarter pulses mean about 3:30. Exact-minute languages are
optional. Haptic hours use 1–12 without an AM/PM marker.

The app has installed successfully on Time 2 and 2 SE. The screenshots show
emulators. Readability, pattern recognition, and double-tap reliability still
need wearer feedback; the face's 1.2.0 physical install is pending. The guide
works with a phone screen reader, but watch menus do not speak their labels.
Independent blind onboarding remains unverified.

**Known settings issue:** a missed update from the watch can leave older values
in the app's phone settings. Saving the form can restore those older choices,
including the haptic language. Check every option before saving, then verify
the language on the watch and replay a known lesson. Reopening the phone form
alone does not guarantee fresh settings.

Send feedback and ideas to [luke@lukesteuber.com](mailto:luke@lukesteuber.com).
I would also like to hear what you would connect to a vibration fingerprint,
such as a remote endpoint, and what each pattern should mean.

Useful feedback names the watch, language, and speed, then describes the time
you expected, the answer you felt, and any hard-to-read digit or label. Known
examples and unlimited replay are a good place to start.

## Distribution status

The app is published as [PulseTime Public Beta](https://apps.repebble.com/1624684ac5dd46ac85eee2a6), version 1.2.0.
Share that link for the app. The optional watchface remains a separate direct
tester package; its store listing is pending.

The three source repositories remain private and have no GitHub release assets.
Their links require repository access. The offline tester archive includes
both packages, the full guide, and checksums.
