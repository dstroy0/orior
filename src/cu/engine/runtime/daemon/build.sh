#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# tessera's host programs: the system's daemon, and tessera_run, the host job's wrapper. The script builds both into
# $BUILD_OUT, or a stamped folder, and publishes them together into build/tessera_host, where every host job and every
# client finds them. It runs through `harness.py build tessera`, and under WSL for Linux.
#
# The script first moves a copy that runs aside under another name. Windows lets a program that runs take a new name,
# but not a new body, and the jobs under the copy keep it. The script deletes the copies that it set aside before, once nothing
# runs them.
set -u

DAEMON="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$DAEMON/../../../../.." && pwd)"
ENGINE="$TOP/src/cu/engine"
source "$TOP/includes/file_defs/file_defs.sh"
source "$TOP/includes/exact_types/exact_types.sh"
source "$TOP/utils/maint/engine/build_stamp.sh"
source "$TOP/utils/maint/engine/tessera_build.sh"
build_stamp tessera_host

HOST_FLAGS=()
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        SUFFIX=.exe
        MSVC_BIN="$(ls -d "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC"/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
        if [ -z "$MSVC_BIN" ]; then
            MSVC_BIN="$(ls -d "/c/Program Files/Microsoft Visual Studio"/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
        fi
        [ -n "$MSVC_BIN" ] || { echo "  no host compiler nvcc accepts on this platform was found."; exit 1; }
        HOST_FLAGS=(-ccbin "$MSVC_BIN" -Xcompiler /Zc:preprocessor)
        ;;
    *)
        SUFFIX=
        HOST_FLAGS=(-Xcompiler -fPIC)
        ;;
esac
INCLUDES=(-I "$ENGINE" -I "$TOP/includes/exact_types/integers" "${FILE_DEFS_MMGR_FLAGS[@]}" "${TESSERA_INCLUDES[@]}")
if command -v nvcc > /dev/null 2>&1; then
    # the seal compiles with -rdc=true for the device of this machine, and the daemon makes no CUDA context
    CAP="$(nvidia-smi --query-gpu=compute_cap --format=csv,noheader 2>/dev/null | head -1 | tr -d ' .')"
    GENCODE=(-gencode "arch=compute_${CAP:-86},code=sm_${CAP:-86}")
    exact_types_device_line || exit 1
    tessera_build "${EXACT_TYPES_DEVICE_LINE[@]}" || exit 1
    tessera_run_build "${EXACT_TYPES_DEVICE_LINE[@]}" || exit 1
else
    # A host with no CUDA toolchain compiles the C with cc and the seal as C++, and links with c++. A Pi is such a
    # host, and so is a WSL distribution without CUDA. Each program links the MMgr sources, the platform and the carcer
    # of the pool
    DAEMON_DIRECTORY="$ENGINE/runtime/daemon"
    SEAL_DIRECTORY="$ENGINE/runtime/obsignatio"
    exact_types_compile "${FILE_DEFS_LINEA[@]}" "${EXACT_TYPES_PLATFORM[@]}" "${EXACT_TYPES_CARCER[@]}" || exit 1
    LINK=("${EXACT_TYPES_OBJECTS[@]}" "${EXACT_TYPES_LIBRARIES[@]}")
    compiled()
    {
        local object="$OUT/$(basename "$1" .c).o"
        cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 "${INCLUDES[@]}" -c "$1" -o "$object" || return 1
        echo "$object"
    }
    SHARED=()
    for source in "$DAEMON_DIRECTORY"/tessera_{paths,frame,self}.c; do
        SHARED+=("$(compiled "$source")") || exit 1
    done
    DAEMON_OBJECTS=()
    for source in "$DAEMON_DIRECTORY"/tessera_{ledger,measure}.c "$DAEMON_DIRECTORY"/tessera_daemon_{state,admission,peers,main}.c; do
        DAEMON_OBJECTS+=("$(compiled "$source")") || exit 1
    done
    RUN_OBJECTS=()
    for source in "$DAEMON_DIRECTORY"/tessera_client_{socket,jobs}.c "${PLATFORM_PROGRAMS[@]}" \
                  "$DAEMON_DIRECTORY"/tessera_run_{common,child,main}.c "${PLATFORM_TESSERA_RUN_OWN[@]}"; do
        RUN_OBJECTS+=("$(compiled "$source")") || exit 1
    done
    SEAL=()
    for name in obsignatio_hash obsignatio_seal; do
        "$FILE_DEFS_CXX" "${FILE_DEFS_LTO_CC[@]}" -std=c++20 -O2 "${INCLUDES[@]}" -c -x c++ "$SEAL_DIRECTORY/$name.cu" \
            -o "$OUT/$name.o" || exit 1
        SEAL+=("$OUT/$name.o")
    done
    rm -f "$OUT/tessera_daemon" "$OUT/tessera_run"
    "$FILE_DEFS_CXX" "${FILE_DEFS_LTO_CC[@]}" -o "$OUT/tessera_daemon" "${SHARED[@]}" "${DAEMON_OBJECTS[@]}" "${SEAL[@]}" \
        "${LINK[@]}" -ldl -lpthread || exit 1
    "$FILE_DEFS_CXX" "${FILE_DEFS_LTO_CC[@]}" -o "$OUT/tessera_run" "${SHARED[@]}" "${RUN_OBJECTS[@]}" "${SEAL[@]}" \
        "${LINK[@]}" -ldl -lpthread || exit 1
fi

HOST="$TOP/build/tessera_host"
mkdir -p "$HOST"
rm -f "$HOST"/*.replaced 2> /dev/null
PUBLISHED=1
for name in "tessera_run$SUFFIX" "tessera_daemon$SUFFIX"; do
    if [ -f "$HOST/$name" ]; then
        mv -f "$HOST/$name" "$HOST/$name.$(date +%Y%m%d_%H%M%S).replaced" || PUBLISHED=0
    fi
    cp -f "$OUT/$name" "$HOST/$name" || PUBLISHED=0
done
if [ "$PUBLISHED" -ne 1 ]; then
    echo "  build failed: could not publish tessera_run and its daemon to $HOST"
    exit 1
fi
echo "  published $HOST/tessera_run$SUFFIX and $HOST/tessera_daemon$SUFFIX"
