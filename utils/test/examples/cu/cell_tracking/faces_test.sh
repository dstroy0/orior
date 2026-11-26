#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
set -u

TEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$TEST/../../../../../examples" && pwd)"
FACES="$TOP/cell_tracking/src/faces"
SORT="$TOP/cell_tracking/src/sort"
SCAN="$TOP/cell_tracking/src/scan"
PEAKS="$TOP/cell_tracking/src/peaks"
source "$TOP/cell_tracking/maint/build_stamp.sh"
build_stamp faces_test

HOST_FLAGS=()
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        BINARY="$OUT/faces_test.exe"
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
        BINARY="$OUT/faces_test"
        HOST_FLAGS=(-Xcompiler -fPIC)
        ;;
esac

INCLUDES=(-I "$ENGINE" -I "$ENGINE/../../cu/engine" -I "$FACES" -I "$SORT" -I "$SCAN" -I "$PEAKS" -I "$TOP/../includes/codecs/crc")
rm -f "$BINARY"
# the test is host work: the part reads and writes files and asks nothing of the device. It is no job
# the test is a program of the platform, and links MMgr's line with the platform (exact_types_platform_link)
source "$TOP/../includes/exact_types/exact_types.sh"
exact_types_device_platform_link || exit 1
nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -rdc=true -std=c++20 -O2 "${INCLUDES[@]}" "${FILE_DEFS_MMGR_FLAGS[@]}" -o "$BINARY" "$TEST/faces_test.cu" \
    "$FACES/faces_find.cu" "${PLATFORM_LINK[@]}"
[ -f "$BINARY" ] || { echo "  build failed: nvcc could not build the test"; exit 1; }
if [ "${BUILD_ONLY:-0}" = "1" ]; then
    echo "  built $BINARY"
    exit 0
fi

# the test writes the synthetic sets under the build directory, afresh each run. An error's set holds no .faces that
# a run before left
rm -rf "$OUT/faces_test_set"
mkdir -p "$OUT/faces_test_set"
"$BINARY" "$OUT/faces_test_set"
STATUS=$?
echo "  faces test exit $STATUS"
exit "$STATUS"
