#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

# A program that uses the device is a job on the device's tessera daemon (engine/runtime/daemon). The program submits
# it through sims/cu/sim_job.cu. TESSERA_INCLUDES are the headers the program and sim_job.cu read.
#
# `tessera_build` compiles the client objects that the program links, and the seal that signs its job. It builds the
# daemon beside the program, which starts it when none answers. It reads TOP, OUT, HOST_FLAGS, GENCODE and INCLUDES.
#
# It takes the objects of exact_types_device_line, the line, the cellblock and the device hooks, which the daemon links.
# Each object compiles through shared_objects_built, one time for its words. It sets the arrays TESSERA_OBJECTS and
# TESSERA_SEAL, and the objects TESSERA_LEDGER and TESSERA_MEASURE. After it, `tessera_run_build` builds tessera_run
# beside the daemon.
#
# The seal reaches the tables of MMgr in each pass of nvcc. It compiles with -rdc=true, and the daemon and tessera_run
# link with the device link.
TESSERA_INCLUDES=(-I "$ENGINE/../../sims/cu" -I "$ENGINE/runtime/daemon" -I "$ENGINE/runtime/obsignatio"
                  -I "$ENGINE/../../cu/engine/runtime/obsignatio")
# the daemon and tessera_run are programs of the platform, as every program is. They link its process stage, its pool
# and system calls, and the carcer of the pool. TESSERA_PLATFORM holds the stage, the pool and the system calls, which
# tessera_build compiles. The carcer comes with the line of the caller
source "$ENGINE/../../../includes/exact_types/exact_types.sh"

# the words that compile a C source of the daemon or of tessera_run, in the array that $1 names
tessera_words()
{
    local -n tessera_words_set="$1"
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*)
            tessera_words_set=(nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC[@]}" -Xcompiler "/std:c11 /O2" "${INCLUDES[@]}"
                               "${TESSERA_INCLUDES[@]}") ;;
        *)
            tessera_words_set=(cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -fPIC "${INCLUDES[@]}" "${TESSERA_INCLUDES[@]}") ;;
    esac
}

tessera_build()
{
    local linea_objects=("$@")
    local daemon_directory="$ENGINE/runtime/daemon"
    local includes=("${INCLUDES[@]}" "${TESSERA_INCLUDES[@]}")
    local daemon_objects=()
    local long_paths=()
    local daemon
    local daemon_libraries=()
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*)
            long_paths=(-Xlinker /MANIFEST:EMBED -Xlinker "/MANIFESTINPUT:$(cygpath -m "$ENGINE/../../cu/long_paths.manifest")")
            daemon="$OUT/tessera_daemon.exe"
            daemon_libraries=(-lpdh)
            ;;
        *)
            daemon="$OUT/tessera_daemon"
            daemon_libraries=(-ldl -lpthread)
            ;;
    esac
    # the two clients, then the three that the clients and the daemon both link, then the daemon's own
    local words
    tessera_words words
    shared_objects_built words "$daemon_directory"/tessera_client_{socket,jobs}.c \
        "$daemon_directory"/tessera_{paths,frame,self,ledger,measure}.c \
        "$daemon_directory"/tessera_daemon_{state,admission,peers,main}.c || return 1
    TESSERA_OBJECTS=("${SHARED_OBJECTS[@]:0:5}")
    daemon_objects=("${SHARED_OBJECTS[@]:2}")
    # the ledger and the measure of the daemon, which a test of either links by itself
    TESSERA_LEDGER="${SHARED_OBJECTS[5]}"
    TESSERA_MEASURE="${SHARED_OBJECTS[6]}"
    local seal_words=(nvcc "${HOST_FLAGS[@]}" -std=c++20 -O2 -rdc=true "${GENCODE[@]}" "${includes[@]}")
    shared_objects_built seal_words "$ENGINE/../../cu/engine/runtime/obsignatio"/obsignatio_{hash,seal}.cu || return 1
    TESSERA_SEAL=("${SHARED_OBJECTS[@]}")
    # the caller's objects stay as the caller compiled them
    local caller_objects=("${EXACT_TYPES_OBJECTS[@]+"${EXACT_TYPES_OBJECTS[@]}"}")
    exact_types_compile "${EXACT_TYPES_PLATFORM[@]}" || return 1
    TESSERA_PLATFORM=("${EXACT_TYPES_OBJECTS[@]}")
    EXACT_TYPES_OBJECTS=("${caller_objects[@]+"${caller_objects[@]}"}")
    rm -f "$daemon"
    nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -rdc=true "${GENCODE[@]}" "${long_paths[@]}" -o "$daemon" "${daemon_objects[@]}" \
        "${linea_objects[@]}" "${TESSERA_PLATFORM[@]}" "${TESSERA_SEAL[@]}" "${daemon_libraries[@]}" \
        "${EXACT_TYPES_LIBRARIES[@]}"
    [ -f "$daemon" ] || { echo "  build failed: the tessera daemon did not link"; return 1; }
}

# tessera_run_build builds tessera_run, the host job's wrapper, beside the daemon tessera_build made, from the same
# client objects and seal. It takes the line's objects.
tessera_run_build()
{
    local linea_objects=("$@")
    local long_paths=()
    local run
    local run_libraries=()
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*)
            long_paths=(-Xlinker /MANIFEST:EMBED -Xlinker "/MANIFESTINPUT:$(cygpath -m "$ENGINE/../../cu/long_paths.manifest")")
            run="$OUT/tessera_run.exe"
            ;;
        *)
            run="$OUT/tessera_run"
            run_libraries=(-ldl -lpthread)
            ;;
    esac
    rm -f "$run"
    local words
    tessera_words words
    shared_objects_built words "$ENGINE/runtime/daemon"/tessera_run_{common,windows,posix,child,main}.c || return 1
    nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -rdc=true "${GENCODE[@]}" "${long_paths[@]}" -o "$run" "${SHARED_OBJECTS[@]}" "${TESSERA_OBJECTS[@]}" \
        "${linea_objects[@]}" "${TESSERA_PLATFORM[@]}" "${TESSERA_SEAL[@]}" "${run_libraries[@]}" \
        "${EXACT_TYPES_LIBRARIES[@]}"
    [ -f "$run" ] || { echo "  build failed: tessera_run did not link"; return 1; }
}
