[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Container })]
    [string]$Root
)

$scriptPath = Join-Path $PSScriptRoot 'TrailingWhitespaceWatcher.ps1'
$runnerPath = Join-Path $PSScriptRoot 'Run-TrailingWhitespaceWatcher.vbs'
$Root = (Resolve-Path -LiteralPath $Root).Path
$taskName = 'MobileClock Trailing Whitespace Watcher'
$action = New-ScheduledTaskAction -Execute 'wscript.exe' -Argument "`"$runnerPath`" `"$Root`""
$trigger = New-ScheduledTaskTrigger -AtLogOn
$settings = New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -StartWhenAvailable
$principal = New-ScheduledTaskPrincipal -UserId "$env:USERDOMAIN\$env:USERNAME" -LogonType Interactive -RunLevel Limited
Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Settings $settings -Principal $principal -Description 'Keeps MobileClock text files free of terminal whitespace.' -Force
Start-ScheduledTask -TaskName $taskName