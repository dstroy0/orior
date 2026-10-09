#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# The measuring stick over the query's own questions: before a series of questions reaches the part, each is read
# against the vendor's own disassembler, and where a base source is given, the identical C is compiled through the
# vendor's compiler and both outputs examined. The carrier pops each question's container out under
# --diff-output-against-vendor (cubin_run.c); nvdisasm reads each; measuring_stick_query.py holds any the vendor
# calls illegal, or reads as a control transfer our gate's operation key could not. Our gate judges by operation key
# and cannot know a whole encoding illegal while the operation it probes is still unlearned, so the part is never
# handed an encoding only the vendor can reject. Nothing here runs on the part.
#
#     src/cu/transpiler/vendor_bin_layouts/nvidia/measuring_stick_query.sh <question-list> [<base.cu>]
#
# The list comes from a dry run of the protocol (run_channel.h), a line a question,
# `<code> <registers> <cases> <answers> <slot> [<launches>]`. Exit 0 where the vendor holds none, 1 where it holds
# one or more, 2 where a tool or a file was not reached.
set -u
TEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$TEST/../../../../.." && pwd)"
CUDA="/c/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.3"
LAYOUTS="$TOP/src/cu/transpiler/vendor_bin_layouts"
MACHINE="$TOP/src/cu/transpiler/lstar/protocol/table/sm_86.khw"
LAYOUT="$LAYOUTS/nvidia/elf64_nvidia.tsv"
OUT="$TOP/build/measuring_stick_query"
SASS="$OUT/sass"
mkdir -p "$SASS"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) PYTHON=python ;;
    *) PYTHON=python3 ;;
esac

LIST="${1:-}"
BASE="${2:-}"
[ -n "$LIST" ] && [ -f "$LIST" ] || { echo "  the question list was not given or not found"; exit 2; }

# the carrier, built with absolute source paths so it finds its layout by __FILE__ from any folder (cubin_write.c)
CARRIER="$OUT/cubin_run"
cc -std=c11 -O2 -Wall -o "$CARRIER" "$LAYOUTS/nvidia/cubin_run.c" "$LAYOUTS/nvidia/cubin_safe.c" \
    "$LAYOUTS/nvidia/cubin_write.c" "$LAYOUTS/container_write.c" "$LAYOUTS/container_pattern.c" \
    "$LAYOUTS/container_layout.c" "$LAYOUTS/nvidia/sass_assemble.c" "$LAYOUTS/nvidia/sass_machine.c" ||
    { echo "  the carrier did not compile"; exit 2; }

# the carrier pops every question's container out under --diff-output-against-vendor, the part taken out
"$CARRIER" --dry --full "$MACHINE" "$LAYOUT" --list "$LIST" >/dev/null 2>&1 ||
    { echo "  the carrier did not read the list"; exit 2; }

# where a base source is given, the identical C through the vendor's compiler, both outputs examined
if [ -n "$BASE" ] && [ -f "$BASE" ]; then
    "$CUDA/bin/nvcc" -cubin -arch=sm_86 -O3 -o "$OUT/base_nvcc.cubin" "$BASE" >/dev/null 2>&1 &&
        "$CUDA/bin/cuobjdump" -sass "$OUT/base_nvcc.cubin" > "$OUT/base_nvcc.sass" 2>/dev/null &&
        echo "  the vendor's compiler read $(basename "$BASE"); its listing is $OUT/base_nvcc.sass" ||
        echo "  the vendor's compiler did not read $(basename "$BASE")"
fi

# each popped container read by the vendor's disassembler, the answers path of each list line giving the stem
rm -f "$SASS"/*.sass
while read -r code registers cases answers slot launches; do
    [ -n "${answers:-}" ] || continue
    cubin="${answers%.*}.cubin"
    [ -f "$cubin" ] || continue
    name="$(basename "${answers%.*}")"
    # the popped container is a cubin the vendor reads as its own ELF; --binary, for raw instruction bytes, is not used
    "$CUDA/bin/nvdisasm" "$cubin" > "$SASS/$name.sass" 2>&1
done < "$LIST"

"$PYTHON" "$TEST/measuring_stick_query.py" "$LIST" "$SASS" "$OUT/measuring_stick_query.md" "$OUT/held.txt"
held=$?
echo "  the report is $OUT/measuring_stick_query.md; the vendor's feedback is $OUT/held.txt"
echo "  a live run holds those encodings with: cubin_run --held $OUT/held.txt ..."
exit $held
