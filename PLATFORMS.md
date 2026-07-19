# PulseTime — platforms track (Wear OS)

| Stage | Status | Evidence |
|-------|--------|----------|
| scaffolded | done | Gradle Wear Compose app, standalone, minSdk 30 |
| built | done | `./gradlew :app:assembleDebug` — APK at `app/build/outputs/apk/debug/` |
| validated | done | `./gradlew :app:testDebugUnitTest` green; TalkBack checklist in README |
| companion | built | Android companion APK with six system guides and Data Layer watch playback |
| device-tested | installed; QA pending | Pixel Watch 2 is installed; human haptic and TalkBack QA remain — see `DEVICE_EVIDENCE.md` |
| store-staged | signed | Phone and Wear AABs under `dist/play/`; unique version codes, one upload certificate, listing copy, phone screenshots, and privacy policy prepared |
| closed-test | human gate | Add real Wear screenshots, upload both form-factor bundles, add testers, and start rollout in Play Console |

Do not submit to Play from automation.
