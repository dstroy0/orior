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
# On Windows, nvcc uses cl.exe as its host compiler, and cl.exe is not on a bare PATH. The script imports
# the environment of the Visual Studio build tools first.
#
# Pass --no-gpu at run time to fall back to the eight-lane arm without a new build.

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$top = Split-Path -Parent (Split-Path -Parent $here)
$out = Join-Path $top "build\miner"
New-Item -ItemType Directory -Force $out | Out-Null

$version = if ($args.Count -gt 0) { $args[0] } else { "v1" }
if ($version -eq "v1") {
    $device = Join-Path $here "device"
    $host_source = Join-Path $here "host\btc_miner.cpp"
    $exe = Join-Path $out "btc_miner.exe"
}
elseif ($version -eq "v2") {
    $device = Join-Path $here "device\v2"
    $host_source = Join-Path $here "host\v2\btc_miner.cpp"
    $exe = Join-Path $out "btc_miner2.exe"
}
else {
    Write-Error "unknown version '$version'. Use v1 or v2."
    exit 1
}

$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
    $vcvars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
}
if (-not (Test-Path $vcvars)) {
    Write-Error "vcvars64.bat not found. Install the VC++ build tools workload."
    exit 1
}

$dump = cmd /c "`"$vcvars`" >nul 2>&1 && set"
foreach ($line in $dump) {
    if ($line -match "^([^=]+)=(.*)$") {
        Set-Item -Path ("Env:" + $matches[1]) -Value $matches[2]
    }
}

$cl = Get-Command cl.exe -ErrorAction SilentlyContinue
if (-not $cl) { Write-Error "cl.exe still not on PATH after vcvars"; exit 1 }

# The client serves sm_86, an RTX 3070. A different card needs its own arch. The arch stays explicit, and
# the script does not guess it from the device present.
#
# ws2_32 serves the stratum socket. The AVX2 flag still matters. Worker zero drives the device, and every
# other worker runs the eight-lane arm. Thus both engines run in one binary.
$core = Join-Path $here "core"
$format = Join-Path $here "format"
& nvcc -O3 -arch=sm_86 -std=c++17 -Xcompiler "/O2 /arch:AVX2 /EHsc" `
    -I $core -I $device -I $format -o $exe `
    $host_source (Join-Path $device "cuda_miner.cu") (Join-Path $core "sha256_core.c") -lws2_32
if ($LASTEXITCODE -ne 0) { Write-Error "nvcc failed on $exe"; exit $LASTEXITCODE }

Write-Host "[+] built $exe with the device arm" -ForegroundColor Green
