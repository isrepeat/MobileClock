[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $PathList,

    [ValidateRange(1, 600)]
    [int] $WaitTimeoutSeconds = 120,

    [ValidateRange(100, 10000)]
    [int] $DocumentWarmupDelayMilliseconds = 1500
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $PathList -PathType Leaf)) {
    throw "IntelliSense warm-up path list not found: $PathList"
}

$pathsToWarmUp = Get-Content -LiteralPath $PathList |
    ForEach-Object { $_.Trim() } |
    Where-Object { $_ -and -not $_.StartsWith('#') }

if (-not $pathsToWarmUp) {
    throw "IntelliSense warm-up path list is empty: $PathList"
}

foreach ($pathToWarmUp in $pathsToWarmUp) {
    if (-not (Test-Path -LiteralPath $pathToWarmUp -PathType Leaf)) {
        throw "IntelliSense warm-up file not found: $pathToWarmUp"
    }
}

$vsWherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vsWherePath -PathType Leaf)) {
    throw "Visual Studio locator not found: $vsWherePath"
}

$visualStudioRoot = & $vsWherePath `
    -latest `
    -products '*' `
    -version '[18.0,19.0)' `
    -property installationPath

if (-not $visualStudioRoot) {
    throw 'Visual Studio 18 installation not found.'
}

$devenvPath = Join-Path $visualStudioRoot 'Common7\IDE\devenv.exe'
if (-not (Test-Path -LiteralPath $devenvPath -PathType Leaf)) {
    throw "devenv.exe not found: $devenvPath"
}

$deadline = [DateTime]::UtcNow.AddSeconds($WaitTimeoutSeconds)
do {
    $runningVisualStudio = Get-Process -Name devenv -ErrorAction SilentlyContinue |
        Where-Object { $_.MainWindowHandle -ne 0 } |
        Select-Object -First 1

    if ($runningVisualStudio) {
        break
    }

    Start-Sleep -Milliseconds 500
} while ([DateTime]::UtcNow -lt $deadline)

if (-not $runningVisualStudio) {
    throw "Visual Studio did not start within $WaitTimeoutSeconds seconds."
}

# The automation object lets the script close only documents it opened itself.
# This avoids closing a header tab the developer already had open.
$dte = $null
do {
    try {
        $dte = [Runtime.InteropServices.Marshal]::GetActiveObject('VisualStudio.DTE.18.0')
    }
    catch {
        # Visual Studio may show its main window before the automation object
        # is registered. Keep waiting until the same timeout expires.
    }

    if ($dte) {
        break
    }

    Start-Sleep -Milliseconds 500
} while ([DateTime]::UtcNow -lt $deadline)

if (-not $dte) {
    throw "Unable to connect to the Visual Studio automation object within $WaitTimeoutSeconds seconds."
}

$openDocumentPaths = @{}
foreach ($document in $dte.Documents) {
    if ($document.FullName) {
        $openDocumentPaths[$document.FullName] = $true
    }
}

foreach ($pathToWarmUp in $pathsToWarmUp) {
    $fullPath = [System.IO.Path]::GetFullPath($pathToWarmUp)
    if ($openDocumentPaths.ContainsKey($fullPath)) {
        Write-Host "Already open; skipped: $fullPath"
        continue
    }

    $window = $dte.ItemOperations.OpenFile($fullPath)
    Start-Sleep -Milliseconds $DocumentWarmupDelayMilliseconds
    $window.Document.Close(2) # vsSaveChangesNo
    Write-Host "Warmed up and closed: $fullPath"
}
