# PulseTime — platforms track (Wear OS)

| Stage | Status | Evidence |
|-------|--------|----------|
| scaffolded | done | Gradle Wear Compose app, standalone, minSdk 30 |
| built | done | `./gradlew :app:assembleDebug` — APK at `app/build/outputs/apk/debug/` |
| validated | done | `./gradlew :app:testDebugUnitTest` green; TalkBack checklist in README |
| device-tested | blocked | No Wear device in adb — see `DEVICE_EVIDENCE.md` + `scripts/install-watch.sh` |
| store-staged | done (local) | `app/build/outputs/bundle/release/app-release.aab` — human publishes |

Do not submit to Play from automation.
