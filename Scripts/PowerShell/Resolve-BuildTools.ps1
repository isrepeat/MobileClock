$utf8Encoding = [System.Text.UTF8Encoding]::new($false)
[Console]::InputEncoding = $utf8Encoding
[Console]::OutputEncoding = $utf8Encoding
$OutputEncoding = $utf8Encoding

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

function Resolve-MobileClockJavaHome {
    $candidateRoots = @(
        $env:JAVA_HOME,
        $env:JDK_HOME,
        (Join-Path $env:ProgramFiles 'Android\Android Studio\jbr'),
        (Join-Path $env:ProgramFiles 'Android\openjdk'),
        (Join-Path $env:ProgramFiles 'Java')
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }

    foreach ($candidateRoot in $candidateRoots) {
        $homes = @($candidateRoot)
        if (Test-Path -LiteralPath $candidateRoot -PathType Container) {
            $homes += Get-ChildItem -LiteralPath $candidateRoot -Directory -ErrorAction SilentlyContinue |
                Sort-Object Name -Descending |
                ForEach-Object FullName
        }
        foreach ($javaHomeCandidate in $homes) {
            if (Test-Path -LiteralPath (Join-Path $javaHomeCandidate 'bin\java.exe') -PathType Leaf) {
                return $javaHomeCandidate
            }
        }
    }

    throw 'Java JDK was not found. Set JAVA_HOME or install Android Studio / Android OpenJDK.'
}

function Resolve-MobileClockAndroidSdk {
    $candidateRoots = @(
        $env:ANDROID_HOME,
        $env:ANDROID_SDK_ROOT,
        (Join-Path $env:LOCALAPPDATA 'Android\Sdk'),
        (Join-Path ${env:ProgramFiles(x86)} 'Android\android-sdk'),
        (Join-Path $env:ProgramFiles 'Android\Sdk')
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }

    foreach ($candidateRoot in $candidateRoots) {
        if (Test-Path -LiteralPath (Join-Path $candidateRoot 'platform-tools\adb.exe') -PathType Leaf) {
            return $candidateRoot
        }
    }

    throw 'Android SDK was not found. Set ANDROID_HOME or install the Android SDK platform tools.'
}