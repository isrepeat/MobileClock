[CmdletBinding()]
param(
    # Recreates CMake's build directory before compiling the native library.
    [switch]$Clean,

    # Builds only the C++ library. Useful while editing renderer code.
    [switch]$NativeOnly,

    # Проект поддерживает только физические устройства ARM64.
    [ValidateSet('arm64-v8a')]
    [string]$Architecture = 'arm64-v8a'
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$androidHostRoot = Join-Path $projectRoot 'MobileClock.AndroidHost'
$applicationRoot = Join-Path $projectRoot 'MobileClock.Application'
$uiRoot = Join-Path $projectRoot 'MobileClock.UI'
$gradleRoot = Join-Path $projectRoot 'Build\Gradle'
$gradleWrapper = Join-Path $gradleRoot 'gradlew.bat'
$apkPath = Join-Path $projectRoot 'Build\MobileClock.Android\outputs\apk\debug\MobileClock.Android-debug.apk'
$updaterApkPath = Join-Path $projectRoot 'Build\MobileClock.AndroidUpdater\outputs\apk\debug\MobileClock.AndroidUpdater-debug.apk'

function Invoke-Checked {
    param(
        [Parameter(Mandatory)] [string]$Program,
        [Parameter(Mandatory)] [string[]]$Arguments
    )

    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $Program $($Arguments -join ' ')"
    }
}

# Prefer the CMake bundled with Visual Studio, but permit a standalone CMake
# installation when this script is run outside Visual Studio.
$visualStudioCmake = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (Test-Path $visualStudioCmake) {
    $cmake = $visualStudioCmake
} else {
    $cmake = (Get-Command cmake -ErrorAction Stop).Source
}

& (Join-Path $PSScriptRoot 'generate-xaml.ps1')

Push-Location $projectRoot
try {
    $cmakePreset = 'android-arm64-debug'
    Write-Host "==> Building native $Architecture library with CMake"
    if ($Clean) {
        Invoke-Checked $cmake @('--fresh', '--preset', $cmakePreset)
    } else {
        Invoke-Checked $cmake @('--preset', $cmakePreset)
    }
    Invoke-Checked $cmake @('--build', '--preset', $cmakePreset)
} finally {
    Pop-Location
}

$nativeLibrary = Join-Path $projectRoot "Build\MobileClock.AndroidHost\android\jniLibs\$Architecture\libmobileclock.so"
if (-not (Test-Path $nativeLibrary)) {
    throw "CMake completed but did not produce $nativeLibrary"
}

if ($NativeOnly) {
    Write-Host "Native library ready: $nativeLibrary"
    exit 0
}

# Gradle deliberately does not invoke CMake here. MobileClock.Android/build.gradle.kts no
# longer has externalNativeBuild, so it only packages the .so emitted above.
$gradleTasks = @(':MobileClock.Android:assembleDebug', ':MobileClock.AndroidUpdater:assembleDebug')
Write-Host "==> Running Gradle tasks: $($gradleTasks -join ', ')"
# gradlew determines the Android project from the current directory. The .bat
# launchers and settings live in Build/Gradle, therefore invoke Gradle there.
Push-Location $gradleRoot
try {
    Invoke-Checked $gradleWrapper (@('--no-daemon') + $gradleTasks)
} finally {
    Pop-Location
}

if (-not (Test-Path $apkPath)) {
    throw "Gradle completed but did not produce $apkPath"
}
if (-not (Test-Path $updaterApkPath)) {
    throw "Gradle completed but did not produce $updaterApkPath"
}

Write-Host "APK ready: $apkPath"
Write-Host "Updater APK ready: $updaterApkPath"