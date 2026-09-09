@echo off
setlocal

set "nativeDirectory=%~dp0"
set "vswherePath=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if not exist "%vswherePath%" (
    echo Visual Studio Installer was not found: "%vswherePath%"
    exit /b 1
)

for /f "usebackq delims=" %%I in (`"%vswherePath%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property productPath`) do (
    set "devenvPath=%%I"
)

if not defined devenvPath (
    echo Visual Studio with C++ tools was not found.
    exit /b 1
)

start "" "%devenvPath%" "%nativeDirectory%"