@echo off
setlocal
set "repository_root=%~dp0.."
wscript.exe "%~dp0PowerShell\Start-TrailingWhitespaceWatcher.vbs" "%repository_root%"
exit