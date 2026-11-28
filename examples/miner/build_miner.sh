#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Builds the stratum client with the device arm compiled in, into build/miner. It builds and runs nothing else.
#
#     bash examples/miner/build_miner.sh [v1|v2|harness|kat]
#
# v1 builds btc_miner.exe from the v1 sources, and v2 builds btc_miner2.exe from the v2 sources. harness builds
# harness.exe, the CPU harness on a real header, and kat builds kat_validation.exe, the known answer tests, both with
# no device arm. The client is a program of the platform. It defines program_main, writes its text through the line of
# MMgr and searches its bytes through MMgr. It links the platform and the line of MMgr (exact_types_platform_link) and
# ws2_32 for the stratum socket.
#
# The client serves sm_86, an RTX 3070. A different card needs its own arch. The arch stays explicit, and the script
# does not take it from the device present. The AVX2 flag still matters. Worker zero drives the device, and every
# other worker runs the eight-lane arm. Both engines then run in one binary.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../.." && pwd)"
VERSION="${1:-v1}"
case "$VERSION" in
v1)
    DEVICE="$HERE/device"
    HOST_SOURCE="$HERE/host/btc_miner.cpp"
    NAME=btc_miner
    ;;
v2)
    DEVICE="$HERE/device/v2"
    HOST_SOURCE="$HERE/host/v2/btc_miner.cpp"
    NAME=btc_miner2
    ;;
harness)
    DEVICE="$HERE/device"
    HOST_SOURCE="$HERE/host/harness.cpp"
    NAME=harness
    ;;
kat)
    DEVICE="$HERE/device"
    HOST_SOURCE="$HERE/test/kat_validation.cpp"
    NAME=kat_validation
    ;;
*)
    echo "  unknown version '$VERSION'. Use v1 or v2."
    exit 1
    ;;
esac
source "$TOP/includes/file_defs/file_defs.sh"
source "$TOP/includes/exact_types/exact_types.sh"
source "$TOP/utils/maint/engine/build_stamp.sh"
build_stamp "miner_$VERSION"

HOST_FLAGS=()
SUFFIX=
case "$(uname -s)" in
MINGW* | MSYS* | CYGWIN*)
    SUFFIX=.exe
    MSVC_BIN="$(ls -d "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC"/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
    if [ -z "$MSVC_BIN" ]; then
        MSVC_BIN="$(ls -d "/c/Program Files/Microsoft Visual Studio"/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
    fi
    if [ -z "$MSVC_BIN" ]; then
        echo "  no host compiler nvcc accepts on this platform was found."
        exit 1
    fi
    HOST_FLAGS=(-ccbin "$MSVC_BIN" -Xcompiler /Zc:preprocessor)
    VECTOR_FLAGS=(-Xcompiler "/O2 /arch:AVX2 /EHsc")
    CORE_FLAGS=(-Xcompiler /arch:AVX2)
    SOCKET=(-lws2_32)
    ;;
*)
    HOST_FLAGS=(-Xcompiler -fPIC)
    VECTOR_FLAGS=(-Xcompiler "-O2 -mavx2")
    CORE_FLAGS=(-mavx2)
    SOCKET=()
    ;;
esac

# the eight-lane arm, a C11 source, compiled as the platform's sources are, with the vector flag of its arm
EXACT_TYPES_EXTRA=("${CORE_FLAGS[@]}" -I "$HERE/core")
exact_types_compile "$HERE/core/sha256_core.c" || exit 1
CORE_OBJECTS=("${EXACT_TYPES_OBJECTS[@]}")
EXACT_TYPES_EXTRA=()
exact_types_platform_link || exit 1
BINARY="$OUT/$NAME$SUFFIX"
rm -f "$BINARY"
# the harness and the known answer tests run on the CPU alone, and so they link no device arm
DEVICE_SOURCES=("$DEVICE/cuda_miner.cu")
{ [ "$VERSION" = harness ] || [ "$VERSION" = kat ]; } && DEVICE_SOURCES=()
nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -O3 -arch=sm_86 -std=c++20 "${VECTOR_FLAGS[@]}" \
    "${EXACT_TYPES_FLAGS[@]}" -I "$HERE/core" -I "$DEVICE" -I "$HERE/format" -o "$BINARY" "$HOST_SOURCE" \
    "${DEVICE_SOURCES[@]}" "${CORE_OBJECTS[@]}" "${PLATFORM_LINK[@]}" "${SOCKET[@]}"
[ -f "$BINARY" ] || { echo "  build failed: nvcc could not build $NAME"; exit 1; }
mkdir -p "$TOP/build/miner"
cp "$BINARY" "$TOP/build/miner/$NAME$SUFFIX" || exit 1
echo "  built $TOP/build/miner/$NAME$SUFFIX"
