#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# The record machine's lane as C source held to the host oracle off the device (record_c_test.cpp): the code generator,
# keymath and key_schedule are host code in .cu files and the test C++, compiled as C++; cycle.c, the exact integer's
# pieces and MMgr's line as C. Each program's lane is built by the host's C++ compiler and run in a work folder under the build's
# output.
set -u

TEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$TEST/../../../../../../.." && pwd)"
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
source "$TOP/utils/maint/engine/build_stamp.sh"
build_stamp record_c_test

INCLUDES=(-I "$TOP/src/cu/engine" -I "$TOP/includes/codecs/crc" -I "$CYCLE" -I "$CYCLE_CU" -I "$CODEGEN" -I "$CODEGEN_CU" -I "$CODEGEN_CU_2" -I "$KEYMATH" -I "$KEYMATH_CU"
          -I "$KEY_SCHEDULE" -I "$KEY_SCHEDULE_CU" -I "$NO_ROUNDING" "${FILE_DEFS_MMGR_FLAGS[@]}")
BINARY="$OUT/record_c_test"
rm -f "$BINARY"
OBJECTS=()
for source in "${FILE_DEFS_FORMATS[@]}" "${FILE_DEFS_LINEA[@]}" "$NO_ROUNDING"/exact_integer_{add,limbs,multiply,divide,gcd,decimal,hash}.c \
              "$CYCLE/cycle.c"; do
    object="$OUT/$(basename "$source" .c)_c.o"
    rm -f "$object"
    cc -std=c11 -O2 -Wall -Wextra "${INCLUDES[@]}" -c "$source" -o "$object"
    [ -f "$object" ] || { echo "  build failed: $(basename "$source") did not compile"; exit 1; }
    OBJECTS+=("$object")
done
for source in "$KEYMATH_CU/keymath.cu" "$KEY_SCHEDULE_CU/key_schedule.cu" "$CODEGEN_CU"/*.cu "$CODEGEN_CU_2"/*.cu "$CODEGEN_CU_2"/../file_defs/krs/ruleset_flat/*.cu "$TEST/record_c_test.cpp"; do
    object="$OUT/$(basename "$source")_c.o"
    rm -f "$object"
    "$FILE_DEFS_CXX" -std=c++17 -O2 -Wall -Wextra "${INCLUDES[@]}" -x c++ -c "$source" -o "$object"
    [ -f "$object" ] || { echo "  build failed: $(basename "$source") did not compile"; exit 1; }
    OBJECTS+=("$object")
done
"$FILE_DEFS_CXX" -o "$BINARY" "${OBJECTS[@]}" -lpthread
[ -f "$BINARY" ] || { echo "  build failed: the test did not link"; exit 1; }

WORK="$OUT/c_work"
rm -rf "$WORK"
mkdir -p "$WORK"
"$BINARY" "$WORK" c++
STATUS=$?
rm -rf "$WORK"
echo "  record c test exit $STATUS"
exit "$STATUS"
