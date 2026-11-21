#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Builds one game theory program of this folder for the device. It links the record machine of the engine, its
# compiler and the tessera client. It then runs the program with the arguments after its name. The daemon goes beside
# it.
#
#   Usage:  bash examples/game_theory/build_game_theory.sh <program> [arguments]
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../.." && pwd)"
PROGRAM="${1:-}"
shift || true
SOURCE="$(find "$HERE" -name "$PROGRAM.cu" | head -1)"
if [ -z "$PROGRAM" ] || [ -z "$SOURCE" ]; then
    echo "  usage: build_game_theory.sh <program> [arguments], a program being a .cu file of this folder"
    exit 2
fi
ENGINE="$TOP/src/cu/engine"
CYCLE="$ENGINE/analysis/cycle"
CODEGEN="$ENGINE/rmc"
PARSERS="$TOP/includes/parsers"
KEYMATH="$ENGINE/analysis/keymath"
KEY_SCHEDULE="$ENGINE/analysis/key_schedule"
NO_ROUNDING="$TOP/includes/exact_types/integers"
EXACT_RECORD="$NO_ROUNDING/exact_record"
SHUFFLE="$TOP/examples/0_experimental/exact_zeta"
SIMS_CU="$TOP/src/sims/cu"
source "$TOP/includes/file_defs/file_defs.sh"
source "$TOP/includes/exact_types/exact_types.sh"
source "$TOP/utils/maint/engine/build_stamp.sh"
source "$TOP/utils/maint/engine/tessera_build.sh"
build_stamp "game_theory_$PROGRAM"

HOST_FLAGS=()
LONG_PATHS=()
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        BINARY="$OUT/$PROGRAM.exe"
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
        LONG_PATHS=(-Xlinker /MANIFEST:EMBED -Xlinker "/MANIFESTINPUT:$(cygpath -m "$TOP/src/cu/long_paths.manifest")")
        ;;
    *)
        BINARY="$OUT/$PROGRAM"
        EXTENSION=o
        HOST_FLAGS=(-Xcompiler -fPIC)
        ;;
esac

ARCHES="${SIM_ARCHES:-}"
if [ -z "$ARCHES" ]; then
    CAP="$(nvidia-smi --query-gpu=compute_cap --format=csv,noheader 2>/dev/null | head -1 | tr -d ' .')"
    ARCHES="sm_${CAP:-86}"
fi
GENCODE=()
for one in $ARCHES; do
    GENCODE+=(-gencode "arch=compute_${one#sm_},code=${one}")
done

INCLUDES=(-I "$ENGINE" -I "$CYCLE" -I "$CODEGEN" -I "$PARSERS" -I "$KEYMATH" -I "$KEY_SCHEDULE" -I "$NO_ROUNDING"
          -I "$EXACT_RECORD" -I "$SHUFFLE" -I "$SIMS_CU" "${EXACT_TYPES_FLAGS[@]}" "${TESSERA_INCLUDES[@]}")
rm -f "$BINARY"
OBJECTS=()
LINEA_OBJECTS=()
for source in "${FILE_DEFS_FORMATS[@]}" "${FILE_DEFS_LINEA[@]}" "${EXACT_TYPES_INTEGERS[@]}" "${EXACT_TYPES_RECORD[@]}" \
              "${EXACT_TYPES_PLATFORM[@]}" "${EXACT_TYPES_CARCER[@]}" "$CYCLE/cycle.c" "$SHUFFLE/zeta_shuffle.c"; do
    object="$OUT/$(basename "$source" .c)_game.$EXTENSION"
    rm -f "$object"
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*)
            nvcc "${HOST_FLAGS[@]}" -Xcompiler "/std:c11 /O2 /experimental:c11atomics" "${INCLUDES[@]}" -c "$source" -o "$object" ;;
        *)
            cc -std=c11 -O2 -fPIC "${INCLUDES[@]}" -c "$source" -o "$object" ;;
    esac
    [ -f "$object" ] || { echo "  build failed: $(basename "$source") did not compile"; exit 1; }
    OBJECTS+=("$object")
    case "$source" in
        */mmgr/src/locus_carcerum/*) ;;
        */mmgr/src/*) LINEA_OBJECTS+=("$object") ;;
    esac
done
tessera_build game "${LINEA_OBJECTS[@]}" || exit 1

nvcc "${HOST_FLAGS[@]}" -std=c++17 -O2 "${GENCODE[@]}" "${INCLUDES[@]}" "${LONG_PATHS[@]}" -o "$BINARY" "$SOURCE" \
    "$SIMS_CU/sim_job.cu" "$CYCLE"/cycle*.cu "$CODEGEN"/*.cu "$PARSERS"/*.cu "$PARSERS"/../file_defs/krs/ruleset_flat/*.cu \
    "$KEYMATH/keymath.cu" "$KEY_SCHEDULE/key_schedule.cu" "${OBJECTS[@]}" "${TESSERA_OBJECTS[@]}" "${TESSERA_SEAL[@]}" \
    "${EXACT_TYPES_LIBRARIES[@]}"
[ -f "$BINARY" ] || { echo "  build failed: nvcc could not build $PROGRAM"; exit 1; }
cd "$TOP" || exit 1
"$BINARY" "$@"
