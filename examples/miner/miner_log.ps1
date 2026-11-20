# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Summarizes the newest miner log in build/miner/logs: shares accepted and rejected, the last difficulty
# changes, and the last status lines.
#
#     examples/miner/miner_log.ps1
#
# The script opens the log with shared read and write access. Thus it can read the log while the miner writes it.

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$top = Split-Path -Parent (Split-Path -Parent $here)
$logs = Join-Path $top "build\miner\logs"

$newest = Get-ChildItem -Path $logs -Filter "miner_*.log" -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime | Select-Object -Last 1
if (-not $newest) {
    Write-Warning "No miner log in $logs"
    exit 1
}

$stream = [System.IO.File]::Open($newest.FullName, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read,
    [System.IO.FileShare]::ReadWrite)
$reader = [System.IO.StreamReader]::new($stream)
$content = $reader.ReadToEnd()
$reader.Close()
$stream.Dispose()

# Status lines end in a carriage return, and the next status line overwrites them on a console. In a file they
# sit on one line. Thus the split takes both line ends.
$lines = $content -split "`r`n|`r|`n"

Write-Host "=== $($newest.Name) ===" -ForegroundColor Cyan
Write-Host ("shares accepted: " + @($lines | Where-Object { $_ -match "\[SHARE\] accepted" }).Count) -ForegroundColor Green
Write-Host ("shares rejected: " + @($lines | Where-Object { $_ -match "\[SHARE\] REJECTED" }).Count) -ForegroundColor Yellow
Write-Host ("blocks:          " + @($lines | Where-Object { $_ -match "\[BLOCK\]" }).Count) -ForegroundColor Green

Write-Host "`n--- difficulty ---" -ForegroundColor Cyan
$lines | Where-Object { $_ -match "\[DIFF\]|suggested difficulty" } | Select-Object -Last 5

Write-Host "`n--- status ---" -ForegroundColor Cyan
$lines | Where-Object { $_ -match "^\[\d+s\]" } | Select-Object -Last 5
