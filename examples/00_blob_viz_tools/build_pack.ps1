# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Builds the surface packer for the device, and puts pack_shapes.exe beside this script, where gpu_pack.py runs it.
#
# The packer is a program of the platform, and build_tool.sh links it with the platform and the line of MMgr, as the
# other programs of the tree link. This script runs that build through bash.

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path

# Git's bash, beside git, which reads a Windows path; the bash on the PATH can be the one of WSL, which does not
$bash = Join-Path (Split-Path -Parent (Split-Path -Parent (Get-Command git).Source)) "bin\bash.exe"
$built = & $bash ((Join-Path $here "build_tool.sh") -replace "\\", "/") pack_shapes | Select-Object -Last 1
if ($LASTEXITCODE -ne 0) { Write-Error "the build of the packer failed"; exit $LASTEXITCODE }

$out = Join-Path $here "pack_shapes.exe"
Copy-Item -Force $built $out
Write-Output $out
