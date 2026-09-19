function Resolve-MobileClockBuildTools {
    param([string]$CMakeExecutable)

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw 'Visual Studio Installer was not found. Install Visual Studio with Desktop development with C++ and CMake tools.'
    }
    $instances = @(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | ConvertFrom-Json)
    if ($instances.Count -eq 0) {
        throw 'Visual Studio C++ tools were not found. Install Desktop development with C++.'
    }
    $instance = $instances[0]
    $major = ([version]$instance.installationVersion).Major
    $generators = @{ 17 = 'Visual Studio 17 2022'; 18 = 'Visual Studio 18 2026' }
    if (-not $generators.ContainsKey($major)) {
        throw "Unsupported Visual Studio version: $($instance.installationVersion)"
    }
    $cmakeRoot = Join-Path $instance.installationPath 'Common7\IDE\CommonExtensions\Microsoft\CMake'
    if (-not $CMakeExecutable) {
        $CMakeExecutable = Join-Path $cmakeRoot 'CMake\bin\cmake.exe'
    }
    if (-not (Test-Path -LiteralPath $CMakeExecutable)) {
        throw "CMake was not found at $CMakeExecutable. Install C++ CMake tools for Windows."
    }
    return @{ CMake = $CMakeExecutable; Ninja = (Join-Path $cmakeRoot 'Ninja\ninja.exe'); VisualStudio = $instance.installationPath; Generator = $generators[$major] }
}