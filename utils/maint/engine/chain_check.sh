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
#   answer_read_check holds the read of an ask to P11's four answers and gray
#   kqr_check        holds R, the .kqr's reader and writer, to what its oracle says of it, on a record it writes and
#                    on the part's own, written as records and read back as its bytes
#   who_check        holds a who to what the who table gives, and every write to the who the run records of it
#   qry_rows_check   holds a row of a table of the run to its who and its columns as handed, and a text of the table
#                    of texts to its number, handed once
#   kdecode_check    holds kdecode's select over a run's tables to what the run holds, on the run the checks above wrote
#   ksc_check        holds the .ksc's reader and writer to what its users read of it, a question's rows replaced under
#                    a who line, the rows let go handed back, and the part's cost rows put again as it was
#   krf_check        holds the .krf's reader and writer to what its users read of it: a row a k-file let go kept with
#                    its type, the run's name, the reason and its text, under a who line
#   frame_check      holds the frame's walk, krs_write's head and fixed lines, the kernel mode's place and a
#                    register's reading at the slot to the .ksc's rows, and krs_write's frame lines to the table of
#                    frame forms and the named ruleset
#   frame_relation_check  holds the frame relations' host answers, their tuple places and the thread their fields are
#                    read at, and the channel's fill word and block to the carrier's own
#   turning_check    holds a ladder form's turning case: the first case of its relation whose three readings in the
#                    register pass stand apart, the shift up's (3, 4) among them
#   standing_check   holds one form standing for the encodings that answer as it does: a turn outside its operands
#                    is its, a bit of an operand, the key or the scheduler's word is another's, and the set folded
#                    keeps the oldest
#   tables_funnel_check  holds the vendor's tables read off a capture of its disassembler: the records, the
#                    archive's LZ4 blocks unpacked with one window, and the latencies gathered from the copies
#   naming_check     holds the vendor's writer's texts on forms written by hand: the name by relation, the mark on a
#                    take form's second word, a flag bit printing nothing, and the guard before the operation
#   ranges_check     holds the cross's cut of the distinct slot encodings over the host's processors: the ranges tile
#                    the encodings end to end, none empty and none past the last
#   tap_check        holds the writer's tap: a reader on the pipe reads one line a blob as the writer places it, a
#                    panic's first, each line its blob's name and size
#   kdm_check        holds the .kdm's reader and writer and the block rule to what their users read of them, the
#                    section and the pipes written again under who lines, and the part's section written again as it was
#   survivors_check  holds the gate to P2 on the ladder's own relations
#   scheduler_check  holds a round to P13 on the run channel's dry carrier
#   overseer_check   holds the overseer's conference to its consensus and its lessons, in a run of its own, on a
#                    carrier that refuses every question and reaches no part
#   query_interface_check walks asks from inside the cell, every ending kept as the answer of the address that caused it
#   launch_queue_check holds the interface's launch queue: eight starts asked at once hold at most four places at a
#                    time, and each start follows the one before it by the spacing at least
#   query_order_check puts the known order of asks to the host on a clock found by asking, and solves every link
#   stem_group_check holds the stem membership rule: groups on anchors, the same in every order
#   branch_side_check asks the host whether the side a branch is read from leaves a mark
#   gnascor_trace    puts gnascor_scenario.txt's sides to the host as real asks, and gnascor_read.py reads the states
#
#     utils/maint/engine/chain_check.sh
#     utils/maint/engine/chain_check.sh sm_86 src/cu/transpiler/lstar/protocol/table/sm_86.kdm
#
# With arguments it writes that part's .kdm to that path; with none it runs every check and stops.
set -u

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$TOP/src/cu/types/file_defs/file_defs.sh"
OUT="$TOP/build/engine"
mkdir -p "$OUT"

for one in "$TOP/src/cu/transpiler/lstar/parser/kdm_write.c" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/chain_check.c"; do
    name="$(basename "$one" .c)"
    cc -std=c11 -O2 -Wall -Wextra -o "$OUT/$name" "$one" \
        "$TOP/src/cu/transpiler/lstar/protocol/gate/chain_build.c" "${FILE_DEFS[@]}" || exit 1
done

# the descent is orior's own, and orior reads exact integers
SIFT="$TOP/src/cu/engine/nbody/orior"
EXACT="$TOP/src/cu/types/integers"
cc -std=c11 -O2 -Wall -Wextra -I"$SIFT" -I"$EXACT" -o "$OUT/gate_descent" \
    "$TOP/utils/test/src/cu/transpiler/lstar/protocol/gate_descent.c" "$TOP/src/cu/transpiler/lstar/protocol/gate/chain_build.c" \
    "$SIFT/orior_core.c" "$SIFT/orior_field.c" "$SIFT/orior_steer.c" \
    "$SIFT/orior_steer_count.c" "$SIFT/orior_steer_plan.c" "$SIFT/scan.c" \
    "$EXACT/exact_integer_add.c" "$EXACT/exact_integer_multiply.c" "$EXACT/exact_integer_limbs.c" \
    "${FILE_DEFS[@]}" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/ask_order_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/ask_order_check.c" \
    "$TOP/src/cu/transpiler/lstar/protocol/order/ask_order.c" \
    "$EXACT/exact_integer_add.c" "$EXACT/exact_integer_multiply.c" "$EXACT/exact_integer_limbs.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/query_ask_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/query_ask_check.c" \
    "$TOP/src/cu/transpiler/lstar/protocol/query/query_ask.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/answer_read_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/answer_read_check.c" \
    "$TOP/src/cu/transpiler/lstar/protocol/query/answer_read.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/frame_relation_check" "$TOP/utils/test/src/cu/transpiler/lstar/parser/frame_relation_check.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/turning_check" "$TOP/utils/test/src/cu/transpiler/lstar/parser/turning_check.c" \
    "$TOP/src/cu/transpiler/lstar/parser/khw_write/turning.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/standing_check" "$TOP/utils/test/src/cu/transpiler/lstar/parser/standing_check.c" \
    "$TOP/src/cu/transpiler/lstar/parser/khw_write/standing.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/tables_funnel_check" "$TOP/utils/test/src/cu/transpiler/vendor_bin_layouts/nvidia/tables_funnel_check.c" \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/tables/funnel.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/naming_check" "$TOP/utils/test/src/cu/transpiler/vendor_bin_layouts/nvidia/naming_check.c" \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/khw_machine_write/pieces.c" \
    "$TOP"/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_machine/*.c \
    "$TOP"/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_assemble/*.c \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_write.c" \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/container_write.c" \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/container_pattern.c" \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/container_layout.c" \
    "${FILE_DEFS[@]}" || exit 1
cc -std=c11 -O2 -Wall -Wextra -pthread -o "$OUT/ranges_check" "$TOP/utils/test/src/cu/transpiler/vendor_bin_layouts/nvidia/ranges_check.c" \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/measuring_stick_query/ranges.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/kqr_check" "$TOP/utils/test/src/cu/types/file_defs/kqr/kqr_check.c" \
    "${FILE_DEFS[@]}" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/who_check" "$TOP/utils/test/src/cu/types/file_defs/who/who_check.c" \
    "${FILE_DEFS[@]}" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/qry_rows_check" "$TOP/utils/test/src/cu/types/file_defs/qry/qry_rows_check.c" \
    "${FILE_DEFS[@]}" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/ksc_check" "$TOP/utils/test/src/cu/types/file_defs/ksc/ksc_check.c" \
    "${FILE_DEFS[@]}" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/krf_check" "$TOP/utils/test/src/cu/types/file_defs/krf/krf_check.c" \
    "${FILE_DEFS[@]}" || exit 1
cc -std=c11 -O2 -Wall -Wextra -pthread -o "$OUT/tap_check" "$TOP/utils/test/src/cu/types/file_defs/qry/tap_check.c" \
    "${FILE_DEFS[@]}" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/frame_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/frame_check.c" \
    "$TOP/src/cu/transpiler/lstar/parser/krs_write/head.c" \
    "$TOP/src/cu/transpiler/lstar/parser/krs_write/fixed.c" \
    "$TOP/src/cu/transpiler/lstar/parser/krs_write/frame.c" \
    "$TOP/src/cu/transpiler/lstar/parser/krs_write/words.c" \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/khw_machine_write/opening.c" \
    "${FILE_DEFS[@]}" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/kdm_check" "$TOP/utils/test/src/cu/types/file_defs/kdm/kdm_check.c" \
    "${FILE_DEFS[@]}" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/survivors_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/survivors_check.c" \
    "$TOP/src/cu/transpiler/lstar/protocol/gate/survivors.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/scheduler_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/scheduler_check.c" \
    "$TOP/src/cu/transpiler/lstar/protocol/order/scheduler.c" "$TOP"/src/cu/transpiler/lstar/protocol/teacher/run_channel/*.c \
    "$TOP"/src/cu/transpiler/lstar/protocol/overseer/*.c "$TOP/src/cu/transpiler/lstar/interface/interface.c" "$TOP"/src/cu/transpiler/lstar/interface/platform/*.c \
    "$TOP/src/cu/transpiler/lstar/interface/interface_names.c" "${FILE_DEFS[@]}" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/overseer_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/overseer_check.c" \
    "$TOP"/src/cu/transpiler/lstar/protocol/overseer/*.c "$TOP"/src/cu/transpiler/lstar/protocol/teacher/run_channel/*.c \
    "$TOP/src/cu/transpiler/lstar/interface/interface.c" "$TOP"/src/cu/transpiler/lstar/interface/platform/*.c \
    "$TOP/src/cu/transpiler/lstar/interface/interface_names.c" "${FILE_DEFS[@]}" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/overseer_carrier" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/overseer_carrier.c" \
    "${FILE_DEFS[@]}" || exit 1
# the walk runs as its own program, in a child the cell can lose
BOOT="$TOP/src/cu/transpiler/lstar/protocol"
INTERFACE="$TOP/src/cu/transpiler/lstar/interface"
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/query_walk" "$BOOT/query/query_walk.c" "$BOOT/query/query_ask.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -I"$TOP/src/cu/engine" -I "$TOP/src/cu/engine" -o "$OUT/query_interface_check" \
    "$TOP/utils/test/src/cu/transpiler/lstar/protocol/query_interface_check.c" "$BOOT/query/query_interface.c" "$INTERFACE/interface.c" "$INTERFACE"/platform/*.c \
    "$INTERFACE/interface_names.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -pthread -I"$TOP/src/cu/engine" -o "$OUT/launch_queue_check" \
    "$TOP/utils/test/src/cu/transpiler/lstar/interface/launch_queue_check.c" "$INTERFACE/interface.c" "$INTERFACE"/platform/*.c \
    "$INTERFACE/interface_names.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/query_order_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/query_order_check.c" \
    "$BOOT/order/query_order.c" "$BOOT/query/query_ask.c" "$BOOT/order/ask_order.c" \
    "$EXACT/exact_integer_add.c" "$EXACT/exact_integer_multiply.c" "$EXACT/exact_integer_limbs.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/stem_group_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/stem_group_check.c" \
    "$BOOT/order/stem_group.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/branch_side_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/branch_side_check.c" \
    "$BOOT/order/query_order.c" "$BOOT/query/query_ask.c" "$BOOT/order/ask_order.c" \
    "$EXACT/exact_integer_add.c" "$EXACT/exact_integer_multiply.c" "$EXACT/exact_integer_limbs.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/gnascor_trace" "$TOP/utils/maint/engine/gnascor_trace.c" "$BOOT/query/query_ask.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/host_entry_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/host_entry_check.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/classify_walk" "$BOOT/query/classify_walk.c" || exit 1
cc -std=c11 -O2 -Wall -Wextra -I"$TOP/src/cu/engine" -I "$TOP/src/cu/engine" -o "$OUT/bus_enum_check" \
    "$TOP/utils/test/src/cu/transpiler/lstar/protocol/bus_enum_check.c" "$BOOT/query/bus_enum.c" "$BOOT/query/bus_enum_walk.c" "$BOOT/query/query_interface.c" \
    "$INTERFACE/interface.c" "$INTERFACE"/platform/*.c "$INTERFACE/interface_names.c" || exit 1
WALK="$OUT/query_walk"
if [ -f "$WALK.exe" ]; then
    WALK="$WALK.exe"
fi
CLASSIFY="$OUT/classify_walk"
if [ -f "$CLASSIFY.exe" ]; then
    CLASSIFY="$CLASSIFY.exe"
fi

cd "$TOP" || exit 1
# the .kdm written in a query run of its own, build/engine/kdm/qry (qry_run.sh), whose writer writes it in its place
if [ "$#" -gt 0 ]; then
    source "$TOP/src/cu/types/file_defs/qry/qry_run.sh"
    qry_writer_built "$OUT"
    run_started "$OUT/kdm/qry"
    run_read "$2"
    "$OUT/kdm_write" "$@"
    exit "$?"
fi
"$OUT/chain_check" || exit 1
"$OUT/gate_descent" || exit 1
"$OUT/ask_order_check" || exit 1
"$OUT/query_ask_check" || exit 1
"$OUT/answer_read_check" || exit 1
"$OUT/frame_relation_check" || exit 1
"$OUT/turning_check" || exit 1
"$OUT/standing_check" || exit 1
"$OUT/tables_funnel_check" || exit 1
"$OUT/naming_check" "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/mnemonic_nvidia.tsv" || exit 1
"$OUT/ranges_check" || exit 1
mkdir -p "$OUT/tap_check.run"
"$OUT/tap_check" "$OUT/tap_check.run" || exit 1
# R's check, the who's and the rows' in a run of their own, whose writer writes the records and the file they are held
# to, ended with the subshell
(
    source "$TOP/src/cu/types/file_defs/qry/qry_run.sh"
    qry_writer_built "$OUT"
    unset QRY
    run_started "$OUT/kqr_runs"
    "$OUT/kqr_check" "$OUT" "$TOP/src/cu/transpiler/lstar/protocol/table/sm_86.kqr" &&
        "$OUT/who_check" "$OUT" &&
        "$OUT/qry_rows_check" &&
        "$OUT/frame_check" "$OUT" &&
        "$OUT/krf_check" "$OUT" &&
        "$OUT/ksc_check" "$OUT" "$TOP/src/cu/transpiler/lstar/protocol/table/sm_86.ksc" &&
        "$OUT/kdm_check" "$OUT" "$TOP/src/cu/transpiler/lstar/protocol/table/sm_86.kdm"
) || exit 1
# the file kdm_check handed its run, in its place once that run has ended: it names the run's .qry, the newest
newest_run="$(ls -t "$OUT"/kqr_runs/query*.qry | head -1)"
command -v cygpath > /dev/null && newest_run="$(cygpath -m "$newest_run")"
if grep -qF "# $newest_run" "$OUT/kdm_check_put.kdm" 2> /dev/null; then
    echo "  the file the run held is in its place now the run has ended"
else
    echo "  the file the run held is NOT in its place now the run has ended"
    exit 1
fi
# kdecode's select over the tables of that run, read once it has ended
bash "$TOP/utils/test/src/cu/types/file_defs/kdecode_check.sh" "$newest_run" || exit 1
"$OUT/survivors_check" || exit 1
mkdir -p "$OUT/scheduler_check.dry"
"$OUT/scheduler_check" "$OUT/scheduler_check.dry" || exit 1
# the overseer's check in a run of its own, whose writer takes the round the teacher hands critical, ended with the
# subshell
(
    OVERSEER_CARRIER="$OUT/overseer_carrier"
    [ -f "$OVERSEER_CARRIER.exe" ] && OVERSEER_CARRIER="$OVERSEER_CARRIER.exe"
    command -v cygpath > /dev/null && OVERSEER_CARRIER="$(cygpath -m "$OVERSEER_CARRIER")"
    source "$TOP/src/cu/types/file_defs/qry/qry_run.sh"
    qry_writer_built "$OUT"
    unset QRY
    run_started "$OUT/overseer_runs"
    "$OUT/overseer_check" "$OVERSEER_CARRIER"
) || exit 1
"$OUT/host_entry_check" || exit 1
"$OUT/bus_enum_check" "$CLASSIFY" "$OUT/classify_walk.out" || exit 1
"$OUT/query_interface_check" "$WALK" "$OUT/query_walk.out" || exit 1
"$OUT/launch_queue_check" || exit 1
"$OUT/query_order_check" || exit 1
"$OUT/stem_group_check" || exit 1
"$OUT/branch_side_check" || exit 1
"$OUT/gnascor_trace" "$TOP/utils/maint/engine/gnascor_scenario.txt" "$OUT/gnascor_trace.txt" || exit 1
python "$TOP/utils/maint/engine/gnascor_read.py" --check || exit 1
python "$TOP/utils/maint/engine/gnascor_read.py" --scenario "$TOP/utils/maint/engine/gnascor_scenario.txt" "$OUT/gnascor_trace.txt"
