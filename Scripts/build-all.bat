@echo off
setlocal

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0PowerShell\build-all.ps1" %*
set "buildExitCode=%ERRORLEVEL%"

echo.
if not "%buildExitCode%"=="0" (
    echo Build or upload failed with exit code %buildExitCode%.
) else (
    echo APK is ready in Firebase App Distribution and Google Drive.
)
pause
exit /b %buildExitCode%