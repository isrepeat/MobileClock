[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [int]$ParentProcessId = 0
)

$ErrorActionPreference = 'Stop'
$utf8Encoding = [System.Text.UTF8Encoding]::new($false)
[Console]::InputEncoding = $utf8Encoding
[Console]::OutputEncoding = $utf8Encoding
$OutputEncoding = $utf8Encoding

function Initialize-VisualStudioEnvironment {
    param(
        [Parameter(Mandatory = $true)]
        [string]$VisualStudioRoot
    )

    # CMake can discover cl.exe from Visual Studio, but Ninja inherits the
    # PowerShell environment and therefore also needs INCLUDE/LIB/PATH from
    # VsDevCmd. This makes the script runnable outside a Developer Prompt.
    $developerCommand = Join-Path $VisualStudioRoot 'Common7\Tools\VsDevCmd.bat'
    if (-not (Test-Path -LiteralPath $developerCommand)) {
        throw "Visual Studio developer command was not found: $developerCommand"
    }

    $environmentLines = & cmd.exe /c "`"$developerCommand`" -arch=x64 -host_arch=x64 >nul && set"
    foreach ($environmentLine in $environmentLines) {
        $separatorIndex = $environmentLine.IndexOf('=')
        if ($separatorIndex -gt 0) {
            $name = $environmentLine.Substring(0, $separatorIndex)
            $value = $environmentLine.Substring($separatorIndex + 1)
            Set-Item -LiteralPath "Env:$name" -Value $value
        }
    }
}

try {
    $projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
    $generateXamlScript = Join-Path $projectRoot 'Scripts\PowerShell\generate-xaml.ps1'
    $artifactDirectory = Join-Path $projectRoot 'Build\MobileClock.PreviewPlugin'
    $previewerRoot = Join-Path (Split-Path -Parent $projectRoot) 'AndroidAppPreviewer'
    $projectFile = Join-Path $previewerRoot 'AndroidAppPreviewer.WPF\AndroidAppPreviewer.WPF.csproj'
    $previewer = Join-Path $previewerRoot "!VS_TMP\Build\$Configuration\x64\AndroidAppPreviewer.WPF\AndroidAppPreviewer.exe"
    $plugin = Join-Path $artifactDirectory "Build\$Configuration\x64\MobileClock.PreviewPlugin\MobileClock.PreviewPlugin.dll"
    $binaryLogDirectory = Join-Path $artifactDirectory 'Logs'
    $binaryLogName = "android-app-previewer-{0:yyyyMMdd-HHmmss}.binlog" -f [DateTime]::Now
    $binaryLogPath = Join-Path $binaryLogDirectory $binaryLogName
    $visualStudioMsBuild = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
    $visualStudioCmake = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    $visualStudioNinja = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
    $cmakeBuildDirectory = Join-Path $artifactDirectory 'Intermediate\CMake'


    if (-not (Test-Path $projectFile)) {
        throw "AndroidAppPreviewer was not found at $previewerRoot. Clone it alongside MobileClock or provide the repository there."
    }

    if (Test-Path $visualStudioMsBuild) {
        $msBuild = $visualStudioMsBuild
    } else {
        $msBuild = (Get-Command MSBuild.exe -ErrorAction Stop).Source
    }
    Initialize-VisualStudioEnvironment -VisualStudioRoot 'C:\Program Files\Microsoft Visual Studio\18\Community'

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
    $cmakeArguments = @('-S', $projectRoot, '-B', $cmakeBuildDirectory, '-G', 'Ninja', "-DCMAKE_BUILD_TYPE=$Configuration", "-DCMAKE_MAKE_PROGRAM=$ninja")
    & $cmake @cmakeArguments
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