#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
set -u

TEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$TEST/../../../../../../.." && pwd)"
MODULE="$TOP/src/cu/engine/analysis/period"
DEVICE_POOL="$TOP/src/cu/engine/runtime/device_pool"
CYCLE="$TOP/src/cu/engine/analysis/cycle"
CODEGEN="$TOP/src/cu/engine/rmc"
LSTAR_PARSER="$TOP/src/cu/types/file_defs/readers"
KEYMATH="$TOP/src/cu/engine/analysis/keymath"
KEY_SCHEDULE="$TOP/src/cu/engine/analysis/key_schedule"
NO_ROUNDING="$TOP/src/cu/types/integers"
EXACT_RECORD="$TOP/src/cu/types/integers/exact_record"
SCRIPTURA="$TOP/src/cu/engine/runtime/scriptura"
source "$TOP/utils/maint/engine/build_stamp.sh"
source "$TOP/utils/maint/engine/tessera_build.sh"
build_stamp period_record_test

HOST_FLAGS=()
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        BINARY="$OUT/period_record_test.exe"
        EXTENSION=obj
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
        EXTENSION=o
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
          -I "$KEY_SCHEDULE" -I "$NO_ROUNDING" -I "$EXACT_RECORD" -I "$SCRIPTURA" "${TESSERA_INCLUDES[@]}")
rm -f "$BINARY"
OBJECTS=()
SCRIPTURA_OBJECTS=()
for source in "$SCRIPTURA"/*.c "$NO_ROUNDING"/exact_integer_{add,limbs,multiply,divide,gcd,decimal,hash}.c \
              "$CYCLE/cycle.c" "$EXACT_RECORD/exact_record.c"; do
    object="$OUT/$(basename "$source" .c)_record.$EXTENSION"
    rm -f "$object"
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*)
            nvcc "${HOST_FLAGS[@]}" -Xcompiler "/std:c11 /O2" "${INCLUDES[@]}" -c "$source" -o "$object" ;;
        *)
            cc -std=c11 -O2 -fPIC "${INCLUDES[@]}" -c "$source" -o "$object" ;;
    esac
    [ -f "$object" ] || { echo "  build failed: $(basename "$source") did not compile"; exit 1; }
    OBJECTS+=("$object")
    case "$(basename "$source")" in
        scriptura*) SCRIPTURA_OBJECTS+=("$object") ;;
    esac
done
# the test is a job on the device's tessera daemon, built beside it
tessera_build record "${SCRIPTURA_OBJECTS[@]}" || exit 1

nvcc "${HOST_FLAGS[@]}" -std=c++17 -O2 "${GENCODE[@]}" "${INCLUDES[@]}" -o "$BINARY" \
    "$TEST"/period_record_test.cu "$TOP/src/sims/cu/sim_job.cu" "$MODULE"/period_{measure,select,record}.cu \
    "$DEVICE_POOL/device_pool.cu" "$CYCLE"/cycle*.cu "$CODEGEN"/*.cu "$LSTAR_PARSER"/*.cu "$KEYMATH/keymath.cu" \
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
