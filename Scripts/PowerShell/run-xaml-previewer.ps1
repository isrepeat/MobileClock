[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [int]$ParentProcessId = 0
)

$ErrorActionPreference = 'Stop'

try {
    $projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
    $generateXamlScript = Join-Path $projectRoot 'Scripts\PowerShell\generate-xaml.ps1'
    $projectFile = Join-Path $projectRoot 'MobileClock.XamlPreviewer\XamlPreviewer.WPF\XamlPreviewer.WPF.csproj'
    $previewer = Join-Path $projectRoot "MobileClock.XamlPreviewer\!VS_TMP\Build\$Configuration\x64\XamlPreviewer.WPF\XamlPreviewer.exe"
    $binaryLogDirectory = Join-Path $projectRoot 'MobileClock.XamlPreviewer\!VS_TMP\Logs'
    $binaryLogName = "xaml-previewer-{0:yyyyMMdd-HHmmss}.binlog" -f [DateTime]::Now
    $binaryLogPath = Join-Path $binaryLogDirectory $binaryLogName
    $visualStudioMsBuild = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'

    if (Test-Path $visualStudioMsBuild) {
        $msBuild = $visualStudioMsBuild
    } else {
        $msBuild = (Get-Command MSBuild.exe -ErrorAction Stop).Source
    }

    if ($ParentProcessId -gt 0) {
        $parentProcess = Get-Process -Id $ParentProcessId -ErrorAction SilentlyContinue
        if ($null -ne $parentProcess) {
            $parentProcess.WaitForExit()
        }
    }

    & $generateXamlScript

    New-Item -ItemType Directory -Path $binaryLogDirectory -Force | Out-Null
    Write-Host "==> Rebuilding XamlPreviewer $Configuration x64"
    & $msBuild $projectFile '/t:Rebuild' "/p:Configuration=$Configuration" '/p:Platform=x64' "/bl:$binaryLogPath;ProjectImports=Embed"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "==> MSBuild binary log: $binaryLogPath"
        throw "XamlPreviewer $Configuration build failed with exit code $LASTEXITCODE."
    }

    Write-Host "==> MSBuild binary log: $binaryLogPath"
    if (-not (Test-Path $previewer)) {
        throw "XamlPreviewer executable was not produced: $previewer"
    }

    Write-Host "==> Starting $previewer"
    Start-Process -FilePath $previewer
} catch {
    Write-Error $_
    exit 1
}