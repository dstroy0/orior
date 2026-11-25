#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# The record machine's lane as VHDL held to the host oracle, where GHDL is on the path: the code generator, keymath and
# key_schedule and krep are host code in .cu files and the test C++, compiled as C++; cycle.c, the exact integer's
# pieces and MMgr's line as C. GHDL analyzes and runs each schedule of each program's lane in a folder of its own
# under the build's output, all of them at once. Where the build's output holds vhdl.kcs (vhdl_construction_set.sh),
# that construction set cuts the lane as well. GHDL's LLVM backend runs the benches where it is installed.
set -u

TEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$TEST/../../.." && pwd)"
CYCLE="$TOP/src/cu/engine/analysis/cycle"
CYCLE_CU="$TOP/src/cu/engine/analysis/cycle"
CODEGEN="$TOP/src/cu/engine/rmc"
CODEGEN_CU="$TOP/src/cu/engine/rmc"
CODEGEN_CU_2="$TOP/includes/parsers"
KEYMATH="$TOP/src/cu/engine/analysis/keymath"
KEYMATH_CU="$TOP/src/cu/engine/analysis/keymath"
KEY_SCHEDULE="$TOP/src/cu/engine/analysis/key_schedule"
KEY_SCHEDULE_CU="$TOP/src/cu/engine/analysis/key_schedule"
KREP="$TOP/includes/file_defs/krep"
KREP_CU="$TOP/includes/file_defs/krep"
NO_ROUNDING="$TOP/includes/exact_types/integers"
source "$TOP/includes/file_defs/file_defs.sh"
source "$TOP/includes/exact_types/exact_types.sh"
source "$TOP/utils/maint/engine/build_stamp.sh"
build_stamp record_vhdl_test

command -v ghdl > /dev/null 2>&1 || { echo "  not run: GHDL is not on the path"; exit 0; }
# The LLVM and GCC backends compile each bench to code of the machine, which runs it far faster than the mcode backend
if [ -x "$(command -v ghdl-llvm)" ]; then
    export GHDL_BACKEND=llvm
elif [ -x "$(command -v ghdl-gcc)" ]; then
    export GHDL_BACKEND=gcc
fi
INCLUDES=(-I "$TOP/src/cu/engine" -I "$TOP/includes/codecs/crc" -I "$CYCLE" -I "$CYCLE_CU" -I "$CODEGEN" -I "$CODEGEN_CU" -I "$CODEGEN_CU_2" -I "$KEYMATH" -I "$KEYMATH_CU"
          -I "$KEY_SCHEDULE" -I "$KEY_SCHEDULE_CU" -I "$KREP" -I "$KREP_CU" -I "$NO_ROUNDING" "${FILE_DEFS_MMGR_FLAGS[@]}")
BINARY="$OUT/record_vhdl_test"
rm -f "$BINARY"
OBJECTS=()
for source in "${FILE_DEFS_FORMATS[@]}" "${FILE_DEFS_LINEA[@]}" "${EXACT_TYPES_INTEGERS[@]}" "${EXACT_TYPES_PLATFORM[@]}" "${EXACT_TYPES_CARCER[@]}" \
              "$CYCLE/cycle.c"; do
    object="$OUT/$(basename "$source" .c)_vhdl.o"
    rm -f "$object"
    cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra "${INCLUDES[@]}" -c "$source" -o "$object"
    [ -f "$object" ] || { echo "  build failed: $(basename "$source") did not compile"; exit 1; }
    OBJECTS+=("$object")
done
for source in "$KEYMATH_CU/keymath.cu" "$KEY_SCHEDULE_CU/key_schedule.cu" "$CODEGEN_CU"/*.cu "$CODEGEN_CU_2"/*.cu "$CODEGEN_CU_2"/../file_defs/krs/ruleset_flat/*.cu "$KREP_CU"/krep_{io,sections}.cu \
              "$TEST/vhdl_target.cu" "$TEST/yosys_script.cu" "$TEST/record_vhdl_test.cpp"; do
    object="$OUT/$(basename "$source")_vhdl.o"
    rm -f "$object"
    "$FILE_DEFS_CXX" "${FILE_DEFS_LTO_CC[@]}" -std=c++20 -O2 -Wall -Wextra "${INCLUDES[@]}" -x c++ -c "$source" -o "$object"
    [ -f "$object" ] || { echo "  build failed: $(basename "$source") did not compile"; exit 1; }
    OBJECTS+=("$object")
done
"$FILE_DEFS_CXX" "${FILE_DEFS_LTO_CC[@]}" -o "$BINARY" "${OBJECTS[@]}" "${EXACT_TYPES_LIBRARIES[@]}"
[ -f "$BINARY" ] || { echo "  build failed: the test did not link"; exit 1; }

# The work folder holds a folder for each job, made new for each run. Where WSL mounts the ext4 filesystem of the
# tests, the work folder is on it. A drive that WSL shares with Windows (/mnt) takes GHDL's small writes slowly
WORK="$OUT/record_vhdl_work"
if [ -d /mnt/orior_test ] && [ -w /mnt/orior_test ]; then
    WORK="/mnt/orior_test/record_vhdl_work"
fi
rm -rf "$WORK"
mkdir -p "$WORK"
# where Yosys is on the path too, the test synthesizes each lane as well
SYNTHESIS=()
if command -v yosys > /dev/null 2>&1; then
    SYNTHESIS=(--synthesis)
fi
CONSTRUCTION=()
if [ -f "$OUT/vhdl.kcs" ]; then
    CONSTRUCTION=(--construction "$OUT/vhdl.kcs")
fi
"$BINARY" "$WORK" "$TEST/record_vhdl_bench.vhd" "${SYNTHESIS[@]}" "${CONSTRUCTION[@]}"
STATUS=$?
echo "  record vhdl test exit $STATUS"
exit "$STATUS"
