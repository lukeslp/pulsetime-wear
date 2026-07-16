// File purpose: Gradle settings for the PulseTime Wear OS haptic clock.
// Primary components: pluginManagement, dependencyResolution, :app module.
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
include(":app")
