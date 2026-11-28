#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
set -u

SIMS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SIMS_CU="$SIMS/cu"
TOP="$(cd "$SIMS/.." && pwd)"
SIM="${1:-}"
shift || true
case "$SIM" in
    nbody_lattice|knf_identity|noise_floor|noise_terms|noise_root|period_power|root_universal|povm_state|ka_psi|chaitin_omega|goodstein|pi_tower|pi_plane|omega_computer|floor_match|floor_track|fixed_pattern|classify_reject_recover) ;;
    *)
        echo "  usage: run.sh nbody_lattice|knf_identity|noise_floor|noise_terms|noise_root|period_power|root_universal|povm_state|ka_psi|chaitin_omega|goodstein|pi_tower|pi_plane|omega_computer|floor_match|floor_track|fixed_pattern|classify_reject_recover [-- sim arguments]"
        exit 2
        ;;
esac
SIM_ARGUMENTS=()
if [ "${1:-}" = "--" ]; then
    shift
    SIM_ARGUMENTS=("$@")
fi

source "$TOP/../includes/file_defs/file_defs.sh"

source "$TOP/../includes/exact_types/exact_types.sh"
NO_ROUNDING="$TOP/../includes/exact_types/integers"
PERIOD="$TOP/cu/engine/analysis/period"
PERIOD_CU="$TOP/cu/engine/analysis/period"
TOWER="$TOP/cu/engine/analysis/tower"
TOWER_CU="$TOP/cu/engine/analysis/tower"
DEVICE_POOL="$TOP/cu/engine/runtime/device_pool"
DEVICE_POOL_CU="$TOP/cu/engine/runtime/device_pool"
COMPRESSION="$TOP/cu/engine/analysis/compression"
COMPRESSION_CU="$TOP/cu/engine/analysis/compression"
CYCLE="$TOP/cu/engine/analysis/cycle"
CYCLE_CU="$TOP/cu/engine/analysis/cycle"
CODEGEN="$TOP/cu/engine/rmc"
CODEGEN_CU="$TOP/cu/engine/rmc"
CODEGEN_CU_2="$TOP/../includes/parsers"
KEYMATH="$TOP/cu/engine/analysis/keymath"
KEYMATH_CU="$TOP/cu/engine/analysis/keymath"
KEY_SCHEDULE="$TOP/cu/engine/analysis/key_schedule"
KEY_SCHEDULE_CU="$TOP/cu/engine/analysis/key_schedule"
ENTROPY_HISTORY="$TOP/cu/engine/analysis/entropy_history"
ENTROPY_HISTORY_CU="$TOP/cu/engine/analysis/entropy_history"
NOISE_DETECTOR="$TOP/cu/engine/analysis/noise_detector"
NOISE_DETECTOR_CU="$TOP/cu/engine/analysis/noise_detector"
# utils/maint/ is at the repository's root, one above src/
source "$(cd "$TOP/.." && pwd)/utils/maint/engine/build_stamp.sh"
build_stamp "sim_$SIM"

# every sim's folder lies under cu/, at the place in the tree its subject has, and carries the sim's name
case "$SIM" in
    fixed_pattern|classify_reject_recover) SOURCES=("$SIMS/cu/engine/analysis/art/$SIM.cu") ;;
    *)
        FOLDER="$(find "$SIMS_CU" -type d -name "$SIM" | head -1)"
        [ -n "$FOLDER" ] || { echo "  no folder named $SIM under $SIMS_CU"; exit 2; }
        SOURCES=("$FOLDER"/*.cu)
        ;;
esac
NOISE_DETECTOR_SOURCES=("$NOISE_DETECTOR_CU"/noise_detector_{flicker,lines,lines_summary,neighbors,clips,pixels}.cu
                        "$NOISE_DETECTOR_CU"/noise_detector_{moments,ladder,crosstalk,structure,halves,root}.cu)
MODULE_SOURCES=()
if [ "$SIM" = "period_power" ]; then
    MODULE_SOURCES+=("$PERIOD_CU"/period_{measure,select}.cu "$DEVICE_POOL_CU/device_pool.cu")
fi
if [ "$SIM" = "root_universal" ]; then
    MODULE_SOURCES+=("$TOWER_CU/tower.cu" "$TOWER_CU/tower_record.cu" "$TOWER_CU/tower_run.cu" "$DEVICE_POOL_CU/device_pool.cu" "$COMPRESSION_CU/compression.cu")
fi
if [ "$SIM" = "knf_identity" ]; then
    MODULE_SOURCES+=("$ENTROPY_HISTORY_CU/entropy_history.cu")
fi
# noise_terms reads each planted volume back through the noise detector's volume readings, and noise_floor fits its
# transfer curves by the detector's line over the level
if [ "$SIM" = "noise_terms" ] || [ "$SIM" = "noise_floor" ]; then
    MODULE_SOURCES+=("${NOISE_DETECTOR_SOURCES[@]}")
fi
# noise_root finds each planted term as a box's root noise, and prices it through the tower and compression's coder
if [ "$SIM" = "noise_root" ]; then
    MODULE_SOURCES+=("${NOISE_DETECTOR_SOURCES[@]}" "$TOWER_CU/tower.cu" "$TOWER_CU/tower_record.cu" "$TOWER_CU/tower_run.cu" "$DEVICE_POOL_CU/device_pool.cu"
                     "$COMPRESSION_CU/compression.cu")
fi
if [ "$SIM" = "floor_match" ] || [ "$SIM" = "floor_track" ]; then
    MODULE_SOURCES+=("$TOWER_CU/tower.cu" "$TOWER_CU/tower_record.cu" "$TOWER_CU/tower_run.cu" "$DEVICE_POOL_CU/device_pool.cu")
fi
# chaitin_omega runs its reduction and pi_tower its BBP terms, as programs on the engine's record machine
HOST_SOURCES=()
if [ "$SIM" = "chaitin_omega" ] || [ "$SIM" = "pi_tower" ]; then
    MODULE_SOURCES+=("$CYCLE_CU/cycle_compile_cache.cu" "$CYCLE_CU/cycle_compile.cu" "$CYCLE_CU/cycle_compile_route.cu" "$CYCLE_CU/cycle_compile_toolchain.cu" "$CYCLE_CU/cycle_launch.cu" "$CYCLE_CU/cycle_prelude.cu" "$CYCLE_CU/cycle_record.cu" "$CYCLE_CU/cycle_record_launch.cu" "$CYCLE_CU/cycle_sweep.cu" "$CODEGEN_CU/asm_printer_layout.cu" "$CODEGEN_CU/asm_printer_program.cu" "$CODEGEN_CU/code_generator.cu" "$CODEGEN_CU/code_generator_entries.cu" "$CODEGEN_CU/codegen.cu" "$CODEGEN_CU/codegen_lowering.cu" "$CODEGEN_CU/codegen_output.cu" "$CODEGEN_CU_2/codegen_reader.cu" "$CODEGEN_CU/codegen_rules.cu" "$CODEGEN_CU_2"/../file_defs/krs/ruleset_flat/*.cu "$CODEGEN_CU_2/target_parse.cu" "$CODEGEN_CU/target_rulesets.cu" "$KEYMATH_CU/keymath.cu" "$KEY_SCHEDULE_CU/key_schedule.cu")
    HOST_SOURCES+=("$CYCLE/cycle.c")
fi

HOST_FLAGS=()
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        BINARY="$OUT/$SIM.exe"
        MSVC_BIN="$(ls -d "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC"/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
        if [ -z "$MSVC_BIN" ]; then
            MSVC_BIN="$(ls -d "/c/Program Files/Microsoft Visual Studio"/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
        fi
        if [ -z "$MSVC_BIN" ]; then
            echo "  no host compiler nvcc accepts on this platform was found."
            exit 1
        fi
        HOST_FLAGS=(-ccbin "$MSVC_BIN" -Xcompiler /Zc:preprocessor)
        LONG_PATHS=(-Xlinker /MANIFEST:EMBED -Xlinker "/MANIFESTINPUT:$(cygpath -m "$TOP/cu/long_paths.manifest")")
        ;;
    *)
        BINARY="$OUT/$SIM"
        HOST_FLAGS=(-Xcompiler -fPIC)
        LONG_PATHS=()
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

DAEMON_DIRECTORY="$TOP/cu/engine/runtime/daemon"
OBSIGNATIO="$TOP/cu/engine/runtime/obsignatio"
OBSIGNATIO_CU="$TOP/cu/engine/runtime/obsignatio"
INCLUDES=(-I "$TOP/cu/engine" -I "$SIMS" -I "$SIMS_CU" "${FILE_DEFS_MMGR_FLAGS[@]}" -I "$NO_ROUNDING" -I "$PERIOD" -I "$PERIOD_CU" -I "$TOWER" -I "$TOWER_CU" -I "$DEVICE_POOL"
          -I "$COMPRESSION" -I "$CYCLE" -I "$CYCLE_CU" -I "$KEYMATH" -I "$KEYMATH_CU" -I "$KEY_SCHEDULE" -I "$KEY_SCHEDULE_CU" -I "$DAEMON_DIRECTORY" -I "$OBSIGNATIO" -I "$OBSIGNATIO_CU")
if [ "$SIM" = "knf_identity" ]; then
    INCLUDES+=(-I "$TOP/../includes/codecs/crc" -I "$TOP/../includes/codecs/crc" -I "$ENTROPY_HISTORY")
fi
if [ "$SIM" = "noise_terms" ] || [ "$SIM" = "noise_root" ] || [ "$SIM" = "noise_floor" ]; then
    INCLUDES+=(-I "$NOISE_DETECTOR" -I "$NOISE_DETECTOR_CU")
fi
# SIM_EXACT_LIMBS sets the exact integer's width in 32-bit limbs, a power of two, for every object the sim links
WIDTH=()
if [ -n "${SIM_EXACT_LIMBS:-}" ]; then
    WIDTH=(-DORIOR_EXACT_LIMBS="${SIM_EXACT_LIMBS}u")
fi
rm -f "$BINARY"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        SIM_WORDS=(nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC[@]}" -Xcompiler "/std:c11 /O2 /experimental:c11atomics" "${WIDTH[@]}" "${INCLUDES[@]}") ;;
    *)
        SIM_WORDS=(cc -std=c11 -O2 -fPIC "${FILE_DEFS_LTO_CC[@]}" "${WIDTH[@]}" "${INCLUDES[@]}") ;;
esac
# Each source compiles one time for its words (shared_objects.sh). The line of MMgr, the cellblock and the device hooks
# compile as CUDA (exact_types_device_line). The sim and the daemon link them with nvcc's device link. The process
# stage, the pool and the system calls of the platform go in the daemon too
exact_types_device_line || exit 1
LINEA_OBJECTS=("${EXACT_TYPES_DEVICE_LINE[@]}")
shared_objects_built SIM_WORDS "${EXACT_TYPES_PLATFORM[@]}" || exit 1
PLATFORM_OBJECTS=("${SHARED_OBJECTS[@]}")
shared_objects_built SIM_WORDS "${FILE_DEFS_FORMATS[@]}" "${EXACT_TYPES_INTEGERS[@]}" "${HOST_SOURCES[@]}" || exit 1
OBJECTS=("${LINEA_OBJECTS[@]}" "${PLATFORM_OBJECTS[@]}" "${SHARED_OBJECTS[@]}")

# A sim that uses the device is a job on the device's tessera daemon. The client goes into the sim, and the script
# builds the daemon beside it, where the sim starts it when none answers. The sources come as the two clients, then the
# three that the clients and the daemon both link, then the daemon's own
shared_objects_built SIM_WORDS "$DAEMON_DIRECTORY"/tessera_client_{socket,jobs}.c \
    "$DAEMON_DIRECTORY"/tessera_{paths,frame,self,ledger,measure}.c \
    "$DAEMON_DIRECTORY"/tessera_daemon_{state,admission,peers,main}.c || exit 1
OBJECTS+=("${SHARED_OBJECTS[@]:0:5}")
DAEMON_OBJECTS=("${SHARED_OBJECTS[@]:2}")
# the client starts the system's daemon through the programs of the platform, which the sim links
shared_objects_built SIM_WORDS "${PLATFORM_PROGRAMS[@]}" || exit 1
OBJECTS+=("${SHARED_OBJECTS[@]}")
SEAL_WORDS=(nvcc "${HOST_FLAGS[@]}" -std=c++20 -O2 -rdc=true "${GENCODE[@]}" "${WIDTH[@]}" "${INCLUDES[@]}")
shared_objects_built SEAL_WORDS "$OBSIGNATIO_CU"/obsignatio_{hash,seal}.cu || exit 1
SEAL_OBJECTS=("${SHARED_OBJECTS[@]}")
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) DAEMON="$OUT/tessera_daemon.exe"; DAEMON_LIBRARIES=(-lpdh) ;;
    *) DAEMON="$OUT/tessera_daemon"; DAEMON_LIBRARIES=(-ldl -lpthread) ;;
esac
rm -f "$DAEMON"
nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -rdc=true "${GENCODE[@]}" "${LONG_PATHS[@]}" -o "$DAEMON" "${DAEMON_OBJECTS[@]}" "${LINEA_OBJECTS[@]}" "${PLATFORM_OBJECTS[@]}" \
    "${SEAL_OBJECTS[@]}" "${DAEMON_LIBRARIES[@]}" "${EXACT_TYPES_LIBRARIES[@]}"
[ -f "$DAEMON" ] || { echo "  build failed: the tessera daemon did not link"; exit 1; }

nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -std=c++20 -O2 -rdc=true "${GENCODE[@]}" "${WIDTH[@]}" "${INCLUDES[@]}" "${LONG_PATHS[@]}" -o "$BINARY" \
    "${SOURCES[@]}" "$SIMS/cu/sim_job.cu" "${MODULE_SOURCES[@]}" "${OBJECTS[@]}" "${SEAL_OBJECTS[@]}"
[ -f "$BINARY" ] || { echo "  build failed: nvcc could not build $SIM"; exit 1; }

"$BINARY" "${SIM_ARGUMENTS[@]}"
STATUS=$?
echo "  $SIM exit $STATUS"
exit "$STATUS"
