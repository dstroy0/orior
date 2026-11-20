# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Builds the boundary raster into build/bench and runs it there. The run writes boundary_raster.ppm, the image, and
# boundary_raster.bin, the dump that `examples/proofing/raster_check.py` grades.
#
#     examples/00_blob_viz_tools/build_raster.ps1
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

# The raster serves sm_86. A different card needs its own arch, and nvcc says so plainly.
$source = Join-Path $here "boundary_raster.cu"
$exe = Join-Path $out "boundary_raster.exe"
& nvcc -O3 -arch=sm_86 -o $exe $source
if ($LASTEXITCODE -ne 0) { Write-Error "nvcc failed"; exit $LASTEXITCODE }

Push-Location $out
try {
    & $exe
    $code = $LASTEXITCODE
}
finally {
    Pop-Location
}
exit $code
