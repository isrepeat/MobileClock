import com.google.firebase.appdistribution.gradle.firebaseAppDistribution

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.compose)
    alias(libs.plugins.google.services)
    alias(libs.plugins.firebase.appdistribution)
}

android {
    namespace = "com.example.mobileclock"
    compileSdk {
        version = release(37)
    }

    defaultConfig {
        applicationId = "com.example.mobileclock"
        minSdk = 24
        targetSdk = 37
        versionCode = 5
        versionName = "0.05"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++17"
            }
        }
    }

    buildTypes {
        debug {
            firebaseAppDistribution {
                appId = "1:118817012419:android:552bcf49ee11cb5ac94076"
                serviceCredentialsFile = "C:/WORK/Secrets/mobileclock-cca50210cd68.json"
                artifactType = "APK"
                testers = "newiskeep@gmail.com"
                releaseNotes = "MobileClock ${defaultConfig.versionName}"
            }
        }
        release {
            optimization {
                enable = false
            }
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
    buildFeatures {
        compose = true
        buildConfig = true
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }
}

dependencies {
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.activity.compose)
    implementation(libs.androidx.compose.material3)
    implementation(libs.androidx.compose.ui)
    implementation(libs.androidx.compose.ui.graphics)
    implementation(libs.androidx.compose.ui.tooling.preview)
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    testImplementation(libs.junit)
    androidTestImplementation(platform(libs.androidx.compose.bom))
    androidTestImplementation(libs.androidx.compose.ui.test.junit4)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(libs.androidx.junit)
    debugImplementation(libs.androidx.compose.ui.test.manifest)
    debugImplementation(libs.androidx.compose.ui.tooling)
}

// The Firebase task uploads the debug APK produced by the Android build.
// Make the IDE run configuration build a fresh APK before each upload.
tasks.matching { it.name == "appDistributionUploadDebug" }.configureEach {
    dependsOn("assembleDebug")
}
