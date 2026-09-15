@echo off
setlocal

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0PowerShell\build-for-drive.ps1" %*
set "buildExitCode=%ERRORLEVEL%"

echo.
if not "%buildExitCode%"=="0" (
    echo Build failed with exit code %buildExitCode%.
) else (
    echo APK is ready for Google Drive.
)
pause
exit /b %buildExitCode%