#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# The measuring stick over the query's own questions: before a series of questions reaches the part, each is read
# against the vendor's own disassembler, and where a base source is given, the identical C is compiled through the
# vendor's compiler and both outputs examined. The carrier pops each question's container out under
# --diff-output-against-vendor (cubin_run.c); nvdisasm reads the slot instruction of each, all of them in one raw run;
# measuring_stick_query.c holds any the vendor calls illegal, reads as a control transfer our gate's operation key
# could not, reads as naming a register at or past the count the question's kernel allots, or reads as writing a
# register the code after the slot addresses memory through, or as a memory access of its own. Our gate judges by
# operation key and by the fields a form has learned, and cannot know a whole encoding illegal while the operation it
# probes is still unlearned, nor where a register sits in a form whose fields are not yet asked: the part is never
# handed an encoding only the vendor can reject. Nothing here runs on the part.
#
#     src/cu/transpiler/vendor_bin_layouts/nvidia/measuring_stick_query.sh [<base.cu>]
#
# The questions are the blob questions.txt of the query run QRY names (qry_buffer.h), from a dry run of the protocol
# (run_channel.h), a line a question, `<code> <registers> <cases> <answers> <slot> [<launches>]`, each name a blob of
# the run beside it. Where no writer runs for QRY, this is a run of its own in build/engine/khw/qry (qry_run.sh), and
# the latest run there that holds a questions.txt has it carried into this one first, with the code and the cases of
# every question it lists. Everything the cross makes
# is a blob of the run: the containers, what the carrier and the vendor's tools printed, the report
# measuring_stick_query.md, and held.txt, the feedback a live carry reads (cubin_run --held held.txt). Exit 0 where
# the vendor holds none, 1 where it holds one or more, 2 where a tool or a blob was not reached.
set -u
TEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$TEST/../../../../.." && pwd)"
CUDA="/c/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.3"
LAYOUTS="$TOP/src/cu/transpiler/vendor_bin_layouts"
INTERFACE="$TOP/src/cu/transpiler/lstar/interface"
MACHINE="$TOP/src/cu/transpiler/lstar/protocol/table/sm_86.khw"
LAYOUT="$LAYOUTS/nvidia/elf64_nvidia.tsv"
OUT="$TOP/build/measuring_stick_query"
SCRATCH="$OUT/scratch"
RUNS="$TOP/build/engine/khw/qry"
mkdir -p "$SCRATCH"
source "$TOP/src/cu/types/file_defs/qry/qry_run.sh"
qry_writer_built "$OUT"

BASE="${1:-}"

# the carrier, built with absolute source paths so it finds its layout by __FILE__ from any folder (cubin_write.c),
# and the cross
CARRIER="$OUT/cubin_run"
cc -std=c11 -O2 -Wall -o "$CARRIER" "$LAYOUTS/nvidia/cubin_run.c" "$LAYOUTS/nvidia/cubin_safe.c" \
    "$LAYOUTS/nvidia/cubin_write.c" "$LAYOUTS/container_write.c" "$LAYOUTS/container_pattern.c" \
    "$LAYOUTS/container_layout.c" "$LAYOUTS/nvidia/sass_assemble.c" "$LAYOUTS/nvidia/sass_machine.c" \
    "$QRY_SOURCE/qry_buffer.c" || { echo "  the carrier did not compile"; exit 2; }
CROSS="$OUT/measuring_stick_query"
cc -std=c11 -O2 -Wall -Wextra -pthread -I "$TOP/src/cu/engine" -o "$CROSS" "$TEST/measuring_stick_query.c" \
    "$INTERFACE/interface.c" "$INTERFACE/interface_names.c" "$QRY_SOURCE/qry_buffer.c" ||
    { echo "  the cross did not compile"; exit 2; }

# the run QRY names where its writer runs, or a run of the cross's own with the latest questions carried into it: the
# list, and each question's code and cases, the first and third names of its line
run_started "$RUNS"
if [ "$QRY_JOINED" = 0 ]; then
    LATEST="$(run_latest "$RUNS" questions.txt)" || { echo "  no run in $RUNS holds a questions.txt to read"; exit 2; }
    { echo questions.txt; QRY="$LATEST" "$QRY_WRITER" latest questions.txt | awk '{ print $1; print $3 }'; } |
        "$QRY_WRITER" carry "$LATEST" - || { echo "  the questions of $LATEST were not carried"; exit 2; }
    # the state every run of the folder carries on: the forms the learn loop keeps and the vendor's last feedback
    run_carried "$RUNS" "$TOP/build/engine/khw/forms_work.txt" "$TOP/build/engine/khw/forms.txt" held.txt
fi
"$QRY_WRITER" latest questions.txt > /dev/null || { echo "  the run holds no questions.txt"; exit 2; }
run_read "$MACHINE" "$LAYOUT" ${BASE:+"$BASE"}

# the carrier pops every question's container out under --diff-output-against-vendor, the part taken out
"$CARRIER" --dry --full "$MACHINE" "$LAYOUT" --list questions.txt 2>&1 | "$QRY_WRITER" hand cubin_run.txt
[ "${PIPESTATUS[0]}" -eq 0 ] || { echo "  the carrier did not read the list"; exit 2; }

# where a base source is given, the identical C through the vendor's compiler, both outputs examined
if [ -n "$BASE" ] && [ -f "$BASE" ]; then
    if "$CUDA/bin/nvcc" -cubin -arch=sm_86 -O3 -o "$SCRATCH/base_nvcc.cubin" "$BASE" 2>&1 | "$QRY_WRITER" hand nvcc.txt &&
        [ "${PIPESTATUS[0]}" -eq 0 ] && "$QRY_WRITER" hand base_nvcc.cubin < "$SCRATCH/base_nvcc.cubin" &&
        "$CUDA/bin/cuobjdump" -sass "$SCRATCH/base_nvcc.cubin" 2> /dev/null | "$QRY_WRITER" hand base_nvcc.sass &&
        [ "${PIPESTATUS[0]}" -eq 0 ]; then
        echo "  the vendor's compiler read $(basename "$BASE"); its listing is the blob base_nvcc.sass"
    else
        echo "  the vendor's compiler did not read $(basename "$BASE")"
    fi
    rm -f "$SCRATCH/base_nvcc.cubin"
fi

# the slot instruction of every popped container, as the part would be handed it, read by the vendor's disassembler
# together as one raw run, split only where the vendor stops at an instruction it calls illegal
# (measuring_stick_query.c)
"$CROSS" questions.txt "$CUDA/bin/nvdisasm" SM86 "$SCRATCH" measuring_stick_query.md held.txt
held=$?
echo "  the report is the blob measuring_stick_query.md of $QRY; the vendor's feedback is its blob held.txt"
echo "  a live run holds those encodings with: cubin_run --held held.txt ..."
exit $held
