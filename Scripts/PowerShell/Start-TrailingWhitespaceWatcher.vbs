Option Explicit

Dim fileSystem
Dim shell
Dim root
Dim scriptPath
Dim command

Set fileSystem = CreateObject("Scripting.FileSystemObject")
Set shell = CreateObject("WScript.Shell")
root = fileSystem.GetAbsolutePathName(WScript.Arguments(0))
scriptPath = fileSystem.BuildPath(fileSystem.GetParentFolderName(WScript.ScriptFullName), "Install-TrailingWhitespaceWatcher.ps1")
command = "powershell.exe -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File """ & scriptPath & """ -Root """ & root & """"
shell.Run command, 0, False