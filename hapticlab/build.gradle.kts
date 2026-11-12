import org.jetbrains.kotlin.gradle.dsl.JvmTarget

// Pulse Lab has its own release key, separate from PulseTime's upload key.
// The four values arrive from the environment only; nothing is read from disk.
val releaseStoreFile = providers.environmentVariable("PULSE_LAB_RELEASE_STORE_FILE").orNull
val releaseStorePassword = providers.environmentVariable("PULSE_LAB_RELEASE_STORE_PASSWORD").orNull
val releaseKeyAlias = providers.environmentVariable("PULSE_LAB_RELEASE_KEY_ALIAS").orNull
val releaseKeyPassword = providers.environmentVariable("PULSE_LAB_RELEASE_KEY_PASSWORD").orNull
val releaseSigningAvailable = listOf(releaseStoreFile, releaseStorePassword, releaseKeyAlias, releaseKeyPassword)
    .all { !it.isNullOrBlank() }

plugins {
    id("com.android.application")
    kotlin("android")
    kotlin("plugin.compose")
}

android {
    namespace = "com.lukesteuber.pulsetime.hapticlab"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.lukesteuber.pulsetime.hapticlab"
        minSdk = 26
        targetSdk = 36
        versionCode = 5
        versionName = "0.2.3"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    signingConfigs {
        create("release") {
            if (releaseSigningAvailable) {
                storeFile = file(releaseStoreFile!!)
                storePassword = releaseStorePassword
                keyAlias = releaseKeyAlias
                keyPassword = releaseKeyPassword
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            if (releaseSigningAvailable) {
                signingConfig = signingConfigs.getByName("release")
            }
        }
    }

    buildFeatures {
        compose = true
        buildConfig = true
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlin { compilerOptions { jvmTarget.set(JvmTarget.JVM_17) } }
}

// An unsigned release APK is easy to mistake for a publishable one. Published
// builds of Pulse Lab are release-signed and never debuggable.
tasks.matching { it.name == "assembleRelease" }.configureEach {
    doFirst {
        check(releaseSigningAvailable) {
            "Pulse Lab release signing is not configured. Run scripts/build-pulse-lab-release.sh through the release toolkit's with-signing command."
        }
    }
}

dependencies {
    implementation(project(":shared"))
    val composeBom = platform("androidx.compose:compose-bom:2025.10.01")
    implementation(composeBom)
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.foundation:foundation")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.activity:activity-compose:1.11.0")
    implementation("androidx.core:core-ktx:1.17.0")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.9.4")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.2")
    testImplementation("junit:junit:4.13.2")
    debugImplementation("androidx.compose.ui:ui-tooling")
}
