[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('Drive')]
    [string]$Destination,

    # Пересобирает APK с текущим versionCode для быстрой тестовой переустановки.
    [switch]$KeepVersion,

    # Конфигурация нативной и Android-сборки.
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$utf8Encoding = [System.Text.UTF8Encoding]::new($false)
[Console]::InputEncoding = $utf8Encoding
[Console]::OutputEncoding = $utf8Encoding
$OutputEncoding = $utf8Encoding

$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$bumpVersion = Join-Path $PSScriptRoot 'bump-version.ps1'
$buildAndroid = Join-Path $PSScriptRoot 'build-android.ps1'
$uploadToDrive = Join-Path $PSScriptRoot 'upload-apk-to-drive.ps1'
$configurationDirectory = $Configuration.ToLowerInvariant()
$apkSuffix = if ($Configuration -eq 'Release') { 'release-unsigned' } else { 'debug' }
$sourceApk = Join-Path $projectRoot "Build\MobileClock.Android\outputs\apk\$configurationDirectory\MobileClock.Android-$apkSuffix.apk"
$sourceUpdaterApk = Join-Path $projectRoot "Build\MobileClock.AndroidUpdater\outputs\apk\$configurationDirectory\MobileClock.AndroidUpdater-$apkSuffix.apk"
$versionProperties = Join-Path $projectRoot 'version.properties'
$distributionOutput = Join-Path $projectRoot 'Build\distribution'

if ($Configuration -eq 'Release') {
    throw 'Release APK is unsigned. Configure a release signing key before uploading it to Google Drive.'
}

if ($KeepVersion) {
    Write-Host '==> Keeping the current Android version for a test reinstall'
} else {
    & $bumpVersion
}
& $buildAndroid -Configuration $Configuration

$properties = ConvertFrom-StringData ([System.IO.File]::ReadAllText($versionProperties))
$destinationApk = Join-Path $distributionOutput "MobileClock-$($properties.VERSION_CODE)-$($properties.VERSION_NAME).apk"
$destinationUpdaterApk = Join-Path $distributionOutput 'MobileClockUpdater.apk'
New-Item -ItemType Directory -Path $distributionOutput -Force | Out-Null
Copy-Item -LiteralPath $sourceApk -Destination $destinationApk -Force
Copy-Item -LiteralPath $sourceUpdaterApk -Destination $destinationUpdaterApk -Force

if ($Destination -eq 'Drive') {
    Write-Host '==> Uploading MobileClock APK to Google Drive'
    & $uploadToDrive -ApkPath $destinationApk
    Write-Host "APK uploaded to Google Drive: $destinationApk"
    Write-Host '==> Uploading MobileClock Updater APK to Google Drive'
    & $uploadToDrive -ApkPath $destinationUpdaterApk
    Write-Host "Updater APK uploaded to Google Drive: $destinationUpdaterApk"
}