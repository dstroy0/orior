#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Writes monolith_differences.md: the monolith's tagged build, a block at a time, held against what our compiler
# writes (monolith_emit.cpp).
#
# NVIDIA's compiler builds the tagged monolith once, and its listing is the answer key. The script links monolith_emit
# from the code generator's objects, the machine file's reader and our assembler. The tool reads sass.krs, the machine
# file and the word web as they are.
#
#     src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/monolith_emit.sh [arch]
set -u

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../../.." && pwd)"
source "$TOP/includes/file_defs/file_defs.sh"
# the program is a program of the platform: it links the process stage, the pool, the system calls and the carcer
source "$TOP/includes/exact_types/exact_types.sh"
HERE="$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding"
CODEGEN="$TOP/src/cu/engine/rmc"
CODEGEN_CU="$TOP/src/cu/engine/rmc"
CODEGEN_CU_2="$TOP/includes/parsers"
CUBIN="$TOP/includes/parsers"
KRS_C="$TOP/includes/parsers"
OUT="$TOP/build/monolith/emit"
# the objects, in a folder that the LTO flags name, so that an object of other flags is not current
OBJECT_OUT="$OUT/objects_$FILE_DEFS_LTO_KEY"
mkdir -p "$OUT" "$OBJECT_OUT"

ARCH="${1:-}"
if [ -z "$ARCH" ]; then
    CAP="$(nvidia-smi --query-gpu=compute_cap --format=csv,noheader 2>/dev/null | head -1 | tr -d ' .')"
    ARCH="sm_${CAP:-86}"
fi

HOST_FLAGS=()
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
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
esac

# the answer key: the tagged monolith built once, and its listing
nvcc "${HOST_FLAGS[@]}" -cubin -arch="$ARCH" -O3 -DMONOLITH_TAGGED=1 -o "$OUT/monolith_tagged.cubin" \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/monolith.cu" || exit 1
cuobjdump -sass -fun monolith "$OUT/monolith_tagged.cubin" > "$OUT/monolith_tagged.listing" || exit 1

# 0 where an object needs a compile: no object is there, or its source or a header that its last compile read is
# newer than it. Each compile writes the files that it reads into the object's .d file (-MMD)
stale()
{
    local source="$1"
    local object="$2"
    local depend="${object%.o}.d"
    if [ ! -f "$object" ] || [ ! -f "$depend" ] || [ "$source" -nt "$object" ]; then
        return 0
    fi
    local header
    for header in $(sed -e '1s/^.*\.o:[[:space:]]*//' -e 's/\\//g' "$depend"); do
        [ "$header" -nt "$object" ] && return 0
    done
    return 1
}

OBJECTS=()
for source in "$CODEGEN_CU"/*.cu "$CODEGEN_CU_2"/*.cu "$CODEGEN_CU_2"/../file_defs/krs/ruleset_flat/*.cu "$HERE/sass_target.cu"; do
    case "$(basename "$source")" in
        asm_printer_*.cu | codegen*.cu) continue ;;
    esac
    object="$OBJECT_OUT/$(basename "$source").o"
    if stale "$source" "$object"; then
        "$FILE_DEFS_CXX" "${FILE_DEFS_LTO_CC[@]}" -std=c++20 -O1 -w -I "$TOP/src/cu/engine" -I "$TOP/src/cu/engine" "${FILE_DEFS_MMGR_FLAGS[@]}" -I "$CODEGEN" -I "$CODEGEN_CU" \
            -I "$CODEGEN_CU_2" -I "$HERE" -MMD -MF "${object%.o}.d" -x c++ -c "$source" -o "$object" || exit 1
    fi
    OBJECTS+=("$object")
done
for source in "$TOP"/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_machine/*.c "${FILE_DEFS_FORMATS[@]}" "${FILE_DEFS_LINEA[@]}" "${EXACT_TYPES_PLATFORM[@]}" "${EXACT_TYPES_CARCER[@]}" "$TOP"/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_assemble/*.c; do
    object="$OBJECT_OUT/$(printf '%s' "${source#"$TOP"/}" | tr '/' '_').o"
    cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O1 -w -I "$TOP/src/cu/engine" -I "$TOP/src/cu/engine" -I "$CUBIN" -I "$KRS_C" \
        "${FILE_DEFS_MMGR_FLAGS[@]}" -c "$source" -o "$object" || exit 1
    OBJECTS+=("$object")
done
"$FILE_DEFS_CXX" "${FILE_DEFS_LTO_CC[@]}" -std=c++20 -O1 -Wall -I "$TOP/src/cu/engine" -I "$TOP/src/cu/engine" "${FILE_DEFS_MMGR_FLAGS[@]}" -I "$CODEGEN" -I "$CODEGEN_CU" \
    -I "$CODEGEN_CU_2" -I "$CUBIN" -c "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/monolith_emit.cpp" -o "$OUT/monolith_emit.o" || exit 1
"$FILE_DEFS_CXX" "${FILE_DEFS_LTO_CC[@]}" -o "$OUT/monolith_emit" "$OUT/monolith_emit.o" "${OBJECTS[@]}" "${EXACT_TYPES_LIBRARIES[@]}" || exit 1

cd "$TOP" || exit 1
"$OUT/monolith_emit" "build/monolith/emit/monolith_tagged.listing" all \
    "src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/monolith_differences.md" > "$OUT/monolith_emit.out" || exit 1
tail -1 "$OUT/monolith_emit.out"
