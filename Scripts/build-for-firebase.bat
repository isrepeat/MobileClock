@echo off
setlocal

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0PowerShell\build-for-firebase.ps1" %*
set "buildExitCode=%ERRORLEVEL%"

echo.
if not "%buildExitCode%"=="0" (
    echo Build or Firebase upload failed with exit code %buildExitCode%.
) else (
    echo APK is ready in Firebase App Distribution.
)
pause
exit /b %buildExitCode%