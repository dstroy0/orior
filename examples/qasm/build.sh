#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Sourced by run.sh and test/run.sh with TOP set. qasm_build <label> builds the qasm objects, the
# record machine's modules and the tessera daemon into $OUT; qasm_link <name> <source>... then links one program there
# as $BINARY.

QASM="$TOP/examples/qasm/src"

QASM_CU="$TOP/examples/qasm/src"
source "$TOP/includes/file_defs/file_defs.sh"
source "$TOP/includes/exact_types/exact_types.sh"
NO_ROUNDING="$TOP/includes/exact_types/integers"
CYCLE="$TOP/src/cu/engine/analysis/cycle"
CYCLE_CU="$TOP/src/cu/engine/analysis/cycle"
CODEGEN="$TOP/src/cu/engine/rmc"
CODEGEN_CU="$TOP/src/cu/engine/rmc"
CODEGEN_CU_2="$TOP/includes/parsers"
KEYMATH="$TOP/src/cu/engine/analysis/keymath"
KEYMATH_CU="$TOP/src/cu/engine/analysis/keymath"
KEY_SCHEDULE="$TOP/src/cu/engine/analysis/key_schedule"
KEY_SCHEDULE_CU="$TOP/src/cu/engine/analysis/key_schedule"
OBSIGNATIO="$TOP/src/cu/engine/runtime/obsignatio"
OBSIGNATIO_CU="$TOP/src/cu/engine/runtime/obsignatio"
DAEMON_DIRECTORY="$TOP/src/cu/engine/runtime/daemon"
INCLUDES=(-I "$TOP/src/cu/engine" -I "$QASM" -I "$QASM_CU" "${FILE_DEFS_MMGR_FLAGS[@]}" -I "$NO_ROUNDING" -I "$CYCLE" -I "$CYCLE_CU" -I "$KEYMATH" -I "$KEYMATH_CU"
          -I "$KEY_SCHEDULE" -I "$KEY_SCHEDULE_CU" -I "$OBSIGNATIO" -I "$OBSIGNATIO_CU" -I "$DAEMON_DIRECTORY")

qasm_host_setup()
{
    HOST_FLAGS=()
    LONG_PATHS=()
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*)
            EXECUTABLE=.exe
            MSVC_BIN="$(ls -d "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC"/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
            if [ -z "$MSVC_BIN" ]; then
                MSVC_BIN="$(ls -d "/c/Program Files/Microsoft Visual Studio"/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
            fi
            if [ -z "$MSVC_BIN" ]; then
                echo "  no host compiler nvcc accepts on this platform was found."
                return 1
            fi
            HOST_FLAGS=(-ccbin "$MSVC_BIN" -Xcompiler /Zc:preprocessor)
            LONG_PATHS=(-Xlinker /MANIFEST:EMBED -Xlinker "/MANIFESTINPUT:$(cygpath -m "$TOP/src/cu/long_paths.manifest")")
            DAEMON_LIBRARIES=(-lpdh)
            ;;
        *)
            EXECUTABLE=
            HOST_FLAGS=(-Xcompiler -fPIC)
            DAEMON_LIBRARIES=(-ldl -lpthread)
            ;;
    esac
    ARCHES="${QASM_ARCHES:-}"
    if [ -z "$ARCHES" ]; then
        CAP="$(nvidia-smi --query-gpu=compute_cap --format=csv,noheader 2>/dev/null | head -1 | tr -d ' .')"
        ARCHES="sm_${CAP:-86}"
    fi
    GENCODE=()
    for one in $ARCHES; do
        GENCODE+=(-gencode "arch=compute_${one#sm_},code=${one}")
    done
    return 0
}

qasm_build()
{
    source "$TOP/utils/maint/engine/build_stamp.sh"
    build_stamp "$1"
    qasm_host_setup || return 1
    local c_words
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*)
            c_words=(nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC[@]}" -Xcompiler "/std:c11 /O2 /experimental:c11atomics" "${INCLUDES[@]}") ;;
        *)
            c_words=(cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -fPIC "${INCLUDES[@]}") ;;
    esac
    local cu_words=(nvcc "${HOST_FLAGS[@]}" -std=c++20 -O2 -rdc=true "${GENCODE[@]}" "${INCLUDES[@]}")
    # Each source compiles one time for its words (shared_objects.sh). The .cu sources reach the tables of MMgr in each
    # pass of nvcc, and they compile with -rdc=true. The line of MMgr, the cellblock and the device hooks compile as CUDA
    # (exact_types_device_line). The program and the daemon link them with the device link. The process stage, the pool
    # and the system calls of the platform go in the daemon too
    exact_types_device_line || return 1
    LINEA_OBJECTS=("${EXACT_TYPES_DEVICE_LINE[@]}")
    shared_objects_built c_words "${EXACT_TYPES_PLATFORM[@]}" || return 1
    PLATFORM_OBJECTS=("${SHARED_OBJECTS[@]}")
    # The formats with the line: the ruleset reader reads a ruleset's .ksc as the run stores it (qry_member.h)
    shared_objects_built c_words "${FILE_DEFS_FORMATS[@]}" "${EXACT_TYPES_INTEGERS[@]}" \
        "$CYCLE/cycle.c" "$QASM"/qasm_{exact,trig,lexer,expression,gates,statements,read}.c \
        "$QASM"/qasm_field_{rational,number}.c "$QASM/qasm_dense.c" "$QASM"/qasm_chain_{matrix,apply}.c \
        "$QASM"/qasm_symbolic_{polynomial,function}.c "$QASM"/qasm_lens_{hash,rank}.c || return 1
    OBJECTS=("${LINEA_OBJECTS[@]}" "${PLATFORM_OBJECTS[@]}" "${SHARED_OBJECTS[@]}")
    shared_objects_built cu_words "$QASM_CU/qasm.cu" "$QASM_CU/qasm_program.cu" "$QASM_CU/qasm_run.cu" \
        "$QASM_CU"/qasm_self_{program,run}.cu "$CYCLE_CU"/cycle*.cu \
        "$CODEGEN_CU"/*.cu "$CODEGEN_CU_2"/*.cu "$CODEGEN_CU_2"/../file_defs/krs/ruleset_flat/*.cu \
        "$KEYMATH_CU/keymath.cu" "$KEY_SCHEDULE_CU/key_schedule.cu" || return 1
    OBJECTS+=("${SHARED_OBJECTS[@]}")
    # A run on the device is a job on the device's tessera daemon. The client goes into the program. The script builds
    # the daemon beside the program, which starts it when none answers. The sources come as the two clients, then the
    # three that the clients and the daemon both link, then the daemon's own
    shared_objects_built c_words "$DAEMON_DIRECTORY"/tessera_client_{socket,jobs}.c \
        "$DAEMON_DIRECTORY"/tessera_{paths,frame,self,ledger,measure}.c \
        "$DAEMON_DIRECTORY"/tessera_daemon_{state,admission,peers,main}.c || return 1
    OBJECTS+=("${SHARED_OBJECTS[@]:0:5}")
    DAEMON_OBJECTS=("${SHARED_OBJECTS[@]:2}")
    shared_objects_built cu_words "$OBSIGNATIO_CU"/obsignatio_{hash,seal}.cu || return 1
    SEAL_OBJECTS=("${SHARED_OBJECTS[@]}")
    OBJECTS+=("${SEAL_OBJECTS[@]}")
    DAEMON="$OUT/tessera_daemon$EXECUTABLE"
    rm -f "$DAEMON"
    nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -rdc=true "${GENCODE[@]}" "${LONG_PATHS[@]}" -o "$DAEMON" "${DAEMON_OBJECTS[@]}" \
        "${LINEA_OBJECTS[@]}" "${PLATFORM_OBJECTS[@]}" "${SEAL_OBJECTS[@]}" "${DAEMON_LIBRARIES[@]}" \
        "${EXACT_TYPES_LIBRARIES[@]}"
    [ -f "$DAEMON" ] || { echo "  build failed: the tessera daemon did not link"; return 1; }
    return 0
}

qasm_link()
{
    local name="$1"
    shift
    local sources=("$@")
    BINARY="$OUT/$name$EXECUTABLE"
    rm -f "$BINARY"
    nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -std=c++20 -O2 -rdc=true "${GENCODE[@]}" "${INCLUDES[@]}" "${LONG_PATHS[@]}" -o "$BINARY" \
        "${sources[@]}" "${OBJECTS[@]}"
    [ -f "$BINARY" ] || { echo "  build failed: $name did not link"; return 1; }
    return 0
}
