$ErrorActionPreference = 'Stop'

$ndkBin = 'C:\Program Files (x86)\Android\AndroidNDK\android-ndk-r27c\toolchains\llvm\prebuilt\windows-x86_64\bin'
$nativeRoot = Join-Path $PSScriptRoot '..\..\app\src\main\cpp'
$driverRoot = Join-Path $nativeRoot 'cmake\vs-android-drivers'

foreach ($architecture in @('arm64', 'x86_64')) {
    $architectureDirectory = Join-Path $driverRoot $architecture

    foreach ($compiler in @('clang.exe', 'clang++.exe')) {
        $source = Join-Path $ndkBin $compiler
        $destination = Join-Path $architectureDirectory $compiler

        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
            throw "NDK compiler not found: $source"
        }

        if (Test-Path -LiteralPath $destination -PathType Leaf) {
            continue
        }

        # Hardlink не копирует 126-МБ бинарник. Разные соседние .cfg задают
        # ARM64/x86_64 target, а basename clang++.exe распознаётся VS 2026.
        New-Item -ItemType HardLink -Path $destination -Target $source | Out-Null
        Write-Host "Created $destination"
    }

    # При скрытом запросе compiler defaults Visual Studio не наследует PATH
    # пресета. Единственная несистемная DLL clang должна лежать рядом с shim.
    $runtimeSource = Join-Path $ndkBin 'libwinpthread-1.dll'
    $runtimeDestination = Join-Path $architectureDirectory 'libwinpthread-1.dll'
    if (-not (Test-Path -LiteralPath $runtimeDestination -PathType Leaf)) {
        New-Item -ItemType HardLink -Path $runtimeDestination -Target $runtimeSource | Out-Null
        Write-Host "Created $runtimeDestination"
    }
}

Write-Host 'Visual Studio Android IntelliSense drivers are ready.'
