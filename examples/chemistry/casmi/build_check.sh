#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# lane_check (src/check): a parquet file's double columns read as their stored lanes, through the tower and
# compression and back, and a record program run on the lowered lanes on the device against the host reference. One
# job on the device's tessera daemon, built beside it. Arguments past the file are architectures, sm_86 and the like
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$ROOT/../../.." && pwd)"
source "$TOP/utils/maint/engine/build_stamp.sh"
source "$TOP/utils/maint/engine/tessera_build.sh"
build_stamp casmi_check

SOURCE="$TOP/src/cu"
CYCLE="$SOURCE/engine/analysis/cycle"
CODEGEN="$SOURCE/transpiler/codegen"
PARSER="$SOURCE/transpiler/lstar/parser"
KEYMATH="$SOURCE/engine/analysis/keymath"
KEY_SCHEDULE="$SOURCE/engine/analysis/key_schedule"
TOWER="$SOURCE/engine/analysis/tower"
COMPRESSION="$SOURCE/engine/analysis/compression"
DEVICE_POOL="$SOURCE/engine/runtime/device_pool"
SCRIPTURA="$SOURCE/engine/runtime/scriptura"
INTEGERS="$SOURCE/types/integers"
SNAPPY="$SOURCE/includes/codecs/snappy"
ZSTD="$SOURCE/includes/codecs/zstd"

HOST_FLAGS=()
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        BINARY="$OUT/lane_check.exe"
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
        BINARY="$OUT/lane_check"
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

INCLUDES=(-I "$SOURCE/engine" -I "$TOP/src/sims/cu" -I "$CYCLE" -I "$KEYMATH" -I "$KEY_SCHEDULE" -I "$TOWER"
          -I "$COMPRESSION" -I "$DEVICE_POOL" -I "$INTEGERS" -I "$SCRIPTURA" -I "$SNAPPY" -I "$ZSTD" -I "$ROOT/src"
          "${TESSERA_INCLUDES[@]}")
rm -f "$BINARY"
OBJECTS=()
SCRIPTURA_OBJECTS=()
for source in "$SCRIPTURA"/*.c "$INTEGERS"/exact_integer_{add,limbs,multiply,divide,gcd,decimal,hash}.c \
              "$INTEGERS/exact_record/exact_record.c" "$SOURCE/types/integerfloats/edouble/edouble_record.c" "$CYCLE/cycle.c" "$SNAPPY"/*.c "$ZSTD"/*.c \
              "$ROOT/src/parquet/parquet.c"; do
    object="$OUT/$(basename "$source" .c)_casmi_check.$EXTENSION"
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
tessera_build casmi_check "${SCRIPTURA_OBJECTS[@]}" || exit 1

nvcc "${HOST_FLAGS[@]}" -std=c++17 -O2 "${GENCODE[@]}" "${INCLUDES[@]}" -o "$BINARY" \
    "$ROOT/src/check/lane_check.cu" "$TOP/src/sims/cu/sim_job.cu" "$CYCLE"/cycle*.cu "$CODEGEN"/*.cu "$PARSER"/*.cu \
    "$KEYMATH/keymath.cu" "$KEY_SCHEDULE/key_schedule.cu" "$TOWER"/tower*.cu "$COMPRESSION/compression.cu" \
    "$DEVICE_POOL/device_pool.cu" "${OBJECTS[@]}" "${TESSERA_OBJECTS[@]}" "${TESSERA_SEAL[@]}"
[ -f "$BINARY" ] || { echo "  build failed: nvcc could not build lane_check"; exit 1; }
echo "  built $BINARY"
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) DAEMON="$OUT/tessera_daemon.exe" ;; *) DAEMON="$OUT/tessera_daemon" ;; esac
build_publish "$DAEMON" || exit 1
build_publish "$BINARY" || exit 1
