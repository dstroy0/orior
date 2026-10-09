#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# The split learn loop: a form's turns and its register-runs each reach the part only after the vendor's disassembler
# has read them, and a round widens in five steps to keep the part from seeing an encoding the vendor calls illegal.
#
#     src/cu/transpiler/vendor_bin_layouts/nvidia/khw_learn.sh discover
#     src/cu/transpiler/vendor_bin_layouts/nvidia/khw_learn.sh cross <slot> [<base.cu>]
#     src/cu/transpiler/vendor_bin_layouts/nvidia/khw_learn.sh turns
#     src/cu/transpiler/vendor_bin_layouts/nvidia/khw_learn.sh cross <slot> [<base.cu>]
#     src/cu/transpiler/vendor_bin_layouts/nvidia/khw_learn.sh registers
#
# `discover` reaches the part: the first time it seeds the kernel's own form into the working set (build/engine/khw/
# forms_work.txt), and after that it widens the forms the part has classified a round, keeping what it finds unasked. It
# carries only relations: the kernel's own form, and the flips of classified forms, whose turns the vendor has already
# read. `cross` reaches nothing: it emits the questions the next pass would ask (khw_write.sh enumerate) and reads each
# against the vendor's disassembler (measuring_stick_query.sh), writing the illegal ones to the held file. `turns`
# reaches the part: it asks the turns of the forms a discover left, the carrier holding the vendor's illegal ones off
# the part, and finds the runs their operands might sit in. `registers` reaches the part: it sets those runs to the
# two registers to sort the operands from the modifiers, again behind the held file, and lays out what the part has
# answered for. A cross runs before each part pass that follows it: run discover, cross, turns, cross, registers,
# round on round until a discover seeds and widens nothing.
set -u

STEP="${1:-}"
TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../.." && pwd)"
OUT="$TOP/build/engine"
WORK="$OUT/khw"
LAYOUTS="$TOP/src/cu/transpiler/vendor_bin_layouts"
PARSER="$TOP/src/cu/transpiler/lstar/parser"
PROTOCOL="$TOP/src/cu/transpiler/lstar/protocol"
INTERFACE="$TOP/src/cu/transpiler/lstar/interface"
MACHINE="$WORK/sm_86.khw"
LAYOUT="$LAYOUTS/nvidia/elf64_nvidia.tsv"
MNEMONICS="$LAYOUTS/nvidia/mnemonic_nvidia.tsv"
HELD="$TOP/build/measuring_stick_query/held.txt"
mkdir -p "$WORK"

# the carrier, khw_write and the vendor's writer, built as khw_write.sh builds them
build_all()
{
    CARRIER="$OUT/cubin_run"
    cc -std=c11 -O2 -Wall -o "$CARRIER" "$LAYOUTS/nvidia/cubin_run.c" "$LAYOUTS/nvidia/cubin_safe.c" \
        "$LAYOUTS/nvidia/cubin_write.c" "$LAYOUTS/container_write.c" "$LAYOUTS/container_pattern.c" \
        "$LAYOUTS/container_layout.c" "$LAYOUTS/nvidia/sass_assemble.c" "$LAYOUTS/nvidia/sass_machine.c" ||
        { echo "  build failed: cubin_run did not compile"; exit 1; }
    BINARY="$OUT/khw_write"
    cc -std=c11 -O2 -Wall -Wextra -I "$TOP/src/cu/engine" -o "$BINARY" "$PARSER/khw_write.c" \
        "$PROTOCOL/teacher/run_channel.c" "$PROTOCOL/query/answer_read.c" "$PROTOCOL/record_R/record.c" \
        "$PROTOCOL/gate/survivors.c" "$PROTOCOL/order/scheduler.c" \
        "$INTERFACE/interface.c" "$INTERFACE/interface_names.c" ||
        { echo "  build failed: khw_write did not compile"; exit 1; }
    WRITER="$OUT/khw_machine_write"
    cc -std=c11 -O2 -Wall -o "$WRITER" "$LAYOUTS/nvidia/khw_machine_write.c" \
        "$LAYOUTS/nvidia/cubin_write.c" "$LAYOUTS/container_write.c" "$LAYOUTS/container_pattern.c" \
        "$LAYOUTS/container_layout.c" "$LAYOUTS/nvidia/sass_assemble.c" "$LAYOUTS/nvidia/sass_machine.c" ||
        { echo "  build failed: khw_machine_write did not compile"; exit 1; }
}

# the carrier's words after the split: the held file where there is one, for the carrier to hold the vendor's illegal
# encodings off the part, then the machine and the layout
carrier_words()
{
    CARRY=("$CARRIER")
    [ -s "$HELD" ] && CARRY+=(--held "$HELD") && echo "  the vendor's feedback holds $(wc -l < "$HELD") off the part"
    CARRY+=("$MACHINE" "$LAYOUT")
}

case "$STEP" in
    discover)
        build_all
        carrier_words
        "$BINARY" sm_86 "$MACHINE" "$LAYOUT" "$MNEMONICS" "$WORK" "$WRITER" --discover -- "${CARRY[@]}"
        ;;
    cross)
        SLOT="${2:-}"
        BASE="${3:-}"
        [ -n "$SLOT" ] || { echo "  cross needs a slot: khw_learn.sh cross <slot> [<base.cu>]"; exit 1; }
        bash "$LAYOUTS/nvidia/khw_write.sh" enumerate "$SLOT" || exit 1
        bash "$LAYOUTS/nvidia/measuring_stick_query.sh" "$WORK/questions.txt" ${BASE:+"$BASE"}
        ;;
    turns)
        build_all
        carrier_words
        "$BINARY" sm_86 "$MACHINE" "$LAYOUT" "$MNEMONICS" "$WORK" "$WRITER" --turns -- "${CARRY[@]}"
        ;;
    registers)
        build_all
        carrier_words
        "$BINARY" sm_86 "$MACHINE" "$LAYOUT" "$MNEMONICS" "$WORK" "$WRITER" --registers -- "${CARRY[@]}"
        ;;
    *)
        echo "  khw_learn.sh discover | cross <slot> [<base.cu>] | turns | registers"
        exit 2
        ;;
esac
