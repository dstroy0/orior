#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
set -u

TEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$TEST/../../../../../../.." && pwd)"
MODULE="$TOP/src/cu/engine/analysis/period"
DEVICE_POOL="$TOP/src/cu/engine/runtime/device_pool"
CYCLE="$TOP/src/cu/engine/analysis/cycle"
CODEGEN="$TOP/src/cu/engine/rmc"
LSTAR_PARSER="$TOP/includes/parsers"
KEYMATH="$TOP/src/cu/engine/analysis/keymath"
KEY_SCHEDULE="$TOP/src/cu/engine/analysis/key_schedule"
NO_ROUNDING="$TOP/includes/exact_types/integers"
EXACT_RECORD="$TOP/includes/exact_types/integers/exact_record"
source "$TOP/includes/file_defs/file_defs.sh"
source "$TOP/includes/exact_types/exact_types.sh"
source "$TOP/utils/maint/engine/build_stamp.sh"
source "$TOP/utils/maint/engine/tessera_build.sh"
build_stamp period_record_test

HOST_FLAGS=()
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        BINARY="$OUT/period_record_test.exe"
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
        BINARY="$OUT/period_record_test"
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

INCLUDES=(-I "$TOP/src/cu/engine" -I "$MODULE" -I "$DEVICE_POOL" -I "$CYCLE" -I "$CODEGEN" -I "$LSTAR_PARSER" -I "$KEYMATH"
          -I "$KEY_SCHEDULE" -I "$NO_ROUNDING" -I "$EXACT_RECORD" "${FILE_DEFS_MMGR_FLAGS[@]}" "${TESSERA_INCLUDES[@]}")
rm -f "$BINARY"
# The sources that the program links, each compiled one time for these words (shared_objects.sh). The line of
# MMgr, the cellblock and the device hooks compile as CUDA (exact_types_device_line), and the tessera daemon
# links them too
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        SHARED_WORDS=(nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC[@]}" -Xcompiler "/std:c11 /O2 /experimental:c11atomics" "${INCLUDES[@]}") ;;
    *)
        SHARED_WORDS=(cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -fPIC "${INCLUDES[@]}") ;;
esac
exact_types_device_line || exit 1
LINEA_OBJECTS=("${EXACT_TYPES_DEVICE_LINE[@]}")
# The ruleset reader reads the blob of a ruleset through the member reader of a query file, as the engine does
shared_objects_built SHARED_WORDS "${EXACT_TYPES_INTEGERS[@]}" "${EXACT_TYPES_PLATFORM[@]}" \
    "$CYCLE/cycle.c" "$EXACT_RECORD/exact_record.c" "$TOP/includes/file_defs/qry/writer/qry_member.c" \
    "${PLATFORM_QRY_RUN[@]}" "$TOP/includes/file_defs/qry/qry_read.c" || exit 1
OBJECTS=("${LINEA_OBJECTS[@]}" "${SHARED_OBJECTS[@]}")
# the test is a job on the device's tessera daemon, built beside it
tessera_build "${LINEA_OBJECTS[@]}" || exit 1

nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -std=c++20 -O2 -rdc=true "${GENCODE[@]}" "${INCLUDES[@]}" -o "$BINARY" \
    "$TEST"/period_record_test.cu "$TOP/src/sims/cu/sim_job.cu" "$MODULE"/period_{measure,select,record}.cu \
    "$DEVICE_POOL/device_pool.cu" "$CYCLE"/cycle*.cu "$CODEGEN"/*.cu "$LSTAR_PARSER"/*.cu "$TOP/includes/file_defs/krs/ruleset_flat"/*.cu "$KEYMATH/keymath.cu" \
    "$KEY_SCHEDULE/key_schedule.cu" "${OBJECTS[@]}" "${TESSERA_OBJECTS[@]}" "${TESSERA_SEAL[@]}"
[ -f "$BINARY" ] || { echo "  build failed: nvcc could not build the test"; exit 1; }
if [ "${BUILD_ONLY:-0}" = "1" ]; then
    echo "  built $BINARY"
    exit 0
fi

"$BINARY"
STATUS=$?
echo "  period record test exit $STATUS"
exit "$STATUS"
