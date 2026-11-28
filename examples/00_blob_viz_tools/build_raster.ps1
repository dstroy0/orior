# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Builds the boundary raster and runs it in build/bench. The run writes boundary_raster.ppm, the image, and
# boundary_raster.bin, the dump that `examples/proofing/raster_check.py` grades.
#
#     examples/00_blob_viz_tools/build_raster.ps1
#
# The raster is a program of the platform, and build_tool.sh links it with the platform and the line of MMgr, as the
# other programs of the tree link. This script runs that build through bash.

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$top = Split-Path -Parent (Split-Path -Parent $here)
$out = Join-Path $top "build\bench"
New-Item -ItemType Directory -Force $out | Out-Null

# Git's bash, beside git, which reads a Windows path; the bash on the PATH can be the one of WSL, which does not
$bash = Join-Path (Split-Path -Parent (Split-Path -Parent (Get-Command git).Source)) "bin\bash.exe"
$built = & $bash ((Join-Path $here "build_tool.sh") -replace "\\", "/") boundary_raster | Select-Object -Last 1
if ($LASTEXITCODE -ne 0) { Write-Error "the build of the raster failed"; exit $LASTEXITCODE }

$exe = Join-Path $out "boundary_raster.exe"
Copy-Item -Force $built $exe
Push-Location $out
try {
    & $exe
    $code = $LASTEXITCODE
}
finally {
    Pop-Location
}
exit $code
