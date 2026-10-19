#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$ROOT/.." && pwd)"
source "$TOP/utils/maint/engine/build_stamp.sh"
build_stamp engine

HOST_FLAGS=()
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        LIBRARY="$OUT/cell_tracking_engine.dll"
        MSVC_BIN="$(ls -d "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC"/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
        if [ -z "$MSVC_BIN" ]; then
            MSVC_BIN="$(ls -d "/c/Program Files/Microsoft Visual Studio"/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
        fi
        if [ -z "$MSVC_BIN" ]; then
            echo "  no host compiler nvcc accepts on this platform was found."
            exit 1
        fi
        HOST_FLAGS=(-ccbin "$MSVC_BIN" -Xcompiler /Zc:preprocessor -Xcompiler -Z7)
        LINK_FLAGS=(-Xlinker -DEBUG -Xlinker -OPT:REF -Xlinker -OPT:ICF)
        ;;
    *)
        LIBRARY="$OUT/libcell_tracking_engine.so"
        HOST_FLAGS=(-Xcompiler -fPIC -g)
        LINK_FLAGS=()
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
echo "  architectures: $ARCHES"

rm -f "$LIBRARY"

EXACT_ROOT="${ANCHOR_EXACT_ROOT:-$TOP/src/cu/includes/types/integers}"
RESIDUAL_LIMBS="$(sed -n 's/^#define ENGINE_RESIDUAL_LIMBS \([0-9]*\)u.*/\1/p' "$TOP/src/cu/engine"/engine_config_*.h)"
[ -n "$RESIDUAL_LIMBS" ] || { echo "  build failed: no ENGINE_RESIDUAL_LIMBS in engine_config_*.h"; exit 1; }
QUESTION_LIMBS=$((RESIDUAL_LIMBS + 1))
RECORD_LIMBS="$(sed -n 's/^#define ENGINE_RECORD_LIMBS_MAX \([0-9]*\)u.*/\1/p' "$TOP/src/cu/engine"/engine_config_*.h)"
[ -n "$RECORD_LIMBS" ] || { echo "  build failed: no ENGINE_RECORD_LIMBS_MAX in engine_config_*.h"; exit 1; }
[ "$RECORD_LIMBS" -gt "$QUESTION_LIMBS" ] && QUESTION_LIMBS="$RECORD_LIMBS"
FITTED_LIMBS=1
while [ "$FITTED_LIMBS" -lt "$QUESTION_LIMBS" ]; do
    FITTED_LIMBS=$((FITTED_LIMBS * 2))
done
EXACT_LIMBS="${ANCHOR_EXACT_LIMBS:-$FITTED_LIMBS}"
EXACT_DIGITS=$(( (EXACT_LIMBS * 32 * 1000 - 1) / 3322 ))
if [ "$EXACT_DIGITS" -gt 1024 ]; then
    EXACT_DIGITS=1024
fi
[ -f "$EXACT_ROOT/exact_integer.h" ] || { echo "  build failed: no exact_integer.h under $EXACT_ROOT"; exit 1; }
EXACT_FLAGS=(-I "$EXACT_ROOT" "-DANCHOR_EXACT_LIMBS=${EXACT_LIMBS}u" "-DANCHOR_EXACT_DIGITS=${EXACT_DIGITS}u")

DEFINES=(-DBODY_OVERLAP_BUILD_DLL=1 -DHEAVIEST_MATCHING_BUILD_DLL=1
         -DSHIFT_AGREEMENT_BUILD_DLL=1)
MODULES=(engine/formats/stack cu/includes/formats/stack cu/includes/file_defs/krep
         engine/analysis/compression cu/engine/analysis/compression engine/analysis/tower
         cu/engine/analysis/tower engine/runtime/device_pool cu/engine/runtime/device_pool
         engine/analysis/entropy_history cu/engine/analysis/entropy_history engine/analysis/noise_detector
         cu/engine/analysis/noise_detector engine/runtime/schedule cu/engine/runtime/schedule
         engine/compiler/keymath cu/engine/analysis/keymath engine/compiler/key_schedule
         cu/engine/analysis/key_schedule engine/compiler/cycle cu/engine/analysis/cycle
         engine/compiler/codegen cu/engine/rmc cu/includes/parsers cu/includes/file_defs/klq engine/runtime/radix_keys
         engine/analysis/unit_sweep cu/engine/analysis/unit_sweep engine/runtime/obsignatio
         cu/engine/runtime/obsignatio engine/analysis/residual cu/engine/analysis/residual
         engine/nbody/max_tree cu/engine/nbody/max_tree engine/nbody/flatten cu/engine/nbody/flatten
         engine/analysis/golden_bands cu/engine/analysis/golden_bands engine/analysis/residual_survey
         cu/engine/analysis/residual_survey engine/nbody/grow cu/engine/nbody/grow
         engine/analysis/shift_agreement cu/engine/analysis/shift_agreement engine/nbody/climb_machine
         cu/engine/nbody/climb_machine engine/nbody/body_overlap cu/engine/nbody/body_overlap
         engine/nbody/fingerprint cu/engine/nbody/fingerprint engine/nbody/print_pair
         cu/engine/nbody/print_pair engine/nbody/velocity cu/engine/nbody/velocity engine/nbody/division
         cu/engine/nbody/division engine/nbody/marginal cu/engine/nbody/marginal engine/nbody/contact_side
         cu/engine/nbody/contact_side engine/nbody/box_history cu/engine/nbody/box_history
         engine/nbody/heaviest_matching engine/arithmetic/double_fields cu/includes/types/integerfloats/double_fields engine/arithmetic/decimal_double
         engine/runtime/scriptura engine/analysis/period cu/engine/analysis/period)
INGEST=(cu/includes/formats/cfg_json cu/includes/formats/zarr cu/includes/codecs/zstd cu/includes/codecs/inflate
        cu/includes/codecs/deflate cu/includes/codecs/lz4 cu/includes/codecs/snappy cu/includes/codecs/blosc
        cu/includes/formats/tiff cu/includes/formats/hdf5 cu/includes/codecs/zip cu/includes/formats/dicom
        cu/includes/formats/npy cu/includes/formats/nrrd cu/includes/formats/nifti
        cu/includes/formats/parquet)
MODULES+=("${INGEST[@]}")
MODULE_INCLUDES=(-I "$TOP/src/cu/engine" -I "$TOP/src/cu/includes/codecs/crc")
MODULE_SOURCES=("$TOP/src/cu/engine"/engine_{record,residual,files,zarr,source,listing,seal,report,history}.cu)
for module in "${MODULES[@]}"; do
    MODULE_INCLUDES+=(-I "$TOP/src/$module")
    for source in "$TOP/src/$module"/*.cu; do
        [ -f "$source" ] && MODULE_SOURCES+=("$source")
    done
done
# Each source compiles to an object of its own, kept between builds in a folder named for the flags it is built with,
# and compiles again only where its object is missing, or its source or a header of the engine's is newer. As many
# compile at once as the host has processors, ENGINE_JOBS where it is set, and the library is linked once from the
# objects.
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) SUFFIX=obj ;;
    *) SUFFIX=o ;;
esac
KEY="$(printf '%s\n' "$(uname -s)" "$ARCHES" "${EXACT_FLAGS[*]}" "${DEFINES[*]}" "${HOST_FLAGS[*]}" | cksum | cut -d' ' -f1)"
CACHE="$FINAL/engine_objects/$KEY"
mkdir -p "$CACHE"
JOBS="${ENGINE_JOBS:-$(nproc 2> /dev/null || echo 4)}"
NEWEST_HEADER="$(find "$TOP/src/cu" "$TOP/src/engine" "$EXACT_ROOT" \( -name '*.h' -o -name '*.cuh' -o -name '*.hpp' \) \
    -printf '%T@\n' 2> /dev/null | sort -n | tail -1)"
NEWEST_HEADER="${NEWEST_HEADER%.*}"
FAILED="$CACHE/failed.txt"
rm -f "$FAILED"

# every object's time, read in one call, and whether the object `$1` of the source `$2` compiles again
declare -A OBJECT_TIME=()
while read -r seconds object; do
    OBJECT_TIME["$object"]="$seconds"
done < <(stat -c '%Y %n' "$CACHE"/*."$SUFFIX" 2> /dev/null)
stale() {
    [ -z "${OBJECT_TIME["$1"]:-}" ] || [ "$2" -nt "$1" ] || [ "${NEWEST_HEADER:-0}" -gt "${OBJECT_TIME["$1"]}" ]
}

# the source `$2` compiled to the object `$1` by the words after them: written beside it and put in its place once it
# is whole, and where it does not compile, listed in FAILED with the compiler's words kept beside the object
compiled() {
    local object="$1" source="$2" part="${1%.*}.part.$SUFFIX"
    shift 2
    if "$@" -c "$source" -o "$part" > "$object.log" 2>&1 && mv -f "$part" "$object"; then
        rm -f "$object.log"
    else
        rm -f "$part"
        echo "$source $object.log" >> "$FAILED"
    fi
}

# a compile started in the background once fewer than JOBS run
queued() {
    while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do
        wait -n
    done
    compiled "$@" &
}

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        C_WORDS=(nvcc "${HOST_FLAGS[@]}" -Xcompiler "/std:c11 /O2" "${DEFINES[@]}" "${MODULE_INCLUDES[@]}" "${EXACT_FLAGS[@]}")
        EXACT_WORDS=(nvcc "${HOST_FLAGS[@]}" -Xcompiler "/std:c11 /O2" "${EXACT_FLAGS[@]}")
        ;;
    *)
        C_WORDS=(cc -std=c11 -O2 -g -fPIC "${MODULE_INCLUDES[@]}" "${EXACT_FLAGS[@]}")
        EXACT_WORDS=(cc -std=c11 -O2 -g -fPIC "${EXACT_FLAGS[@]}")
        ;;
esac
CU_WORDS=(nvcc "${HOST_FLAGS[@]}" -std=c++17 -O2 -fmad=false "${GENCODE[@]}" "${DEFINES[@]}" "${MODULE_INCLUDES[@]}"
          "${EXACT_FLAGS[@]}")

OBJECTS=()
COMPILED=0
BEGAN=$(date +%s)
# each source with the words it compiles by, its object named by its path under src, every object kept for the link
# and the stale ones compiled
listed() {
    local source="$1" relative="${1#"$TOP/src/"}" object
    shift
    object="$CACHE/${relative//\//__}.$SUFFIX"
    OBJECTS+=("$object")
    if stale "$object" "$source"; then
        COMPILED=$((COMPILED + 1))
        queued "$object" "$source" "$@"
    fi
}
for source in "${MODULE_SOURCES[@]}"; do
    listed "$source" "${CU_WORDS[@]}"
done
for portable in cu/engine/nbody/body_overlap cu/engine/nbody/heaviest_matching cu/engine/analysis/shift_agreement \
                cu/engine/nbody/max_tree cu/engine/analysis/cycle cu/engine/nbody/marginal \
                cu/includes/types/integerfloats/double_fields cu/includes/types/integerfloats/decimal_double cu/engine/runtime/scriptura \
                cu/includes/types/integers/exact_record "${INGEST[@]}"; do
    # A folder with no C file leaves the pattern unexpanded, and it names no file to build.
    for source in "$TOP/src/$portable"/*.c; do
        [ -f "$source" ] && listed "$source" "${C_WORDS[@]}"
    done
done
for name in exact_integer_{add,limbs,multiply,divide,gcd,decimal,hash}; do
    listed "$EXACT_ROOT/$name.c" "${EXACT_WORDS[@]}"
done
wait
if [ -f "$FAILED" ]; then
    while read -r source log; do
        echo "  build failed: ${source#"$TOP/"} did not compile"
        tail -20 "$log"
    done < "$FAILED"
    exit 1
fi
echo "  compiled $COMPILED of ${#OBJECTS[@]} objects, $JOBS at once, in $(( $(date +%s) - BEGAN )) s; the rest were current"

BEGAN=$(date +%s)
nvcc "${HOST_FLAGS[@]}" -shared "${LINK_FLAGS[@]}" -o "$LIBRARY" "${OBJECTS[@]}"
NVCC_STATUS=$?

if [ "$NVCC_STATUS" -ne 0 ] || [ ! -f "$LIBRARY" ]; then
    echo "  build failed: nvcc exited $NVCC_STATUS"
    exit 1
fi
echo "  linked $LIBRARY in $(( $(date +%s) - BEGAN )) s"
# the objects linked, a line each, for a program that links the engine into itself: on Windows the library gives a
# program only the calls the engine marks for export
printf '%s\n' "${OBJECTS[@]}" > "$OUT/cell_tracking_engine.objects"
build_publish "$LIBRARY" || exit 1
