[CmdletBinding()]
param(
    # Recreates CMake's build directory before compiling the native library.
    [switch]$Clean,

    # Builds only the C++ library. Useful while editing renderer code.
    [switch]$NativeOnly,

    # Uploads the debug APK to Firebase App Distribution after it is built.
    [switch]$UploadFirebase,

    # Проект поддерживает только физические устройства ARM64.
    [ValidateSet('arm64-v8a')]
    [string]$Architecture = 'arm64-v8a'
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$nativeRoot = Join-Path $projectRoot 'Native'
$xamlCompilerRoot = Join-Path $nativeRoot 'UtilityHelpersLib\NugetProjects\XamlRuntime\Nuget\XamlCompiler'
$xamlCompilerBuild = Join-Path $projectRoot 'out\xaml-compiler'
$xamlCompiler = Join-Path $xamlCompilerBuild 'Debug\XamlCompiler.exe'
$xamlSourceRoot = Join-Path $nativeRoot 'UI'
$xamlGeneratedRoot = Join-Path $nativeRoot '!Generated\Xaml'
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

Write-Host '==> Building XamlCompiler host tool'
Invoke-Checked $cmake @('--fresh', '-S', $xamlCompilerRoot, '-B', $xamlCompilerBuild, '-G', 'Visual Studio 18 2026', '-A', 'x64')
Invoke-Checked $cmake @('--build', $xamlCompilerBuild, '--config', 'Debug')
if (-not (Test-Path $xamlCompiler)) {
    throw "XamlCompiler build completed but did not produce $xamlCompiler"
}

Get-ChildItem -LiteralPath $xamlSourceRoot -Filter '*.xaml' -File | ForEach-Object {
    $generatedPath = Join-Path $xamlGeneratedRoot ($_.Name + '.cpp')
    Write-Host "==> Compiling $($_.Name) into native UI classes"
    Invoke-Checked $xamlCompiler @($_.FullName, $generatedPath)
}

Push-Location $nativeRoot
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

$nativeLibrary = Join-Path $projectRoot "app\src\main\jniLibs\$Architecture\libmobileclock.so"
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
# gradlew determines the Android project from the current directory. The .bat
# launchers live in Scripts, therefore explicitly return to the project root
# before calling it; otherwise Gradle treats Scripts as a separate project.
Push-Location $projectRoot
try {
    Invoke-Checked $gradleWrapper @('--no-daemon', $gradleTask)
} finally {
    Pop-Location
}

if (-not (Test-Path $apkPath)) {
    throw "Gradle completed but did not produce $apkPath"
}

Write-Host "APK ready: $apkPath"
if ($UploadFirebase) {
    Write-Host 'APK uploaded to Firebase App Distribution.'
}