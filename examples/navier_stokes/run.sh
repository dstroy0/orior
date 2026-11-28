#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds a driver on the host and runs it on the cfg named: bash run.sh core_rule cfg/core_rule.cfg
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$ROOT/.." && pwd)"
SRC="$(cd "$TOP/../src" && pwd)"
PROGRAM="${1:-}"
CFG="${2:-}"
PROGRAMS="core_rule|matching_rule|core_radius|core_tangent"
case "$PROGRAM" in
    core_rule|matching_rule|core_radius|core_tangent) ;;
    *) echo "  usage: run.sh $PROGRAMS <cfg>"; exit 2 ;;
esac
[ -n "$CFG" ] && [ -f "$CFG" ] || { echo "  usage: run.sh $PROGRAMS <cfg>"; exit 2; }
# the driver's modules, each src/<module>/<module>.cu with its header beside it
MODULES=(run_cfg record series_rule jet_rule core_radius_weights)
source "$TOP/../utils/maint/engine/build_stamp.sh"
build_stamp navier_stokes

SIMS_CU="$SRC/sims/cu"
source "$SRC/../includes/file_defs/file_defs.sh"
source "$SRC/../includes/exact_types/exact_types.sh"
NO_ROUNDING="$SRC/../includes/exact_types/integers"
CFG_JSON="$SRC/../includes/formats/cfg_json"
DAEMON_DIRECTORY="$SRC/cu/engine/runtime/daemon"
OBSIGNATIO="$SRC/cu/engine/runtime/obsignatio"
OBSIGNATIO_CU="$SRC/cu/engine/runtime/obsignatio"

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
        # an exact rational at the widest builds is kilobytes, and a driver holds many on its stack
        LINK_FLAGS=(-Xlinker /STACK:268435456)
        ;;
    *)
        BINARY="$OUT/$PROGRAM"
        HOST_FLAGS=(-Xcompiler -fPIC)
        LINK_FLAGS=()
        ;;
esac

# SIM_EXACT_LIMBS sets the exact integer's width in 32-bit limbs, a power of two, for every object linked
WIDTH=()
if [ -n "${SIM_EXACT_LIMBS:-}" ]; then
    WIDTH=(-DORIOR_EXACT_LIMBS="${SIM_EXACT_LIMBS}u")
fi
INCLUDES=(-I "$SRC/cu/engine" -I "$SIMS_CU" "${FILE_DEFS_MMGR_FLAGS[@]}" -I "$NO_ROUNDING" -I "$CFG_JSON"
          -I "$DAEMON_DIRECTORY" -I "$OBSIGNATIO" -I "$OBSIGNATIO_CU")
for module in "${MODULES[@]}"; do
    INCLUDES+=(-I "$ROOT/src/$module")
done

# Each source compiles one time for its words (shared_objects.sh), and again only where it or a header that its last
# compile read is newer. The .cu sources reach the tables of MMgr in each pass of nvcc, and they compile with
# -rdc=true. The line of MMgr, the cellblock and the device hooks compile as CUDA (exact_types_device_line). The program
# links them with the device link
CU_STANDARD=-std=c++20
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        C_WORDS=(nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC[@]}" -Xcompiler "/std:c11 /O2" "${WIDTH[@]}" "${INCLUDES[@]}") ;;
    *)
        C_WORDS=(cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -fPIC "${WIDTH[@]}" "${INCLUDES[@]}") ;;
esac
CU_WORDS=(nvcc "${HOST_FLAGS[@]}" "$CU_STANDARD" -O2 -rdc=true "${WIDTH[@]}" "${INCLUDES[@]}")
exact_types_device_line || exit 1
OBJECTS=("${EXACT_TYPES_DEVICE_LINE[@]}")
# sim_close releases a tessera job, and the drivers submit none. The build links the client, and nothing calls it
shared_objects_built C_WORDS "${EXACT_TYPES_INTEGERS[@]}" "${EXACT_TYPES_PLATFORM[@]}" "$CFG_JSON/cfg_json.c" \
    "$DAEMON_DIRECTORY"/tessera_client_{socket,jobs}.c "$DAEMON_DIRECTORY"/tessera_{paths,frame,self}.c \
    "${PLATFORM_PROGRAMS[@]}" || exit 1
OBJECTS+=("${SHARED_OBJECTS[@]}")
UNITS=("$OBSIGNATIO_CU"/obsignatio_{hash,seal}.cu)
for module in "${MODULES[@]}"; do
    UNITS+=("$ROOT/src/$module/$module.cu")
done
UNITS+=("$SIMS_CU/sim_job.cu" "$ROOT/src/$PROGRAM/$PROGRAM.cu")
shared_objects_built CU_WORDS "${UNITS[@]}" || exit 1
OBJECTS+=("${SHARED_OBJECTS[@]}")

rm -f "$BINARY"
nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -rdc=true "${LINK_FLAGS[@]}" -O2 -o "$BINARY" "${OBJECTS[@]}"
[ -f "$BINARY" ] || { echo "  build failed: nvcc could not build $PROGRAM"; exit 1; }

"$BINARY" "$CFG"
STATUS=$?
echo "  $PROGRAM exit $STATUS"
exit "$STATUS"
