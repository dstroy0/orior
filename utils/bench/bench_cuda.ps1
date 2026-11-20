# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Builds bench_cuda and bench_keyhole against the v1 device arm of examples/miner into build/bench, and runs one.
#
# Run utils/bench/bench_cuda.ps1 to build both and run bench_cuda.
# Run utils/bench/bench_cuda.ps1 keyhole to build both and run bench_keyhole. The arguments after the
# word keyhole go to bench_keyhole.
#
# On Windows, nvcc uses cl.exe as its host compiler, and cl.exe is not on a bare PATH. The script imports the
# environment of the Visual Studio build tools first.

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$top = Split-Path -Parent (Split-Path -Parent $here)
$out = Join-Path $top "build\bench"
New-Item -ItemType Directory -Force $out | Out-Null

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

# The benches serve sm_86. A different card needs its own arch. The arch stays explicit, and the script does not
# guess it from the device present.
$miner = Join-Path $top "examples\miner"
$core = Join-Path $miner "core"
$device = Join-Path $miner "device"
foreach ($driver in @("bench_cuda", "bench_keyhole")) {
    $exe = Join-Path $out "$driver.exe"
    & nvcc -O3 -arch=sm_86 -std=c++17 -Xcompiler "/O2 /arch:AVX2 /EHsc" -I $core -I $device -o $exe `
        (Join-Path $here "$driver.cpp") (Join-Path $device "cuda_miner.cu") (Join-Path $core "sha256_core.c")
    if ($LASTEXITCODE -ne 0) { Write-Error "nvcc failed on $driver"; exit $LASTEXITCODE }
}

$run = "bench_cuda"
$rest = @()
if (($args.Count -gt 0) -and ($args[0] -eq "keyhole")) {
    $run = "bench_keyhole"
    $rest = @($args | Select-Object -Skip 1)
}
Push-Location $out
try {
    & (Join-Path $out "$run.exe") @rest
    $code = $LASTEXITCODE
}
finally {
    Pop-Location
}
exit $code
