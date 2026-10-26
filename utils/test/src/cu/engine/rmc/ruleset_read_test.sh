#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Every ruleset read against its code generator's schema, and the files each is read from held against each other for
# the relations no set of them should hold (ruleset_read_test.cu); files given as arguments are held against each
# other instead. The code generator is host code in .cu files, compiled as C++, and needs no CUDA toolchain and no
# device
set -u

TEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$TEST/../../../../../.." && pwd)"
CODEGEN="$TOP/src/cu/engine/rmc"
CODEGEN_CU="$TOP/src/cu/engine/rmc"
CODEGEN_CU_2="$TOP/src/cu/includes/parsers"
source "$TOP/utils/maint/engine/build_stamp.sh"
source "$TOP/src/cu/includes/file_defs/file_defs.sh"
build_stamp ruleset_read_test

INCLUDES=(-I "$TOP/src/cu/engine" -I "$CODEGEN" -I "$CODEGEN_CU" -I "$CODEGEN_CU_2" "${FILE_DEFS_MMGR_FLAGS[@]}")
BINARY="$OUT/ruleset_read_test"
rm -f "$BINARY"
OBJECTS=()
# the formats and MMgr's line, as C: the ruleset reader reads a ruleset's .ksc as the run stores it (qry_member.h)
for source in "${FILE_DEFS_FORMATS[@]}" "${FILE_DEFS_LINEA[@]}"; do
    object="$OUT/$(basename "$source" .c)_rr.o"
    rm -f "$object"
    cc -std=c11 -O2 "${INCLUDES[@]}" -c "$source" -o "$object"
    [ -f "$object" ] || { echo "  build failed: $(basename "$source") did not compile"; exit 1; }
    OBJECTS+=("$object")
done
for source in "$CODEGEN_CU"/*.cu "$CODEGEN_CU_2"/*.cu "$CODEGEN_CU_2"/../file_defs/krs/ruleset_flat/*.cu "$TEST/ruleset_read_test.cu"; do
    case "$(basename "$source")" in
        asm_printer_*.cu|codegen*.cu) continue ;;
    esac
    object="$OUT/$(basename "$source")_rr.o"
    rm -f "$object"
    "$FILE_DEFS_CXX" -std=c++17 -O2 -Wall -Wextra "${INCLUDES[@]}" -x c++ -c "$source" -o "$object"
    [ -f "$object" ] || { echo "  build failed: $(basename "$source") did not compile"; exit 1; }
    OBJECTS+=("$object")
done
"$FILE_DEFS_CXX" -o "$BINARY" "${OBJECTS[@]}" -static
[ -f "$BINARY" ] || { echo "  build failed: the test did not link"; exit 1; }

"$BINARY" "$@"
STATUS=$?
echo "  ruleset read test exit $STATUS"
exit "$STATUS"
