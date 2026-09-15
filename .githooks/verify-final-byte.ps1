$invalid = [System.Collections.Generic.List[string]]::new()
git status --porcelain | ForEach-Object {
    $path = $_.Substring(3)
    if (-not [System.IO.File]::Exists($path)) {
        return
    }
    $bytes = [System.IO.File]::ReadAllBytes($path)
    if ($bytes -contains 0) {
        return
    }
    if ($bytes.Length -eq 0 -or $bytes[$bytes.Length - 1] -in 9, 10, 13, 32) {
        $invalid.Add($path)
    }
}
if ($invalid.Count -gt 0) {
    Write-Error "Final byte must be part of the last content line: $($invalid -join ', ')"
    exit 1
}