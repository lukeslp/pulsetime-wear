// File purpose: Gradle settings for the PulseTime Wear OS haptic clock.
// Primary components: pluginManagement, dependencyResolution, watch, phone, shared modules.
// I/O: Resolves Android/Kotlin/Compose plugins; produces Wear APK.
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "pulsetime-wear"
include(":app", ":mobile", ":shared", ":hapticlab")
