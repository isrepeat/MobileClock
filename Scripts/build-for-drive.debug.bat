@echo off
setlocal

chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0PowerShell\build-for-drive.ps1" -Configuration Debug %*
set "buildExitCode=%ERRORLEVEL%"

echo.
if not "%buildExitCode%"=="0" (
    echo Debug build failed with exit code %buildExitCode%.
) else (
    echo Debug APK is ready for Google Drive.
)
pause
exit /b %buildExitCode%