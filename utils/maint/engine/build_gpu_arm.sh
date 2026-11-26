#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Build the CUDA arm and grade it against the portable one.
#
#   bash utils/maint/engine/build_gpu_arm.sh [--limbs N] [--digits N] [--positions N] [sm_XX ...]
#
#   - `--limbs N` is the exact width in 32 bit limbs, a power of two from 1 to 32768. The default is
#     the header's 128. exact_integer.h errors on any other value at compile time.
#   - `--digits N` is the decimal digit floor, which a width below 4096 bits needs.
#   - `--positions N` is the positions in the planted run. The default is the bench's 4096. At 32768
#     limbs one position is 128 KiB on the host and twice that on the device.
#
# `nvcc` needs a host compiler, and on Windows that host compiler is MSVC, never MinGW. The rest of
# this tree builds with MinGW, and MinGW objects do not link against MSVC objects. The GPU arm
# gets its own build and does not join the CMake one. The host compiler of nvcc compiles all that
# it needs here. That keeps the ABI the same inside this binary.
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

WIDTH_DEFINES=""
POSITIONS=""
ARCHES=""
while [ "$#" -gt 0 ]; do
    case "$1" in
        --limbs)
            [ "$#" -ge 2 ] || { echo "  --limbs needs a value"; exit 1; }
            WIDTH_DEFINES="$WIDTH_DEFINES -DORIOR_EXACT_LIMBS=${2}u"
            shift 2
            ;;
        --digits)
            [ "$#" -ge 2 ] || { echo "  --digits needs a value"; exit 1; }
            WIDTH_DEFINES="$WIDTH_DEFINES -DORIOR_EXACT_DIGITS=${2}u"
            shift 2
            ;;
        --positions)
            [ "$#" -ge 2 ] || { echo "  --positions needs a value"; exit 1; }
            POSITIONS="$2"
            shift 2
            ;;
        *)
            ARCHES="$ARCHES $1"
            shift
            ;;
    esac
done

if [ -z "$ARCHES" ]; then
    CAP="$(nvidia-smi --query-gpu=compute_cap --format=csv,noheader 2>/dev/null | head -1 | tr -d ' .')"
    ARCHES="sm_${CAP:-86}"
fi
echo "  architectures: $ARCHES"

GENCODE=()
for one in $ARCHES; do
    GENCODE+=(-gencode "arch=compute_${one#sm_},code=${one}")
done

# The script deletes the binary before the build. A binary of an earlier build must not outlast a
# compile that fails. The bench would then run it as though it were this one, and the test below
# would pass on its file.
rm -f "$OUT/bench_exact_gpu.exe"

# PIPESTATUS and not $?, as the pipe into grep would otherwise give grep's status. Grep succeeds
# whatever nvcc did. A filter on the output must never decide whether the build passed.
#
# WIDTH_WORDS holds the defines of the width as words, split where WIDTH_DEFINES has spaces. WIDTH_DEFINES has no
# quotes there. An empty WIDTH_DEFINES then gives no word at all, where quotes would give nvcc an empty one.
# shellcheck disable=SC2206
WIDTH_WORDS=($WIDTH_DEFINES)
source "$ROOT/includes/exact_types/exact_types.sh"
HOST_FLAGS=(-ccbin "$MSVC_BIN" -Xcompiler /Zc:preprocessor)
EXACT_TYPES_EXTRA=(-DORIOR_EXACT_HAVE_CUDA=1 "${WIDTH_WORDS[@]}")
# The .cu reaches the tables of MMgr in each pass of nvcc. The line, the cellblock and the device hooks compile as
# CUDA (exact_types_device_platform_link), and nvcc links the bench with its device link. The exact integers and
# the C of the arms compile as C11, each one time for its words
exact_types_device_platform_link || exit 1
exact_types_compile "${EXACT_TYPES_INTEGERS[@]}" "$ROOT/includes/exact_types/integers/arm.c" \
    "$ROOT/utils/bench/bench_exact_arms.c" || exit 1
nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -std=c++20 -O2 -rdc=true "${GENCODE[@]}" \
    "${EXACT_TYPES_FLAGS[@]}" \
    -DORIOR_EXACT_HAVE_CUDA=1 "${WIDTH_WORDS[@]}" \
    -o "$OUT/bench_exact_gpu.exe" \
    "$ROOT/includes/exact_types/integers/arm.cu" \
    "${EXACT_TYPES_OBJECTS[@]}" "${PLATFORM_LINK[@]}" \
    2>&1 | grep -viE "^\s*$|Copyright|Microsoft \(R\)|exact_integer\.c$|arm\.c$|bench_exact_arms\.c$|arm_cuda\.cu$" | head -20
NVCC_STATUS=${PIPESTATUS[0]}

# Both conditions, as each one alone passes a build that failed. A status of zero with no file is
# a linker that wrote nothing. A file with a status other than zero is a stale binary. Neither is
# a build.
if [ "$NVCC_STATUS" -ne 0 ] || [ ! -f "$OUT/bench_exact_gpu.exe" ]; then
    echo "  build failed: nvcc exited $NVCC_STATUS"
    exit 1
fi

echo "  built $OUT/bench_exact_gpu.exe"
# shellcheck disable=SC2086
"$OUT/bench_exact_gpu.exe" $POSITIONS
