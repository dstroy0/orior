# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Starts the miner detached and hidden, with the settings in examples/miner/miner.json. The miner survives the
# shell that starts it.
#
#     examples/miner/run_miner.ps1
#
# Output goes to build/miner/logs/miner_<time>.log, and errors to the .err file of the same name. The process
# id goes to build/miner/miner.pid, where relaunch_miner.ps1 finds it. Where restart is true, a loop starts the
# miner again each time that it exits, and the process id is the id of that loop. Where cache names a directory,
# the script copies the binary there and runs that copy.

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $here "miner_config.ps1")

$config = Read-MinerConfig
$arguments = Get-MinerArguments $config

$exe = $config.exe
if ($config.cache) {
    New-Item -ItemType Directory -Force $config.cache | Out-Null
    $copy = Join-Path $config.cache (Split-Path -Leaf $exe)
    Copy-Item $exe $copy -Force
    $exe = $copy
}

New-Item -ItemType Directory -Force $config.logs | Out-Null
$stamp = Get-Date -Format "yyyyMMdd_HHmmss_fff"
$log = Join-Path $config.logs "miner_$stamp.log"
$err = Join-Path $config.logs "miner_$stamp.err"

if ($config.restart) {
    $loop = @("-NoProfile", "-File", (Join-Path $here "miner_loop.ps1"), $exe) + $arguments
    $process = Start-Process -FilePath "pwsh" -ArgumentList $loop -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput $log -RedirectStandardError $err
}
else {
    $process = Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput $log -RedirectStandardError $err
}
$process.Id | Out-File $config.pid_file
Write-Output ("miner started, pid " + $process.Id + ", log " + $log)
