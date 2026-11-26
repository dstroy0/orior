#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
set -u

TEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODULE="$(cd "$TEST/../../../../../../../src/cu/engine/runtime/daemon" && pwd)"
TOP="$(cd "$MODULE/../../../.." && pwd)"
# utils/maint/ and build/ are at the repository's root, one above src/
REPOSITORY="$(cd "$TOP/.." && pwd)"
source "$REPOSITORY/utils/maint/engine/build_stamp.sh"
TOP="$REPOSITORY" build_stamp tessera_test

source "$TOP/../includes/file_defs/file_defs.sh"
source "$TOP/../includes/exact_types/exact_types.sh"
OBSIGNATIO="$TOP/cu/engine/runtime/obsignatio"
OBSIGNATIO_CU="$TOP/cu/engine/runtime/obsignatio"
INCLUDES=(-I "$TOP/cu/engine" -I "$MODULE" "${FILE_DEFS_MMGR_FLAGS[@]}" -I "$OBSIGNATIO" -I "$OBSIGNATIO_CU")
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        MSVC_BIN="$(ls -d "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC"/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
        if [ -z "$MSVC_BIN" ]; then
            MSVC_BIN="$(ls -d "/c/Program Files/Microsoft Visual Studio"/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
        fi
        [ -n "$MSVC_BIN" ] || { echo "  no host compiler nvcc accepts on this platform was found."; exit 1; }
        HOST_FLAGS=(-ccbin "$MSVC_BIN" -Xcompiler /Zc:preprocessor)
        SUFFIX=.exe
        OBJECT=obj
        build_host()
        {
            nvcc -ccbin "$MSVC_BIN" -Xcompiler /Zc:preprocessor "${FILE_DEFS_LTO_NVCC[@]}" -Xcompiler "/std:c11 /O2 /W4" "${INCLUDES[@]}" "$@"
        }
        build_device()
        {
            nvcc -ccbin "$MSVC_BIN" -Xcompiler /Zc:preprocessor -Xcompiler -W4 -std=c++20 -O2 -rdc=true "${INCLUDES[@]}" "$@" -lpdh
        }
        build_seal()
        {
            build_device -c "$@"
        }
        DEVICE_TOOLCHAIN=1
        LONG_PATHS=(-Xlinker /MANIFEST:EMBED -Xlinker "/MANIFESTINPUT:$(cygpath -m "$TOP/cu/long_paths.manifest")")
        ;;
    *)
        SUFFIX=
        OBJECT=o
        build_host()
        {
            cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra "${INCLUDES[@]}" "$@"
        }
        if command -v nvcc > /dev/null 2>&1; then
            build_device()
            {
                nvcc -std=c++20 -O2 -rdc=true -Xcompiler -Wall "${INCLUDES[@]}" "$@" -ldl
            }
            build_seal()
            {
                build_device -c "$@"
            }
            DEVICE_TOOLCHAIN=1
        else
            # A part with no CUDA toolchain (the Pi) builds the seal as C++ and links with c++. The script builds and
            # tests the daemon and tessera_run, and leaves out the two tests that take device memory
            build_device()
            {
                "$FILE_DEFS_CXX" "${FILE_DEFS_LTO_CC[@]}" -std=c++20 -O2 -Wall -Wextra "${INCLUDES[@]}" "$@" -ldl -lpthread
            }
            build_seal()
            {
                "$FILE_DEFS_CXX" "${FILE_DEFS_LTO_CC[@]}" -std=c++20 -O2 -Wall -Wextra "${INCLUDES[@]}" -c -x c++ "$@"
            }
            DEVICE_TOOLCHAIN=0
        fi
        LONG_PATHS=()
        ;;
esac

# each program of the suite that links daemon code is a program of the platform. It links the MMgr sources, the process
# stage, the pool and the system calls of the platform, and the carcer of the pool
exact_types_compile "${FILE_DEFS_LINEA[@]}" "${EXACT_TYPES_PLATFORM[@]}" "${EXACT_TYPES_CARCER[@]}" || exit 1
PLATFORM_LINK=("${EXACT_TYPES_OBJECTS[@]}" "${EXACT_TYPES_LIBRARIES[@]}")
# A program that build_device links takes DEVICE_LINK. Where the toolchain has nvcc, the seal and the .cu tests reach the
# tables of MMgr in each pass. The device link of nvcc then joins them (exact_types_device_platform_link). A part with
# no CUDA toolchain links the C objects
DEVICE_LINK=("${PLATFORM_LINK[@]}")
if [ "$DEVICE_TOOLCHAIN" = "1" ]; then
    HOST_LINK=("${PLATFORM_LINK[@]}")
    exact_types_device_platform_link || exit 1
    DEVICE_LINK=("${PLATFORM_LINK[@]}")
    PLATFORM_LINK=("${HOST_LINK[@]}")
fi

FAILED=0
run_one()
{
    local name="$1"
    local binary="$OUT/$name$SUFFIX"
    [ -f "$binary" ] || { echo "  build failed: $name did not compile"; FAILED=1; return; }
    "$binary"
    local status=$?
    echo "  $name exit $status"
    [ "$status" -eq 0 ] || FAILED=1
}

rm -f "$OUT/tessera_ledger_test$SUFFIX" "$OUT/tessera_frame_test$SUFFIX" "$OUT/tessera_measure_test$SUFFIX"
build_host "$TEST"/tessera_ledger_test_{heap,scenarios}.c "$MODULE/tessera_ledger.c" "${PLATFORM_LINK[@]}" \
    -o "$OUT/tessera_ledger_test$SUFFIX"
run_one tessera_ledger_test
build_host "$TEST/tessera_frame_test.c" "$MODULE/tessera_frame.c" "${PLATFORM_LINK[@]}" -o "$OUT/tessera_frame_test$SUFFIX"
run_one tessera_frame_test
if [ "${TESSERA_DEVICE:-1}" = "1" ]; then
    if [ "$DEVICE_TOOLCHAIN" = "1" ]; then
        MEASURE_OBJECT="$OUT/tessera_measure_host.$OBJECT"
        SELF_OBJECT="$OUT/tessera_self_host.$OBJECT"
        rm -f "$MEASURE_OBJECT" "$SELF_OBJECT"
        build_host -c "$MODULE/tessera_measure.c" -o "$MEASURE_OBJECT"
        build_host -c "$MODULE/tessera_self.c" -o "$SELF_OBJECT"
        build_device "${FILE_DEFS_LTO_NVCC_LINK[@]}" "$TEST/tessera_measure_test.cu" "$MEASURE_OBJECT" "$SELF_OBJECT" "${DEVICE_LINK[@]}" \
            "${LONG_PATHS[@]}" \
            -o "$OUT/tessera_measure_test$SUFFIX"
        run_one tessera_measure_test
    else
        echo "  tessera_measure_test not built: no CUDA toolchain"
    fi

    # The tests' daemons answer endpoints of their own ($TESSERA_RUNTIME), apart from the daemons that real jobs use.
    # The endpoint is a pipe name on Windows, and on Linux a short folder, since a socket's path holds 108 bytes
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*) RUNTIME="tessera_test_$$" ;;
        *) RUNTIME="$(mktemp -d /tmp/tessera_test.XXXXXX)" ;;
    esac

    # the daemon, and the client against it end to end
    rm -f "$OUT/tessera_daemon$SUFFIX" "$OUT/tessera_job_test$SUFFIX"
    OBJECTS=()
    for source in "$MODULE"/tessera_ledger.c "$MODULE"/tessera_measure.c "$MODULE"/tessera_paths.c \
                  "$MODULE"/tessera_frame.c "$MODULE"/tessera_self.c "$MODULE"/tessera_client_{socket,jobs}.c \
                  "$MODULE"/tessera_daemon_{state,admission,peers,main}.c; do
        object="$OUT/$(basename "$source" .c)_tessera.$OBJECT"
        rm -f "$object"
        build_host -c "$source" -o "$object"
        OBJECTS+=("$object")
    done
    SEAL_OBJECTS=()
    for name in obsignatio_{hash,seal}; do
        object="$OUT/${name}_tessera.$OBJECT"
        rm -f "$object"
        build_seal "$OBSIGNATIO_CU/$name.cu" -o "$object"
        SEAL_OBJECTS+=("$object")
    done
    DAEMON_OBJECTS=()
    CLIENT_OBJECTS=()
    for object in "${OBJECTS[@]}"; do
        case "$object" in
            *tessera_client_*_tessera.$OBJECT) CLIENT_OBJECTS+=("$object") ;;
            *tessera_daemon_*_tessera.$OBJECT) DAEMON_OBJECTS+=("$object") ;;
            *tessera_ledger_tessera.$OBJECT|*tessera_measure_tessera.$OBJECT) DAEMON_OBJECTS+=("$object") ;;
            *) DAEMON_OBJECTS+=("$object"); CLIENT_OBJECTS+=("$object") ;;
        esac
    done
    build_device "${FILE_DEFS_LTO_NVCC_LINK[@]}" "${DAEMON_OBJECTS[@]}" "${SEAL_OBJECTS[@]}" "${DEVICE_LINK[@]}" "${LONG_PATHS[@]}" -o "$OUT/tessera_daemon$SUFFIX"
    [ -f "$OUT/tessera_daemon$SUFFIX" ] || { echo "  build failed: the daemon did not link"; FAILED=1; }
    if [ "$DEVICE_TOOLCHAIN" = "1" ]; then
        build_device "${FILE_DEFS_LTO_NVCC_LINK[@]}" "$TEST/tessera_job_test.cu" "${CLIENT_OBJECTS[@]}" "${SEAL_OBJECTS[@]}" "${DEVICE_LINK[@]}" \
            "${LONG_PATHS[@]}" \
            -o "$OUT/tessera_job_test$SUFFIX"
    fi
    if [ "$DEVICE_TOOLCHAIN" != "1" ]; then
        echo "  tessera_job_test not built: no CUDA toolchain"
    elif [ -f "$OUT/tessera_job_test$SUFFIX" ]; then
        # the test damages the history deliberately: it runs in a state of its own; the daemon it starts inherits it
        STATE="$OUT/tessera_state"
        rm -rf "$STATE"
        mkdir -p "$STATE"
        TESSERA_STATE="$(cygpath -w "$STATE" 2>/dev/null || echo "$STATE")" TESSERA_RUNTIME="$RUNTIME" \
        "$OUT/tessera_job_test$SUFFIX" "$(cygpath -w "$OUT/tessera_daemon$SUFFIX" 2>/dev/null || echo "$OUT/tessera_daemon$SUFFIX")"
        STATUS=$?
        echo "  tessera_job_test exit $STATUS"
        [ "$STATUS" -eq 0 ] || FAILED=1
    else
        echo "  build failed: tessera_job_test did not compile"
        FAILED=1
    fi

    # the host's processors: tessera_run holds a host ticket around its command, beside the daemon it starts
    rm -f "$OUT/tessera_run$SUFFIX" "$OUT/tessera_burn$SUFFIX"
    RUN_OBJECTS=()
    for name in tessera_run_{common,windows,posix,child,main}; do
        object="$OUT/${name}_tessera.$OBJECT"
        rm -f "$object"
        build_host -c "$MODULE/$name.c" -o "$object"
        RUN_OBJECTS+=("$object")
    done
    build_device "${FILE_DEFS_LTO_NVCC_LINK[@]}" "${RUN_OBJECTS[@]}" "${CLIENT_OBJECTS[@]}" "${SEAL_OBJECTS[@]}" "${DEVICE_LINK[@]}" \
        "${LONG_PATHS[@]}" \
        -o "$OUT/tessera_run$SUFFIX"
    build_host "$TEST/tessera_burn.c" "${PLATFORM_LINK[@]}" -o "$OUT/tessera_burn$SUFFIX"
    if [ -f "$OUT/tessera_run$SUFFIX" ] && [ -f "$OUT/tessera_burn$SUFFIX" ]; then
        # the host test runs in a state of its own, which the host daemon it starts inherits
        HOST_STATE="$OUT/tessera_host_state"
        rm -rf "$HOST_STATE"
        mkdir -p "$HOST_STATE"
        TESSERA_STATE="$(cygpath -w "$HOST_STATE" 2>/dev/null || echo "$HOST_STATE")" TESSERA_RUNTIME="$RUNTIME" \
        bash "$TEST/tessera_run_test.sh" "$OUT/tessera_run$SUFFIX" "$OUT/tessera_burn$SUFFIX" "$OUT/tessera_run_scratch"
        STATUS=$?
        echo "  tessera_run_test exit $STATUS"
        [ "$STATUS" -eq 0 ] || FAILED=1
        # Once the test holds, the script publishes tessera_run and the daemon it starts together into
        # build/tessera_host. Every build that wraps itself in a host job finds them there. The script does not touch
        # the device daemons beside the programs.
        #
        # The script first moves a copy that runs aside under another name. Windows lets a program that runs take a
        # new name, but not a new body. The jobs already under the copy keep it. The script deletes the copies that it
        # set aside before, once nothing runs them
        if [ "$STATUS" -eq 0 ] && [ "$FINAL" != "$OUT" ]; then
            HOST="$FINAL/tessera_host"
            mkdir -p "$HOST"
            rm -f "$HOST"/*.replaced 2> /dev/null
            PUBLISHED=1
            for name in "tessera_run$SUFFIX" "tessera_daemon$SUFFIX"; do
                if [ -f "$HOST/$name" ]; then
                    mv -f "$HOST/$name" "$HOST/$name.$(date +%Y%m%d_%H%M%S).replaced" || PUBLISHED=0
                fi
                cp -f "$OUT/$name" "$HOST/$name" || PUBLISHED=0
            done
            if [ "$PUBLISHED" -eq 1 ]; then
                echo "  published $HOST/tessera_run$SUFFIX and its daemon"
            else
                echo "  build failed: could not publish tessera_run and its daemon to $HOST"
                FAILED=1
            fi
        fi
    else
        echo "  build failed: tessera_run or tessera_burn did not compile"
        FAILED=1
    fi
fi
echo "  tessera tests exit $FAILED"
exit "$FAILED"
