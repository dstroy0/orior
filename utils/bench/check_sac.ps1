# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Compiles bench_sac.cu to an object only. Thus a syntax check can run while a measurement holds a lock on
# bench_sac.exe. The object goes to build/bench.
#
# Run utils/bench/check_sac.ps1 with no arguments.

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

$object = Join-Path $out "bench_sac_check.obj"
& nvcc -O1 -arch=sm_86 -c -o $object (Join-Path $here "bench_sac.cu") -Xcompiler "/EHsc"
if ($LASTEXITCODE -ne 0) {
    Write-Host "[!] bench_sac.cu does not compile" -ForegroundColor Red
    exit 1
}
Write-Host "[*] bench_sac.cu compiles" -ForegroundColor Green
