#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# The record machine's host oracle, built with no CUDA toolchain where there is none: keymath and key_schedule are
# host code in .cu files, compiled as C++; cycle.c, the exact integer's pieces and MMgr's line as C. On Windows nvcc
# drives MSVC, as the record tests' host objects are built there. Its lines are what two parts compare.
set -u

TEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$TEST/../../../../../../.." && pwd)"
CYCLE="$TOP/src/cu/engine/analysis/cycle"
CYCLE_CU="$TOP/src/cu/engine/analysis/cycle"
KEYMATH="$TOP/src/cu/engine/analysis/keymath"
KEYMATH_CU="$TOP/src/cu/engine/analysis/keymath"
KEY_SCHEDULE="$TOP/src/cu/engine/analysis/key_schedule"
KEY_SCHEDULE_CU="$TOP/src/cu/engine/analysis/key_schedule"
NO_ROUNDING="$TOP/includes/exact_types/integers"
source "$TOP/includes/file_defs/file_defs.sh"
source "$TOP/includes/exact_types/exact_types.sh"
source "$TOP/utils/maint/engine/build_stamp.sh"
build_stamp record_test

INCLUDES=(-I "$TOP/src/cu/engine" -I "$CYCLE" -I "$CYCLE_CU" -I "$KEYMATH" -I "$KEYMATH_CU" -I "$KEY_SCHEDULE" -I "$KEY_SCHEDULE_CU" -I "$NO_ROUNDING" "${FILE_DEFS_MMGR_FLAGS[@]}")
# The words of the C and of the C++, and the link. On Windows nvcc compiles the .cu sources with -x c++, as host C++
# with no device pass, as the C++ compiler compiles them elsewhere
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        BINARY="$OUT/record_test.exe"
        MSVC_BIN="$(ls -d "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC"/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
        if [ -z "$MSVC_BIN" ]; then
            MSVC_BIN="$(ls -d "/c/Program Files/Microsoft Visual Studio"/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
        fi
        [ -n "$MSVC_BIN" ] || { echo "  no host compiler nvcc accepts on this platform was found."; exit 1; }
        C_WORDS=(nvcc -ccbin "$MSVC_BIN" -Xcompiler /Zc:preprocessor "${FILE_DEFS_LTO_NVCC[@]}" -Xcompiler "/std:c11 /O2" "${INCLUDES[@]}")
        CPP_WORDS=(nvcc -ccbin "$MSVC_BIN" -Xcompiler /Zc:preprocessor -std=c++20 -O2 -x c++ "${INCLUDES[@]}")
        link_all()
        {
            nvcc -ccbin "$MSVC_BIN" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -o "$BINARY" "$@"
        }
        ;;
    *)
        BINARY="$OUT/record_test"
        C_WORDS=(cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra "${INCLUDES[@]}")
        CPP_WORDS=("$FILE_DEFS_CXX" "${FILE_DEFS_LTO_CC[@]}" -std=c++20 -O2 -Wall -Wextra "${INCLUDES[@]}" -x c++)
        link_all()
        {
            "$FILE_DEFS_CXX" "${FILE_DEFS_LTO_CC[@]}" -o "$BINARY" "$@"
        }
        ;;
esac

rm -f "$BINARY"
# each source compiled one time for its words (shared_objects.sh)
shared_objects_built C_WORDS "${FILE_DEFS_LINEA[@]}" "${EXACT_TYPES_INTEGERS[@]}" "${EXACT_TYPES_PLATFORM[@]}" \
    "${EXACT_TYPES_CARCER[@]}" "$CYCLE/cycle.c" "$TEST/record_test.c" || exit 1
OBJECTS=("${SHARED_OBJECTS[@]}")
shared_objects_built CPP_WORDS "$KEYMATH_CU/keymath.cu" "$KEY_SCHEDULE_CU/key_schedule.cu" || exit 1
OBJECTS+=("${SHARED_OBJECTS[@]}")
link_all "${OBJECTS[@]}"
[ -f "$BINARY" ] || { echo "  build failed: the test did not link"; exit 1; }

"$BINARY"
STATUS=$?
echo "  record host test exit $STATUS"
exit "$STATUS"
