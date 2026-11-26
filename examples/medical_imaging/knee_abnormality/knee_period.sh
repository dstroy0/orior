#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Builds knee_period, the knee's period pass on the record machine, with the engine it calls and the tessera daemon
# beside it. It prints the binary's path on its last line.
#
#   Usage:  bash examples/medical_imaging/knee_abnormality/knee_period.sh [sm_NN ...]
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../.." && pwd)"
source "$TOP/utils/maint/engine/build_stamp.sh"
source "$TOP/utils/maint/engine/tessera_build.sh"
build_stamp knee_period

HOST_FLAGS=()
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        BINARY="$OUT/knee_period.exe"
        MSVC_BIN="$(ls -d "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC"/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
        if [ -z "$MSVC_BIN" ]; then
            MSVC_BIN="$(ls -d "/c/Program Files/Microsoft Visual Studio"/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
        fi
        if [ -z "$MSVC_BIN" ]; then
            echo "  no host compiler nvcc accepts on this platform was found."
            exit 1
        fi
        HOST_FLAGS=(-ccbin "$MSVC_BIN" -Xcompiler /Zc:preprocessor)
        LINK_FLAGS=(-Xlinker /MANIFEST:EMBED -Xlinker "/MANIFESTINPUT:$(cygpath -m "$ENGINE/../../cu/long_paths.manifest")")
        ;;
    *)
        BINARY="$OUT/knee_period"
        HOST_FLAGS=(-Xcompiler -fPIC)
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

# the exact integer's width, fitted to the engine's widest question as the driver builds fit it
EXACT_ROOT="$ENGINE/../../../includes/exact_types/integers"
RESIDUAL_LIMBS="$(sed -n 's/^#define ENGINE_RESIDUAL_LIMBS \([0-9]*\)u.*/\1/p' "$ENGINE"/engine_config_*.h)"
RECORD_LIMBS="$(sed -n 's/^#define ENGINE_RECORD_LIMBS_MAX \([0-9]*\)u.*/\1/p' "$ENGINE"/engine_config_*.h)"
[ -n "$RESIDUAL_LIMBS" ] && [ -n "$RECORD_LIMBS" ] || { echo "  build failed: engine_config_*.h widths"; exit 1; }
QUESTION_LIMBS=$((RESIDUAL_LIMBS + 1))
[ "$RECORD_LIMBS" -gt "$QUESTION_LIMBS" ] && QUESTION_LIMBS="$RECORD_LIMBS"
EXACT_LIMBS=1
while [ "$EXACT_LIMBS" -lt "$QUESTION_LIMBS" ]; do
    EXACT_LIMBS=$((EXACT_LIMBS * 2))
done
EXACT_DIGITS=$(( (EXACT_LIMBS * 32 * 1000 - 1) / 3322 ))
[ "$EXACT_DIGITS" -gt 1024 ] && EXACT_DIGITS=1024
EXACT_FLAGS=(-I "$EXACT_ROOT" "-DORIOR_EXACT_LIMBS=${EXACT_LIMBS}u" "-DORIOR_EXACT_DIGITS=${EXACT_DIGITS}u")

MODULES=(../includes/formats/stack ../includes/file_defs/krep cu/engine/analysis/compression cu/engine/analysis/tower
         cu/engine/runtime/device_pool cu/engine/analysis/entropy_history cu/engine/analysis/noise_detector
         cu/engine/runtime/schedule cu/engine/analysis/keymath cu/engine/analysis/key_schedule
         cu/engine/analysis/cycle cu/engine/rmc ../includes/parsers ../includes/file_defs/krs/ruleset_flat ../includes/file_defs/klq
         cu/engine/runtime/radix_keys
         cu/engine/analysis/unit_sweep cu/engine/runtime/obsignatio cu/engine/analysis/residual
         cu/engine/nbody/max_tree cu/engine/nbody/flatten cu/engine/analysis/golden_bands
         cu/engine/analysis/residual_survey cu/engine/nbody/grow cu/engine/analysis/shift_agreement
         cu/engine/nbody/climb_machine cu/engine/nbody/body_overlap cu/engine/nbody/fingerprint
         cu/engine/nbody/print_pair cu/engine/nbody/velocity cu/engine/nbody/division cu/engine/nbody/marginal
         cu/engine/nbody/contact_side cu/engine/nbody/box_history cu/engine/nbody/heaviest_matching
         ../includes/exact_types/integerfloats/double_fields ../includes/exact_types/integerfloats/decimal_double
         cu/engine/analysis/period ../includes/exact_types/integers/exact_record)
INGEST=(../includes/formats/cfg_json ../includes/formats/zarr ../includes/codecs/zstd ../includes/codecs/inflate
        ../includes/codecs/deflate ../includes/codecs/lz4 ../includes/codecs/snappy ../includes/codecs/blosc
        ../includes/formats/tiff ../includes/formats/hdf5 ../includes/codecs/zip ../includes/formats/dicom
        ../includes/formats/npy ../includes/formats/nrrd ../includes/formats/nifti
        ../includes/formats/parquet)
MODULES+=("${INGEST[@]}")
INCLUDES=(-I "$ENGINE" -I "$ENGINE/../../../includes/codecs/crc" -I "$ENGINE/runtime/daemon")
SOURCES=("$HERE/knee_period.cu" "$ENGINE"/engine_{record,residual,files,zarr,source,listing,seal,report,history}.cu)
for module in "${MODULES[@]}"; do
    folder="$(build_path "$module")"
    [ -d "$folder" ] || { echo "  build failed: no $module"; exit 1; }
    INCLUDES+=(-I "$folder")
    for source in "$folder"/*.cu; do
        [ -f "$source" ] && SOURCES+=("$source")
    done
done
INCLUDES+=("${TESSERA_INCLUDES[@]}")
# MMgr's line, which the engine writes its text through: its flags for every source, its sources among the portable ones
source "$ENGINE/../../../includes/file_defs/file_defs.sh"
source "$ENGINE/../../../includes/exact_types/exact_types.sh"
# the exact sources reach MMgr and embedded_types through their own headers
EXACT_FLAGS+=("${FILE_DEFS_MMGR_FLAGS[@]}")
INCLUDES+=("${FILE_DEFS_MMGR_FLAGS[@]}")

PORTABLE_SOURCES=()
for portable in cu/engine/nbody/body_overlap cu/engine/analysis/shift_agreement cu/engine/nbody/max_tree \
                cu/engine/analysis/cycle cu/engine/nbody/heaviest_matching cu/engine/nbody/marginal \
                ../includes/exact_types/integerfloats/double_fields ../includes/exact_types/integerfloats/decimal_double \
                ../includes/exact_types/integers/exact_record "${INGEST[@]}"; do
    PORTABLE_SOURCES+=("$(build_path "$portable")"/*.c)
done
# the member reader of a query file, which the ruleset reader reads the blob of a ruleset through
PORTABLE_SOURCES+=("$TOP/includes/file_defs/qry/writer/qry_member.c" "$TOP/includes/file_defs/qry/writer/platform/"*.c
    "$TOP/includes/file_defs/qry/qry_read.c" "${EXACT_TYPES_PLATFORM[@]}")
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        PORTABLE_WORDS=(nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC[@]}" -Xcompiler "/std:c11 /O2 /experimental:c11atomics" "${INCLUDES[@]}" "${EXACT_FLAGS[@]}")
        EXACT_WORDS=(nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC[@]}" -Xcompiler "/std:c11 /O2" "${EXACT_FLAGS[@]}")
        ;;
    *)
        PORTABLE_WORDS=(cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -fPIC "${INCLUDES[@]}" "${EXACT_FLAGS[@]}")
        EXACT_WORDS=(cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -fPIC "${EXACT_FLAGS[@]}")
        ;;
esac
# Each source compiles one time for its words (shared_objects.sh). The line of MMgr, the cellblock and the device hooks
# compile as CUDA (exact_types_device_line). The program and the tessera daemon link them with nvcc's device link
exact_types_device_line || exit 1
LINEA_OBJECTS=("${EXACT_TYPES_DEVICE_LINE[@]}")
shared_objects_built PORTABLE_WORDS "${PORTABLE_SOURCES[@]}" || exit 1
OBJECTS=("${LINEA_OBJECTS[@]}" "${SHARED_OBJECTS[@]}")
shared_objects_built EXACT_WORDS "$EXACT_ROOT"/exact_integer_{add,limbs,multiply,divide,gcd,decimal,hash}.c || exit 1
OBJECTS+=("${SHARED_OBJECTS[@]}")
# every load, draw and read is a job on the device's tessera daemon, built beside the binary
tessera_build "${LINEA_OBJECTS[@]}" || exit 1

rm -f "$BINARY"
nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -std=c++20 -O2 -fmad=false -rdc=true "${GENCODE[@]}" "${LINK_FLAGS[@]}" "${INCLUDES[@]}" "${EXACT_FLAGS[@]}" \
    -o "$BINARY" "${SOURCES[@]}" "${OBJECTS[@]}" "${TESSERA_OBJECTS[@]}"
[ -f "$BINARY" ] || { echo "  build failed: nvcc could not build knee_period"; exit 1; }
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) cygpath -m "$BINARY" ;;
    *) echo "$BINARY" ;;
esac
