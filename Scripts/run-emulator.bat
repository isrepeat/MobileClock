@echo off
setlocal

rem Starts the emulator, builds the x86_64 APK, installs it, and launches MobileClock.
rem All PowerShell arguments are forwarded, for example:
rem   run-emulator.bat -Clean
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0PowerShell\run-emulator.ps1" %*
set "exitCode=%ERRORLEVEL%"

echo.
if not "%exitCode%"=="0" echo Emulator run failed with exit code %exitCode%.
pause
exit /b %exitCode%
