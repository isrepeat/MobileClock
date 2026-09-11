@echo off
setlocal

set "parentProcessId=%~1"
if "%parentProcessId%"=="" set "parentProcessId=0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0PowerShell\run-xaml-previewer.ps1" -Configuration Debug -ParentProcessId %parentProcessId%
set "exitCode=%ERRORLEVEL%"
if not "%exitCode%"=="0" (
    echo.
    echo Previewer build failed. Copy the error text above.
    pause
)
exit /b %exitCode%