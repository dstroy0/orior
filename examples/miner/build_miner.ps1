# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Builds the stratum client with the device arm compiled in, into build/miner.
#
# Run examples/miner/build_miner.ps1 for btc_miner.exe from the v1 sources.
# Run examples/miner/build_miner.ps1 v2 for btc_miner2.exe from the v2 sources.
#
# A host build reaches only the eight-lane CPU arm, at 70.81 MH/s against 2059.79 MH/s on the device.
# The link of cuda_miner.cu closes that gap of 29 times. Thus the client is an nvcc build.
#
# The client is a program of the platform, and build_miner.sh links it with the platform and the line of MMgr, as
# the other programs of the tree link. This script runs that build through bash.
#
# Pass --no-gpu at run time to fall back to the eight-lane arm without a new build.

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$version = if ($args.Count -gt 0) { $args[0] } else { "v1" }
if (($version -ne "v1") -and ($version -ne "v2")) {
    Write-Error "unknown version '$version'. Use v1 or v2."
    exit 1
}

& bash (Join-Path $here "build_miner.sh") $version
if ($LASTEXITCODE -ne 0) { Write-Error "the build of the $version client failed"; exit $LASTEXITCODE }

Write-Host "[+] built the $version client with the device arm" -ForegroundColor Green
