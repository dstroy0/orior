#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds and runs measuring_stick_engine: each kernel of the measuring stick read form by form through cu.krs and
# written in sass.krs, held against nvcc's listing on the host. No device.
#
#     src/cu/scaffolding/measuring_stick_engine.sh
set -u

TEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$TEST/../../.." && pwd)"
CYCLE="$TOP/src/cu/engine/analysis/cycle"
CYCLE_CU="$TOP/src/cu/engine/analysis/cycle"
CODEGEN="$TOP/src/cu/transpiler/codegen"
CODEGEN_CU="$TOP/src/cu/transpiler/codegen"
CODEGEN_CU_2="$TOP/src/cu/transpiler/lstar/parser"
KEYMATH="$TOP/src/cu/engine/analysis/keymath"
KEYMATH_CU="$TOP/src/cu/engine/analysis/keymath"
KEY_SCHEDULE="$TOP/src/cu/engine/analysis/key_schedule"
KEY_SCHEDULE_CU="$TOP/src/cu/engine/analysis/key_schedule"
NO_ROUNDING="$TOP/src/cu/types/integers"
SCRIPTURA="$TOP/src/cu/engine/runtime/scriptura"
CUBIN="$TOP/src/cu/scaffolding"
CUDA="/c/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.3"
OUT="${BUILD_OUT:-$TOP/build/measuring_stick}/engine"
mkdir -p "$OUT/objects"

INCLUDES=(-I "$TOP/src/cu/engine" -I "$TOP/src/cu/includes/codecs/crc" -I "$CYCLE"
    -I "$CYCLE_CU" -I "$CODEGEN" -I "$CODEGEN_CU" -I "$CODEGEN_CU_2" -I "$KEYMATH" -I "$KEYMATH_CU" -I "$KEY_SCHEDULE"
    -I "$KEY_SCHEDULE_CU" -I "$NO_ROUNDING" -I "$SCRIPTURA" -I "$CUBIN" -I "$TOP/utils/test/src/cu/engine/analysis/cycle")
OBJECTS=()
# an object is compiled again only where its source, or a header of the code generator or the interface, is newer
build_object()
{
    local object="$OUT/objects/$2.o"
    if [ ! -f "$object" ] || [ "$3" -nt "$object" ] || [ "$1" = c++-tool ] ||
        [ -n "$(find "$CODEGEN_CU" "$CODEGEN_CU_2" "$TOP/utils/test/src/cu/transpiler/lstar/interface" -name '*.h' \
            -newer "$object" -print -quit)" ]; then
        case "$1" in
            c) cc -std=c11 -O2 -Wall -Wextra "${INCLUDES[@]}" -I "$TOP/src/cu/transpiler/lstar/parser" \
                   -I "$TOP/src/cu/transpiler/lstar/interface" -c "$3" -o "$object" || exit 1 ;;
            *) c++ -std=c++17 -O2 -Wall "${INCLUDES[@]}" -x c++ -c "$3" -o "$object" || exit 1 ;;
        esac
    fi
    OBJECTS+=("$object")
}
for source in "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_machine.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_assemble.c" "$SCRIPTURA"/*.c \
    "$TOP/src/cu/scaffolding/interface_sass_probe_class.c" "$NO_ROUNDING"/exact_integer_{add,limbs,multiply,divide,gcd,decimal,hash}.c; do
    build_object c "$(basename "$source")" "$source"
done
# the assembly printer and the device's code generator run on the record machine and call the host oracle, which
# nothing here runs
for source in "$KEYMATH_CU/keymath.cu" "$KEY_SCHEDULE_CU/key_schedule.cu" "$CODEGEN_CU"/*.cu "$CODEGEN_CU_2"/*.cu "$CUBIN/cu_target.cu" "$CUBIN/sass_target.cu"; do
    case "$(basename "$source")" in
        asm_printer_*.cu | codegen*.cu) continue ;;
    esac
    build_object c++ "$(basename "$source")" "$source"
done
build_object c++-tool measuring_stick_engine.cpp "$TEST/measuring_stick_engine.cpp"
c++ -o "$OUT/measuring_stick_engine" "${OBJECTS[@]}" -static -lpthread || exit 1

# the rulesets are named from the top of the tree, which is where the generator reads them from; the tool writes the
# folds it finds into sass.ksc beside sass.krs; nvdisasm reads each kernel's encodings back as the part's own
# disassembler reads them. The assembler's refusals of the readings it gates go to assembler.log
cd "$TOP" || exit 1
rm -f "$OUT"/*.sass "$OUT"/*.bin "$OUT"/*.dis "$OUT"/*.registers
STICK="$(cygpath -m "${BUILD_OUT:-$TOP/build/measuring_stick}")"
"$OUT/measuring_stick_engine" "$STICK/measuring_stick.cu" "$STICK/measuring_stick_nvcc.sass" "$(cygpath -m "$OUT")" \
    > "$OUT/assembler.log" || exit 1
for code in "$OUT"/*.bin; do
    [ -f "$code" ] || continue
    "$CUDA/bin/nvdisasm" -b SM86 "$(cygpath -m "$code")" > "${code%.bin}.dis" 2>&1
done

# the stick's record, with each kernel written through the rulesets held against nvcc's listing
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) PYTHON=python ;;
    *) PYTHON=python3 ;;
esac
"$PYTHON" "$TEST/measuring_stick_read.py" "$STICK/measuring_stick_nvcc.sass" "$STICK/measuring_stick.tsv" \
    "$(cygpath -m "$TOP/src/cu/transpiler/lstar/coherence")/sass.krs" "$(cygpath -m "$TEST")/measuring_stick.md" \
    "$(cygpath -m "$OUT")/engine.tsv" "$(cygpath -m "$OUT")"
