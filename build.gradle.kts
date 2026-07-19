// File purpose: Shared plugins for PulseTime Wear.
// Primary components: Android application, Kotlin, Compose compiler plugins.
// I/O: Supplies versioned build plugins to the :app module.
plugins {
    id("com.android.application") version "8.13.0" apply false
    id("com.android.library") version "8.13.0" apply false
    kotlin("android") version "2.2.10" apply false
    kotlin("plugin.compose") version "2.2.10" apply false
}
