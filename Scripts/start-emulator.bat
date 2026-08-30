@echo off
setlocal

rem Starts (and, on first run, creates) the Android debug emulator.
rem All PowerShell arguments are forwarded, for example:
rem   start-emulator.bat -NoWindow
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0PowerShell\start-emulator.ps1" %*
set "exitCode=%ERRORLEVEL%"

echo.
if not "%exitCode%"=="0" echo Emulator startup failed with exit code %exitCode%.
pause
exit /b %exitCode%
