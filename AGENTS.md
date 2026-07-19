# PulseTime project instructions

## Publishing the drummer download

Never assume the existing APK under `app/build/` is current. Whenever the
PulseTime download is published or refreshed:

1. Clean-build the APK from the current committed source with JDK 21 and the
   Android SDK configured.
2. Back up every remote file that will be replaced.
3. Publish the APK and update its `SHA256SUMS.txt`, the Android manifest, and
   the live downloads catalog together.
4. Verify the public APK hash, package headers, checksum, catalog entry, and
   landing-page link after deployment.

The live site is split across drummer:

- APK, checksum, and Android manifest: `~/html/downloads/android/`
- Landing page and catalog: `~/www/dr.eamer.dev/downloads/`
