# PulseTime — project plan (MVP v0.1)

## Objective

Standalone Wear OS haptic clock for blind/low-vision users. Terse-quarter
encoding, Tile-first invocation, training + calibration. No Ambient Time
coupling.

## Architecture

- `:app` only — Wear Compose Material3 + Tile service
- Pure `TerseQuarterEncoder` (JVM unit tests)
- `AndroidHapticPlayer` via `VibratorManager` / amplitude waveform
- `PulseSettingsRepository` (DataStore): amplitude scale, precision, training
- Surfaces: Tile → `TellTimeRelayActivity` (blank); in-app Tell time / Training / Calibration

## Milestone status

| ID | Item | Status |
|----|------|--------|
| M0 | Gradle scaffold | done |
| M1 | Encoding + tests | done |
| M2 | Haptic player + cancel | done |
| M3 | Tile + Tell time | done |
| M4 | Calibration + training | done |
| M5 | Device evidence + AAB stage | AAB staged; watch adb pending human wireless debug |

## Outstanding

1. Pixel Watch 2 wireless ADB → `./scripts/install-watch.sh`
2. Human Play internal-track upload (replace debug signing with upload keystore)
