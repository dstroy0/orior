#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Every ruleset read against its code generator's schema, and the files each is read from held against each other for
# the relations no set of them should hold (ruleset_read_test.cu); files given as arguments are held against each
# other instead. The code generator is host code in .cu files, compiled as C++, and needs no CUDA toolchain and no
# device
set -u

TEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$TEST/../../../../../.." && pwd)"
CODEGEN="$TOP/src/cu/transpiler/codegen"
CODEGEN_CU="$TOP/src/cu/transpiler/codegen"
CODEGEN_CU_2="$TOP/src/cu/transpiler/lstar/parser"
source "$TOP/utils/maint/engine/build_stamp.sh"
build_stamp ruleset_read_test

INCLUDES=(-I "$TOP/src/cu/engine" -I "$CODEGEN" -I "$CODEGEN_CU" -I "$CODEGEN_CU_2")
BINARY="$OUT/ruleset_read_test"
rm -f "$BINARY"
OBJECTS=()
for source in "$CODEGEN_CU"/*.cu "$CODEGEN_CU_2"/*.cu "$TEST/ruleset_read_test.cu"; do
    case "$(basename "$source")" in
        asm_printer_*.cu|codegen*.cu) continue ;;
    esac
    object="$OUT/$(basename "$source")_rr.o"
    rm -f "$object"
    c++ -std=c++17 -O2 -Wall -Wextra "${INCLUDES[@]}" -x c++ -c "$source" -o "$object"
    [ -f "$object" ] || { echo "  build failed: $(basename "$source") did not compile"; exit 1; }
    OBJECTS+=("$object")
done
c++ -o "$BINARY" "${OBJECTS[@]}" -static
[ -f "$BINARY" ] || { echo "  build failed: the test did not link"; exit 1; }

"$BINARY" "$@"
STATUS=$?
echo "  ruleset read test exit $STATUS"
exit "$STATUS"
