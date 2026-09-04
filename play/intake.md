# PulseTime Play intake

- Product: PulseTime 0.3.0
- Maintainer: PulseTime contributors
- License: MIT
- Play Console account: `lucas.steuber@gmail.com`, the same developer account used for Alt Text Intelligence
- Play package: `com.lukesteuber.pulsetime.wear`
- Play record: confirmed absent from developer account `6919353697792485209`
  on 2026-07-20; the account contained only `one.whatcoloristhis.twa`
- Source: native Kotlin and Jetpack Compose modules for Android phones (`:mobile`) and Wear OS (`:app`), with shared encoding and playback code in `:shared`
- Explicit targets: Android phones and Wear OS under one Play listing, using separate version-code ranges
- Excluded target: the phone-only Pulse Lab (`:hapticlab`) remains a separate testing app and package

## Accessibility

The release requires TalkBack labels, logical focus order, usable touch targets, eyes-free training, and direct haptic checks. Phone screenshots and device installation evidence exist. Human TalkBack, haptic-strength, tile, and paired phone-to-watch checks remain open on the Pixel Watch 2.

## Toolchain and release risks

- The account is personal, has no publishing service account invited, and
  currently requires at least 12 opted-in closed-test users for at least 14 days
  before production access.
- `com.lukesteuber.pulsetime.wear` is not yet registered under Android developer
  verification.
- The `platforms` command is not installed, so Gradle, Android SDK, Java 21, and ADB checks provide the release evidence directly.
- The Play package ID becomes permanent after the first upload.
- The phone and Wear bundles must keep unique version codes and the same upload certificate.
- Back up the upload keystore and its Keychain password before the first Play upload.
- Play Console login, upload, tester management, rollout, and submission remain human actions.

## Next route

Use the Android mobile adapter to rebuild and validate both signed bundles, collect current device evidence, then prepare the missing Play Store assets and submission checklist.
