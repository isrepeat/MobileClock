[CmdletBinding()]
param(
    [string]$AvdName = 'MobileClock_API_35',
    # Android CLI uses slash-separated SDK package identifiers.
    [string]$SystemImage = 'system-images/android-35/google_apis/x86_64',
    [string]$Device = 'pixel_6',
    [switch]$WaitForBoot,
    [switch]$NoWindow
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$localProperties = Join-Path $projectRoot 'local.properties'

function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed with exit code ${LASTEXITCODE}: $Program $($Arguments -join ' ')" }
}

if (-not (Test-Path $localProperties)) { throw "Android SDK location is not configured: $localProperties" }
$sdkLine = Get-Content $localProperties | Where-Object { $_ -match '^sdk\.dir=' } | Select-Object -First 1
if (-not $sdkLine) { throw 'local.properties does not contain sdk.dir.' }
$sdkPath = ($sdkLine -replace '^sdk\.dir=', '') -replace '\\:', ':' -replace '\\\\', '\'
$emulator = Join-Path $sdkPath 'emulator\emulator.exe'
$androidCli = Join-Path $sdkPath 'cmdline-tools\latest\bin\android.exe'
$avdManager = Join-Path $sdkPath 'cmdline-tools\latest\bin\avdmanager.bat'
$adb = Join-Path $sdkPath 'platform-tools\adb.exe'
foreach ($tool in @($emulator, $androidCli, $avdManager, $adb)) { if (-not (Test-Path $tool)) { throw "Required Android SDK tool was not found: $tool" } }

$knownAvds = @(& $emulator -list-avds)
if ($knownAvds -notcontains $AvdName) {
    $systemImageDirectory = Join-Path $sdkPath ($SystemImage -replace '/', '\\')
    if (-not (Test-Path $systemImageDirectory)) {
        Write-Host "==> Installing emulator image: $SystemImage"
        Invoke-Checked $androidCli @('sdk', 'install', $SystemImage)
        # New Android CLI may detach after it has downloaded the archive and
        # keep unpacking in its helper process. Wait for the package directory
        # rather than treating that hand-off as a failed installation.
        $installDeadline = (Get-Date).AddMinutes(10)
        while (-not (Test-Path $systemImageDirectory) -and (Get-Date) -lt $installDeadline) {
            Start-Sleep -Seconds 2
        }
        if (-not (Test-Path $systemImageDirectory)) { throw "Timed out installing $SystemImage." }
    }
    Write-Host "==> Creating virtual device: $AvdName"
    # avdmanager is retained by the SDK for AVD definitions and uses its
    # legacy semicolon-separated form, unlike the new Android CLI above.
    $avdSystemImage = $SystemImage -replace '/', ';'
    'no' | & $avdManager create avd --force --name $AvdName --package $avdSystemImage --device $Device
    if ($LASTEXITCODE -ne 0) { throw "AVD creation failed with exit code $LASTEXITCODE." }
}

# The CLI's Pixel profile defaults to a 12 GB userdata image. This workspace
# has less free space than that after downloading the system image; 6 GB is
# ample for this debug-only AVD and lets it boot reliably.
$avdConfig = Join-Path $env:USERPROFILE ".android\avd\$AvdName.avd\config.ini"
if (Test-Path $avdConfig) {
    $configLines = Get-Content $avdConfig
    if ($configLines -match '^disk\.dataPartition\.size=') {
        $configLines = $configLines -replace '^disk\.dataPartition\.size=.*$', 'disk.dataPartition.size=6G'
    } else {
        $configLines += 'disk.dataPartition.size=6G'
    }
    Set-Content -LiteralPath $avdConfig -Value $configLines -Encoding utf8
}

$running = @(& $adb devices | Select-String '^emulator-[0-9]+\s+device$')
if ($running.Count -eq 0) {
    # x86_64 emulator images require VT-x/AMD-V (or nested virtualization)
    # exposed to this Windows session. Fail before starting the long boot wait.
    & $emulator -accel-check
    if ($LASTEXITCODE -ne 0) {
        throw 'Android Emulator acceleration is unavailable. Enable virtualization/nested virtualization for this Windows machine, then run the script again.'
    }
    Write-Host "==> Starting emulator: $AvdName"
    $arguments = @('-avd', $AvdName)
    if ($NoWindow) { $arguments += '-no-window' }
    Start-Process -FilePath $emulator -ArgumentList $arguments | Out-Null
} else { Write-Host '==> An Android emulator is already running.' }

if ($WaitForBoot) {
    Write-Host '==> Waiting for Android to finish booting'
    Invoke-Checked $adb @('wait-for-device')
    do { Start-Sleep -Milliseconds 500; $booted = (& $adb shell getprop sys.boot_completed).Trim() } while ($booted -ne '1')
    Write-Host 'Emulator is ready.'
}
