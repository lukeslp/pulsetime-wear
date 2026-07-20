# PulseTime Play closed test

The phone and Wear OS bundles share `com.lukesteuber.pulsetime.wear` and one upload certificate. Play distributes them independently by form factor.

## Build signed artifacts

Run once:

```sh
scripts/create-play-upload-key.sh
```

Back up `~/.config/pulsetime/pulsetime-upload.jks` and its macOS Keychain password before uploading anything to Play.

Then build:

```sh
scripts/build-play-release.sh
```

The validated bundles, hashes, version information, and upload certificate fingerprint are staged under `dist/play/`.

## Human Play Console steps

Sign in to Play Console as `lucas.steuber@gmail.com`, the same developer account used for Alt Text Intelligence.

1. Under that account, create or select one app using package `com.lukesteuber.pulsetime.wear` and enroll it in Play App Signing.
2. Complete the store listing, app access, ads, target audience, content rating, and Data safety forms using `app-content.md`, `content-rating.md`, and `data-safety.md`. PulseTime collects and shares no data. Use `https://dr.eamer.dev/downloads/apps/pulse-time/privacy/` for the privacy policy.
3. Create a closed testing release for Android phones and upload `pulsetime-phone-0.3.0-10000003.aab`.
4. Under Advanced settings, add the Wear OS form factor and accept its review terms.
5. Create the Wear OS closed testing release and upload `pulsetime-wear-0.3.0-20000003.aab`.
6. Add at least one real 1:1 Wear OS screenshot and mention the PulseTime tile in the listing.
7. Add the tester email list or Google Group, review both releases, and start the closed-test rollout.
8. Testers should uninstall the older sideloaded debug builds before installing from Play.

Play Console login, upload, tester management, and rollout are human submission gates.
