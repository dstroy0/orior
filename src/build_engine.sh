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

EXACT_ROOT="${ORIOR_EXACT_ROOT:-$TOP/includes/exact_types/integers}"
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
EXACT_LIMBS="${ORIOR_EXACT_LIMBS:-$FITTED_LIMBS}"
EXACT_DIGITS=$(( (EXACT_LIMBS * 32 * 1000 - 1) / 3322 ))
if [ "$EXACT_DIGITS" -gt 1024 ]; then
    EXACT_DIGITS=1024
fi
[ -f "$EXACT_ROOT/exact_integer.h" ] || { echo "  build failed: no exact_integer.h under $EXACT_ROOT"; exit 1; }
EXACT_FLAGS=(-I "$EXACT_ROOT" "-DORIOR_EXACT_LIMBS=${EXACT_LIMBS}u" "-DORIOR_EXACT_DIGITS=${EXACT_DIGITS}u")

DEFINES=(-DBODY_OVERLAP_BUILD_DLL=1 -DHEAVIEST_MATCHING_BUILD_DLL=1
         -DSHIFT_AGREEMENT_BUILD_DLL=1)
MODULES=(engine/formats/stack ../includes/formats/stack ../includes/file_defs/krep
         engine/analysis/compression cu/engine/analysis/compression engine/analysis/tower
         cu/engine/analysis/tower engine/runtime/device_pool cu/engine/runtime/device_pool
         engine/analysis/entropy_history cu/engine/analysis/entropy_history engine/analysis/noise_detector
         cu/engine/analysis/noise_detector engine/runtime/schedule cu/engine/runtime/schedule
         engine/compiler/keymath cu/engine/analysis/keymath engine/compiler/key_schedule
         cu/engine/analysis/key_schedule engine/compiler/cycle cu/engine/analysis/cycle
         engine/compiler/codegen cu/engine/rmc ../includes/parsers ../includes/file_defs/krs/ruleset_flat ../includes/file_defs/klq engine/runtime/radix_keys
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
         engine/nbody/heaviest_matching engine/arithmetic/double_fields ../includes/exact_types/integerfloats/double_fields engine/arithmetic/decimal_double
         engine/analysis/period cu/engine/analysis/period)
INGEST=(../includes/formats/cfg_json ../includes/formats/zarr ../includes/codecs/zstd ../includes/codecs/inflate
        ../includes/codecs/deflate ../includes/codecs/lz4 ../includes/codecs/snappy ../includes/codecs/blosc
        ../includes/formats/tiff ../includes/formats/hdf5 ../includes/codecs/zip ../includes/formats/dicom
        ../includes/formats/npy ../includes/formats/nrrd ../includes/formats/nifti
        ../includes/formats/parquet)
MODULES+=("${INGEST[@]}")
MODULE_INCLUDES=(-I "$TOP/src/cu/engine" -I "$TOP/includes/codecs/crc")
# MMgr's line, which the engine writes its text through: its flags for every source, its sources built with the C ones
source "$TOP/includes/file_defs/file_defs.sh"
source "$TOP/includes/exact_types/exact_types.sh"
# the exact sources reach MMgr and embedded_types through their own headers
EXACT_FLAGS+=("${FILE_DEFS_MMGR_FLAGS[@]}")
MODULE_INCLUDES+=("${FILE_DEFS_MMGR_FLAGS[@]}")
MODULE_SOURCES=("$TOP/src/cu/engine"/engine_{record,residual,files,zarr,source,listing,seal,report,history}.cu)
for module in "${MODULES[@]}"; do
    MODULE_INCLUDES+=(-I "$TOP/src/$module")
    for source in "$TOP/src/$module"/*.cu; do
        [ -f "$source" ] && MODULE_SOURCES+=("$source")
    done
done
# Each source compiles to an object of its own through shared_objects_built, one time for its words. An object compiles
# again only where its source or a header that its last compile read is newer. As many compile at once as the host has
# processors, or COMPILE_CACHE_JOBS where the harness sets it. The build links the library once from the objects.
#
# The .cu sources reach the tables of MMgr in each pass of nvcc. They compile with -rdc=true, and the library links
# with the device link. The line of MMgr, the cellblock and the device hooks of the platform compile as CUDA
# (exact_types_device_line). The library links them in place of their C objects
CU_STANDARD=-std=c++20
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        C_WORDS=(nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC[@]}" -Xcompiler "/std:c11 /O2 /experimental:c11atomics" "${DEFINES[@]}" "${MODULE_INCLUDES[@]}" "${EXACT_FLAGS[@]}")
        EXACT_WORDS=(nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC[@]}" -Xcompiler "/std:c11 /O2" "${EXACT_FLAGS[@]}")
        ;;
    *)
        C_WORDS=(cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -g -fPIC "${MODULE_INCLUDES[@]}" "${EXACT_FLAGS[@]}")
        EXACT_WORDS=(cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -g -fPIC "${EXACT_FLAGS[@]}")
        ;;
esac
CU_WORDS=(nvcc "${HOST_FLAGS[@]}" "$CU_STANDARD" -O2 -fmad=false -rdc=true "${GENCODE[@]}" "${DEFINES[@]}"
          "${MODULE_INCLUDES[@]}" "${EXACT_FLAGS[@]}")

BEGAN=$(date +%s)
shared_objects_built CU_WORDS "${MODULE_SOURCES[@]}" || exit 1
OBJECTS=("${SHARED_OBJECTS[@]}")
COMPILED=$SHARED_OBJECTS_COMPILED
# The C sources: the portable C of the engine, the member reader of a query file and the platform of the program. The
# ruleset reader reads the blob of a ruleset through the member reader, and exact_types takes its cells from the
# platform. A folder with no C file leaves the pattern unexpanded, and it names no file to build.
C_SOURCES=()
for portable in cu/engine/nbody/body_overlap cu/engine/nbody/heaviest_matching cu/engine/analysis/shift_agreement \
                cu/engine/nbody/max_tree cu/engine/analysis/cycle cu/engine/nbody/marginal \
                ../includes/exact_types/integerfloats/double_fields ../includes/exact_types/integerfloats/decimal_double \
                ../includes/exact_types/integers/exact_record "${INGEST[@]}"; do
    for source in "$TOP/src/$portable"/*.c; do
        [ -f "$source" ] && C_SOURCES+=("$source")
    done
done
C_SOURCES+=("$TOP/includes/file_defs/qry/writer/qry_member.c" "$TOP/includes/file_defs/qry/writer/platform/"*.c
    "$TOP/includes/file_defs/qry/qry_read.c" "${EXACT_TYPES_PLATFORM_LIBRARY[@]}")
shared_objects_built C_WORDS "${C_SOURCES[@]}" || exit 1
OBJECTS+=("${SHARED_OBJECTS[@]}")
COMPILED=$((COMPILED + SHARED_OBJECTS_COMPILED))
shared_objects_built EXACT_WORDS "$EXACT_ROOT"/exact_integer_{add,limbs,multiply,divide,gcd,decimal,hash}.c || exit 1
OBJECTS+=("${SHARED_OBJECTS[@]}")
COMPILED=$((COMPILED + SHARED_OBJECTS_COMPILED))
exact_types_device_line || exit 1
OBJECTS+=("${EXACT_TYPES_DEVICE_LINE[@]}")
COMPILED=$((COMPILED + SHARED_OBJECTS_COMPILED))
echo "  compiled $COMPILED of ${#OBJECTS[@]} objects, $SHARED_OBJECTS_JOBS at once, in $(( $(date +%s) - BEGAN )) s; the rest were current"

BEGAN=$(date +%s)
nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -shared -rdc=true "${GENCODE[@]}" "${LINK_FLAGS[@]}" -o "$LIBRARY" "${OBJECTS[@]}"
NVCC_STATUS=$?

if [ "$NVCC_STATUS" -ne 0 ] || [ ! -f "$LIBRARY" ]; then
    echo "  build failed: nvcc exited $NVCC_STATUS"
    exit 1
fi
echo "  linked $LIBRARY in $(( $(date +%s) - BEGAN )) s"
# The objects linked, a line each, for a program that links the engine into itself. On Windows the library gives a
# program only the calls that the engine marks for export
printf '%s\n' "${OBJECTS[@]}" > "$OUT/cell_tracking_engine.objects"
build_publish "$LIBRARY" || exit 1
