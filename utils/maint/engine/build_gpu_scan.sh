#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Build the CUDA steering scan arm and grade it against the portable one.
#
#   bash utils/maint/engine/build_gpu_scan.sh [sm_XX ...]
#
# The companion of build_gpu_arm.sh, for the scan family in place of the exact one. `nvcc` needs a
# host compiler, and on Windows that host compiler is MSVC, never MinGW. The rest of this tree
# builds with MinGW, and MinGW objects do not link against MSVC objects. The GPU arm gets its own
# build and does not join the CMake one. The host compiler of nvcc compiles all that it needs here.
# That keeps the ABI the same inside this binary.
#
# The architectures default to the one this machine carries. Naming others compiles for them too.
# So the code compiles, and the assembler takes it, for an architecture that nobody here owns. The
# script does not run the code there.
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT="$ROOT/build/engine_gpu"
mkdir -p "$OUT"

MSVC_BIN="$(ls -d "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC"/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
if [ -z "$MSVC_BIN" ]; then
    MSVC_BIN="$(ls -d "/c/Program Files/Microsoft Visual Studio"/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
fi
if [ -z "$MSVC_BIN" ]; then
    echo "  no MSVC host compiler found. nvcc cannot build on Windows without one."
    exit 1
fi
echo "  host compiler: $MSVC_BIN"

ARCHES="${*:-}"
if [ -z "$ARCHES" ]; then
    CAP="$(nvidia-smi --query-gpu=compute_cap --format=csv,noheader 2>/dev/null | head -1 | tr -d ' .')"
    ARCHES="sm_${CAP:-86}"
fi
echo "  architectures: $ARCHES"

GENCODE=""
for one in $ARCHES; do
    NUM="${one#sm_}"
    GENCODE="$GENCODE -gencode arch=compute_${NUM},code=${one}"
done

# The script deletes the binary before the build. A binary of an earlier build must not outlast a
# compile that fails. The bench would then run it as though it were this one, and the test below
# would pass on its file.
rm -f "$OUT/bench_steer_gpu.exe"

# PIPESTATUS and not $?, as the pipe into grep would otherwise give grep's status. Grep succeeds
# whatever nvcc did. A filter on the output must never decide whether the build passed.
source "$ROOT/includes/exact_types/exact_types.sh"
HOST_FLAGS=(-ccbin "$MSVC_BIN" -Xcompiler /Zc:preprocessor)
exact_types_compile gpu_scan "${EXACT_TYPES_ARITHMETIC[@]}" || exit 1
nvcc -ccbin "$MSVC_BIN" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -O2 $GENCODE \
    -I "$ROOT/src/cu/engine/nbody/orior" \
    "${EXACT_TYPES_FLAGS[@]}" \
    -DORIOR_STEER_HAVE_CUDA=1 \
    -o "$OUT/bench_steer_gpu.exe" \
    "$ROOT/src/cu/engine/nbody/orior/scan.cu" \
    "$ROOT/src/cu/engine/nbody/orior/scan.c" \
    "$ROOT/src/cu/engine/nbody/orior"/orior_{core,steer,field,steer_plan,steer_count}.c \
    "${EXACT_TYPES_OBJECTS[@]}" "${EXACT_TYPES_LIBRARIES[@]}" \
    "$ROOT/includes/exact_types/integers/arm.c" \
    "$ROOT/utils/bench/bench_steer_arms.c" \
    2>&1 | grep -viE "^\s*$|Copyright|Microsoft \(R\)|scan_cuda\.cu$|scan\.c$|orior\.c$|exact_integer\.c$|arm\.c$|bench_steer_arms\.c$" | head -30
NVCC_STATUS=${PIPESTATUS[0]}

# Both conditions, as each one alone passes a build that failed. A status of zero with no file is
# a linker that wrote nothing. A file with a status other than zero is a stale binary. Neither is
# a build.
if [ "$NVCC_STATUS" -ne 0 ] || [ ! -f "$OUT/bench_steer_gpu.exe" ]; then
    echo "  build failed: nvcc exited $NVCC_STATUS"
    exit 1
fi

echo "  built $OUT/bench_steer_gpu.exe"
"$OUT/bench_steer_gpu.exe"
