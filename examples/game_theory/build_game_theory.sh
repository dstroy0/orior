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
shared_objects_built SHARED_WORDS "${FILE_DEFS_FORMATS[@]}" "${EXACT_TYPES_INTEGERS[@]}" "${EXACT_TYPES_RECORD[@]}" \
    "${EXACT_TYPES_PLATFORM[@]}" "$CYCLE/cycle.c" "$SHUFFLE/zeta_shuffle.c" || exit 1
OBJECTS=("${LINEA_OBJECTS[@]}" "${SHARED_OBJECTS[@]}")
tessera_build "${LINEA_OBJECTS[@]}" || exit 1

nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -std=c++20 -O2 -rdc=true "${GENCODE[@]}" "${INCLUDES[@]}" "${LONG_PATHS[@]}" -o "$BINARY" "$SOURCE" \
    "$SIMS_CU/sim_job.cu" "$CYCLE"/cycle*.cu "$CODEGEN"/*.cu "$PARSERS"/*.cu "$PARSERS"/../file_defs/krs/ruleset_flat/*.cu \
    "$KEYMATH/keymath.cu" "$KEY_SCHEDULE/key_schedule.cu" "${OBJECTS[@]}" "${TESSERA_OBJECTS[@]}" "${TESSERA_SEAL[@]}" \
    "${EXACT_TYPES_LIBRARIES[@]}"
[ -f "$BINARY" ] || { echo "  build failed: nvcc could not build $PROGRAM"; exit 1; }
cd "$TOP" || exit 1
"$BINARY" "$@"
