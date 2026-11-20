# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Builds the standalone device benches into build/bench. Each is one .cu file with its own device SHA-256.
#
# Run utils/bench/bench_device.ps1 for all of them. Name benches to build only those, as in
# utils/bench/bench_device.ps1 bench_scope bench_words. The script builds, and does not run. Run each binary from
# build/bench.
#
# On Windows, nvcc uses cl.exe as its host compiler, and cl.exe is not on a bare PATH. The script imports the
# environment of the Visual Studio build tools first.
#
# These files carry their own device SHA-256, and compute the midstate on the host. They do not link sha256_core.
# The front end of nvcc can crash on the C++ headers of this tree. bench_scope links cuFFT, which ships with the
# toolkit.

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$top = Split-Path -Parent (Split-Path -Parent $here)
$out = Join-Path $top "build\bench"
New-Item -ItemType Directory -Force $out | Out-Null

# Each entry is the source, the binary, and any extra libraries.
$all = [ordered]@{
    "bench_scope"          = @("bench_scope.cu", "bench_scope.exe", @("-lcufft"))
    "bench_narrow"         = @("bench_narrow.cu", "bench_narrow.exe", @())
    "bench_depth_cuda"     = @("bench_depth_cuda.cu", "bench_depth_gpu.exe", @())
    "bench_nature_cuda"    = @("bench_nature_cuda.cu", "bench_nature_gpu.exe", @())
    "bench_sparse"         = @("bench_sparse.cu", "bench_sparse.exe", @())
    "bench_words"          = @("bench_words.cu", "bench_words.exe", @())
    "bench_trap"           = @("bench_trap.cu", "bench_trap.exe", @())
    "bench_precision_cuda" = @("bench_precision_cuda.cu", "bench_precision_cuda.exe", @())
}
$wanted = if ($args.Count -gt 0) { $args } else { $all.Keys }
foreach ($name in $wanted) {
    if (-not $all.Contains($name)) {
        Write-Error "unknown bench '$name'. Known: $($all.Keys -join ', ')"
        exit 1
    }
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

# The benches serve sm_86. A different card needs its own arch. The arch stays explicit, and the script does not
# guess it from the device present.
foreach ($name in $wanted) {
    $source, $binary, $libraries = $all[$name]
    $exe = Join-Path $out $binary
    & nvcc -O3 -arch=sm_86 -o $exe (Join-Path $here $source) @libraries -Xcompiler "/O2 /EHsc"
    if ($LASTEXITCODE -ne 0) { Write-Error "nvcc failed on $source"; exit $LASTEXITCODE }
    Write-Host "[+] built $exe" -ForegroundColor Green
}
