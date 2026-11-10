# Source and artwork notices

PulseTime's original code, documentation, and project artwork are by Luke
Steuber and are distributed under the [MIT license](LICENSE).

## Bundled dependency

The Pebble phone-settings code includes **pebble-clay 1.0.4**, copyright
2016 Pebble Technology, under MIT. Retain its complete notice alongside the
bundle when redistributing it:

- [Watchapp Clay notice](appstores/pebble/pulsetime/src/pkjs/vendor/PEBBLE-CLAY-LICENSE)
- [Watchface Clay notice](appstores/pebble/pulsetime-face/src/pkjs/vendor/PEBBLE-CLAY-LICENSE)

The Gradle wrapper (`gradle/wrapper/gradle-wrapper.jar`, `gradlew`,
`gradlew.bat`) is checked in and is part of Gradle, which is distributed under
the Apache License 2.0.

## Android build dependencies

No Android library source is copied into this repository. Gradle downloads the
libraries below at build time, and a built APK or AAB contains them. Each
keeps its own license. The license shown is the one each library publishes in
its Maven POM, read on 2026-11-10 for the exact version; "BOM" versions are
resolved by `androidx.compose:compose-bom:2025.10.01`. Transitive dependencies
are not listed one by one.

| Library | Version | Used by | Published license |
|---|---|---|---|
| `androidx.compose.ui:ui`, `ui-tooling-preview`, `ui-tooling` (debug) | 1.9.4 (BOM) | watch, phone, Pulse Lab | Apache License 2.0 |
| `androidx.compose.foundation:foundation` | 1.9.4 (BOM) | watch, phone, Pulse Lab | Apache License 2.0 |
| `androidx.compose.material3:material3` | 1.4.0 (BOM) | phone, Pulse Lab | Apache License 2.0 |
| `androidx.activity:activity-compose` | 1.11.0 | watch, phone, Pulse Lab | Apache License 2.0 |
| `androidx.core:core-ktx` | 1.17.0 | watch, Pulse Lab | Apache License 2.0 |
| `androidx.lifecycle:lifecycle-runtime-ktx`, `lifecycle-runtime-compose`, `lifecycle-viewmodel-compose`, `lifecycle-viewmodel-ktx` | 2.9.4 | watch, phone, Pulse Lab | Apache License 2.0 |
| `androidx.datastore:datastore-preferences` | 1.1.7 | watch | Apache License 2.0 |
| `androidx.wear.compose:compose-material3`, `compose-foundation` | 1.5.0 | watch | Apache License 2.0 |
| `androidx.wear:wear` | 1.3.0 | watch | Apache License 2.0 |
| `androidx.wear.tiles:tiles`, `tiles-material` | 1.4.1 | watch | Apache License 2.0 |
| `androidx.wear.protolayout:protolayout`, `protolayout-material` | 1.2.1 | watch | Apache License 2.0 |
| `com.google.guava:guava` | 33.3.1-android | watch | Apache License 2.0 |
| `org.jetbrains.kotlinx:kotlinx-coroutines-android`, `kotlinx-coroutines-play-services` | 1.10.2 | watch, phone, Pulse Lab | Apache License 2.0 |
| `org.jetbrains.kotlin:kotlin-stdlib` (via the Kotlin plugin) | 2.2.10 | all modules | Apache License 2.0 |
| `com.google.android.gms:play-services-wearable` | 20.0.1 | watch, phone | Android Software Development Kit License |
| `org.jetbrains.kotlinx:kotlinx-coroutines-test` (unit tests only) | 1.10.2 | watch | Apache License 2.0 |
| `junit:junit` (unit tests only) | 4.13.2 | all modules | Eclipse Public License 1.0 |

`play-services-wearable` is not open source. It is distributed under the
[Android Software Development Kit License](https://developer.android.com/studio/terms.html),
which applies to anyone who builds or redistributes the watch or phone app.
The Android Gradle Plugin (8.13.0) and the Kotlin Gradle and Compose compiler
plugins (2.2.10) are build tools under the Apache License 2.0.

The Garmin port is built with the Connect IQ SDK and the Pebble ports with the
Pebble SDK. Neither SDK is in this repository, and each keeps its own license.
The source license does not relicense the platform SDKs.

License texts: [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0.txt),
[Eclipse Public License 1.0](https://www.eclipse.org/legal/epl-v10.html).

## Project graphics and screenshots

The SVG marks use simple paths, circles, rectangles, gradients, and text; they
contain no linked image or font files. Raster project marks accompany these
vector designs. The Play feature graphic's editable source is
[feature-graphic.svg](play/assets/feature-graphic.svg).

Images under `play/assets/phone/`, `hapticlab/device-evidence/`, and the Pebble
`docs/` and `store/` directories document PulseTime's app or emulator interface.
The Pulse Lab market and other signal examples are fictional demonstrations.
Platform status bars and platform names retain their respective owners' rights.
The repository contains no bundled font files.

The dated validation documents distinguish emulator captures, device installs,
and wearer checks. An image of a running interface does not demonstrate that
its vibration pattern is comfortable or reliably recognized.
