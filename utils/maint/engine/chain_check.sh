#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds and runs the things that work off the ladder's relations and the query protocol's ask, none of which needs
# a device, a toolchain or a machine file:
#
#   kdm_write     writes a part's .kdm: every arrangement of primitives that produces each operator
#   chain_check   reads how much of a relation the ladder's own cases decide
#   gate_descent  runs the gate as orior's descent and checks it against every arrangement asked every case
#   ask_order_check  holds the known order of asks to its exact claims and measures the contention read
#   query_ask_check  holds the ask to what it answers at addresses whose state is known, and finds a clock
#   query_interface_check walks asks from inside the cell, every ending kept as the answer of the address that caused it
#   query_order_check puts the known order of asks to the host on a clock found by asking, and solves every link
#   stem_group_check holds the stem membership rule: groups on anchors, the same in every order
#   branch_side_check asks the host whether the side a branch is read from leaves a mark
#   gnascor_trace    puts gnascor_scenario.txt's sides to the host as real asks, and gnascor_read.py reads the states
#
#     utils/maint/engine/chain_check.sh
#     utils/maint/engine/chain_check.sh sm_86 src/cu/transpiler/lstar/coherence/sm_86.kdm
#
# With arguments it writes that part's .kdm to that path; with none it runs every check and stops.
set -u

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT="$TOP/build/engine"
mkdir -p "$OUT"

for one in "$TOP/src/cu/transpiler/lstar/parser/kdm_write.c" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/chain_check.c"; do
    name="$(basename "$one" .c)"
    cc -std=c11 -O2 -Wall -Wextra -o "$OUT/$name" "$one" \
        "$TOP/src/cu/transpiler/lstar/protocol/chain_build.c" || exit 1
done

# the descent is orior's own, and orior reads exact integers
SIFT="$TOP/src/cu/engine/nbody/orior"
EXACT="$TOP/src/cu/types/integers"
cc -std=c11 -O2 -Wall -Wextra -I"$SIFT" -I"$EXACT" -o "$OUT/gate_descent" \
    "$TOP/utils/test/src/cu/transpiler/lstar/protocol/gate_descent.c" "$TOP/src/cu/transpiler/lstar/protocol/chain_build.c" \
    "$SIFT/orior_core.c" "$SIFT/orior_field.c" "$SIFT/orior_steer.c" \
    "$SIFT/orior_steer_count.c" "$SIFT/orior_steer_plan.c" "$SIFT/scan.c" \
    "$EXACT/exact_integer_add.c" "$EXACT/exact_integer_multiply.c" "$EXACT/exact_integer_limbs.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/ask_order_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/ask_order_check.c" \
    "$TOP/src/cu/transpiler/lstar/protocol/ask_order.c" \
    "$EXACT/exact_integer_add.c" "$EXACT/exact_integer_multiply.c" "$EXACT/exact_integer_limbs.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/query_ask_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/query_ask_check.c" \
    "$TOP/src/cu/transpiler/lstar/protocol/query_ask.c" || exit 1
# the walk runs as its own program, in a child the cell can lose
BOOT="$TOP/src/cu/transpiler/lstar/protocol"
INTERFACE="$TOP/src/cu/transpiler/lstar/interface"
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/query_walk" "$BOOT/query_walk.c" "$BOOT/query_ask.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -I"$TOP/src/cu/engine" -I "$TOP/src/cu/engine" -o "$OUT/query_interface_check" \
    "$TOP/utils/test/src/cu/transpiler/lstar/protocol/query_interface_check.c" "$BOOT/query_interface.c" "$INTERFACE/interface.c" \
    "$INTERFACE/interface_names.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/query_order_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/query_order_check.c" \
    "$BOOT/query_order.c" "$BOOT/query_ask.c" "$BOOT/ask_order.c" \
    "$EXACT/exact_integer_add.c" "$EXACT/exact_integer_multiply.c" "$EXACT/exact_integer_limbs.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/stem_group_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/stem_group_check.c" \
    "$BOOT/stem_group.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/branch_side_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/branch_side_check.c" \
    "$BOOT/query_order.c" "$BOOT/query_ask.c" "$BOOT/ask_order.c" \
    "$EXACT/exact_integer_add.c" "$EXACT/exact_integer_multiply.c" "$EXACT/exact_integer_limbs.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/gnascor_trace" "$TOP/utils/maint/engine/gnascor_trace.c" "$BOOT/query_ask.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/host_entry_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/host_entry_check.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/classify_walk" "$BOOT/classify_walk.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -I"$TOP/src/cu/engine" -I "$TOP/src/cu/engine" -o "$OUT/bus_enum_check" \
    "$TOP/utils/test/src/cu/transpiler/lstar/protocol/bus_enum_check.c" "$BOOT/bus_enum.c" "$BOOT/bus_enum_walk.c" "$BOOT/query_interface.c" \
    "$INTERFACE/interface.c" "$INTERFACE/interface_names.c" || exit 1
WALK="$OUT/query_walk"
if [ -f "$WALK.exe" ]; then
    WALK="$WALK.exe"
fi
CLASSIFY="$OUT/classify_walk"
if [ -f "$CLASSIFY.exe" ]; then
    CLASSIFY="$CLASSIFY.exe"
fi

cd "$TOP" || exit 1
if [ "$#" -gt 0 ]; then
    "$OUT/kdm_write" "$@"
    exit "$?"
fi
"$OUT/chain_check" || exit 1
"$OUT/gate_descent" || exit 1
"$OUT/ask_order_check" || exit 1
"$OUT/query_ask_check" || exit 1
"$OUT/host_entry_check" || exit 1
"$OUT/bus_enum_check" "$CLASSIFY" "$OUT/classify_walk.out" || exit 1
"$OUT/query_interface_check" "$WALK" "$OUT/query_walk.out" || exit 1
"$OUT/query_order_check" || exit 1
"$OUT/stem_group_check" || exit 1
"$OUT/branch_side_check" || exit 1
"$OUT/gnascor_trace" "$TOP/utils/maint/engine/gnascor_scenario.txt" "$OUT/gnascor_trace.txt" || exit 1
python "$TOP/utils/maint/engine/gnascor_read.py" --check || exit 1
python "$TOP/utils/maint/engine/gnascor_read.py" --scenario "$TOP/utils/maint/engine/gnascor_scenario.txt" "$OUT/gnascor_trace.txt"
