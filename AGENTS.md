# PulseTime project instructions

## Publishing the direct download

Never assume the existing APK under `app/build/` is current. Whenever the
PulseTime download is published or refreshed:

1. Clean-build the APK from the current committed source with JDK 21 and the
   Android SDK configured.
2. Back up every remote file that will be replaced.
3. Publish the APK and update its `SHA256SUMS.txt`, the Android manifest, and
   the live downloads catalog together.
4. Verify the public APK hash, package headers, checksum, catalog entry, and
   landing-page link after deployment.

The live download site has two parts on the owner's web host, and both must
change together:

- The Android file shelf (APK, checksum, and Android manifest), served under
  `https://dr.eamer.dev/downloads/android/pulse-time/`.
- The landing page and downloads catalog, served at
  `https://dr.eamer.dev/downloads/apps/pulse-time/`.

The host name, login, and server paths are in the owner's private operations
notes, not in this repository. Read them before publishing. Without that
access, stop after the clean build and hand over the artifacts with their
checksums.

## Public repository

Keep machine names, home or server paths, network addresses, device serial
numbers, account emails, and account identifiers out of tracked files. Refer
to "the Play Console owner account" and "the owner's web host" instead.
