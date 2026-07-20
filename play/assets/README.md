# Play Store graphics

## Files

- `play-icon.png`: PulseTime ring-and-pulses mark. Alt text: “Three amber pulses inside a cyan ring on a dark background.”
- `feature-graphic.png`: Wide PulseTime graphic. Alt text: “Feel the time beside a cyan and amber sequence of tactile pulses.”
- `phone/phone-home.png`: Companion home screen. Alt text: “PulseTime companion home with lessons, watch practice, and settings.”
- `phone/phone-time-systems.png`: Time-system library. Alt text: “PulseTime list of tactile clocks, including Terse Quarter, Five-Minute Ladder, Decimal Place Rhythm, and Binary Clock.”
- `phone/phone-terse-quarter.png`: Terse Quarter guide. Alt text: “Terse Quarter guide explaining hour pulses, quarter pulses, minute ticks, and a worked example.”
- `phone/phone-watch-settings.png`: Watch settings guide. Alt text: “PulseTime watch settings for playback speed, layered strength, and exact minutes.”

The phone screenshots show the actual app interface from a Pixel 10. They contain no added captions or device frames.

## Rebuild the feature graphic

```sh
profile_dir="$(mktemp -d /tmp/pulsetime-feature.XXXXXX)"
'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' \
  --headless=new --disable-gpu --hide-scrollbars --no-first-run \
  --user-data-dir="$profile_dir" --window-size=1024,500 \
  --force-device-scale-factor=1 \
  --screenshot=play/assets/feature-graphic.png \
  "file://$PWD/play/assets/feature-graphic.svg"
```

Google Play requires the final feature graphic to be an opaque 1024 × 500 JPEG or 24-bit PNG.
