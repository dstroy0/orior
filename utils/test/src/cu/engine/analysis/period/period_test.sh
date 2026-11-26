#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
set -u

TEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$TEST/../../../../../../.." && pwd)"
MODULE="$TOP/src/cu/engine/analysis/period"
MODULE_CU="$TOP/src/cu/engine/analysis/period"
DEVICE_POOL="$TOP/src/cu/engine/runtime/device_pool"
DEVICE_POOL_CU="$TOP/src/cu/engine/runtime/device_pool"
NO_ROUNDING="$TOP/includes/exact_types/integers"
source "$TOP/includes/file_defs/file_defs.sh"
source "$TOP/includes/exact_types/exact_types.sh"
source "$TOP/utils/maint/engine/build_stamp.sh"
source "$TOP/utils/maint/engine/tessera_build.sh"
build_stamp period_test

HOST_FLAGS=()
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        BINARY="$OUT/period_test.exe"
        MSVC_BIN="$(ls -d "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC"/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
        if [ -z "$MSVC_BIN" ]; then
            MSVC_BIN="$(ls -d "/c/Program Files/Microsoft Visual Studio"/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
        fi
        if [ -z "$MSVC_BIN" ]; then
            echo "  no host compiler nvcc accepts on this platform was found."
            exit 1
        fi
        HOST_FLAGS=(-ccbin "$MSVC_BIN" -Xcompiler /Zc:preprocessor)
        ;;
    *)
        BINARY="$OUT/period_test"
        HOST_FLAGS=(-Xcompiler -fPIC)
        ;;
esac

ARCHES="${*:-}"
if [ -z "$ARCHES" ]; then
    CAP="$(nvidia-smi --query-gpu=compute_cap --format=csv,noheader 2>/dev/null | head -1 | tr -d ' .')"
    ARCHES="sm_${CAP:-86}"
fi
GENCODE=()
for one in $ARCHES; do
    GENCODE+=(-gencode "arch=compute_${one#sm_},code=${one}")
done

INCLUDES=(-I "$TOP/src/cu/engine" -I "$MODULE" -I "$MODULE_CU" -I "$DEVICE_POOL" -I "$NO_ROUNDING" "${FILE_DEFS_MMGR_FLAGS[@]}" "${TESSERA_INCLUDES[@]}")
rm -f "$BINARY"
# The sources that the program links, each compiled one time for these words (shared_objects.sh). The line of
# MMgr, the cellblock and the device hooks compile as CUDA (exact_types_device_line), and the tessera daemon
# links them too
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        SHARED_WORDS=(nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC[@]}" -Xcompiler "/std:c11 /O2" "${INCLUDES[@]}") ;;
    *)
        SHARED_WORDS=(cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -fPIC "${INCLUDES[@]}") ;;
esac
exact_types_device_line || exit 1
LINEA_OBJECTS=("${EXACT_TYPES_DEVICE_LINE[@]}")
shared_objects_built SHARED_WORDS "${EXACT_TYPES_INTEGERS[@]}" "${EXACT_TYPES_PLATFORM[@]}" || exit 1
OBJECTS=("${LINEA_OBJECTS[@]}" "${SHARED_OBJECTS[@]}")
# the test is a job on the device's tessera daemon, built beside it
tessera_build "${LINEA_OBJECTS[@]}" || exit 1

nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -std=c++20 -O2 -rdc=true "${GENCODE[@]}" "${INCLUDES[@]}" -o "$BINARY" \
    "$TEST"/period_test_{reference,main}.cu "$TOP/src/sims/cu/sim_job.cu" "$MODULE_CU"/period_{measure,select}.cu \
    "$DEVICE_POOL_CU/device_pool.cu" \
    "${OBJECTS[@]}" "${TESSERA_OBJECTS[@]}" "${TESSERA_SEAL[@]}"
[ -f "$BINARY" ] || { echo "  build failed: nvcc could not build the test"; exit 1; }
if [ "${BUILD_ONLY:-0}" = "1" ]; then
    echo "  built $BINARY"
    exit 0
fi

"$BINARY"
STATUS=$?
echo "  period test exit $STATUS"
exit "$STATUS"
