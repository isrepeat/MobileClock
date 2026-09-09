pluginManagement {
    repositories {
        google {
            content {
                includeGroupByRegex("com\\.android.*")
                includeGroupByRegex("com\\.google.*")
                includeGroupByRegex("androidx.*")
            }
        }
        mavenCentral()
        gradlePluginPortal()
    }
}
plugins {
    id("org.gradle.toolchains.foojay-resolver-convention") version "1.0.0"
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

val repositoryRoot = file("../..")

rootProject.name = "MobileClock"
rootProject.layout.buildDirectory.set(rootProject.layout.projectDirectory.dir("../MobileClock"))
include(":MobileClock.Android")
include(":MobileClock.AndroidUpdater")

project(":MobileClock.Android").projectDir = repositoryRoot.resolve("MobileClock.Android")
project(":MobileClock.AndroidUpdater").projectDir = repositoryRoot.resolve("MobileClock.AndroidUpdater")

subprojects {
    layout.buildDirectory.set(rootProject.layout.projectDirectory.dir("../$name"))
}