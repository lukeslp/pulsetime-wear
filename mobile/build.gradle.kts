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
    namespace = "com.lukesteuber.pulsetime.mobile"
    compileSdk = 36
    defaultConfig {
        applicationId = "com.lukesteuber.pulsetime.wear"
        minSdk = 26
        targetSdk = 36
        // Phone and Wear artifacts share a package but use disjoint Play ranges.
        versionCode = 10_000_004
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
        }
    }
    buildFeatures { compose = true }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlin { compilerOptions { jvmTarget.set(JvmTarget.JVM_17) } }
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
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.activity:activity-compose:1.11.0")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.9.4")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.9.4")
    implementation("androidx.lifecycle:lifecycle-viewmodel-ktx:2.9.4")
    implementation("com.google.android.gms:play-services-wearable:20.0.1")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.2")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-play-services:1.10.2")
    testImplementation("junit:junit:4.13.2")
    debugImplementation("androidx.compose.ui:ui-tooling")
}
