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
$xamlCompilerRoot = Join-Path $projectRoot 'UtilityHelpersLib\NugetProjects\XamlRuntime\Nuget\XamlCompiler'
$xamlCompilerBuild = Join-Path $projectRoot 'Build\MobileClock.Application\xaml-compiler'
$xamlCompiler = Join-Path $xamlCompilerBuild 'Debug\XamlCompiler.exe'
$xamlSourceRoots = @(
    (Join-Path $applicationRoot 'UI'),
    $uiRoot
)
$xamlGeneratedRoot = Join-Path $projectRoot 'Build\MobileClock.Application\!Generated\Xaml'
$xamlIgnoreConfigurationPath = Join-Path $applicationRoot 'UI\XamlCompilerIgnore.json'
$xamlIgnoreConfiguration = Get-Content -LiteralPath $xamlIgnoreConfigurationPath -Raw | ConvertFrom-Json
$xamlIgnoredDirectories = @($xamlIgnoreConfiguration.directories)
$xamlIgnoredFileSuffixes = @($xamlIgnoreConfiguration.fileSuffixes)
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

Write-Host '==> Building XamlCompiler host tool'
Invoke-Checked $cmake @('--fresh', '-S', $xamlCompilerRoot, '-B', $xamlCompilerBuild, '-G', 'Visual Studio 18 2026', '-A', 'x64')
Invoke-Checked $cmake @('--build', $xamlCompilerBuild, '--config', 'Debug')
if (-not (Test-Path $xamlCompiler)) {
    throw "XamlCompiler build completed but did not produce $xamlCompiler"
}

foreach ($xamlSourceRoot in $xamlSourceRoots) {
    Get-ChildItem -LiteralPath $xamlSourceRoot -Filter '*.xaml' -File -Recurse | ForEach-Object {
        # Windows PowerShell 5.1 работает на .NET Framework, где ещё нет
        # System.IO.Path.GetRelativePath. Все найденные файлы гарантированно
        # находятся внутри $xamlSourceRoot, поэтому достаточно убрать этот префикс.
        $relativePath = $_.FullName.Substring($xamlSourceRoot.Length).TrimStart('\', '/')
        $generatedPath = Join-Path $xamlGeneratedRoot ($relativePath + '.cpp')
        $compilerArguments = @(
            $_.FullName,
            $generatedPath,
            '--control-include-prefix',
            'MobileClock.UI/Controls'
        )
        foreach ($directory in $xamlIgnoredDirectories) {
            $compilerArguments += '--ignore-directory', $directory
        }
        foreach ($suffix in $xamlIgnoredFileSuffixes) {
            $compilerArguments += '--ignore-file-suffix', $suffix
        }
        Write-Host "==> Compiling $($_.Name) into native UI classes"
        Invoke-Checked $xamlCompiler $compilerArguments
    }
}

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