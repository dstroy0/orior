#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds a driver on the host and runs it on the cfg named: bash run.sh axis_heat cfg/water_20c.cfg
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$ROOT/.." && pwd)"
SRC="$(cd "$TOP/../src" && pwd)"
PROGRAM="${1:-}"
CFG="${2:-}"
# matching_values runs twice: at the default width it writes every value's residues, then this script runs it again
# at the width the residues name, given them as a third argument
RESIDUES="${3:-}"
case "$PROGRAM" in
    axis_heat|axis_series|join_series|join_datum|witness_known|core_cubes|datum_cubes|matching_functions|matching_values) ;;
    *) echo "  usage: run.sh axis_heat|axis_series|join_series|join_datum|witness_known|core_cubes|datum_cubes|matching_functions|matching_values <cfg>"; exit 2 ;;
esac
[ -n "$CFG" ] && [ -f "$CFG" ] || { echo "  usage: run.sh axis_heat|axis_series|join_series|join_datum|witness_known|core_cubes|datum_cubes|matching_functions|matching_values <cfg>"; exit 2; }
# the driver's modules, each src/<module>/<module>.cu with its header beside it
MODULES=(run_cfg report term_form record witness_cube taylor ode_series eta_function core_series decay_integral blend pressure_datum blend_field matching term_value)
source "$TOP/../utils/maint/engine/build_stamp.sh"
build_stamp navier_stokes

SIMS_CU="$SRC/sims/cu"
SCRIPTURA="$SRC/cu/engine/runtime/scriptura"
NO_ROUNDING="$SRC/cu/types/integers"
CFG_JSON="$SRC/cu/includes/formats/cfg_json"
DAEMON_DIRECTORY="$SRC/cu/engine/runtime/daemon"
OBSIGNATIO="$SRC/cu/engine/runtime/obsignatio"
OBSIGNATIO_CU="$SRC/cu/engine/runtime/obsignatio"

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
        # an exact rational at the widest builds is kilobytes, and a driver holds many on its stack
        LINK_FLAGS=(-Xlinker /STACK:268435456)
        ;;
    *)
        BINARY="$OUT/$PROGRAM"
        EXTENSION=o
        HOST_FLAGS=(-Xcompiler -fPIC)
        LINK_FLAGS=()
        ;;
esac

# SIM_EXACT_LIMBS sets the exact integer's width in 32-bit limbs, a power of two, for every object linked
WIDTH=()
if [ -n "${SIM_EXACT_LIMBS:-}" ]; then
    WIDTH=(-DANCHOR_EXACT_LIMBS="${SIM_EXACT_LIMBS}u")
fi
INCLUDES=(-I "$SRC/cu/engine" -I "$SIMS_CU" -I "$SCRIPTURA" -I "$NO_ROUNDING" -I "$CFG_JSON"
          -I "$DAEMON_DIRECTORY" -I "$OBSIGNATIO" -I "$OBSIGNATIO_CU")
for module in "${MODULES[@]}"; do
    INCLUDES+=(-I "$ROOT/src/$module")
done
# matching_values is a program of the record machine: the imprint, the layout and the run, the code generator beside
# them, built for this device
RECORD_DIRECTORIES=("$SRC/cu/engine/analysis/cycle" "$SRC/cu/engine/analysis/keymath" "$SRC/cu/engine/analysis/key_schedule"
                    "$SRC/cu/transpiler/codegen" "$SRC/cu/transpiler/lstar/parser")
GENCODE=()
if [ "$PROGRAM" = matching_values ]; then
    for directory in "${RECORD_DIRECTORIES[@]}"; do
        INCLUDES+=(-I "$directory")
    done
    CAP="$(nvidia-smi --query-gpu=compute_cap --format=csv,noheader 2>/dev/null | head -1 | tr -d ' .')"
    GENCODE=(-gencode "arch=compute_${CAP:-86},code=sm_${CAP:-86}")
fi

# the objects are kept per width in one place, and an object is compiled again only where its source, or a header in a
# directory the build reads, is newer than it
OBJECT_DIRECTORY="$TOP/build/navier_stokes_objects_${SIM_EXACT_LIMBS:-default}"
mkdir -p "$OBJECT_DIRECTORY"
HEADER_DIRECTORIES=("$SIMS_CU" "$SCRIPTURA" "$NO_ROUNDING" "$CFG_JSON" "$DAEMON_DIRECTORY" "$OBSIGNATIO" "$OBSIGNATIO_CU")
for module in "${MODULES[@]}"; do
    HEADER_DIRECTORIES+=("$ROOT/src/$module")
done

current()
{
    local source="$1"
    local object="$2"
    [ -f "$object" ] && [ ! "$source" -nt "$object" ] &&
        [ -z "$(find "${HEADER_DIRECTORIES[@]}" -maxdepth 1 -name '*.h' -newer "$object" -print -quit)" ]
}

build_object()
{
    local source="$1"
    local object="$2"
    current "$source" "$object" && return 0
    rm -f "$object"
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*)
            nvcc "${HOST_FLAGS[@]}" -Xcompiler "/std:c11 /O2" "${WIDTH[@]}" "${INCLUDES[@]}" -c "$source" -o "$object" ;;
        *)
            cc -std=c11 -O2 -fPIC "${WIDTH[@]}" "${INCLUDES[@]}" -c "$source" -o "$object" ;;
    esac
    [ -f "$object" ] || { echo "  build failed: $(basename "$source") did not compile"; exit 1; }
}

# a C++ source compiled to its object, current or not as above
build_unit()
{
    local source="$1"
    local object="$2"
    current "$source" "$object" && return 0
    rm -f "$object"
    nvcc "${HOST_FLAGS[@]}" -std=c++17 -O2 "${WIDTH[@]}" "${INCLUDES[@]}" -c "$source" -o "$object"
    [ -f "$object" ] || { echo "  build failed: $(basename "$source") did not compile"; exit 1; }
}

OBJECTS=()
for source in "$SCRIPTURA"/*.c "$NO_ROUNDING"/exact_integer_{add,limbs,multiply,divide,gcd,decimal,hash}.c \
              "$CFG_JSON/cfg_json.c"; do
    object="$OBJECT_DIRECTORY/$(basename "$source" .c).$EXTENSION"
    build_object "$source" "$object"
    OBJECTS+=("$object")
done
# sim_close releases a tessera job, and the drivers submit none: the client is linked and never asked
for name in tessera_client_{socket,jobs} tessera_paths tessera_frame tessera_self; do
    object="$OBJECT_DIRECTORY/$name.$EXTENSION"
    build_object "$DAEMON_DIRECTORY/$name.c" "$object"
    OBJECTS+=("$object")
done
for name in obsignatio_{hash,seal}; do
    object="$OBJECT_DIRECTORY/$name.$EXTENSION"
    if ! current "$OBSIGNATIO_CU/$name.cu" "$object"; then
        rm -f "$object"
        nvcc "${HOST_FLAGS[@]}" -O2 "${WIDTH[@]}" "${INCLUDES[@]}" -c "$OBSIGNATIO_CU/$name.cu" -o "$object"
    fi
    [ -f "$object" ] || { echo "  build failed: $name.cu did not compile"; exit 1; }
    OBJECTS+=("$object")
done
for module in "${MODULES[@]}"; do
    object="$OBJECT_DIRECTORY/$module.$EXTENSION"
    build_unit "$ROOT/src/$module/$module.cu" "$object"
    OBJECTS+=("$object")
done
build_unit "$SIMS_CU/sim_job.cu" "$OBJECT_DIRECTORY/sim_job.$EXTENSION"
OBJECTS+=("$OBJECT_DIRECTORY/sim_job.$EXTENSION")
if [ "$PROGRAM" = matching_values ]; then
    object="$OBJECT_DIRECTORY/cycle.$EXTENSION"
    build_object "$SRC/cu/engine/analysis/cycle/cycle.c" "$object"
    OBJECTS+=("$object")
    for source in "$SRC/cu/engine/analysis/cycle"/cycle*.cu "$SRC/cu/transpiler/codegen"/*.cu \
                  "$SRC/cu/transpiler/lstar/parser"/*.cu "$SRC/cu/engine/analysis/keymath/keymath.cu" \
                  "$SRC/cu/engine/analysis/key_schedule/key_schedule.cu"; do
        object="$OBJECT_DIRECTORY/record_$(basename "$source" .cu).$EXTENSION"
        if ! current "$source" "$object"; then
            rm -f "$object"
            nvcc "${HOST_FLAGS[@]}" -std=c++17 -O2 "${GENCODE[@]}" "${WIDTH[@]}" "${INCLUDES[@]}" -c "$source" -o "$object"
        fi
        [ -f "$object" ] || { echo "  build failed: $(basename "$source") did not compile"; exit 1; }
        OBJECTS+=("$object")
    done
    # the device's tessera daemon beside the program, which starts it where none answers; the program links the
    # client objects above
    source "$TOP/../utils/maint/engine/tessera_build.sh"
    SCRIPTURA_OBJECTS=()
    for source in "$SCRIPTURA"/*.c; do
        SCRIPTURA_OBJECTS+=("$OBJECT_DIRECTORY/$(basename "$source" .c).$EXTENSION")
    done
    tessera_build navier_stokes "${SCRIPTURA_OBJECTS[@]}" || exit 1
fi
build_unit "$ROOT/src/$PROGRAM/$PROGRAM.cu" "$OBJECT_DIRECTORY/$PROGRAM.$EXTENSION"
OBJECTS+=("$OBJECT_DIRECTORY/$PROGRAM.$EXTENSION")

rm -f "$BINARY"
nvcc "${HOST_FLAGS[@]}" "${LINK_FLAGS[@]}" "${GENCODE[@]}" -O2 -o "$BINARY" "${OBJECTS[@]}"
[ -f "$BINARY" ] || { echo "  build failed: nvcc could not build $PROGRAM"; exit 1; }

if [ "$PROGRAM" = matching_values ] && [ -n "$RESIDUES" ]; then
    "$BINARY" "$CFG" "$RESIDUES" read
elif [ "$PROGRAM" = matching_values ]; then
    RESIDUES="$OUT/matching_values_residues.txt"
    "$BINARY" "$CFG" "$RESIDUES"
    STATUS=$?
    echo "  $PROGRAM exit $STATUS"
    [ "$STATUS" -eq 0 ] || exit "$STATUS"
    LIMBS="$(sed -n 's/^limbs //p' "$RESIDUES")"
    SIM_EXACT_LIMBS="$LIMBS" bash "$ROOT/run.sh" matching_values "$CFG" "$RESIDUES"
    exit $?
else
    "$BINARY" "$CFG"
fi
STATUS=$?
echo "  $PROGRAM exit $STATUS"
exit "$STATUS"
