#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds the interface (compiler/interface), its PTX probe and its SASS probe, then runs the SASS probe: the PTX probe assembles
# each membership question's kernel, written from ptx.krs, into a cubin; nvdisasm lists each, which gives each form's
# machine code; and each operation's 128 bits are turned over one at a time and decoded, which gives its fields. The
# SASS probe runs in one query run, query<n>-<datetime>.qry in the build's qry folder (qry_run.sh), whose writer writes
# each machine file it learns in its place.
set -u

TEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_CU="$TEST"
TOP="$(cd "$TEST/../../../../../.." && pwd)"
source "$TOP/includes/file_defs/file_defs.sh"
INTERFACE="$TOP/src/cu/transpiler/lstar/interface"
CODEGEN="$TOP/src/cu/engine/rmc"
CODEGEN_CU="$TOP/src/cu/engine/rmc"
CODEGEN_CU_2="$TOP/includes/parsers"
CUBIN="$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding"
source "$TOP/utils/maint/engine/build_stamp.sh"
build_stamp interface_sass_test

type -P nvdisasm > /dev/null || { echo "  no nvdisasm on the PATH: the CUDA toolkit's disassembler is the oracle"; exit 1; }
type -P cuobjdump > /dev/null || { echo "  no cuobjdump on the PATH: the CUDA toolkit reads the cubin's ELF"; exit 1; }

HOST_FLAGS=()
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        BINARY="$OUT/interface_sass_probe.exe"
        PROBE="$OUT/interface_ptx_probe.exe"
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
        ;;
    *)
        BINARY="$OUT/interface_sass_probe"
        PROBE="$OUT/interface_ptx_probe"
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

INCLUDES=(-I "$TOP/src/cu/engine" -I "$INTERFACE" -I "$CUBIN" "${FILE_DEFS_MMGR_FLAGS[@]}")
rm -f "$BINARY" "$PROBE"
OBJECTS=()
# the formats' objects, which the PTX probe links too: its ruleset reader reads a ruleset's .ksc as the run stores it
# (qry_member.h). The probe is a program of the platform, and links MMgr's line with the platform
# (exact_types_platform_link)
source "$TOP/includes/exact_types/exact_types.sh"
exact_types_device_platform_link || exit 1
declare -A FORMAT_SOURCE=()
for source in "${FILE_DEFS_FORMATS[@]}"; do
    FORMAT_SOURCE["$source"]=1
done
FORMAT_OBJECTS=()
for source in "$INTERFACE/interface.c" "$INTERFACE"/platform/*.c "$INTERFACE/interface_names.c" "$TOP"/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_machine/*.c "${FILE_DEFS_FORMATS[@]}" \
              "$TOP"/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_assemble/*.c "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_write.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/container_write.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/container_pattern.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/container_layout.c" "$TOP"/src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_safe/*.c "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/tables/read.c" "$TEST/interface_sass_probe_main.c" "$TEST"/interface_sass_probe_main/*.c "$TEST"/interface_sass_probe_machine/*.c \
              "$TEST"/interface_sass_probe_query/*.c "$TEST/interface_sass_probe_class.c" "$TEST/interface_sass_probe_cubin.c" \
              "$TEST/interface_sass_probe_read.c" "$TEST/interface_sass_probe_check.c"; do
    object="$OUT/$(printf '%s' "${source#"$TOP"/}" | tr '/' '_')"
    object="${object%.c}.$EXTENSION"
    rm -f "$object"
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*)
            nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC[@]}" -Xcompiler "/std:c11 /O2 /experimental:c11atomics" "${INCLUDES[@]}" -c "$source" -o "$object" ;;
        *)
            cc "${FILE_DEFS_LTO_CC[@]}" -std=c11 -O2 "${INCLUDES[@]}" -c "$source" -o "$object" ;;
    esac
    [ -f "$object" ] || { echo "  build failed: $(basename "$source") did not compile"; exit 1; }
    OBJECTS+=("$object")
    [ -n "${FORMAT_SOURCE[$source]:-}" ] && FORMAT_OBJECTS+=("$object")
done
# the SASS probe is a program of the platform too, and takes MMgr's line with the platform from PLATFORM_LINK
nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -rdc=true "${GENCODE[@]}" -o "$BINARY" "${OBJECTS[@]}" "${PLATFORM_LINK[@]}"
[ -f "$BINARY" ] || { echo "  build failed: the interface's SASS probe did not link"; exit 1; }
# The probe reads rulesets and writes forms. The record machine runs the code generator's assembly printer
# (asm_printer_*.cu) and the code generator on the device (codegen*.cu). The probe does not link the record machine
CODEGEN_SOURCES=()
for source in "$CODEGEN_CU"/*.cu "$CODEGEN_CU_2"/*.cu "$CODEGEN_CU_2"/../file_defs/krs/ruleset_flat/*.cu; do
    case "$(basename "$source")" in
        asm_printer_*.cu|codegen*.cu) ;;
        *) CODEGEN_SOURCES+=("$source") ;;
    esac
done
nvcc "${HOST_FLAGS[@]}" "${FILE_DEFS_LTO_NVCC_LINK[@]}" -rdc=true -std=c++20 -O2 "${GENCODE[@]}" -I "$TOP/src/cu/engine" -I "$TOP/src/cu/engine" "${FILE_DEFS_MMGR_FLAGS[@]}" \
    -o "$PROBE" "$TEST_CU"/interface_ptx_probe_{questions,runs,main}.cu "$TEST_CU/ptx_target.cu" \
    "${CODEGEN_SOURCES[@]}" "${FORMAT_OBJECTS[@]}" -lnvrtc -lnvJitLink "${PLATFORM_LINK[@]}"
[ -f "$PROBE" ] || { echo "  build failed: the PTX probe did not build"; exit 1; }
source "$TOP/src/cu/transpiler/lstar/protocol/qry_run.sh"
qry_writer_built "$OUT"
run_started "$OUT/qry"

# SASS_PATTERN names the sass folder of an earlier run. The probe asks the part how it loops against those cubins and
# the machine file that the tree holds for the first architecture. The probe learns nothing again
if [ -n "${SASS_PATTERN:-}" ]; then
    [ -f "$SASS_PATTERN/form_0.cubin" ] || { echo "  no form_0.cubin in $SASS_PATTERN"; exit 1; }
    FIRST="${ARCHES%% *}"
    "$BINARY" "$PROBE" "$SASS_PATTERN" loop "$TOP/src/cu/transpiler/lstar/protocol/table/$FIRST.khw"
    STATUS=$?
    echo "  interface sass loop exit $STATUS"
    exit "$STATUS"
fi
mkdir -p "$OUT/sass"
# With SASS_LEARN set, the probe learns the machine again through the disassembler, bit by bit, past half an hour.
# With it unset, the probe puts the queries against the machine file that the tree holds for the first architecture
if [ -z "${SASS_LEARN:-}" ]; then
    FIRST="${ARCHES%% *}"
    "$BINARY" "$PROBE" "$OUT/sass" queries "$TOP/src/cu/transpiler/lstar/protocol/table/$FIRST.khw"
    STATUS=$?
    echo "  interface sass queries exit $STATUS"
    exit "$STATUS"
fi
"$BINARY" "$PROBE" "$OUT/sass" "$TOP/src/cu/transpiler/lstar/protocol/table"
STATUS=$?
echo "  interface sass test exit $STATUS"
exit "$STATUS"
