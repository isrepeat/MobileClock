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
    $projectFile = Join-Path $projectRoot 'XamlPreviewer\XamlPreviewer.WPF\XamlPreviewer.WPF.csproj'
    $pluginProjectFile = Join-Path $projectRoot 'MobileClock.PreviewPlugin\MobileClock.PreviewPlugin.vcxproj'
    $previewer = Join-Path $projectRoot "XamlPreviewer\!VS_TMP\Build\$Configuration\x64\XamlPreviewer.WPF\XamlPreviewer.exe"
    $plugin = Join-Path $projectRoot "MobileClock.PreviewPlugin\!VS_TMP\Build\$Configuration\x64\MobileClock.PreviewPlugin\MobileClock.PreviewPlugin.dll"
    $binaryLogDirectory = Join-Path $projectRoot 'XamlPreviewer\!VS_TMP\Logs'
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
    Write-Host "==> Rebuilding MobileClock preview plugin $Configuration x64"
    & $msBuild $pluginProjectFile '/t:Rebuild' "/p:Configuration=$Configuration" '/p:Platform=x64' '/p:BuildProjectReferences=false' "/bl:$binaryLogPath;ProjectImports=Embed"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "==> MSBuild binary log: $binaryLogPath"
        throw "MobileClock preview plugin $Configuration build failed with exit code $LASTEXITCODE."
    }
    if (-not (Test-Path $plugin)) {
        throw "MobileClock preview plugin was not produced: $plugin"
    }

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
    Start-Process -FilePath $previewer -ArgumentList '--plugin', $plugin
} catch {
    Write-Error $_
    exit 1
}