@echo off
setlocal

set "SCRIPT_PATH=%~dp0PowerShell\warmup-vs-android-intellisense.ps1"
set "PATH_LIST=%~dp0vs-intellisense-warmup-paths.txt"

if not exist "%SCRIPT_PATH%" (
    echo PowerShell script not found: "%SCRIPT_PATH%"
    exit /b 1
)

if not exist "%PATH_LIST%" (
    echo Warm-up path list not found: "%PATH_LIST%"
    exit /b 1
)

"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_PATH%" -PathList "%PATH_LIST%" %*
exit /b %ERRORLEVEL%
