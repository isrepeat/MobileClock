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
    $previewerRoot = Join-Path (Split-Path -Parent $projectRoot) 'AndroidAppPreviewer'
    $projectFile = Join-Path $previewerRoot 'AndroidAppPreviewer.WPF\AndroidAppPreviewer.WPF.csproj'
    $previewer = Join-Path $previewerRoot "!VS_TMP\Build\$Configuration\x64\AndroidAppPreviewer.WPF\AndroidAppPreviewer.exe"
    $plugin = Join-Path $projectRoot "MobileClock.PreviewPlugin\!VS_TMP\Build\$Configuration\x64\MobileClock.PreviewPlugin\MobileClock.PreviewPlugin.dll"
    $binaryLogDirectory = Join-Path $projectRoot 'MobileClock.PreviewPlugin\!VS_TMP\Logs'
    $binaryLogName = "android-app-previewer-{0:yyyyMMdd-HHmmss}.binlog" -f [DateTime]::Now
    $binaryLogPath = Join-Path $binaryLogDirectory $binaryLogName
    $visualStudioMsBuild = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
    $visualStudioCmake = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    $visualStudioNinja = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
    $cmakeBuildDirectory = Join-Path $projectRoot 'Build\cmake\previewer-x64'

    if (-not (Test-Path $projectFile)) {
        throw "AndroidAppPreviewer was not found at $previewerRoot. Clone it alongside MobileClock or provide the repository there."
    }

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
    if (Test-Path $visualStudioCmake) {
        $cmake = $visualStudioCmake
    } else {
        $cmake = (Get-Command cmake.exe -ErrorAction Stop).Source
    }
    if (Test-Path $visualStudioNinja) {
        $ninja = $visualStudioNinja
    } else {
        $ninja = (Get-Command ninja.exe -ErrorAction Stop).Source
    }
    & $cmake '-S' $projectRoot '-B' $cmakeBuildDirectory '-G' 'Ninja' "-DCMAKE_BUILD_TYPE=$Configuration" "-DCMAKE_MAKE_PROGRAM=$ninja"
    if ($LASTEXITCODE -ne 0) {
        throw "MobileClock preview-plugin CMake configure failed with exit code $LASTEXITCODE."
    }
    & $cmake '--build' $cmakeBuildDirectory '--target' 'mobileclock_preview_plugin' '--' '-j' '2'
    if ($LASTEXITCODE -ne 0) {
        throw "MobileClock preview plugin $Configuration build failed with exit code $LASTEXITCODE."
    }
    if (-not (Test-Path $plugin)) {
        throw "MobileClock preview plugin was not produced: $plugin"
    }

    Write-Host "==> Rebuilding AndroidAppPreviewer $Configuration x64"
    & $msBuild $projectFile '/t:Rebuild' "/p:Configuration=$Configuration" '/p:Platform=x64' "/bl:$binaryLogPath;ProjectImports=Embed"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "==> MSBuild binary log: $binaryLogPath"
        throw "AndroidAppPreviewer $Configuration build failed with exit code $LASTEXITCODE."
    }

    Write-Host "==> MSBuild binary log: $binaryLogPath"
    if (-not (Test-Path $previewer)) {
        throw "AndroidAppPreviewer executable was not produced: $previewer"
    }

    Write-Host "==> Starting $previewer"
    Start-Process -FilePath $previewer -ArgumentList '--plugin', $plugin
} catch {
    Write-Error $_
    exit 1
}