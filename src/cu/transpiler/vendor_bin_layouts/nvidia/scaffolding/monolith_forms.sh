#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Writes monolith_forms.md: every form the record programs' lanes decide asked of NVIDIA's compiler in one program, and
# each read back off its listing and its PTX beside the form sass.krs and ptx.krs give (monolith_forms.cpp). Nothing is
# run on the part: the program is built and read.
#
#     src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/monolith_forms.sh [apply] [arch]
#
# With apply, the tool writes each form whose every question reads whole and alike into sass.krs and ptx.krs in place.
set -u

APPLY=""
if [ "${1:-}" = "apply" ]; then
    APPLY="apply"
    shift
fi

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../../.." && pwd)"
source "$TOP/includes/file_defs/file_defs.sh"
source "$TOP/includes/exact_types/exact_types.sh"
HERE="$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding"
RULESETS="$TOP/src/cu/transpiler/lstar/protocol/table"
CYCLE="$TOP/src/cu/engine/analysis/cycle"
CYCLE_CU="$TOP/src/cu/engine/analysis/cycle"
CODEGEN="$TOP/src/cu/engine/rmc"
CODEGEN_CU="$TOP/src/cu/engine/rmc"
CODEGEN_CU_2="$TOP/includes/parsers"
KEYMATH="$TOP/src/cu/engine/analysis/keymath"
KEYMATH_CU="$TOP/src/cu/engine/analysis/keymath"
KEY_SCHEDULE="$TOP/src/cu/engine/analysis/key_schedule"
KEY_SCHEDULE_CU="$TOP/src/cu/engine/analysis/key_schedule"
NO_ROUNDING="$TOP/includes/exact_types/integers"
source "$TOP/includes/file_defs/file_defs.sh"
OUT="$TOP/build/monolith/forms"
mkdir -p "$OUT"
# The tool is a member of a query run (qry_run.sh). The run's writer writes sass.ksc in its place as the run ends, and
# a tool with no run writes no file through it
source "$TOP/src/cu/transpiler/lstar/protocol/qry_run.sh"
qry_writer_built "$OUT"

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
        [ -n "$MSVC_BIN" ] || { echo "  no host compiler nvcc accepts on this platform was found."; exit 1; }
        HOST_FLAGS=(-ccbin "$MSVC_BIN" -Xcompiler /Zc:preprocessor)
        ;;
esac

# the tool, linked from the code generator and the record programs as sass_lane_needs is
INCLUDES=(-I "$TOP/src/cu/engine" -I "$TOP/includes/codecs/crc" -I "$CYCLE"
    -I "$CYCLE_CU" -I "$CODEGEN" -I "$CODEGEN_CU" -I "$CODEGEN_CU_2" -I "$KEYMATH" -I "$KEYMATH_CU" -I "$KEY_SCHEDULE"
    -I "$KEY_SCHEDULE_CU" -I "$NO_ROUNDING" "${FILE_DEFS_MMGR_FLAGS[@]}" -I "$TOP/utils/test/src/cu/engine/analysis/cycle")

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
for source in "${FILE_DEFS_LINEA[@]}" "${EXACT_TYPES_INTEGERS[@]}" "${EXACT_TYPES_PLATFORM[@]}" "${EXACT_TYPES_CARCER[@]}"; do
    object="$OUT/$(basename "$source" .c).o"
    if stale "$source" "$object"; then
        cc -std=c11 -O2 -w "${INCLUDES[@]}" -MMD -MF "${object%.o}.d" -c "$source" -o "$object" || exit 1
    fi
    OBJECTS+=("$object")
done
for source in "$KEYMATH_CU/keymath.cu" "$KEY_SCHEDULE_CU/key_schedule.cu" "$CODEGEN_CU"/*.cu "$CODEGEN_CU_2"/*.cu "$CODEGEN_CU_2"/../file_defs/krs/ruleset_flat/*.cu \
    "$TOP/src/cu/scaffolding/c_target.cu"; do
    case "$(basename "$source")" in
        asm_printer_*.cu | codegen*.cu) continue ;;
    esac
    object="$OUT/$(basename "$source").o"
    if stale "$source" "$object"; then
        "$FILE_DEFS_CXX" -std=c++17 -O2 -w "${INCLUDES[@]}" -MMD -MF "${object%.o}.d" -x c++ -c "$source" -o "$object" ||
            exit 1
    fi
    OBJECTS+=("$object")
done
# The machine file's reader and our assembler, which assemble each SASS form that the tool reads before it writes the
# form. Then the system classification, which takes the folds that this pass finds (interface_sass_probe_class.c)
for source in "$TOP"/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_machine/*.c "${FILE_DEFS_FORMATS[@]}" "$TOP"/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_assemble/*.c \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/interface_sass_probe_class.c"; do
    object="$OUT/$(printf '%s' "${source#"$TOP"/}" | tr '/' '_').o"
    if stale "$source" "$object"; then
        cc -std=c11 -O2 -w -I "$TOP/src/cu/engine" -I "$TOP/includes/parsers" \
            -I "$TOP/src/cu/transpiler/lstar/interface" "${FILE_DEFS_MMGR_FLAGS[@]}" -MMD -MF "${object%.o}.d" \
            -c "$source" -o "$object" || exit 1
    fi
    OBJECTS+=("$object")
done
"$FILE_DEFS_CXX" -std=c++17 -O2 -Wall "${INCLUDES[@]}" -c "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/monolith_forms.cpp" -o "$OUT/monolith_forms.o" || exit 1
"$FILE_DEFS_CXX" -o "$OUT/monolith_forms" "$OUT/monolith_forms.o" "${OBJECTS[@]}" "${EXACT_TYPES_LIBRARIES[@]}" || exit 1

# the paths of the rulesets start at the top of the tree, which is where the generator reads them from
cd "$TOP" || exit 1
run_started "$OUT/qry"
"$OUT/monolith_forms" write "$RULESETS/c.krs" "$RULESETS/ptx.krs" "$OUT/monolith_forms.cu" "$OUT/questions.tsv" || exit 1
nvcc "${HOST_FLAGS[@]}" -cubin -arch="$ARCH" -O3 -o "$OUT/monolith_forms.cubin" "$OUT/monolith_forms.cu" || exit 1
nvcc "${HOST_FLAGS[@]}" -ptx -arch="$ARCH" -O3 -o "$OUT/monolith_forms.ptx" "$OUT/monolith_forms.cu" || exit 1
cuobjdump -sass "$OUT/monolith_forms.cubin" > "$OUT/monolith_forms.sass" || exit 1
"$OUT/monolith_forms" read "$OUT/questions.tsv" "$OUT/monolith_forms.sass" "$OUT/monolith_forms.ptx" \
    "$RULESETS/sass.krs" "$RULESETS/ptx.krs" "$RULESETS/sm_86.khw" "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/monolith_forms.md" \
    $APPLY || exit 1
