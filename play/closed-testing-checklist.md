# Closed testing checklist

## Before upload

- [x] Phone and Wear OS bundles use the same package name
- [x] Phone and Wear OS bundles use one upload certificate
- [x] Version codes are unique across form factors
- [x] Release tests and lint pass
- [x] Phone screenshots captured on a Pixel 10
- [x] 512 × 512 Play icon prepared
- [x] Public privacy policy published
- [ ] Upload keystore and Keychain password backed up securely
- [x] 1024 × 500 Play feature graphic prepared
- [ ] Current 1:1 Wear OS screenshots captured from the Pixel Watch
- [ ] Human haptic and TalkBack checklist completed on the watch

## Play Console

- [ ] Sign in as `lucas.steuber@gmail.com`, the same developer account used for Alt Text Intelligence
- [ ] Create or select the PulseTime app with package `com.lukesteuber.pulsetime.wear`
- [ ] Enroll in Play App Signing and verify the upload certificate fingerprint
- [ ] Complete the main listing and App content forms
- [ ] Create the Android phone closed-test release
- [ ] Add the Wear OS form factor under Advanced settings
- [ ] Create the Wear OS closed-test release
- [ ] Add the tester email list or Google Group
- [ ] Start both closed-test rollouts

## Tester verification

- [ ] Uninstall old sideloaded debug builds from phone and watch
- [ ] Join the closed test with the tester Google account
- [ ] Install the companion from Play on the phone
- [ ] Install PulseTime from Play on the watch
- [ ] Verify that phone demonstrations play on the paired watch
- [ ] Verify that the watch app and tile work without the phone
