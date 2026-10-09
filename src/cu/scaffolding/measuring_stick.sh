#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# The measuring stick: every function of the CUDA language in a kernel of its own (measuring_stick.py), compiled by
# nvcc for sm_86 and disassembled, the answer key what the engine builds for each function is held against. nvcc's
# listing is read kernel by kernel, and against sass.krs, into measuring_stick.md (measuring_stick_read.py). nvcc runs
# only where the stick's text has changed since the cubin beside it was compiled. Nothing goes to a device.
#
#     src/cu/scaffolding/measuring_stick.sh
set -u

TEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$TEST/../../.." && pwd)"
CUDA="/c/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.3"
source "$TOP/utils/maint/engine/build_stamp.sh"
OUT="${BUILD_OUT:-$TOP/build/measuring_stick}"
mkdir -p "$OUT"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) PYTHON=python ;;
    *) PYTHON=python3 ;;
esac
WIN_OUT="$(cygpath -m "$OUT")"

"$PYTHON" "$TEST/measuring_stick.py" "$WIN_OUT/measuring_stick.cu.new" "$WIN_OUT/measuring_stick.tsv" || exit 1
if cmp -s "$OUT/measuring_stick.cu.new" "$OUT/measuring_stick.cu" && [ -f "$OUT/measuring_stick_nvcc.cubin" ]; then
    rm -f "$OUT/measuring_stick.cu.new"
    echo "  the stick is unchanged: nvcc's cubin kept"
else
    mv -f "$OUT/measuring_stick.cu.new" "$OUT/measuring_stick.cu"
    rm -f "$OUT/measuring_stick_nvcc.cubin"
    "$CUDA/bin/nvcc" ${CYCLE_HOST_CCBIN:+-ccbin "$CYCLE_HOST_CCBIN"} -cubin -arch=sm_86 -O3 -diag-suppress 177 \
        -o "$WIN_OUT/measuring_stick_nvcc.cubin" "$WIN_OUT/measuring_stick.cu" || { echo "  nvcc did not compile the stick"; exit 1; }
    "$CUDA/bin/cuobjdump" -sass "$OUT/measuring_stick_nvcc.cubin" > "$OUT/measuring_stick_nvcc.sass" || exit 1
fi

"$PYTHON" "$TEST/measuring_stick_read.py" "$WIN_OUT/measuring_stick_nvcc.sass" "$WIN_OUT/measuring_stick.tsv" \
    "$(cygpath -m "$TOP/src/cu/transpiler/lstar/protocol/table")/sass.krs" "$(cygpath -m "$TEST")/measuring_stick.md"
