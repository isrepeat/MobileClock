@echo off
setlocal

set "projectRoot=%~dp0.."
set "projectFile=%projectRoot%\MobileClock.XamlPreviewer\XamlPreviewer.WPF\XamlPreviewer.WPF.csproj"
set "previewer=%projectRoot%\MobileClock.XamlPreviewer\!VS_TMP\Build\Release\AnyCPU\XamlPreviewer.WPF\XamlPreviewer.exe"
set "msbuild=C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"

if not exist "%msbuild%" (
    echo Visual Studio MSBuild was not found: %msbuild%
    pause
    exit /b 1
)

"%msbuild%" "%projectFile%" /t:Build /p:Configuration=Release /p:Platform=AnyCPU /m
if errorlevel 1 (
    echo XamlPreviewer Release build failed.
    pause
    exit /b 1
)

if not exist "%previewer%" (
    echo XamlPreviewer Release executable was not produced.
    pause
    exit /b 1
)

start "" "%previewer%"