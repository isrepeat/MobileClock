[CmdletBinding()]
param(
    [switch]$KeepVersion
)

$ErrorActionPreference = 'Stop'
$utf8Encoding = [System.Text.UTF8Encoding]::new($false)
[Console]::InputEncoding = $utf8Encoding
[Console]::OutputEncoding = $utf8Encoding
$OutputEncoding = $utf8Encoding

$buildAndDistribute = Join-Path $PSScriptRoot 'build-and-distribute.ps1'
& $buildAndDistribute -Destination All -KeepVersion:$KeepVersion
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}