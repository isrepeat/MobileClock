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
$utf8Encoding = [System.Text.UTF8Encoding]::new($false)
[Console]::InputEncoding = $utf8Encoding
[Console]::OutputEncoding = $utf8Encoding
$OutputEncoding = $utf8Encoding

$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$androidHostRoot = Join-Path $projectRoot 'MobileClock.AndroidHost'
$applicationRoot = Join-Path $projectRoot 'MobileClock.Application'
$uiRoot = Join-Path $projectRoot 'MobileClock.UI'
$gradleRoot = Join-Path $projectRoot 'Tools\Gradle'
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

# Resolve the installed Visual Studio tools without fixing an edition or path.
. (Join-Path $PSScriptRoot 'Resolve-BuildTools.ps1')
$tools = Resolve-MobileClockBuildTools
$cmake = $tools.CMake
$javaHome = Resolve-MobileClockJavaHome
$androidSdk = Resolve-MobileClockAndroidSdk
$env:JAVA_HOME = $javaHome
$env:ANDROID_HOME = $androidSdk
$env:ANDROID_SDK_ROOT = $androidSdk
$env:Path = "$(Join-Path $javaHome 'bin');$env:Path"

& (Join-Path $PSScriptRoot 'generate-xaml.ps1')

Push-Location $projectRoot
try {
    $cmakePreset = 'android-arm64-debug'
    Write-Host "==> Building native $Architecture library with CMake"
    if ($Clean) {
        Invoke-Checked $cmake @('--fresh', '--preset', $cmakePreset, "-DCMAKE_MAKE_PROGRAM=$($tools.Ninja)")
    } else {
        Invoke-Checked $cmake @('--preset', $cmakePreset, "-DCMAKE_MAKE_PROGRAM=$($tools.Ninja)")
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
Write-Host "==> Using Java: $javaHome"
Write-Host "==> Using Android SDK: $androidSdk"
# Gradle определяет Android-проект по текущему каталогу. Launcher и settings
# находятся в Tools/Gradle, поэтому Gradle запускается оттуда.
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