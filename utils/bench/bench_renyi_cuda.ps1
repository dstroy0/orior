# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Builds bench_renyi with the device count arm, as build/bench/bench_renyi_gpu.exe, and runs it there. The arguments
# go to the bench.
#
# Run utils/bench/bench_renyi_cuda.ps1 with the arguments of bench_renyi.
#
# Only the .cu goes through nvcc. The front end of nvcc crashes on bench_renyi.cpp: "internal error: assertion
# failed: form_constant" out of cudafe++, in a header of MSVC. Thus cl compiles the host half directly, and nvcc
# links it against the device object. MinGW objects do not link against MSVC objects, and nvcc drives cl. Thus the
# whole toolchain here is MSVC.
#
# The host-only build with g++ is the reference, and this build must agree with it. The acceptance test at the end
# of the bench checks that. BENCH_RENYI_HAS_CUDA is the only switch that turns the device path on. Without it, the
# same file builds host-only.

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$top = Split-Path -Parent (Split-Path -Parent $here)
$out = Join-Path $top "build\bench"
New-Item -ItemType Directory -Force $out | Out-Null
$core = Join-Path $top "examples\miner\core"

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

Push-Location $out
try {
    & nvcc -O3 -arch=sm_86 -c (Join-Path $here "bench_renyi_cuda.cu") -o bench_renyi_cuda.obj -Xcompiler "/O2 /EHsc"
    if ($LASTEXITCODE -ne 0) { Write-Error "nvcc failed on the device arm"; exit 1 }

    & cl /nologo /c /O2 /arch:AVX2 /Fo:sha256_core_msvc.obj (Join-Path $core "sha256_core.c")
    if ($LASTEXITCODE -ne 0) { Write-Error "cl failed on the core"; exit 1 }

    & cl /nologo /c /O2 /EHsc /std:c++17 /arch:AVX2 /DBENCH_RENYI_HAS_CUDA=1 "/I$core" "/I$here" `
        /Fo:bench_renyi_msvc.obj (Join-Path $here "bench_renyi.cpp")
    if ($LASTEXITCODE -ne 0) { Write-Error "cl failed on the host arm"; exit 1 }

    & nvcc -arch=sm_86 -o bench_renyi_gpu.exe bench_renyi_msvc.obj sha256_core_msvc.obj bench_renyi_cuda.obj
    if ($LASTEXITCODE -ne 0) { Write-Error "link failed"; exit 1 }

    & (Join-Path $out "bench_renyi_gpu.exe") @args
    $code = $LASTEXITCODE
}
finally {
    Pop-Location
}
exit $code
