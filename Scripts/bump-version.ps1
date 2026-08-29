[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$versionFile = Join-Path (Split-Path -Parent $PSScriptRoot) 'version.properties'

if (-not (Test-Path $versionFile)) {
    throw "Version file not found: $versionFile"
}

$invariantCulture = [System.Globalization.CultureInfo]::InvariantCulture
$lines = Get-Content -LiteralPath $versionFile
$oldCode = $null
$oldName = $null

foreach ($line in $lines) {
    if ($line -match '^\s*VERSION_CODE\s*=\s*(\d+)\s*$') {
        $oldCode = [int]$Matches[1]
    }
    if ($line -match '^\s*VERSION_NAME\s*=\s*([0-9]+(?:\.[0-9]+)?)\s*$') {
        $oldName = [decimal]::Parse($Matches[1], $invariantCulture)
    }
}

if ($null -eq $oldCode -or $null -eq $oldName) {
    throw 'version.properties must define numeric VERSION_CODE and VERSION_NAME values.'
}
if ($oldCode -eq [int]::MaxValue) {
    throw 'VERSION_CODE cannot be increased further.'
}

# Upload releases always move both version fields forward. Decimal is used so
# 0.05 + 0.01 stays exactly 0.06 rather than becoming a floating-point value.
$newCode = $oldCode + 1
$newName = ($oldName + [decimal]'0.01').ToString('0.00', $invariantCulture)

$updatedLines = foreach ($line in $lines) {
    if ($line -match '^\s*VERSION_CODE\s*=') {
        "VERSION_CODE=$newCode"
    } elseif ($line -match '^\s*VERSION_NAME\s*=') {
        "VERSION_NAME=$newName"
    } else {
        $line
    }
}

Set-Content -LiteralPath $versionFile -Value $updatedLines -Encoding utf8
Write-Host "Version increased: $oldCode / $($oldName.ToString('0.00', $invariantCulture)) -> $newCode / $newName"
