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
# Each part pass is one query run, query<n>-<datetime>.qry in build/engine/khw/qry (qry_run.sh), and runs three stages
# in it, never skipping the middle one:
#   emit   the pass is run dry (its carrier a single word, `dry`): it hands the run the exact questions it would carry,
#          questions.txt listing them, and the working set it touched is put back as it was; the live carry that
#          follows then enumerates the same questions from the same state;
#   cross  those questions are read against the vendor's disassembler, and the identical C through the vendor's compiler
#          where a base source is given (measuring_stick_query.sh); the illegal ones are the run's blob held.txt. Where
#          the vendor's reader is not reached at all, the pass stops here and nothing reaches the part;
#   carry  the pass is run live, the carrier handed held.txt always (cubin_run --held), which it will not reach the
#          part without: the vendor's verdict is the airlock a live carry passes through.
#
# `discover` seeds the kernel's own form into the working set the first time, and after that widens the forms the part
# has classified a round, keeping what it finds unasked. `turns` asks the turns of the forms a discover left and finds
# the runs their operands might sit in. `registers` sets those runs to the two registers to sort the operands from the
# modifiers and lays out what the part has answered for. Run discover, turns, registers, round on round until a discover
# seeds and widens nothing. `cross` alone reads the latest emitted questions again for inspection, carried into a run of
# its own from the latest run that holds them, and reaches no part.
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
RUNS="$WORK/qry"
# the working set a dry emit changes, put back before the live carry so it enumerates from the same state: the forms,
# blobs of the run carried in from the run before it, and the machine file and the part's .ksc, files the gate and the
# carrier read by path
STATE=(forms_work.txt forms.txt)
FILES=("$MACHINE" "$WORK/sm_86.ksc")
mkdir -p "$WORK"
source "$TOP/src/cu/types/file_defs/qry/qry_run.sh"

# the run's writer, the carrier, khw_write and the vendor's writer, built with absolute source paths so the carrier
# finds its layout by __FILE__ from any folder (cubin_write.c)
build_all()
{
    qry_writer_built "$OUT"
    CARRIER="$OUT/cubin_run"
    cc -std=c11 -O2 -Wall -o "$CARRIER" "$LAYOUTS/nvidia/cubin_run.c" "$LAYOUTS/nvidia/cubin_safe.c" \
        "$LAYOUTS/nvidia/cubin_write.c" "$LAYOUTS/container_write.c" "$LAYOUTS/container_pattern.c" \
        "$LAYOUTS/container_layout.c" "$LAYOUTS/nvidia/sass_assemble.c" "$LAYOUTS/nvidia/sass_machine.c" \
        "$QRY_SOURCE/qry_buffer.c" || { echo "  build failed: cubin_run did not compile"; exit 1; }
    BINARY="$OUT/khw_write"
    cc -std=c11 -O2 -Wall -Wextra -I "$TOP/src/cu/engine" -o "$BINARY" "$PARSER/khw_write.c" \
        "$PROTOCOL/teacher/run_channel.c" "$PROTOCOL/query/answer_read.c" "$PROTOCOL/record_R/record.c" \
        "$PROTOCOL/gate/survivors.c" "$PROTOCOL/order/scheduler.c" \
        "$INTERFACE/interface.c" "$INTERFACE/interface_names.c" "$QRY_SOURCE/qry_buffer.c" ||
        { echo "  build failed: khw_write did not compile"; exit 1; }
    WRITER="$OUT/khw_machine_write"
    cc -std=c11 -O2 -Wall -o "$WRITER" "$LAYOUTS/nvidia/khw_machine_write.c" \
        "$LAYOUTS/nvidia/cubin_write.c" "$LAYOUTS/container_write.c" "$LAYOUTS/container_pattern.c" \
        "$LAYOUTS/container_layout.c" "$LAYOUTS/nvidia/sass_assemble.c" "$LAYOUTS/nvidia/sass_machine.c" \
        "$QRY_SOURCE/qry_buffer.c" || { echo "  build failed: khw_machine_write did not compile"; exit 1; }
}

# the questions of the run's latest questions.txt, counted
questions_counted()
{
    "$QRY_WRITER" latest questions.txt 2> /dev/null | grep -c .
}

# the emit stage: the pass `$1` run dry, which hands the run the questions it would carry and holds none, the working
# set put back as the run read it, each one's first blob in the run: the live carry enumerates the same questions
# from the same state. A form list the run read none of is put back empty, and a file it read none of is taken away
emit()
{
    "$BINARY" sm_86 "$MACHINE" "$LAYOUT" "$MNEMONICS" "$WRITER" "--$1" -- dry
    local one named
    for one in "${STATE[@]}"; do
        "$QRY_WRITER" first "$one" 2> /dev/null | "$QRY_WRITER" hand "$one"
    done
    for one in "${FILES[@]}"; do
        named="$(run_named "$one")"
        if "$QRY_WRITER" first "$named" > /dev/null 2>&1; then
            "$QRY_WRITER" first "$named" > "$one"
        else
            rm -f "$one"
        fi
    done
    echo "  emit $1: $(questions_counted) questions it would carry"
}

# the cross stage: the run's questions read against the vendor's reader, held.txt handed fresh. The pass stops the
# whole loop where the reader is not reached: nothing that was not read reaches the part
cross()
{
    if [ "$(questions_counted)" = 0 ]; then
        echo "  cross: no question to read"
        "$QRY_WRITER" hand held.txt < /dev/null
        return 0
    fi
    bash "$LAYOUTS/nvidia/measuring_stick_query.sh" ${BASE:+"$BASE"}
    local read=$?
    if [ "$read" = 2 ]; then
        echo "  cross: the vendor's reader was not reached; nothing goes to the part"
        exit 2
    fi
    echo "  cross: the vendor holds $("$QRY_WRITER" latest held.txt | grep -c .) encoding(s) off the part"
}

# the carrier's words for a live carry: held.txt always, which the carrier will not reach the part without
carrier_words()
{
    CARRY=("$CARRIER" --held held.txt "$MACHINE" "$LAYOUT")
}

# a part pass `$1`: emit, cross, carry, in that order and never out of it, in one run. The run holds what it ran
# against: the forms the run before it left, carried in (the files beside the machine file where no run holds them),
# and every file it reads as it read it, and those it changes as it left them
pass()
{
    build_all
    run_started "$RUNS"
    run_carried "$RUNS" "$WORK/forms_work.txt" "$WORK/forms.txt" held.txt
    run_read "$MACHINE" "$WORK/sm_86.ksc" "$WORK/sm_86.kqr" "$LAYOUT" "$MNEMONICS"
    run_left "$MACHINE" "$WORK/sm_86.ksc" "$WORK/sm_86.kqr"
    emit "$1"
    cross
    carrier_words
    "$BINARY" sm_86 "$MACHINE" "$LAYOUT" "$MNEMONICS" "$WRITER" "--$1" -- "${CARRY[@]}"
}

case "$STEP" in
    discover | turns | registers)
        pass "$STEP"
        ;;
    cross)
        # a run of the cross's own, the latest emitted questions carried into it (measuring_stick_query.sh)
        bash "$LAYOUTS/nvidia/measuring_stick_query.sh" ${BASE:+"$BASE"}
        [ "$?" != 2 ] || { echo "  cross: the vendor's reader was not reached"; exit 2; }
        ;;
    *)
        echo "  khw_learn.sh discover | turns | registers | cross  [<base.cu>]"
        exit 2
        ;;
esac
