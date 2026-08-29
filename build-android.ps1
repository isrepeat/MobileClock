[CmdletBinding()]
param(
    # Recreates CMake's build directory before compiling the native library.
    [switch]$Clean,

    # Builds only the C++ library. Useful while editing renderer code.
    [switch]$NativeOnly,

    # Uploads the debug APK to Firebase App Distribution after it is built.
    [switch]$UploadFirebase
)

$ErrorActionPreference = 'Stop'

$projectRoot = $PSScriptRoot
$nativeRoot = Join-Path $projectRoot 'app\src\main\cpp'
$gradleWrapper = Join-Path $projectRoot 'gradlew.bat'
$apkPath = Join-Path $projectRoot 'app\build\outputs\apk\debug\app-debug.apk'

if ($NativeOnly -and $UploadFirebase) {
    throw '-NativeOnly and -UploadFirebase cannot be used together.'
}

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

Push-Location $nativeRoot
try {
    Write-Host '==> Building native ARM64 library with CMake'
    if ($Clean) {
        Invoke-Checked $cmake @('--fresh', '--preset', 'android-arm64-debug')
    } else {
        Invoke-Checked $cmake @('--preset', 'android-arm64-debug')
    }
    Invoke-Checked $cmake @('--build', '--preset', 'android-arm64-debug')
} finally {
    Pop-Location
}

$nativeLibrary = Join-Path $projectRoot 'app\src\main\jniLibs\arm64-v8a\libmobileclock.so'
if (-not (Test-Path $nativeLibrary)) {
    throw "CMake completed but did not produce $nativeLibrary"
}

if ($NativeOnly) {
    Write-Host "Native library ready: $nativeLibrary"
    exit 0
}

# Gradle deliberately does not invoke CMake here. app/build.gradle.kts no
# longer has externalNativeBuild, so it only packages the .so emitted above.
# Firebase's upload task already depends on assembleDebug, so it builds the
# APK first and then uploads exactly that artifact.
$gradleTask = if ($UploadFirebase) { 'appDistributionUploadDebug' } else { 'assembleDebug' }
Write-Host "==> Running Gradle task: $gradleTask"
Invoke-Checked $gradleWrapper @('--no-daemon', $gradleTask)

if (-not (Test-Path $apkPath)) {
    throw "Gradle completed but did not produce $apkPath"
}

Write-Host "APK ready: $apkPath"
if ($UploadFirebase) {
    Write-Host 'APK uploaded to Firebase App Distribution.'
}
