[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('Drive')]
    [string]$Destination,

    # Пересобирает APK с текущим versionCode для быстрой тестовой переустановки.
    [switch]$KeepVersion
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$bumpVersion = Join-Path $PSScriptRoot 'bump-version.ps1'
$buildAndroid = Join-Path $PSScriptRoot 'build-android.ps1'
$uploadToDrive = Join-Path $PSScriptRoot 'upload-apk-to-drive.ps1'
$sourceApk = Join-Path $projectRoot 'app\build\outputs\apk\debug\app-debug.apk'
$sourceUpdaterApk = Join-Path $projectRoot 'updater\build\outputs\apk\debug\updater-debug.apk'
$versionProperties = Join-Path $projectRoot 'version.properties'
$distributionOutput = Join-Path $projectRoot 'out\distribution'

if ($KeepVersion) {
    Write-Host '==> Keeping the current Android version for a test reinstall'
} else {
    & $bumpVersion
}
& $buildAndroid

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