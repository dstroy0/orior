# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Builds bench_sac into build/bench and runs it there. The bench writes its tables into the directory that it runs
# in. The readers under examples/00_blob_viz_tools, utils/maint/check and utils/maint/radar open them there.
#
#     utils/bench/bench_sac.ps1                       builds, then runs 18 45 64 shadow
#     utils/bench/bench_sac.ps1 18 45 64 sources      builds, then runs the arm that the arguments name
#
# On Windows, nvcc uses cl.exe as its host compiler, and cl.exe is not on a bare PATH. The script imports the
# environment of the Visual Studio build tools first. Without it, nvcc stops with "Cannot find compiler 'cl.exe' in
# PATH". That reads as a CUDA install that is not there, and it is an environment that is not there.

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

# The bench serves sm_86. A different card needs its own arch, and nvcc says so plainly. The arch stays explicit,
# and the script does not guess it from the device present.
$source = Join-Path $here "bench_sac.cu"
$exe = Join-Path $out "bench_sac.exe"
& nvcc -O3 -arch=sm_86 -o $exe $source -Xcompiler "/O2 /EHsc"
if ($LASTEXITCODE -ne 0) { Write-Error "nvcc failed"; exit $LASTEXITCODE }

$arguments = if ($args.Count -gt 0) { $args } else { @("18", "45", "64", "shadow") }
Push-Location $out
try {
    & $exe @arguments
    $code = $LASTEXITCODE
}
finally {
    Pop-Location
}
exit $code
