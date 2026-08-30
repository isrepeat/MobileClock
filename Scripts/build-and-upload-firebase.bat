@echo off
setlocal

rem Release flow: first persist a higher Android version, then build the ARM64
rem library, package the debug APK, and upload it to Firebase App Distribution.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0PowerShell\bump-version.ps1"
if errorlevel 1 (
    echo Version increase failed. Firebase upload was not started.
    pause
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0PowerShell\build-android.ps1" -UploadFirebase
set "buildExitCode=%ERRORLEVEL%"

echo.
if not "%buildExitCode%"=="0" (
    echo Build or Firebase upload failed with exit code %buildExitCode%.
) else (
    echo Build and Firebase upload finished successfully.
)
pause
exit /b %buildExitCode%
