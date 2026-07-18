# PulseTime — platforms track (Wear OS)

| Stage | Status | Evidence |
|-------|--------|----------|
| scaffolded | done | Gradle Wear Compose app, standalone, minSdk 30 |
| built | done | `./gradlew :app:assembleDebug` — APK at `app/build/outputs/apk/debug/` |
| validated | done | `./gradlew :app:testDebugUnitTest` green; TalkBack checklist in README |
| device-tested | installed; QA pending | Pixel Watch 2 is installed; human haptic and TalkBack QA remain — see `DEVICE_EVIDENCE.md` |
| store-staged | done (local) | `app/build/outputs/bundle/release/app-release.aab` — human publishes |

Do not submit to Play from automation.
