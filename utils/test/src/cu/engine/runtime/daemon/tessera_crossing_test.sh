#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# One job through the job interface of tessera (tessera_job.h): the values cross in, one cooperative launch runs the
# job, and the answer crosses out for the host to agree with.
set -u

TEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$TEST/../../../../../../.." && pwd)"
source "$TOP/utils/maint/engine/build_stamp.sh"
build_stamp tessera_crossing_test

HOST_FLAGS=()
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        BINARY="$OUT/tessera_crossing_test.exe"
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
        BINARY="$OUT/tessera_crossing_test"
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

rm -f "$BINARY"
# the test is a program of the platform. Its kernels take their cells from the pool of the device, which the device
# line of the platform holds (exact_types_device_platform_link)
source "$TOP/includes/exact_types/exact_types.sh"
exact_types_device_platform_link || exit 1
nvcc "${FILE_DEFS_LTO_NVCC_LINK[@]}" -rdc=true -std=c++20 -O2 "${HOST_FLAGS[@]}" "${GENCODE[@]}" "${EXACT_TYPES_FLAGS[@]}" \
    -o "$BINARY" "$TEST/tessera_crossing_test.cu" "$TOP/src/cu/engine/runtime/daemon/tessera_job.cu" "${PLATFORM_LINK[@]}"
[ -f "$BINARY" ] || { echo "  build failed: nvcc could not build the test"; exit 1; }

"$BINARY"
STATUS=$?
echo "  tessera crossing test exit $STATUS"
exit "$STATUS"
