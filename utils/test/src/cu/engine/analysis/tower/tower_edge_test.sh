#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
set -u

TEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$TEST/../../../../../../.." && pwd)"
TOWER="$TOP/src/cu/engine/analysis/tower"
TOWER_CU="$TOP/src/cu/engine/analysis/tower"
DEVICE_POOL="$TOP/src/cu/engine/runtime/device_pool"
DEVICE_POOL_CU="$TOP/src/cu/engine/runtime/device_pool"
source "$TOP/utils/maint/engine/build_stamp.sh"
build_stamp tower_edge_test

HOST_FLAGS=()
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        BINARY="$OUT/tower_edge_test.exe"
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
        BINARY="$OUT/tower_edge_test"
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

source "$TOP/includes/exact_types/exact_types.sh"
# the program is a program of the platform, and links what exact_types_platform_link compiles
exact_types_platform_link tower_edge_test || exit 1
INCLUDES=(-I "$TOP/src/cu/engine" -I "$TOWER" -I "$TOWER_CU" -I "$DEVICE_POOL" "${FILE_DEFS_MMGR_FLAGS[@]}")
rm -f "$BINARY"
nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -std=c++20 -O2 "${GENCODE[@]}" "${INCLUDES[@]}" -o "$BINARY" \
    "$TEST/tower_edge_test.cu" "$TOWER_CU/tower.cu" "$TOWER_CU/tower_record.cu" "$TOWER_CU/tower_run.cu" "$DEVICE_POOL_CU/device_pool.cu" "${PLATFORM_LINK[@]}"
[ -f "$BINARY" ] || { echo "  build failed: nvcc could not build the test"; exit 1; }

"$BINARY"
STATUS=$?
echo "  tower edge test exit $STATUS"
exit "$STATUS"
