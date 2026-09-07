# Share PulseTime for Pebble

## Short introduction

I built PulseTime so you can feel the time on a Pebble. Start with a simple
hour-and-quarter rhythm, then learn exact minutes at your own pace. During
lessons and time checks, huge black-and-white digits follow the vibration so
you can connect what you see with what you feel.

PulseTime 1.2.0 supports rectangular and round Pebbles, including Time 2 and
2 SE. The app has guided practice and side-button replies. The optional
PulseTime Face lets you double-tap its clock to hear or feel the pattern;
sound requires Time 2 or 2 Duo. Both work offline after installation.

Created by Luke Steuber. MIT licensed.

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

Useful feedback names the watch, language, and speed, then describes the time
you expected, the answer you felt, and any hard-to-read digit or label. Known
examples and unlimited replay are a good place to start.

## Distribution status

The three source repositories are private and have no GitHub release assets.
Their links require repository access. Use the tester archive for direct
sharing until a public download is available. Pebble app and watchface store
submissions remain pending.
