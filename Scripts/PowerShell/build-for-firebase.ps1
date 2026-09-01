[CmdletBinding()]
param(
    [switch]$KeepVersion
)

$ErrorActionPreference = 'Stop'

$buildAndDistribute = Join-Path $PSScriptRoot 'build-and-distribute.ps1'
& $buildAndDistribute -Destination Firebase -KeepVersion:$KeepVersion
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}