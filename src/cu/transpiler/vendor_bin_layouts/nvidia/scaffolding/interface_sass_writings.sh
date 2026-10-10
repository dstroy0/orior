#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Searches the part for a writing of each ladder relation and each precept in one instruction
# (interface_sass_writings.c). Every form of the machine file that writes a register from registers, predicates and
# numbers alone is written into cubins of its own, held to cubin_safe, run on the part through interface_sass_run
# over every case at once, and read back against each relation into interface_sass_writings.md. Every arrangement of
# the part's .kdm is then written node by node from the writings found, run and read back into
# interface_sass_chains.md. Last, the cases the descent places for each relation (gate_descent) are put to the part
# over every arrangement the descent ran over, and the part's verdicts held to the descent's in
# interface_sass_descent.md. sass.krs's forms for the word web's words are held to the writings found in
# interface_sass_krs.md.
#
#     src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/interface_sass_writings.sh [<earlier run folder>]
#
# The earlier run, for its pattern cubin and kernel, defaults to the newest under build/ and the harness's build. With
# WRITINGS_SEARCHED set, the search and the chains already in build/writings are kept and only the descent's cases are
# put.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../../../../.." && pwd)"
CUB="$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding"
INT="$TOP/src/cu/transpiler/lstar/interface"
KRS="$TOP/src/cu/types/file_defs/readers"
BOOT="$TOP/src/cu/transpiler/lstar/protocol"
SIFT="$TOP/src/cu/engine/nbody/orior"
EXACT="$TOP/src/cu/types/integers"
OUT="$TOP/build/writings"
CUDA="/c/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.3"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) PYTHON=python ;;
    *) PYTHON=python3 ;;
esac
# the cubins a runner pass runs at most, and the seconds a pass is given before it is cut off: a pass short enough
# that only a cubin that never ends reaches the cap
MOST=100
CAP=60
mkdir -p "$OUT"

RUN="${1:-}"
if [ -z "$RUN" ]; then
    RUN="$(ls -d "$TOP"/build/*/sass/form_0.text "$TOP"/../build/harness/*/sass/form_0.text 2>/dev/null | sort | tail -1)"
    RUN="${RUN%/sass/form_0.text}"
fi
[ -f "$RUN/sass/form_0.cubin" ] && [ -f "$RUN/sass/form_0.text" ] || { echo "  no earlier run with sass/form_0"; exit 1; }

INCLUDES=(-I "$TOP/src/cu/engine" -I "$CUB" -I "$INT" -I "$KRS" -I "$BOOT")
cc -std=c11 -O1 -Wall "${INCLUDES[@]}" -o "$OUT/interface_sass_writings" "$HERE/interface_sass_writings.c" \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_assemble.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_write.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/container_write.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/container_pattern.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/container_layout.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_safe.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_machine.c" || exit 1
# the runner holds every cubin to cubin_safe on the host before the driver is handed it
cc -std=c11 -O1 -Wall -I "$CUDA/include" -o "$OUT/interface_sass_run" "$HERE/interface_sass_run.c" \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_safe.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_write.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/container_write.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/container_pattern.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/container_layout.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_assemble.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_machine.c" \
    -L "$CUDA/lib/x64" -lcuda 2>/dev/null || { echo "  the runner did not link against the CUDA driver"; exit 1; }
# the descent is orior's own, and orior reads exact integers
cc -std=c11 -O2 -Wall -I "$SIFT" -I "$EXACT" -o "$OUT/gate_descent" \
    "$TOP/utils/test/src/cu/transpiler/lstar/protocol/gate_descent.c" "$BOOT/gate/chain_build.c" \
    "$SIFT/orior_core.c" "$SIFT/orior_field.c" "$SIFT/orior_steer.c" "$SIFT/orior_steer_count.c" \
    "$SIFT/orior_steer_plan.c" "$SIFT/scan.c" "$EXACT/exact_integer_add.c" "$EXACT/exact_integer_multiply.c" \
    "$EXACT/exact_integer_limbs.c" || exit 1

MACHINE="$TOP/src/cu/transpiler/lstar/protocol/table/sm_86.khw"
WIN_OUT="$(cygpath -m "$OUT")"
PATTERN="$(cygpath -m "$RUN")/sass/form_0.cubin"
KERNEL_TEXT="$(cygpath -m "$RUN")/sass/form_0.text"

# every cubin of the list in `$1` run over the cases in `$3`, the search's where none is named, its answers into `$2`,
# the runner started again past each refusal
run_list() {
    local list="$1" answers="$2" cases="${3:-$WIN_OUT/cases.txt}"
    local held="${answers%.txt}.held.txt" first=0 guard=0 status last hung crossed
    # the airlock: the vendor reads every cubin of the list before any reaches the part, and the runner is handed that
    # verdict always; without it the runner holds every cubin off the part
    "$PYTHON" "$HERE/interface_sass_held.py" "$list" "$CUDA/bin/nvdisasm" SM86 "$WIN_OUT/cross" "$held"
    crossed=$?
    [ "$crossed" = 2 ] && { echo "  cross: the vendor's reader was not reached; nothing goes to the part"; return 1; }
    while :; do
        timeout "$CAP" "$OUT/interface_sass_run" --held "$held" "$(cygpath -m "$MACHINE")" "$list" "$first" "$answers" \
            "$cases" "$MOST"
        status=$?
        last="$(tail -1 "$answers" 2>/dev/null | cut -d' ' -f1)"
        case "$status" in
            0) return 0 ;;
            3 | 4) first=$(( last + 1 )) ;;
            124) hung=$(( ${last:--1} + 1 )); echo "$hung hung timeout" >> "$answers"; first=$(( hung + 1 )) ;;
            *) echo "  the runner exited $status"; return 1 ;;
        esac
        guard=$(( guard + 1 ))
        [ "$guard" -gt 2000 ] && { echo "  the runner was started 2000 times"; return 1; }
    done
}

if [ -z "${WRITINGS_SEARCHED:-}" ]; then
    rm -f "$OUT"/form_*.cubin "$OUT/answers.txt"
    "$OUT/interface_sass_writings" "$PATTERN" "$KERNEL_TEXT" "$(cygpath -m "$MACHINE")" "$WIN_OUT" || exit 1
    run_list "$WIN_OUT/list.txt" "$WIN_OUT/answers.txt" || exit 1
    "$OUT/interface_sass_writings" read "$WIN_OUT" "$(cygpath -m "$HERE")/interface_sass_writings.md" || exit 1

    # every arrangement of the .kdm written from the writings found, run and read back
    mkdir -p "$OUT/chains"
    rm -f "$OUT"/chains/chain_*.cubin "$OUT/chains/answers.txt"
    "$OUT/interface_sass_writings" chains "$PATTERN" "$KERNEL_TEXT" "$(cygpath -m "$MACHINE")" "$WIN_OUT" \
        "$(cygpath -m "$MACHINE").kdm" || exit 1
    run_list "$WIN_OUT/chains/list.txt" "$WIN_OUT/chains/answers.txt" || exit 1
    "$OUT/interface_sass_writings" chains-read "$WIN_OUT" "$(cygpath -m "$HERE")/interface_sass_chains.md" || exit 1
fi

# the descent's cases for each relation put to every arrangement it ran over
DESCENT="$OUT/descent"
rm -rf "$DESCENT"
mkdir -p "$DESCENT"
"$OUT/gate_descent" "$(cygpath -m "$DESCENT")" > "$DESCENT/gate_descent.out" || exit 1
PAIRS=()
for rows in "$DESCENT"/*.kdm; do
    name="$(basename "$rows" .kdm)"
    into="$DESCENT/$name"
    mkdir -p "$into"
    "$OUT/interface_sass_writings" chains "$PATTERN" "$KERNEL_TEXT" "$(cygpath -m "$MACHINE")" "$WIN_OUT" \
        "$(cygpath -m "$rows")" "$(cygpath -m "$into")" || exit 1
    run_list "$(cygpath -m "$into")/list.txt" "$(cygpath -m "$into")/answers.txt" \
        "$(cygpath -m "$DESCENT")/${name}_cases.txt" || exit 1
    PAIRS+=("$(cygpath -m "$into")" "$(cygpath -m "$DESCENT")/${name}_cases.txt")
done
"$OUT/interface_sass_writings" descent-read "$(cygpath -m "$HERE")/interface_sass_descent.md" "${PAIRS[@]}" || exit 1

# the ruleset's forms for the word web's words held to the writings found; a form that does not hold is reported and
# ends nothing
"$OUT/interface_sass_writings" krs-read "$WIN_OUT" "$(cygpath -m "$TOP")/src/cu/transpiler/lstar/protocol/table/sass.krs" \
    "$(cygpath -m "$HERE")/interface_sass_krs.md"
exit 0
