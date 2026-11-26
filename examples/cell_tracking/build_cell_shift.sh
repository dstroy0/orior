#!/usr/bin/env bash
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$ROOT/.." && pwd)"
source "$ROOT/maint/build_stamp.sh"
source "$ROOT/../../includes/exact_types/exact_types.sh"
build_stamp cell_shift
rm -f "$OUT/cell_shift.exe"
NO_ROUNDING="$ENGINE/../../../includes/exact_types/integers"

NO_ROUNDING_CU="$ENGINE/../../../includes/exact_types/integers"
MSVC_BIN="$(ls -d "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC"/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
CAP="$(nvidia-smi --query-gpu=compute_cap --format=csv,noheader 2>/dev/null | head -1 | tr -d ' .')"
ARCH="${1:-sm_${CAP:-86}}"

if command -v nvcc >/dev/null 2>&1 && [ -n "$MSVC_BIN" ] && [ -n "$CAP" ]; then
    echo "  device engine, $ARCH"
    HOST_FLAGS=(-ccbin "$MSVC_BIN" -Xcompiler /Zc:preprocessor)
    EXACT_TYPES_EXTRA=(-DORIOR_EXACT_HAVE_CUDA=1)
    GENCODE=(-gencode "arch=compute_${ARCH#sm_},code=${ARCH}")
    # The .cu reaches the tables of MMgr in each pass of nvcc. The line, the cellblock and the device hooks compile as
    # CUDA (exact_types_device_platform_link), and nvcc links the program with its device link. The exact integers and
    # the C of the program compile as C11, each one time for its words
    exact_types_device_platform_link || exit 1
    exact_types_compile "${EXACT_TYPES_INTEGERS[@]}" "$ROOT/src/cell_shift.c" "$NO_ROUNDING/arm.c" || exit 1
    nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -std=c++20 -O2 -rdc=true "${GENCODE[@]}" \
        -I "$NO_ROUNDING" "${EXACT_TYPES_FLAGS[@]}" \
        -DORIOR_EXACT_HAVE_CUDA=1 \
        -o "$OUT/cell_shift.exe" \
        "$NO_ROUNDING_CU/arm.cu" \
        "${EXACT_TYPES_OBJECTS[@]}" "${PLATFORM_LINK[@]}"
    STATUS=$?
else
    echo "  portable engine only: no nvcc, no host compiler, or no device"
    cc "${FILE_DEFS_LTO_CC[@]}" -O2 -std=c11 -I "$NO_ROUNDING" "${EXACT_TYPES_FLAGS[@]}" \
        -o "$OUT/cell_shift.exe" \
        "$ROOT/src/cell_shift.c" \
        "${EXACT_TYPES_ARITHMETIC[@]}" \
        "$NO_ROUNDING/arm.c" "${EXACT_TYPES_LIBRARIES[@]}"
    STATUS=$?
fi

if [ $STATUS -ne 0 ] || [ ! -f "$OUT/cell_shift.exe" ]; then
    echo "  build failed with status $STATUS"
    exit 1
fi
echo "  built $OUT/cell_shift.exe"
build_publish "$OUT/cell_shift.exe" || exit 1
