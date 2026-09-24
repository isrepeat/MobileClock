[CmdletBinding()]
param(
    [switch]$KeepVersion,

    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$utf8Encoding = [System.Text.UTF8Encoding]::new($false)
[Console]::InputEncoding = $utf8Encoding
[Console]::OutputEncoding = $utf8Encoding
$OutputEncoding = $utf8Encoding

$buildAndDistribute = Join-Path $PSScriptRoot 'build-and-distribute.ps1'
& $buildAndDistribute -Destination Drive -KeepVersion:$KeepVersion -Configuration $Configuration
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}