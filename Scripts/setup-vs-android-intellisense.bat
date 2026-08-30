@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0PowerShell\setup-vs-android-intellisense.ps1"
exit /b %errorlevel%
