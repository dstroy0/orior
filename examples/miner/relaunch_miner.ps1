# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Stops the miner that run_miner.ps1 started, then starts it again with the settings in miner.json.
#
#     examples/miner/relaunch_miner.ps1
#
# The script stops the process in build/miner/miner.pid. It also stops each miner process that runs from
# build/miner or the cache directory. The restart loop keeps the miner as a child process.

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $here "miner_config.ps1")

$config = Read-MinerConfig

if (Test-Path $config.pid_file) {
    $old = [int](Get-Content $config.pid_file | Select-Object -First 1)
    Stop-Process -Id $old -Force -ErrorAction SilentlyContinue
}

$paths = @($config.exe)
if ($config.cache) { $paths += Join-Path $config.cache (Split-Path -Leaf $config.exe) }
$name = [System.IO.Path]::GetFileNameWithoutExtension($config.exe)
Get-Process -Name $name -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -in $paths } |
    Stop-Process -Force -ErrorAction SilentlyContinue
Write-Output "stopped the previous miner"

& (Join-Path $here "run_miner.ps1")
