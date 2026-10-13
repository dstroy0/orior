#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# The split learn loop, with the safety airlock wired in: a part pass is one live run whose every round is checked
# before the part, fixed where a count of registers fixes it, and checked again (run_channel.h). The vendor's reader
# reads the exact code a round would carry before any of it reaches the part, the round the pass makes from the answers
# before it as much as its first. The cross is a crash veto and nothing more: an encoding it calls illegal, or one it
# never read, is held off the part, since an unread encoding can be the very one that faults the part and takes the
# machine with it. A question it holds only for a register past the count its kernel allots is put again with the count
# that clears it, which changes how it is put and not what it asks. Nothing the vendor says enters the learning, the
# protocol's own order of questions, which holds for every language; the internal assembly is relational for that
# reason. Every hold and every count given is a line of the teacher's trace, so that a branch the learner took is
# traced back to the check that put its question.
#
#     src/cu/transpiler/vendor_bin_layouts/nvidia/khw_learn.sh discover [<base.cu>]
#     src/cu/transpiler/vendor_bin_layouts/nvidia/khw_learn.sh seed <listing.sass>
#     src/cu/transpiler/vendor_bin_layouts/nvidia/khw_learn.sh turns [<base.cu>]
#     src/cu/transpiler/vendor_bin_layouts/nvidia/khw_learn.sh registers [<base.cu>]
#     src/cu/transpiler/vendor_bin_layouts/nvidia/khw_learn.sh cross [<base.cu>]
#
# Each part pass is one query run, query<n>-<datetime>.qry in build/engine/khw/qry (qry_run.sh). The part's k-files
# are the tree's, sm_86.khw, sm_86.ksc and sm_86.kqr beside the rulesets: a pass reads each as the run holds it and
# the run's writer writes each in its place as the pass ends, so that the query reads what the part has answered for.
# Each round of it is handed to the airlock as airlock.txt: the carrier with the part taken out pops each question's container, the cross
# reads every slot instruction and frame against the vendor's disassembler (measuring_stick_query.c) and hands held.txt
# and the holds the teacher reads, and the round reaches the carrier only once the cross has read it. Where the vendor's
# reader is not reached, the round is carried by nothing. Where a base source is given, the identical C is put through
# the vendor's compiler once as the pass starts, its listing the run's blob base_nvcc.sass.
#
# `discover` seeds the kernel's own form into the working set the first time, and after that widens the forms the part
# has classified a round, keeping what it finds unasked. `seed` is a discover pass with the working set seeded as well
# from a listing of the vendor's compiler, one instruction of each operation key (sass_listing_seeds), each asked its
# relation in the slot through the airlock: the gate holds a key it does not know, and the conference the hold calls
# admits the encoding where the cross read it clear (overseer.h), the lessons carried from run to run in overseer.txt. `turns` asks the turns of the forms a discover left and finds
# the runs their operands might sit in. `registers` sets those runs to the two registers to sort the operands from the
# modifiers and lays out what the part has answered for. Run discover, turns, registers, round on round until a discover
# seeds and widens nothing. `cross` alone reads the latest listed questions again for inspection, those a dry run of
# khw_write.sh listed, carried into a run of its own from the latest run that holds them, and reaches no part.
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
COHERENCE="$PROTOCOL/table"
MACHINE="$COHERENCE/sm_86.khw"
LAYOUT="$LAYOUTS/nvidia/elf64_nvidia.tsv"
MNEMONICS="$LAYOUTS/nvidia/mnemonic_nvidia.tsv"
FIELDS="$LAYOUTS/nvidia/sass_nvidia.tsv"
RUNS="$WORK/qry"
mkdir -p "$WORK"
source "$TOP/src/cu/types/file_defs/qry/qry_run.sh"

# the run's writer, the carrier, the cross, khw_write and the vendor's writer, built with absolute source paths so the
# carrier finds its layout by __FILE__ from any folder (cubin_write.c), and the vendor's disassembler beside its
# compiler
build_all()
{
    qry_writer_built "$OUT"
    CARRIER="$OUT/cubin_run"
    cc -std=c11 -O2 -Wall -o "$CARRIER" "$LAYOUTS/nvidia/cubin_run.c" "$LAYOUTS"/nvidia/cubin_run/*.c "$LAYOUTS"/nvidia/cubin_safe/*.c \
        "$LAYOUTS/nvidia/cubin_write.c" "$LAYOUTS/container_write.c" "$LAYOUTS/container_pattern.c" \
        "$LAYOUTS/container_layout.c" "$LAYOUTS"/nvidia/sass_assemble/*.c "$LAYOUTS"/nvidia/sass_machine/*.c \
        "${FILE_DEFS[@]}" || { echo "  build failed: cubin_run did not compile"; exit 1; }
    CROSS="$OUT/measuring_stick_query"
    cc -std=c11 -O2 -Wall -Wextra -pthread -I "$TOP/src/cu/engine" -o "$CROSS" \
        "$LAYOUTS/nvidia/measuring_stick_query.c" "$LAYOUTS"/nvidia/measuring_stick_query/*.c \
        "$INTERFACE/interface.c" "$INTERFACE"/platform/*.c "$INTERFACE/interface_names.c" \
        "${FILE_DEFS[@]}" || { echo "  build failed: the cross did not compile"; exit 1; }
    NVCC="$(command -v nvcc)"
    NVDISASM="$(dirname "$NVCC")/nvdisasm"
    [ -n "$NVCC" ] && { [ -x "$NVDISASM" ] || [ -x "$NVDISASM.exe" ]; } ||
        { echo "  no nvdisasm beside nvcc: the airlock has no reader, and nothing goes to the part"; exit 2; }
    BINARY="$OUT/khw_write"
    cc -std=c11 -O2 -Wall -Wextra -I "$TOP/src/cu/engine" -o "$BINARY" "$PARSER/khw_write.c" "$PARSER"/khw_write/*.c \
        "$PROTOCOL"/teacher/run_channel/*.c "$PROTOCOL"/overseer/*.c "$PROTOCOL/query/answer_read.c" \
        "$PROTOCOL/gate/survivors.c" "$PROTOCOL/order/scheduler.c" "$LAYOUTS/container_layout.c" \
        "$INTERFACE/interface.c" "$INTERFACE"/platform/*.c "$INTERFACE/interface_names.c" "${FILE_DEFS[@]}" ||
        { echo "  build failed: khw_write did not compile"; exit 1; }
    WRITER="$OUT/khw_machine_write"
    cc -std=c11 -O2 -Wall -o "$WRITER" "$LAYOUTS/nvidia/khw_machine_write.c" "$LAYOUTS"/nvidia/khw_machine_write/*.c \
        "$LAYOUTS/nvidia/cubin_write.c" "$LAYOUTS/container_write.c" "$LAYOUTS/container_pattern.c" \
        "$LAYOUTS/container_layout.c" "$LAYOUTS"/nvidia/sass_assemble/*.c "$LAYOUTS"/nvidia/sass_machine/*.c \
        "${FILE_DEFS[@]}" || { echo "  build failed: khw_machine_write did not compile"; exit 1; }
}

# the identical C of the base source through the vendor's compiler, where one is given, its listing handed to the run
# as base_nvcc.sass
base_compiled()
{
    [ -n "$BASE" ] && [ -f "$BASE" ] || return 0
    local cubin="$WORK/base_nvcc.cubin"
    if "$NVCC" -cubin -arch=sm_86 -O3 -o "$cubin" "$BASE" 2>&1 | "$QRY_WRITER" hand nvcc.txt &&
        [ "${PIPESTATUS[0]}" -eq 0 ] && "$(dirname "$NVCC")/cuobjdump" -sass "$cubin" 2> /dev/null |
        "$QRY_WRITER" hand base_nvcc.sass && [ "${PIPESTATUS[0]}" -eq 0 ]; then
        echo "  the vendor's compiler read $(basename "$BASE"); its listing is the blob base_nvcc.sass"
    else
        echo "  the vendor's compiler did not read $(basename "$BASE")"
    fi
    rm -f "$cubin"
}

# the carrier's words for a live pass: held.txt always, which the carrier will not reach the part without, the lessons
# the conferences taught, whose admissions the gate puts past its key rule, and the airlock every round passes before
# the part, the pop and the cross over the round's list
carrier_words()
{
    CARRY=("$CARRIER" --held held.txt --lessons overseer.txt "$MACHINE" "$LAYOUT"
        --pop "$CARRIER" --dry --full --lessons overseer.txt "$MACHINE" "$LAYOUT"
        --cross "$CROSS" "$NVDISASM" SM86 airlock.md held.txt)
}

# a part pass `$1`, with the words after it khw_write's, one live run, every round of it checked before the part. The
# run holds what it ran against: the forms the run before it left and the lessons its conferences taught, carried in
# (the files beside the machine file where no run holds them), and every file it reads as it read it, and those it
# changes as it left them; where a listing is to seed the working set, its seeds the run's blob seeds.txt; and what
# khw_write prints, its blob khw_write.console (qry_writer tee). The pass exits as khw_write did
pass()
{
    local step="$1"
    shift
    build_all
    run_started "$RUNS"
    run_carried "$RUNS" "$WORK/forms_work.txt" "$WORK/forms.txt" faulted.txt overseer.txt
    run_read "$MACHINE" "${MACHINE%.khw}.ksc" "${MACHINE%.khw}.kqr" "$LAYOUT" "$MNEMONICS" "$FIELDS" ${BASE:+"$BASE"}
    if [ -n "${SEED_LISTING:-}" ]; then
        SEEDS="$OUT/sass_listing_seeds"
        cc -std=c11 -O2 -Wall -Wextra -o "$SEEDS" "$LAYOUTS/nvidia/sass_listing_seeds.c" ||
            { echo "  build failed: sass_listing_seeds did not compile"; exit 1; }
        "$SEEDS" "$SEED_LISTING" | "$QRY_WRITER" hand seeds.txt || { echo "  the seeds were not handed"; exit 1; }
    fi
    base_compiled
    carrier_words
    "$BINARY" sm_86 "$MACHINE" "$LAYOUT" "$MNEMONICS" "$FIELDS" "$WRITER" "--$step" "$@" -- "${CARRY[@]}" 2>&1 |
        "$QRY_WRITER" tee khw_write
    return "${PIPESTATUS[0]}"
}

case "$STEP" in
    discover | turns | registers)
        pass "$STEP"
        ;;
    seed)
        # a discover pass seeded from the listing given, the vendor's reader run beside this script
        [ -n "$BASE" ] && [ -f "$BASE" ] || { echo "  khw_learn.sh seed <listing.sass>"; exit 2; }
        SEED_LISTING="$BASE"
        BASE=""
        pass discover --seeds seeds.txt
        ;;
    cross)
        # a run of the cross's own, the latest listed questions carried into it (measuring_stick_query.sh)
        bash "$LAYOUTS/nvidia/measuring_stick_query.sh" ${BASE:+"$BASE"}
        [ "$?" != 2 ] || { echo "  cross: the vendor's reader was not reached"; exit 2; }
        ;;
    *)
        echo "  khw_learn.sh discover | turns | registers | cross  [<base.cu>]"
        exit 2
        ;;
esac
