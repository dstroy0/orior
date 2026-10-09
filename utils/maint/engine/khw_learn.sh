#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# The split learn loop: a form's fields reach the part only after the vendor's disassembler has read the questions the
# field pass would ask, and a round widens in three steps to keep the part from seeing an encoding the vendor calls
# illegal.
#
#     utils/maint/engine/khw_learn.sh discover
#     utils/maint/engine/khw_learn.sh cross <slot> [<base.cu>]
#     utils/maint/engine/khw_learn.sh fields
#
# `discover` reaches the part: it widens the working set (build/engine/khw/forms_work.txt) a round and keeps what it
# finds with its fields unasked, carrying only the turns of forms the part has already answered for, which the vendor
# has already read. `cross` reaches nothing: it emits the turns the field pass would ask (khw_write.sh enumerate) and
# reads each against the vendor's disassembler (measuring_stick_query.sh), writing the illegal ones to the held file.
# `fields` reaches the part: it asks the fields of what a discover left, the carrier holding the vendor's illegal ones
# off the part. The working set and the held file carry the loop; a part pass runs only after a cross has read its
# questions. Run `discover`, then `cross`, then `fields`, round on round until a discover finds nothing.
set -u

STEP="${1:-}"
TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
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
        bash "$TOP/utils/maint/engine/khw_write.sh" enumerate "$SLOT" || exit 1
        bash "$TOP/src/cu/scaffolding/measuring_stick_query.sh" "$WORK/questions.txt" ${BASE:+"$BASE"}
        ;;
    fields)
        build_all
        carrier_words
        "$BINARY" sm_86 "$MACHINE" "$LAYOUT" "$MNEMONICS" "$WORK" "$WRITER" --fields -- "${CARRY[@]}"
        ;;
    *)
        echo "  khw_learn.sh discover | cross <slot> [<base.cu>] | fields"
        exit 2
        ;;
esac
