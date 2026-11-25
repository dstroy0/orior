#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds and runs the things that work off the ladder's relations and the query protocol's ask, none of which needs
# a device, a toolchain or a machine file:
#
#   - `kdm_write` writes a part's .kdm: every arrangement of primitives that produces each operator.
#   - `chain_check` reads how much of a relation the ladder's own cases decide.
#   - `gate_descent` runs the gate as orior's descent, and checks it against every arrangement asked every case.
#   - `hadamard_order_check` holds the known order of asks to its exact claims, and measures the contention read.
#   - `query_put_check` holds the ask to what it answers at addresses of a known state, and finds a clock.
#   - `answer_read_check` holds the read of an ask to P11's four answers and gray.
#   - `kqr_check` holds R, the .kqr's reader and writer, to what its oracle says of it. It does this on a record it
#     writes, and on the part's own, written as records and read back as its bytes.
#   - `who_check` holds a who to what the who table gives, and every write to the who that the run records of it.
#   - `qry_rows_check` holds a row of a table of the run to its who and its columns as passed. It holds a text of the
#     table of texts to its number, passed once.
#   - `kdecode_check` holds orior_kdecode's select over a run's tables to what the run holds. It does this on the run
#     that the checks above wrote.
#   - `ksc_check` holds the .ksc's reader and writer to what its users read of it. A question's rows go again under a
#     who line, the rows let go come back, and the part's cost rows go again as they were.
#   - `krf_check` holds the .krf's reader and writer to what its users read of it. A row that a k-file let go stays
#     with its type, the run's name, the reason and its text, under a who line.
#   - `frame_check` holds the frame's walk, orior_krs_write's head and fixed lines, the kernel mode's place, and a
#     register's reading at the position to the .ksc's rows. It holds orior_krs_write's frame lines to the table of
#     frame forms and the named ruleset.
#   - `frame_relation_check` holds the frame relations' host answers, their tuple places, and the thread that their
#     fields come from. It holds the channel's fill word and block to the carrier's own.
#   - `turning_check` holds a ladder form's case of a turn. That is the first case of its relation whose three readings
#     in the register pass are apart, the shift up's (3, 4) among them.
#   - `standing_check` holds one form located for the encodings that answer as it does. A turn outside its operands is
#     its own. A bit of an operand, the key or the scheduler's word is another's. The set folded keeps the oldest.
#   - `tables_funnel_check` holds the vendor's tables read off a capture of its disassembler. These are the records,
#     the archive's LZ4 blocks unpacked with one window, and the latencies gathered from the copies.
#   - `tables_read_check` holds the vendor's instruction set read: the classes, their opcodes and fields, the fields
#     joined on one line, and the class of an encoding.
#   - `constant_check` holds rule 13: a read of the constant bank past the bank, or of a bank the container does not
#     have.
#   - `naming_check` holds the vendor's writer's texts on forms written by hand. These are the name by relation, the
#     mark on a take form's second word, and a flag bit that prints nothing. Last is the guard before the operation.
#   - `cubin_listing_check` holds the cut of a cubin's listing into each kernel's, as the vendor prints a kernel's code
#     read alone. That is its instruction lines, each label put as its offset, and a branch note put as one space.
#   - `kdm_check` holds the .kdm's reader and writer and the block rule to what their users read of them. The section
#     and the pipes go again under who lines, and the part's section goes again as it was.
#   - `survivors_check` holds the gate to P2 on the ladder's own relations.
#   - `scheduler_check` holds a round to P13 on the run channel's dry carrier.
#   - `overseer_check` holds the overseer's conference to its consensus and its lessons, in a run of its own. The
#     carrier refuses every question and reaches no part.
#   - `query_interface_check` walks asks from inside the cell. Every ending stays as the answer of the address that
#     caused it.
#   - `launch_queue_check` holds the interface's launch queue. Eight starts asked at once hold at max four places at a
#     time. Each start follows the one before it by the spacing at least.
#   - `query_order_check` puts the known order of asks to the host on a clock that the asks find, and solves every
#     link.
#   - `stem_group_check` holds the stem membership rule: groups on anchors, the same in every order.
#   - `branch_side_check` asks the host whether the side that a branch reads from leaves a mark.
#   - `gnascor_trace` puts gnascor_scenario.txt's sides to the host as real asks, and gnascor_read.py reads the states.
#
#     utils/maint/engine/chain_check.sh
#     utils/maint/engine/chain_check.sh sm_86 src/cu/transpiler/lstar/protocol/table/sm_86.kdm
#
# With arguments it writes that part's .kdm to that path; with none it runs every check and stops.
set -u

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$TOP/includes/file_defs/file_defs.sh"
source "$TOP/includes/exact_types/exact_types.sh"
OUT="$TOP/build/engine"
mkdir -p "$OUT"
# the checks are programs of the platform. They link MMgr's line, the platform and the carcer, which the cc of the
# checks compiles once. The libraries of the system come after them. The list goes last in each command, as the
# libraries come after every source
CHAIN_LINEA=()
for source in "${FILE_DEFS_LINEA[@]}" "${EXACT_TYPES_PLATFORM[@]}" "${EXACT_TYPES_CARCER[@]}"; do
    object="$OUT/$(basename "$source" .c)_chain.o"
    cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 "${FILE_DEFS_MMGR_FLAGS[@]}" -c "$source" -o "$object" || exit 1
    CHAIN_LINEA+=("$object")
done
CHAIN_LINEA+=("${EXACT_TYPES_LIBRARIES[@]}")

for one in "$TOP/includes/file_defs/kdm/kdm_write.c" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/chain_check.c"; do
    name="$(basename "$one" .c)"
    cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/$name" "$one" \
        "$TOP/src/cu/transpiler/lstar/protocol/gate/chain_build.c" "${FILE_DEFS_FORMATS[@]}" "${FILE_DEFS_MMGR_FLAGS[@]}" "${EXACT_TYPES_PLATFORM_PROGRAM[@]}" || exit 1
done

# the descent is orior's own, and orior reads exact integers
SIFT="$TOP/src/cu/engine/nbody/orior"
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -I"$SIFT" "${EXACT_TYPES_FLAGS[@]}" -o "$OUT/gate_descent" \
    "$TOP/utils/test/src/cu/transpiler/lstar/protocol/gate_descent.c" "$TOP/src/cu/transpiler/lstar/protocol/gate/chain_build.c" \
    "$SIFT/orior_core.c" "$SIFT/orior_field.c" "$SIFT/orior_steer.c" \
    "$SIFT/orior_steer_count.c" "$SIFT/orior_steer_plan.c" "$SIFT/scan.c" \
    "${FILE_DEFS_FORMATS[@]}" "${EXACT_TYPES_ARITHMETIC[@]}" "${EXACT_TYPES_LIBRARIES[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra "${EXACT_TYPES_FLAGS[@]}" -o "$OUT/hadamard_order_check" \
    "$TOP/utils/test/src/cu/transpiler/lstar/protocol/hadamard_order_check.c" \
    "$TOP/src/cu/transpiler/lstar/protocol/order/hadamard_order.c" \
    "${EXACT_TYPES_ARITHMETIC[@]}" "${EXACT_TYPES_LIBRARIES[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/query_put_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/query_put_check.c" \
    "$TOP/src/cu/transpiler/lstar/protocol/query/query_put.c" "${FILE_DEFS_MMGR_FLAGS[@]}" "${CHAIN_LINEA[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/answer_read_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/answer_read_check.c" \
    "$TOP/src/cu/transpiler/lstar/protocol/query/answer_read.c" "${FILE_DEFS_MMGR_FLAGS[@]}" "${CHAIN_LINEA[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/frame_relation_check" "$TOP/utils/test/src/cu/transpiler/lstar/parser/frame_relation_check.c" \
    "${FILE_DEFS_MMGR_FLAGS[@]}" "${CHAIN_LINEA[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/turning_check" "$TOP/utils/test/src/cu/transpiler/lstar/parser/turning_check.c" \
    "$TOP/src/cu/transpiler/lstar/protocol/khw_learn/turning.c" "${FILE_DEFS_MMGR_FLAGS[@]}" "${CHAIN_LINEA[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/standing_check" "$TOP/utils/test/src/cu/transpiler/lstar/parser/standing_check.c" \
    "$TOP/src/cu/transpiler/lstar/protocol/khw_learn/standing.c" "${FILE_DEFS_MMGR_FLAGS[@]}" "${CHAIN_LINEA[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/tables_funnel_check" "$TOP/utils/test/src/cu/transpiler/vendor_bin_layouts/nvidia/tables_funnel_check.c" \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/tables/funnel.c" "$TOP/includes/parsers/line_read.c" "${FILE_DEFS_MMGR_FLAGS[@]}" "${CHAIN_LINEA[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/tables_read_check" "$TOP/utils/test/src/cu/transpiler/vendor_bin_layouts/nvidia/tables_read_check.c" \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/tables/read.c" "$TOP/includes/parsers/line_read.c" "${FILE_DEFS_MMGR_FLAGS[@]}" "${CHAIN_LINEA[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/constant_check" "$TOP/utils/test/src/cu/transpiler/vendor_bin_layouts/nvidia/constant_check.c" \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_safe/constant.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/tables/read.c" "$TOP/includes/parsers/line_read.c" "${FILE_DEFS_MMGR_FLAGS[@]}" "${CHAIN_LINEA[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/naming_check" "$TOP/utils/test/src/cu/transpiler/vendor_bin_layouts/nvidia/naming_check.c" \
    "$TOP/includes/file_defs/khw/khw_machine_write/pieces.c" \
    "$TOP"/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_machine/*.c \
    "$TOP"/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_assemble/*.c \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_write.c" \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/container_write.c" \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/container_pattern.c" \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/container_layout.c" \
    "${FILE_DEFS_FORMATS[@]}" "${FILE_DEFS_MMGR_FLAGS[@]}" "${EXACT_TYPES_PLATFORM_PROGRAM[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/cubin_listing_check" "$TOP/utils/test/src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_listing_check.c" \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_listing/cut.c" "${FILE_DEFS_MMGR_FLAGS[@]}" "${CHAIN_LINEA[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/kqr_check" "$TOP/utils/test/src/cu/types/file_defs/kqr/kqr_check.c" \
    "${FILE_DEFS_FORMATS[@]}" "${FILE_DEFS_MMGR_FLAGS[@]}" "${EXACT_TYPES_PLATFORM_PROGRAM[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/who_check" "$TOP/utils/test/src/cu/types/file_defs/who/who_check.c" \
    "${FILE_DEFS_FORMATS[@]}" "${FILE_DEFS_MMGR_FLAGS[@]}" "${EXACT_TYPES_PLATFORM_PROGRAM[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/qry_rows_check" "$TOP/utils/test/src/cu/types/file_defs/qry/qry_rows_check.c" \
    "${FILE_DEFS_FORMATS[@]}" "${FILE_DEFS_MMGR_FLAGS[@]}" "${EXACT_TYPES_PLATFORM_PROGRAM[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/ksc_check" "$TOP/utils/test/src/cu/types/file_defs/ksc/ksc_check.c" \
    "${FILE_DEFS_FORMATS[@]}" "${FILE_DEFS_MMGR_FLAGS[@]}" "${EXACT_TYPES_PLATFORM_PROGRAM[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/krf_check" "$TOP/utils/test/src/cu/types/file_defs/krf/krf_check.c" \
    "${FILE_DEFS_FORMATS[@]}" "${FILE_DEFS_MMGR_FLAGS[@]}" "${EXACT_TYPES_PLATFORM_PROGRAM[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/frame_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/frame_check.c" \
    "$TOP/includes/file_defs/krs/krs_write/head.c" \
    "$TOP/includes/file_defs/krs/krs_write/fixed.c" \
    "$TOP/includes/file_defs/krs/krs_write/frame_forms.c" \
    "$TOP/includes/file_defs/krs/krs_write/words.c" \
    "$TOP/includes/file_defs/khw/khw_machine_write/opening.c" \
    "${FILE_DEFS_FORMATS[@]}" "${FILE_DEFS_MMGR_FLAGS[@]}" "${EXACT_TYPES_PLATFORM_PROGRAM[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/kdm_check" "$TOP/utils/test/src/cu/types/file_defs/kdm/kdm_check.c" \
    "${FILE_DEFS_FORMATS[@]}" "${FILE_DEFS_MMGR_FLAGS[@]}" "${EXACT_TYPES_PLATFORM_PROGRAM[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/survivors_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/survivors_check.c" \
    "$TOP/src/cu/transpiler/lstar/protocol/gate/survivors.c" "${FILE_DEFS_MMGR_FLAGS[@]}" "${CHAIN_LINEA[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/scheduler_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/scheduler_check.c" \
    "$TOP/src/cu/transpiler/lstar/protocol/order/scheduler.c" "$TOP"/src/cu/transpiler/lstar/protocol/teacher/run_channel/*.c \
    "$TOP"/src/cu/transpiler/lstar/protocol/overseer/*.c "$TOP/src/cu/transpiler/lstar/interface/interface.c" "$TOP"/src/cu/transpiler/lstar/interface/platform/*.c \
    "$TOP/src/cu/transpiler/lstar/interface/interface_names.c" "${FILE_DEFS_FORMATS[@]}" "${FILE_DEFS_MMGR_FLAGS[@]}" "${EXACT_TYPES_PLATFORM_PROGRAM[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/overseer_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/overseer_check.c" \
    "$TOP"/src/cu/transpiler/lstar/protocol/overseer/*.c "$TOP"/src/cu/transpiler/lstar/protocol/teacher/run_channel/*.c \
    "$TOP/src/cu/transpiler/lstar/interface/interface.c" "$TOP"/src/cu/transpiler/lstar/interface/platform/*.c \
    "$TOP/src/cu/transpiler/lstar/interface/interface_names.c" "${FILE_DEFS_FORMATS[@]}" "${FILE_DEFS_MMGR_FLAGS[@]}" "${EXACT_TYPES_PLATFORM_PROGRAM[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/overseer_carrier" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/overseer_carrier.c" \
    "${FILE_DEFS_FORMATS[@]}" "${FILE_DEFS_MMGR_FLAGS[@]}" "${EXACT_TYPES_PLATFORM_PROGRAM[@]}" || exit 1
# the walk runs as its own program, in a child the cell can lose
BOOT="$TOP/src/cu/transpiler/lstar/protocol"
INTERFACE="$TOP/src/cu/transpiler/lstar/interface"
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/query_walk" "$BOOT/query/query_walk.c" "$BOOT/query/query_put.c" \
    "${FILE_DEFS_FORMATS[@]}" "${FILE_DEFS_MMGR_FLAGS[@]}" "${EXACT_TYPES_PLATFORM_PROGRAM[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -I"$TOP/src/cu/engine" -I "$TOP/src/cu/engine" -o "$OUT/query_interface_check" \
    "$TOP/utils/test/src/cu/transpiler/lstar/protocol/query_interface_check.c" "$BOOT/query/query_interface.c" "$INTERFACE/interface.c" "$INTERFACE"/platform/*.c \
    "$INTERFACE/interface_names.c" "${FILE_DEFS_FORMATS[@]}" "${FILE_DEFS_MMGR_FLAGS[@]}" "${EXACT_TYPES_PLATFORM_PROGRAM[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -pthread -I"$TOP/src/cu/engine" -o "$OUT/launch_queue_check" \
    "$TOP/utils/test/src/cu/transpiler/lstar/interface/launch_queue_check.c" "$INTERFACE/interface.c" "$INTERFACE"/platform/*.c \
    "$INTERFACE/interface_names.c" "${FILE_DEFS_MMGR_FLAGS[@]}" "${CHAIN_LINEA[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra "${EXACT_TYPES_FLAGS[@]}" -o "$OUT/query_order_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/query_order_check.c" \
    "$BOOT/order/query_order.c" "$BOOT/query/query_put.c" "$BOOT/order/hadamard_order.c" \
    "${EXACT_TYPES_ARITHMETIC[@]}" "${EXACT_TYPES_LIBRARIES[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/stem_group_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/stem_group_check.c" \
    "$BOOT/order/stem_group.c" "${FILE_DEFS_MMGR_FLAGS[@]}" "${CHAIN_LINEA[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra "${EXACT_TYPES_FLAGS[@]}" -o "$OUT/branch_side_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/branch_side_check.c" \
    "$BOOT/order/query_order.c" "$BOOT/query/query_put.c" "$BOOT/order/hadamard_order.c" \
    "${EXACT_TYPES_ARITHMETIC[@]}" "${EXACT_TYPES_LIBRARIES[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/gnascor_trace" "$TOP/utils/maint/engine/gnascor_trace.c" "$BOOT/query/query_put.c" "${FILE_DEFS_MMGR_FLAGS[@]}" "${CHAIN_LINEA[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/host_entry_check" "$TOP/utils/test/src/cu/transpiler/lstar/protocol/host_entry_check.c" "${FILE_DEFS_MMGR_FLAGS[@]}" "${CHAIN_LINEA[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -o "$OUT/classify_walk" "$BOOT/query/classify_walk.c" "${FILE_DEFS_FORMATS[@]}" "${FILE_DEFS_MMGR_FLAGS[@]}" "${EXACT_TYPES_PLATFORM_PROGRAM[@]}" || exit 1
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -O2 -Wall -Wextra -I"$TOP/src/cu/engine" -I "$TOP/src/cu/engine" -o "$OUT/bus_enum_check" \
    "$TOP/utils/test/src/cu/transpiler/lstar/protocol/bus_enum_check.c" "$BOOT/query/bus_enum.c" "$BOOT/query/bus_enum_walk.c" "$BOOT/query/query_interface.c" \
    "$INTERFACE/interface.c" "$INTERFACE"/platform/*.c "$INTERFACE/interface_names.c" "${FILE_DEFS_FORMATS[@]}" "${FILE_DEFS_MMGR_FLAGS[@]}" "${EXACT_TYPES_PLATFORM_PROGRAM[@]}" || exit 1
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
    source "$TOP/src/cu/transpiler/lstar/protocol/qry_run.sh"
    qry_writer_built "$OUT"
    run_started "$OUT/kdm/qry"
    "$OUT/kdm_write" "$@"
    exit "$?"
fi
"$OUT/chain_check" || exit 1
"$OUT/gate_descent" || exit 1
"$OUT/hadamard_order_check" || exit 1
"$OUT/query_put_check" || exit 1
"$OUT/answer_read_check" || exit 1
"$OUT/frame_relation_check" || exit 1
"$OUT/turning_check" || exit 1
"$OUT/standing_check" || exit 1
"$OUT/tables_funnel_check" || exit 1
"$OUT/tables_read_check" || exit 1
"$OUT/constant_check" || exit 1
"$OUT/naming_check" "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/mnemonic_nvidia.tsv" || exit 1
"$OUT/cubin_listing_check" || exit 1
# R's check, the who's and the rows' in a run of their own, which ends with the subshell. Its writer writes the records
# and the file that holds them
(
    source "$TOP/src/cu/transpiler/lstar/protocol/qry_run.sh"
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
# the file kdm_check passed its run, in its place once that run ended: it names the run's .qry, the newest
newest_run="$(ls -t "$OUT"/kqr_runs/query*.qry | head -1)"
command -v cygpath > /dev/null && newest_run="$(cygpath -m "$newest_run")"
if grep -qF "# $newest_run" "$OUT/kdm_check_put.kdm" 2> /dev/null; then
    echo "  the file the run held is in its place now the run has ended"
else
    echo "  the file the run held is NOT in its place now the run has ended"
    exit 1
fi
# orior_kdecode's select over the tables of that run, read once it ended
bash "$TOP/utils/test/src/cu/types/file_defs/kdecode_check.sh" "$newest_run" || exit 1
"$OUT/survivors_check" || exit 1
# the scheduler's check in a run of its own, ended with the subshell
(
    source "$TOP/src/cu/transpiler/lstar/protocol/qry_run.sh"
    qry_writer_built "$OUT"
    unset QRY
    run_started "$OUT/scheduler_runs"
    SCHEDULER_CARRIER="$OUT/overseer_carrier"
    [ -f "$SCHEDULER_CARRIER.exe" ] && SCHEDULER_CARRIER="$SCHEDULER_CARRIER.exe"
    command -v cygpath > /dev/null && SCHEDULER_CARRIER="$(cygpath -m "$SCHEDULER_CARRIER")"
    "$OUT/scheduler_check" "$OUT/scheduler_check.dry" "$SCHEDULER_CARRIER"
) || exit 1
# the overseer's check in a run of its own, whose writer takes the round the teacher passes critical, ended with the
# subshell
(
    OVERSEER_CARRIER="$OUT/overseer_carrier"
    [ -f "$OVERSEER_CARRIER.exe" ] && OVERSEER_CARRIER="$OVERSEER_CARRIER.exe"
    command -v cygpath > /dev/null && OVERSEER_CARRIER="$(cygpath -m "$OVERSEER_CARRIER")"
    source "$TOP/src/cu/transpiler/lstar/protocol/qry_run.sh"
    qry_writer_built "$OUT"
    unset QRY
    run_started "$OUT/overseer_runs"
    "$OUT/overseer_check" "$OVERSEER_CARRIER"
) || exit 1
"$OUT/host_entry_check" || exit 1
# the two walks in a run of their own, whose records each walk passes its lines in, ended with the subshell
(
    source "$TOP/src/cu/transpiler/lstar/protocol/qry_run.sh"
    qry_writer_built "$OUT"
    unset QRY
    run_started "$OUT/walk_runs"
    "$OUT/bus_enum_check" "$CLASSIFY" || exit 1
    "$OUT/query_interface_check" "$WALK" || exit 1
) || exit 1
"$OUT/launch_queue_check" || exit 1
"$OUT/query_order_check" || exit 1
"$OUT/stem_group_check" || exit 1
"$OUT/branch_side_check" || exit 1
"$OUT/gnascor_trace" "$TOP/utils/maint/engine/gnascor_scenario.txt" "$OUT/gnascor_trace.txt" || exit 1
python "$TOP/utils/maint/engine/gnascor_read.py" --check || exit 1
python "$TOP/utils/maint/engine/gnascor_read.py" --scenario "$TOP/utils/maint/engine/gnascor_scenario.txt" "$OUT/gnascor_trace.txt"
