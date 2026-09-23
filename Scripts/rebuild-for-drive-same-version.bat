@echo off
setlocal

rem Builds a test APK without changing version.properties, then uploads it to Drive.
chcp 65001 >nul
call "%~dp0build-for-drive.bat" -KeepVersion
exit /b %ERRORLEVEL%