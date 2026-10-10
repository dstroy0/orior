#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# The split learn loop, with the safety airlock wired in: a part pass reaches the part only through emit, cross, carry,
# in that order and in one step. The vendor's reader has read the exact code the pass would carry before any of it
# reaches the part. The cross is a crash veto and nothing more: an encoding it calls illegal, or one it never read, is
# held off the part, since an unread encoding can be the very one that faults the part and takes the machine with it.
# Nothing the vendor says enters the learning, the protocol's own order of questions, which holds for every language;
# the internal assembly is relational for that reason.
#
#     src/cu/transpiler/vendor_bin_layouts/nvidia/khw_learn.sh discover [<base.cu>]
#     src/cu/transpiler/vendor_bin_layouts/nvidia/khw_learn.sh turns [<base.cu>]
#     src/cu/transpiler/vendor_bin_layouts/nvidia/khw_learn.sh registers [<base.cu>]
#     src/cu/transpiler/vendor_bin_layouts/nvidia/khw_learn.sh cross [<base.cu>]
#
# Each part pass runs three stages and never skips the middle one:
#   emit   the pass is run dry (its carrier a single word, `dry`): it writes to build/engine/khw/questions.txt the
#          exact questions it would carry, and the working set it touched is put back as it was; the live carry that
#          follows then enumerates the same questions from the same state;
#   cross  those questions are read against the vendor's disassembler, and the identical C through the vendor's compiler
#          where a base source is given (measuring_stick_query.sh); the illegal ones are written to the held file. Where
#          the vendor's reader is not reached at all, the pass stops here and nothing reaches the part;
#   carry  the pass is run live, the carrier handed the held file always (cubin_run --held), which it will not reach the
#          part without: the vendor's verdict is the airlock a live carry passes through.
#
# `discover` seeds the kernel's own form into the working set the first time, and after that widens the forms the part
# has classified a round, keeping what it finds unasked. `turns` asks the turns of the forms a discover left and finds
# the runs their operands might sit in. `registers` sets those runs to the two registers to sort the operands from the
# modifiers and lays out what the part has answered for. Run discover, turns, registers, round on round until a discover
# seeds and widens nothing. `cross` alone reads the current questions again for inspection and reaches no part.
set -u

STEP="${1:-}"
BASE="${2:-}"
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
# the working-set files a dry emit touches, put back before the live carry so it enumerates from the same state
KEPT=(forms_work.txt forms.txt sm_86.khw sm_86.ksc)
mkdir -p "$WORK"

# the carrier, khw_write and the vendor's writer, built with absolute source paths so the carrier finds its layout by
# __FILE__ from any folder (cubin_write.c)
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

# the emit stage: the pass `$1` run dry, which writes the questions it would carry and holds none, the working set put
# back as it was so the live carry enumerates the same questions from the same state
emit()
{
    rm -f "$WORK/questions.txt" "$WORK"/question*.bin "$WORK"/cases*.txt "$WORK"/answers*.txt
    mkdir -p "$WORK/kept"
    rm -f "$WORK/kept"/*
    for one in "${KEPT[@]}"; do [ -f "$WORK/$one" ] && cp "$WORK/$one" "$WORK/kept/$one"; done
    "$BINARY" sm_86 "$MACHINE" "$LAYOUT" "$MNEMONICS" "$WORK" "$WRITER" "--$1" -- dry
    for one in "${KEPT[@]}"; do
        if [ -f "$WORK/kept/$one" ]; then cp "$WORK/kept/$one" "$WORK/$one"; else rm -f "$WORK/$one"; fi
    done
    echo "  emit $1: $( [ -f "$WORK/questions.txt" ] && wc -l < "$WORK/questions.txt" || echo 0) questions it would carry"
}

# the cross stage: the emitted questions read against the vendor's reader, the held file left fresh. The pass stops the
# whole loop where the reader is not reached: nothing that was not read reaches the part
cross()
{
    [ -s "$WORK/questions.txt" ] || { echo "  cross: no question to read"; : > "$HELD"; return 0; }
    mkdir -p "$(dirname "$HELD")"
    rm -f "$HELD"
    bash "$LAYOUTS/nvidia/measuring_stick_query.sh" "$WORK/questions.txt" ${BASE:+"$BASE"}
    local read=$?
    if [ "$read" = 2 ]; then
        echo "  cross: the vendor's reader was not reached; nothing goes to the part"
        exit 2
    fi
    [ -f "$HELD" ] || : > "$HELD"
    echo "  cross: the vendor holds $(wc -l < "$HELD") encoding(s) off the part"
}

# the carrier's words for a live carry: the held file always, which the carrier will not reach the part without
carrier_words()
{
    CARRY=("$CARRIER" --held "$HELD" "$MACHINE" "$LAYOUT")
}

# a part pass `$1`: emit, cross, carry, in that order and never out of it
pass()
{
    build_all
    emit "$1"
    cross
    carrier_words
    "$BINARY" sm_86 "$MACHINE" "$LAYOUT" "$MNEMONICS" "$WORK" "$WRITER" "--$1" -- "${CARRY[@]}"
}

case "$STEP" in
    discover | turns | registers)
        pass "$STEP"
        ;;
    cross)
        [ -s "$WORK/questions.txt" ] || { echo "  cross: emit a pass first; no questions.txt to read"; exit 1; }
        cross
        ;;
    *)
        echo "  khw_learn.sh discover | turns | registers | cross  [<base.cu>]"
        exit 2
        ;;
esac
