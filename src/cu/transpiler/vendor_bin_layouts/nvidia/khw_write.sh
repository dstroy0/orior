#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds and runs khw_write (src/cu/transpiler/lstar/parser/khw_write.c): the part's forms learned by asking the
# part, and sm_86.khw written from the answers. Every question is carried by vendor_bin_layouts/nvidia/cubin_run and
# held to cubin_safe on the host before the driver sees it. Nothing of a vendor's toolchain is run and no listing is
# read: the kernel is the code the container the system accepted already holds, the cases are the ladder's, and the
# host computes each case beside the part.
#
#     src/cu/transpiler/vendor_bin_layouts/nvidia/khw_write.sh [dry|enumerate <slot>] [<rounds>]
#
# With no argument the forms found are widened for one round. The file it writes is the file the gate reads each
# question against, and a run rewrites it as the part answers.
#
# Every mode is one query run, query<n>-<datetime>.qry in build/engine/khw/qry (qry_run.sh): every question, its cases
# and answers, the lists, the forms and what the vendor's writer and the carrier print are blobs of it, and no file of
# theirs is written beside it. Each run carries in the forms the run before it left, forms.txt and forms_work.txt, and
# holds every file it reads as it read it and those it changes as it left them. The live run is handed held.txt, the
# vendor's feedback, carried from the latest run there that holds one (measuring_stick_query.sh): the carrier holds
# every encoding it names off the part.
#
# With `enumerate <slot>` nothing reaches the part either, and no carrier runs: the slot's questions are emitted for
# the vendor's disassembler to read before a deeper run carries them. Each form a prior run learned (the run's
# forms_work.txt, else its forms.txt) has its fields turned and its runs set to the two registers, and the kernel's own
# slot form alone where there is none. The run's questions.txt lists them, for measuring_stick_query.sh beside this file to
# hold the ones the vendor calls illegal before the next run.
#
# With `dry` nothing reaches the part, in two steps. The protocol runs on the run channel's dry carrier, which carries
# nothing and answers nothing, and hands the run every question it puts with questions.txt listing them
# (run_channel.h). That list is then handed to the carrier with the part taken out (cubin_run --dry), which writes each
# question, holds it to cubin_safe and reads it back into the run's dry.txt. The machine file is written in
# build/engine/khw and not in the tree.
set -u

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../.." && pwd)"
OUT="$TOP/build/engine"
WORK="$OUT/khw"
mkdir -p "$WORK"
PARSER="$TOP/src/cu/transpiler/lstar/parser"
PROTOCOL="$TOP/src/cu/transpiler/lstar/protocol"
INTERFACE="$TOP/src/cu/transpiler/lstar/interface"
COHERENCE="$TOP/src/cu/transpiler/lstar/protocol/table"
LAYOUTS="$TOP/src/cu/transpiler/vendor_bin_layouts"
RUNS="$WORK/qry"
source "$TOP/src/cu/types/file_defs/qry/qry_run.sh"
qry_writer_built "$OUT"
MODE=live
MACHINE="$COHERENCE/sm_86.khw"
if [ "${1:-}" = "dry" ]; then
    MODE=dry
    MACHINE="$WORK/sm_86.khw"
    shift
elif [ "${1:-}" = "enumerate" ]; then
    MODE=enumerate
    MACHINE="$WORK/sm_86.khw"
    SLOT="${2:-}"
    [ -n "$SLOT" ] || { echo "  enumerate needs a slot: khw_write.sh enumerate <slot>"; exit 1; }
fi
ROUNDS="${1:-1}"

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

# the vendor's writer, a program the protocol runs through the interface to name the answers and lay out the .khw
WRITER="$OUT/khw_machine_write"
cc -std=c11 -O2 -Wall -o "$WRITER" "$LAYOUTS/nvidia/khw_machine_write.c" \
    "$LAYOUTS/nvidia/cubin_write.c" "$LAYOUTS/container_write.c" "$LAYOUTS/container_pattern.c" \
    "$LAYOUTS/container_layout.c" "$LAYOUTS/nvidia/sass_assemble.c" "$LAYOUTS/nvidia/sass_machine.c" \
    "$QRY_SOURCE/qry_buffer.c" || { echo "  build failed: khw_machine_write did not compile"; exit 1; }

# the questions of the run's latest questions.txt, counted
questions_counted()
{
    "$QRY_WRITER" latest questions.txt 2> /dev/null | grep -c .
}

# the run, holding what it runs against: the forms the run before it left, carried in (the files beside the machine
# file where no run holds them), and every file it reads as it read it, and those it changes as it left them
run_started "$RUNS"
run_carried "$RUNS" "$WORK/forms_work.txt" "$WORK/forms.txt" held.txt
run_read "$MACHINE" "${MACHINE%.khw}.ksc" "${MACHINE%.khw}.kqr" "$LAYOUTS/nvidia/elf64_nvidia.tsv" \
    "$LAYOUTS/nvidia/mnemonic_nvidia.tsv"
run_left "$MACHINE" "${MACHINE%.khw}.ksc" "${MACHINE%.khw}.kqr"
if [ "$MODE" = "enumerate" ]; then
    # the slot's questions emitted off the part for the vendor's disassembler, seeded from the forms a prior run
    # learned where there are any: each known form's fields and register-runs listed, none carried, nothing reached

    # the split's working set where there is one, else the forms a prior learn run laid out: the working set holds the
    # forms a discover found with their fields unasked, the ones whose turns a cross reads before a field pass asks them
    FORMS=forms_work.txt
    [ "$("$QRY_WRITER" latest "$FORMS" 2> /dev/null | grep -c .)" != 0 ] || FORMS=forms.txt
    FORMS_WORDS=()
    seeded="$("$QRY_WRITER" latest "$FORMS" 2> /dev/null | grep -c .)"
    if [ "$seeded" != 0 ]; then
        FORMS_WORDS=(--forms "$FORMS")
        echo "  seeding from the $seeded forms in $FORMS"
    fi
    "$BINARY" sm_86 "$MACHINE" "$LAYOUTS/nvidia/elf64_nvidia.tsv" "$LAYOUTS/nvidia/mnemonic_nvidia.tsv" \
        "$WRITER" --slot "$SLOT" "${FORMS_WORDS[@]}" -- dry || { echo "  the enumerate did not run"; exit 1; }
    [ "$(questions_counted)" != 0 ] || { echo "  the enumerate put no question"; exit 1; }
    echo "  the slot $SLOT's questions: questions.txt of $QRY, $(questions_counted) of them"
    echo "  cross-check them: $LAYOUTS/nvidia/measuring_stick_query.sh <base.cu>"
    exit 0
fi

if [ "$MODE" = "live" ]; then
    # the vendor's feedback, where a cross-check has handed it to a run (measuring_stick_query.sh), carried into this
    # one above: the carrier holds every encoding it names off the part before the driver sees it, however our own gate
    # read it
    HELD_WORDS=()
    if "$QRY_WRITER" latest held.txt > /dev/null 2>&1; then
        HELD_WORDS=(--held held.txt)
        echo "  the vendor's feedback holds $("$QRY_WRITER" latest held.txt | grep -c .) encodings off the part this run"
    fi
    "$BINARY" sm_86 "$MACHINE" "$LAYOUTS/nvidia/elf64_nvidia.tsv" \
        "$LAYOUTS/nvidia/mnemonic_nvidia.tsv" "$WRITER" "$ROUNDS" -- \
        "$CARRIER" "${HELD_WORDS[@]}" "$MACHINE" "$LAYOUTS/nvidia/elf64_nvidia.tsv"
    exit $?
fi

# the protocol, dry: every question it puts handed to the run and listed, none carried
"$BINARY" sm_86 "$MACHINE" "$LAYOUTS/nvidia/elf64_nvidia.tsv" "$LAYOUTS/nvidia/mnemonic_nvidia.tsv" \
    "$WRITER" "$ROUNDS" -- dry
[ "$(questions_counted)" != 0 ] || { echo "  the protocol put no question"; exit 1; }
echo "  the protocol put $(questions_counted) questions; each held to cubin_safe with the part taken out:"
# what it put, through the gate with the part taken out
"$CARRIER" --dry "$MACHINE" "$LAYOUTS/nvidia/elf64_nvidia.tsv" --list questions.txt ||
    { echo "  the carrier did not read the list"; exit 1; }
"$QRY_WRITER" read dry.txt | grep -o 'the gate: [a-z ,]*' | sort | uniq -c
