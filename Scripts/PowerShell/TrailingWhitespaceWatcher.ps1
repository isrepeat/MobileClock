[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Container })]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$Root = (Resolve-Path -LiteralPath $Root).Path

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Collections.Concurrent;
using System.IO;

public sealed class WatcherEventRecord {
    public string Path;
    public string Kind;
    public DateTime ReceivedAtUtc;
}

public sealed class DirectFileWatcher : IDisposable {
    private readonly ConcurrentQueue<WatcherEventRecord> events = new ConcurrentQueue<WatcherEventRecord>();
    private readonly FileSystemWatcher watcher;

    public DirectFileWatcher(string root) {
        watcher = new FileSystemWatcher(root) {
            IncludeSubdirectories = true,
            NotifyFilter = NotifyFilters.FileName | NotifyFilters.LastWrite | NotifyFilters.Size,
            InternalBufferSize = 65536,
            EnableRaisingEvents = true
        };
        watcher.Changed += OnChanged;
        watcher.Created += OnChanged;
        watcher.Renamed += OnRenamed;
    }

    public bool TryDequeue(out WatcherEventRecord value) {
        return events.TryDequeue(out value);
    }

    public void Dispose() {
        watcher.Dispose();
    }

    private void OnChanged(object sender, FileSystemEventArgs args) {
        events.Enqueue(new WatcherEventRecord { Path = args.FullPath, Kind = args.ChangeType.ToString(), ReceivedAtUtc = DateTime.UtcNow });
    }

    private void OnRenamed(object sender, RenamedEventArgs args) {
        events.Enqueue(new WatcherEventRecord { Path = args.FullPath, Kind = args.ChangeType.ToString(), ReceivedAtUtc = DateTime.UtcNow });
    }
}
'@

$textExtensions = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
@('.bat', '.cmd', '.cmake', '.cpp', '.cs', '.h', '.hpp', '.json', '.kt', '.md', '.ps1', '.properties', '.sln', '.txt', '.vcxproj', '.xaml', '.xml', '.yml', '.yaml') |
    ForEach-Object { [void]$textExtensions.Add($_) }
$ignoredDirectories = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
@('.git', 'Build', 'bin', 'Logs', 'obj') | ForEach-Object { [void]$ignoredDirectories.Add($_) }

$logDirectory = Join-Path $PSScriptRoot 'Logs'
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
$logPath = Join-Path $logDirectory 'trailing-whitespace-watcher.log'

function Write-WatcherLog {
    param([string]$Level, [string]$Message)

    $timestamp = [DateTime]::Now.ToString('yyyy-MM-dd HH:mm:ss.fff')
    Add-Content -LiteralPath $logPath -Value "[$timestamp] [$Level] $Message" -Encoding utf8
}

function Test-WatchedTextFile {
    param([string]$Path)

    if ([System.IO.Path]::GetFileName($Path) -eq 'CMakeLists.txt') {
        return $true
    }
    foreach ($segment in $Path.Split([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)) {
        if ($ignoredDirectories.Contains($segment)) {
            return $false
        }
    }
    return $textExtensions.Contains([System.IO.Path]::GetExtension($Path))
}

function Remove-TerminalWhitespace {
    param([string]$Path, [string]$EventName, [DateTime]$ReceivedAtUtc)

    if (-not (Test-WatchedTextFile $Path) -or -not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return
    }
    try {
        $bytes = [System.IO.File]::ReadAllBytes($Path)
        if ($bytes.Length -eq 0) {
            Write-WatcherLog 'INFO' "$EventName ignored empty file: $Path"
            return
        }
        $length = $bytes.Length
        while ($length -gt 0) {
            $value = [int]$bytes[$length - 1]
            if ($value -ne 9 -and $value -ne 10 -and $value -ne 13 -and $value -ne 32) {
                break
            }
            --$length
        }
        if ($length -eq $bytes.Length) {
            return
        }
        if ($length -eq 0) {
            Write-WatcherLog 'WARN' "$EventName did not modify whitespace-only file: $Path"
            return
        }
        $removed = @($bytes[$length..($bytes.Length - 1)] | ForEach-Object { [int]$_ }) -join ', '
        $prefix = [System.Text.Encoding]::UTF8.GetString($bytes, 0, $length)
        $line = 1 + ([regex]::Matches($prefix, "`n")).Count
        [System.IO.File]::WriteAllBytes($Path, $bytes[0..($length - 1)])
        $delay = [Math]::Round(([DateTime]::UtcNow - $ReceivedAtUtc).TotalMilliseconds)
        Write-WatcherLog 'FIXED' "$EventName changed $Path; line $line; removed terminal byte(s): $removed; event-to-fix delay: ${delay}ms"
    } catch {
        Write-WatcherLog 'ERROR' "${EventName} could not process ${Path}: $($_.Exception.Message)"
    }
}

$watcher = [DirectFileWatcher]::new($Root)
$pending = @{}

$notifyIcon = [System.Windows.Forms.NotifyIcon]::new()
$notifyIcon.Icon = [System.Drawing.SystemIcons]::Shield
$notifyIcon.Text = 'MobileClock: trailing whitespace watcher'
$notifyIcon.Visible = $true
$menu = [System.Windows.Forms.ContextMenuStrip]::new()
$checkItem = $menu.Items.Add('Check all files')
$openLogItem = $menu.Items.Add('Open log')
$exitItem = $menu.Items.Add('Exit')
$notifyIcon.ContextMenuStrip = $menu
$running = $true
$checkItem.add_Click({ Get-ChildItem -LiteralPath $Root -Recurse -File | ForEach-Object { Remove-TerminalWhitespace $_.FullName 'ManualCheck' [DateTime]::UtcNow } })
$openLogItem.add_Click({ Start-Process notepad.exe -ArgumentList $logPath })
$exitItem.add_Click({ $script:running = $false })

Write-WatcherLog 'INFO' "Watcher started for $Root; log: $logPath"
try {
    while ($running) {
        $event = $null
        while ($watcher.TryDequeue([ref]$event)) {
            if (-not (Test-WatchedTextFile $event.Path)) {
                continue
            }
            $pending[$event.Path] = $event
            Write-WatcherLog 'EVENT' "$($event.Kind) received: $($event.Path)"
        }
        foreach ($path in @($pending.Keys)) {
            $event = $pending[$path]
            if (([DateTime]::UtcNow - $event.ReceivedAtUtc).TotalMilliseconds -ge 250) {
                Remove-TerminalWhitespace $path $event.Kind $event.ReceivedAtUtc
                $pending.Remove($path)
            }
        }
        [System.Windows.Forms.Application]::DoEvents()
        Start-Sleep -Milliseconds 25
    }
} finally {
    $watcher.Dispose()
    $notifyIcon.Dispose()
    Write-WatcherLog 'INFO' 'Watcher stopped.'
}