#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# The path of the entropy history on the device, with the engine linked into the program. The script builds the
# engine into the folder of the test, and links its objects into the program with the process stage of the platform.
# The program holds the history and the cloud to the counts of the host and to the signum of the device path.
set -u

TEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$TEST/../../../../../../.." && pwd)"
source "$TOP/utils/maint/engine/build_stamp.sh"
build_stamp entropy_history_paths_test

HOST_FLAGS=()
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        BINARY="$OUT/entropy_history_paths_test.exe"
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
        BINARY="$OUT/entropy_history_paths_test"
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

# the engine, built into a folder of the test. Its list of objects names what the program links
BUILD_OUT="$OUT/engine" bash "$TOP/src/build_engine.sh" $ARCHES > "$OUT/engine_build.txt" 2>&1 ||
    { tail -20 "$OUT/engine_build.txt"; echo "  build failed: the engine did not build"; exit 1; }
OBJECTS_LIST="$OUT/engine/cell_tracking_engine.objects"
[ -f "$OBJECTS_LIST" ] || { echo "  build failed: the engine wrote no list of objects"; exit 1; }
ENGINE_OBJECTS=()
while IFS= read -r object; do
    [ -n "$object" ] && ENGINE_OBJECTS+=("$object")
done < "$OBJECTS_LIST"

source "$TOP/includes/exact_types/exact_types.sh"
# the process stage of the platform: main, which calls program_main. The engine holds the rest of the platform
exact_types_compile "$TOP/src/platform/platform_process.c" || exit 1
PROCESS_OBJECTS=("${EXACT_TYPES_OBJECTS[@]}")
INCLUDES=(-I "$TOP/src/cu/engine" -I "$TOP/includes/codecs/crc" "${FILE_DEFS_MMGR_FLAGS[@]}")
rm -f "$BINARY"
nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -rdc=true -std=c++20 -O2 "${GENCODE[@]}" "${INCLUDES[@]}" -o "$BINARY" \
    "$TEST/entropy_history_paths_test.cu" "${ENGINE_OBJECTS[@]}" "${PROCESS_OBJECTS[@]}" "${EXACT_TYPES_LIBRARIES[@]}"
[ -f "$BINARY" ] || { echo "  build failed: nvcc could not build the test"; exit 1; }


"$BINARY"
STATUS=$?
echo "  entropy history paths test exit $STATUS"
exit "$STATUS"
