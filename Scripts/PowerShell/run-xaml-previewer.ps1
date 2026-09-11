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

    Write-Host "==> Building XamlPreviewer $Configuration x64"
    & $msBuild $projectFile '/t:Build' "/p:Configuration=$Configuration" '/p:Platform=x64' '/m'
    if ($LASTEXITCODE -ne 0) {
        throw "XamlPreviewer $Configuration build failed with exit code $LASTEXITCODE."
    }

    if (-not (Test-Path $previewer)) {
        throw "XamlPreviewer executable was not produced: $previewer"
    }

    Write-Host "==> Starting $previewer"
    Start-Process -FilePath $previewer
} catch {
    Write-Error $_
    exit 1
}