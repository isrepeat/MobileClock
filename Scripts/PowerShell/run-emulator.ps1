[CmdletBinding()]
param(
    [string]$AvdName = 'MobileClock_API_35',
    [switch]$Clean,
    [switch]$NoWindow
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$startEmulator = Join-Path $PSScriptRoot 'start-emulator.ps1'
$buildAndroid = Join-Path $PSScriptRoot 'build-android.ps1'
$localProperties = Join-Path $projectRoot 'local.properties'
$sdkLine = Get-Content $localProperties | Where-Object { $_ -match '^sdk\.dir=' } | Select-Object -First 1
if (-not $sdkLine) { throw 'local.properties does not contain sdk.dir.' }
$sdkPath = ($sdkLine -replace '^sdk\.dir=', '') -replace '\\:', ':' -replace '\\\\', '\'
$adb = Join-Path $sdkPath 'platform-tools\adb.exe'
$apkPath = Join-Path $projectRoot 'app\build\outputs\apk\debug\app-debug.apk'

function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed with exit code ${LASTEXITCODE}: $Program $($Arguments -join ' ')" }
}

if (-not (Test-Path $adb)) { throw "Android Debug Bridge was not found: $adb" }

& $startEmulator -AvdName $AvdName -WaitForBoot -NoWindow:$NoWindow
if ($LASTEXITCODE -ne 0) { throw 'Could not start the Android emulator.' }

& $buildAndroid -Architecture x86_64 -Clean:$Clean
if ($LASTEXITCODE -ne 0) { throw 'Android build failed.' }

Invoke-Checked $adb @('install', '-r', $apkPath)
Invoke-Checked $adb @('shell', 'monkey', '-p', 'com.example.mobileclock', '1')
Write-Host 'MobileClock is running in the emulator.'
