@echo off
setlocal

rem Builds a test APK without changing version.properties, then uploads it to Drive.
call "%~dp0build-for-drive.bat" -KeepVersion
exit /b %ERRORLEVEL%