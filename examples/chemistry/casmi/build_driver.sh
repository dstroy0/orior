#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# casmi_driver (src/casmi_driver) with the engine it runs on, its core and the modules it reaches, compiled beside it,
# and the device's tessera daemon built beside it. Arguments are architectures, sm_86 and the like
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$ROOT/../../.." && pwd)"
source "$TOP/utils/maint/engine/build_stamp.sh"
source "$TOP/utils/maint/engine/tessera_build.sh"
build_stamp casmi_driver

HOST_FLAGS=()
LINK_FLAGS=()
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        BINARY="$OUT/casmi_driver.exe"
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
        LINK_FLAGS=(-Xlinker /MANIFEST:EMBED -Xlinker "/MANIFESTINPUT:$(cygpath -m "$ENGINE/../../cu/long_paths.manifest")")
        ;;
    *)
        BINARY="$OUT/casmi_driver"
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

EXACT_ROOT="$ENGINE/../types/integers"
RESIDUAL_LIMBS="$(sed -n 's/^#define ENGINE_RESIDUAL_LIMBS \([0-9]*\)u.*/\1/p' "$ENGINE"/engine_config_*.h)"
[ -n "$RESIDUAL_LIMBS" ] || { echo "  build failed: no ENGINE_RESIDUAL_LIMBS in engine_config_*.h"; exit 1; }
QUESTION_LIMBS=$((RESIDUAL_LIMBS + 1))
RECORD_LIMBS="$(sed -n 's/^#define ENGINE_RECORD_LIMBS_MAX \([0-9]*\)u.*/\1/p' "$ENGINE"/engine_config_*.h)"
[ -n "$RECORD_LIMBS" ] || { echo "  build failed: no ENGINE_RECORD_LIMBS_MAX in engine_config_*.h"; exit 1; }
[ "$RECORD_LIMBS" -gt "$QUESTION_LIMBS" ] && QUESTION_LIMBS="$RECORD_LIMBS"
FITTED_LIMBS=1
while [ "$FITTED_LIMBS" -lt "$QUESTION_LIMBS" ]; do
    FITTED_LIMBS=$((FITTED_LIMBS * 2))
done
EXACT_DIGITS=$(( (FITTED_LIMBS * 32 * 1000 - 1) / 3322 ))
[ "$EXACT_DIGITS" -gt 1024 ] && EXACT_DIGITS=1024
EXACT_FLAGS=(-I "$EXACT_ROOT" "-DANCHOR_EXACT_LIMBS=${FITTED_LIMBS}u" "-DANCHOR_EXACT_DIGITS=${EXACT_DIGITS}u")

# the engine's core reaches every module here; the reader formats are its sources
MODULES=(cu/includes/formats/cfg_json cu/includes/formats/stack cu/engine/parser
         cu/engine/analysis/compression cu/engine/analysis/tower cu/engine/runtime/device_pool
         cu/engine/analysis/entropy_history cu/engine/analysis/noise_detector cu/engine/runtime/schedule
         cu/engine/analysis/keymath cu/engine/analysis/key_schedule cu/engine/analysis/cycle
         cu/engine/rmc cu/types/file_defs/readers
         cu/engine/runtime/radix_keys cu/engine/analysis/unit_sweep cu/engine/runtime/obsignatio
         cu/engine/analysis/residual cu/engine/nbody/max_tree cu/engine/nbody/flatten cu/engine/analysis/golden_bands
         cu/engine/analysis/residual_survey cu/engine/nbody/grow cu/engine/analysis/shift_agreement
         cu/engine/nbody/climb_machine cu/engine/nbody/body_overlap cu/engine/nbody/fingerprint
         cu/engine/nbody/print_pair cu/engine/nbody/velocity cu/engine/nbody/division cu/engine/nbody/marginal
         cu/engine/nbody/contact_side cu/engine/nbody/box_history cu/engine/nbody/heaviest_matching
         cu/types/integerfloats/double_fields cu/types/integerfloats/decimal_double cu/types/integerfloats/edouble
         cu/types/integers/exact_record cu/engine/runtime/scriptura)
INGEST=(cu/includes/formats/zarr cu/includes/codecs/zstd cu/includes/codecs/inflate cu/includes/codecs/deflate
        cu/includes/codecs/lz4 cu/includes/codecs/snappy cu/includes/codecs/blosc cu/includes/formats/tiff
        cu/includes/formats/hdf5 cu/includes/codecs/zip cu/includes/formats/dicom cu/includes/formats/npy
        cu/includes/formats/nrrd cu/includes/formats/nifti)
MODULES+=("${INGEST[@]}")
INCLUDES=(-I "$ENGINE" -I "$ENGINE/../includes/codecs/crc" -I "$TOP/src/sims/cu" -I "$ROOT/src")
SOURCES=("$ENGINE"/engine_{record,residual,files,zarr,source,listing,seal,report,history}.cu)
for module in "${MODULES[@]}"; do
    folder="$(build_path "$module")"
    [ -d "$folder" ] || { echo "  build failed: no $module"; exit 1; }
    INCLUDES+=(-I "$folder")
    for source in "$folder"/*.cu; do
        [ -f "$source" ] && SOURCES+=("$source")
    done
done
INCLUDES+=("${TESSERA_INCLUDES[@]}")

OBJECTS=()
SCRIPTURA_OBJECTS=()
PORTABLE=()
for module in cu/engine/nbody/body_overlap cu/engine/analysis/shift_agreement cu/includes/formats/cfg_json \
              cu/engine/nbody/max_tree cu/engine/analysis/cycle cu/engine/nbody/heaviest_matching cu/engine/nbody/marginal \
              cu/types/integerfloats/double_fields cu/types/integerfloats/decimal_double cu/types/integerfloats/edouble \
              cu/types/integers/exact_record cu/engine/runtime/scriptura "${INGEST[@]}"; do
    for source in "$(build_path "$module")"/*.c; do
        [ -f "$source" ] && PORTABLE+=("$source")
    done
done
for name in exact_integer_{add,limbs,multiply,divide,gcd,decimal,hash}; do
    PORTABLE+=("$EXACT_ROOT/$name.c")
done
PORTABLE+=("$ROOT/src/parquet/parquet.c")
for source in "${PORTABLE[@]}"; do
    object="$OUT/$(basename "$source" .c)_casmi_driver.$EXTENSION"
    rm -f "$object"
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*)
            nvcc "${HOST_FLAGS[@]}" -Xcompiler "/std:c11 /O2" "${INCLUDES[@]}" "${EXACT_FLAGS[@]}" -c "$source" -o "$object" ;;
        *)
            cc -std=c11 -O2 -fPIC "${INCLUDES[@]}" "${EXACT_FLAGS[@]}" -c "$source" -o "$object" ;;
    esac
    [ -f "$object" ] || { echo "  build failed: $(basename "$source") did not compile"; exit 1; }
    OBJECTS+=("$object")
    case "$(basename "$source")" in
        scriptura*) SCRIPTURA_OBJECTS+=("$object") ;;
    esac
done
tessera_build casmi_driver "${SCRIPTURA_OBJECTS[@]}" || exit 1

rm -f "$BINARY"
nvcc "${HOST_FLAGS[@]}" -std=c++17 -O2 -fmad=false "${GENCODE[@]}" "${LINK_FLAGS[@]}" "${INCLUDES[@]}" "${EXACT_FLAGS[@]}" \
    -o "$BINARY" "$ROOT/src/casmi_driver/casmi_driver.cu" "$ROOT/src/forms/forms.cu" "$TOP/src/sims/cu/sim_job.cu" \
    "${SOURCES[@]}" \
    "${OBJECTS[@]}" "${TESSERA_OBJECTS[@]}"
[ -f "$BINARY" ] || { echo "  build failed: nvcc could not build casmi_driver"; exit 1; }
echo "  built $BINARY"
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) DAEMON="$OUT/tessera_daemon.exe" ;; *) DAEMON="$OUT/tessera_daemon" ;; esac
build_publish "$DAEMON" || exit 1
build_publish "$BINARY" || exit 1
