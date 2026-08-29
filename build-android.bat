@echo off
setlocal

rem Double-click launcher for the PowerShell build pipeline.
rem All optional arguments are forwarded, for example:
rem   build-android.bat -NativeOnly
rem   build-android.bat -UploadFirebase
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-android.ps1" %*
set "buildExitCode=%ERRORLEVEL%"

echo.
if not "%buildExitCode%"=="0" (
    echo Build failed with exit code %buildExitCode%.
) else (
    echo Build finished successfully.
)
pause
exit /b %buildExitCode%
