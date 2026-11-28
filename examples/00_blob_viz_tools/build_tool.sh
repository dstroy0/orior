#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# build_tool.sh: one CUDA tool of this folder, `<name>.cu`, built with the platform and MMgr it calls into
# build/<stamp>_<name>, as examples/navier_stokes/run.sh builds a driver. It prints the binary's path on its last line.
#   bash examples/00_blob_viz_tools/build_tool.sh pack_shapes
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../.." && pwd)"
NAME="${1:-}"
[ -n "$NAME" ] && [ -f "$HERE/$NAME.cu" ] || { echo "  usage: build_tool.sh <name>, <name>.cu a tool of $HERE"; exit 2; }
source "$TOP/utils/maint/engine/build_stamp.sh"
build_stamp "$NAME"
source "$TOP/includes/exact_types/exact_types.sh"

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        BINARY="$OUT/$NAME.exe"
        MSVC_BIN="$(ls -d "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC"/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
        if [ -z "$MSVC_BIN" ]; then
            MSVC_BIN="$(ls -d "/c/Program Files/Microsoft Visual Studio"/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
        fi
        [ -n "$MSVC_BIN" ] || { echo "  no host compiler nvcc accepts on this platform was found."; exit 1; }
        HOST_FLAGS=(-ccbin "$MSVC_BIN" -Xcompiler /Zc:preprocessor)
        C_WORDS=(nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC[@]}" -Xcompiler "/std:c11 /O2" "${EXACT_TYPES_FLAGS[@]}")
        ;;
    *)
        BINARY="$OUT/$NAME"
        HOST_FLAGS=(-Xcompiler -fPIC)
        C_WORDS=(cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -fPIC "${EXACT_TYPES_FLAGS[@]}")
        ;;
esac
CU_WORDS=(nvcc "${HOST_FLAGS[@]}" -std=c++20 -O3 -rdc=true "${EXACT_TYPES_FLAGS[@]}")
exact_types_device_line || exit 1
OBJECTS=("${EXACT_TYPES_DEVICE_LINE[@]}")
shared_objects_built C_WORDS "${EXACT_TYPES_PLATFORM[@]}" || exit 1
OBJECTS+=("${SHARED_OBJECTS[@]}")
shared_objects_built CU_WORDS "$HERE/$NAME.cu" || exit 1
OBJECTS+=("${SHARED_OBJECTS[@]}")
rm -f "$BINARY"
nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -rdc=true -O3 -o "$BINARY" "${OBJECTS[@]}"
[ -f "$BINARY" ] || { echo "  build failed: nvcc could not build $NAME"; exit 1; }
echo "$BINARY"
