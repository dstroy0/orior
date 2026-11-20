# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Reads the miner configuration for the run scripts. Dot-source this file. It defines Read-MinerConfig and
# Get-MinerArguments.
#
# The configuration is examples/miner/miner.json. Git ignores that file, because it holds a payout address or
# a pool account name. Copy miner.example.json to miner.json and fill it in. The fields:
#
# Set version to v1 for btc_miner.exe, or to v2 for btc_miner2.exe.
# Set pool and port to the stratum host and port.
# Set address to the payout address, for a pool that pays an address (ckpool, Ocean).
# Set user to the account worker name, for an account pool (Braiins).
#
# Set difficulty to the difficulty to suggest to the pool, or 0 for none.
# Set threads to the count of CPU worker threads, or 0 for all cores but one.
# Set normal_priority to true to run at normal priority, and not below normal.
# Set no_gpu to true to run only the eight-lane CPU arm.
#
# Set dry_run to true to subscribe and parse only, with no authorize, mine or submit.
# Set restart to true to start the miner again each time that it exits.
# Set cache to a directory for a copy of the binary, such as a RAM disk, or leave it empty.
#
# A pool that pays an address needs address. An account pool needs user, and must not get the address.

$script:MinerHere = Split-Path -Parent $MyInvocation.MyCommand.Path
$script:MinerTop = Split-Path -Parent (Split-Path -Parent $script:MinerHere)

function Read-MinerConfig {
    $path = Join-Path $script:MinerHere "miner.json"
    if (-not (Test-Path $path)) {
        throw "No $path. Copy miner.example.json to miner.json and fill it in."
    }
    $config = Get-Content -Raw -Path $path | ConvertFrom-Json -AsHashtable
    $example = Get-Content -Raw -Path (Join-Path $script:MinerHere "miner.example.json") | ConvertFrom-Json -AsHashtable
    foreach ($key in $example.Keys) {
        if (-not $config.ContainsKey($key)) { $config[$key] = $example[$key] }
    }
    if ($config.version -notin @("v1", "v2")) {
        throw "version in $path is '$($config.version)'. Use v1 or v2."
    }
    if ((-not $config.address) -and (-not $config.user) -and (-not $config.dry_run)) {
        throw "Neither address nor user is set in $path. The miner refuses to mine without one."
    }
    $name = if ($config.version -eq "v2") { "btc_miner2.exe" } else { "btc_miner.exe" }
    $config.exe = Join-Path $script:MinerTop "build\miner\$name"
    if (-not (Test-Path $config.exe)) {
        throw "No $($config.exe). Build it first with examples/miner/build_miner.ps1 $($config.version)."
    }
    $config.logs = Join-Path $script:MinerTop "build\miner\logs"
    $config.pid_file = Join-Path $script:MinerTop "build\miner\miner.pid"
    return $config
}

function Get-MinerArguments([hashtable] $config) {
    $arguments = @("--pool", [string]$config.pool, "--port", [string]$config.port)
    if ($config.address) { $arguments += @("--address", [string]$config.address) }
    if ($config.user) { $arguments += @("--user", [string]$config.user) }
    if ($config.difficulty -gt 0) { $arguments += @("--difficulty", [string]$config.difficulty) }
    if ($config.threads -gt 0) { $arguments += @("--threads", [string]$config.threads) }
    if ($config.normal_priority) { $arguments += "--normal-priority" }
    if ($config.no_gpu) { $arguments += "--no-gpu" }
    if ($config.dry_run) { $arguments += "--dry-run" }
    return , $arguments
}
