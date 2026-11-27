#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# The device arm of the renderer against the host arm. The script compiles the host arm as C11 with the device arm
# marked present, and the device arm and the test as CUDA. It links them with the line of MMgr, the platform and the
# job interface of tessera through the device link of nvcc. The program holds each render of the device to the host.
set -u

TEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$TEST/../../../../../.." && pwd)"
RENDER="$TOP/src/cu/engine/render"
source "$TOP/utils/maint/engine/build_stamp.sh"
build_stamp raster_device_test

HOST_FLAGS=()
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        BINARY="$OUT/raster_device_test.exe"
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
        BINARY="$OUT/raster_device_test"
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
# the host arm, its device stubs left out as the device arm is present
EXACT_TYPES_EXTRA=(-I "$RENDER" -DORIOR_RASTER_HAVE_CUDA=1)
exact_types_device_platform_link || exit 1
exact_types_compile "$RENDER/orior_raster.c" "$RENDER/orior_raster_output.c" || exit 1
HOST_OBJECTS=("${EXACT_TYPES_OBJECTS[@]}")
rm -f "$BINARY"
nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -rdc=true -std=c++20 -O2 "${GENCODE[@]}" \
    -I "$RENDER" -DORIOR_RASTER_HAVE_CUDA=1 "${EXACT_TYPES_FLAGS[@]}" -o "$BINARY" \
    "$TEST/raster_device_test.cu" "$RENDER/raster.cu" "$RENDER/raster_entry.cu" \
    "${HOST_OBJECTS[@]}" "${PLATFORM_LINK[@]}"
[ -f "$BINARY" ] || { echo "  build failed: nvcc could not build the test"; exit 1; }

"$BINARY"
STATUS=$?
echo "  raster device test exit $STATUS"
exit "$STATUS"
