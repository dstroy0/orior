#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
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
CODEGEN="$SOURCE/engine/rmc"
PARSER="$TOP/includes/parsers"
KEYMATH="$SOURCE/engine/analysis/keymath"
KEY_SCHEDULE="$SOURCE/engine/analysis/key_schedule"
TOWER="$SOURCE/engine/analysis/tower"
COMPRESSION="$SOURCE/engine/analysis/compression"
DEVICE_POOL="$SOURCE/engine/runtime/device_pool"
source "$TOP/includes/file_defs/file_defs.sh"
source "$TOP/includes/exact_types/exact_types.sh"
INTEGERS="$TOP/includes/exact_types/integers"
SNAPPY="$TOP/includes/codecs/snappy"
ZSTD="$TOP/includes/codecs/zstd"

HOST_FLAGS=()
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        BINARY="$OUT/lane_check.exe"
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
          -I "$COMPRESSION" -I "$DEVICE_POOL" -I "$INTEGERS" "${FILE_DEFS_MMGR_FLAGS[@]}" -I "$SNAPPY" -I "$ZSTD" -I "$ROOT/src"
          "${TESSERA_INCLUDES[@]}")
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
# the formats with the line: the ruleset reader reads a ruleset's .ksc as the run stores it (qry_member.h)
shared_objects_built SHARED_WORDS "${FILE_DEFS_FORMATS[@]}" "${EXACT_TYPES_INTEGERS[@]}" "${EXACT_TYPES_PLATFORM[@]}" \
    "$INTEGERS/exact_record/exact_record.c" "$TOP/includes/exact_types/integerfloats/edouble/edouble_record.c" "$CYCLE/cycle.c" "$SNAPPY"/*.c "$ZSTD"/*.c \
    "$ROOT/src/parquet/parquet.c" || exit 1
OBJECTS=("${LINEA_OBJECTS[@]}" "${SHARED_OBJECTS[@]}")
tessera_build "${LINEA_OBJECTS[@]}" || exit 1

nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -std=c++20 -O2 -rdc=true "${GENCODE[@]}" "${INCLUDES[@]}" -o "$BINARY" \
    "$ROOT/src/check/lane_check.cu" "$TOP/src/sims/cu/sim_job.cu" "$CYCLE"/cycle*.cu "$CODEGEN"/*.cu "$PARSER"/*.cu \
    "$TOP/includes/file_defs/krs/ruleset_flat"/*.cu \
    "$KEYMATH/keymath.cu" "$KEY_SCHEDULE/key_schedule.cu" "$TOWER"/tower*.cu "$COMPRESSION/compression.cu" \
    "$DEVICE_POOL/device_pool.cu" "${OBJECTS[@]}" "${TESSERA_OBJECTS[@]}" "${TESSERA_SEAL[@]}"
[ -f "$BINARY" ] || { echo "  build failed: nvcc could not build lane_check"; exit 1; }
echo "  built $BINARY"
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) DAEMON="$OUT/tessera_daemon.exe" ;; *) DAEMON="$OUT/tessera_daemon" ;; esac
build_publish "$DAEMON" || exit 1
build_publish "$BINARY" || exit 1
