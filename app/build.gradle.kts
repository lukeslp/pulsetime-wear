// File purpose: Builds the PulseTime Wear OS haptic clock APK.
// Primary components: Wear Compose Material3 app, Tile service, vibrator API.
// I/O: Consumes Android SDK; emits debug/release Wear APKs for sideload.
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

val playStoreFile = providers.environmentVariable("PULSETIME_UPLOAD_STORE_FILE").orNull
val playStorePassword = providers.environmentVariable("PULSETIME_UPLOAD_STORE_PASSWORD").orNull
val playKeyAlias = providers.environmentVariable("PULSETIME_UPLOAD_KEY_ALIAS").orNull
val playKeyPassword = providers.environmentVariable("PULSETIME_UPLOAD_KEY_PASSWORD").orNull
val playSigningAvailable = listOf(playStoreFile, playStorePassword, playKeyAlias, playKeyPassword)
    .all { !it.isNullOrBlank() }

plugins {
    id("com.android.application")
    kotlin("android")
    kotlin("plugin.compose")
}

android {
    namespace = "com.lukesteuber.pulsetime.wear"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.lukesteuber.pulsetime.wear"
        minSdk = 30
        targetSdk = 35
        // Play requires version codes to be unique across phone and Wear artifacts.
        versionCode = 20_000_004
        versionName = "0.3.1"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    signingConfigs {
        create("play") {
            if (playSigningAvailable) {
                storeFile = file(playStoreFile!!)
                storePassword = playStorePassword
                keyAlias = playKeyAlias
                keyPassword = playKeyPassword
            }
        }
    }

    buildTypes {
        debug {
            applicationIdSuffix = ".debug"
            versionNameSuffix = "-debug"
        }
        release {
            isMinifyEnabled = false
            if (playSigningAvailable) {
                signingConfig = signingConfigs.getByName("play")
            }
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro",
            )
        }
    }

    buildFeatures {
        compose = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlin {
        compilerOptions {
            jvmTarget.set(JvmTarget.JVM_17)
        }
    }

    packaging {
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
        }
    }
}

tasks.matching { it.name == "bundleRelease" || it.name == "assembleRelease" }.configureEach {
    doFirst {
        check(playSigningAvailable) {
            "PulseTime Play signing is not configured. Run scripts/create-play-upload-key.sh, then scripts/build-play-release.sh."
        }
    }
}

dependencies {
    implementation(project(":shared"))
    val composeBom = platform("androidx.compose:compose-bom:2025.10.01")
    implementation(composeBom)
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.foundation:foundation")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.activity:activity-compose:1.11.0")
    implementation("androidx.core:core-ktx:1.17.0")
    implementation("androidx.wear.compose:compose-material3:1.5.0")
    implementation("androidx.wear.compose:compose-foundation:1.5.0")
    implementation("androidx.wear:wear:1.3.0")
    implementation("androidx.wear.tiles:tiles:1.4.1")
    implementation("androidx.wear.tiles:tiles-material:1.4.1")
    implementation("androidx.wear.protolayout:protolayout:1.2.1")
    implementation("androidx.wear.protolayout:protolayout-material:1.2.1")
    implementation("com.google.guava:guava:33.3.1-android")
    implementation("androidx.datastore:datastore-preferences:1.1.7")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.2")
    implementation("com.google.android.gms:play-services-wearable:20.0.1")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.9.4")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.9.4")

    testImplementation("junit:junit:4.13.2")
    testImplementation("org.jetbrains.kotlinx:kotlinx-coroutines-test:1.10.2")
    debugImplementation("androidx.compose.ui:ui-tooling")
}
