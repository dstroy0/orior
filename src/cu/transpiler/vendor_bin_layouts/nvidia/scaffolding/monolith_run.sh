#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds the monolith's cubin with NVIDIA's compiler and runs it on the part (monolith_run.c). The cubin is the vendor's
# own, the cross-check's reference side: the transpiler emits the same precepts in its own relational assembly, the sass
# writer turns that into sass and the cubin writer packages it, both the vendor's and ours are disassembled, and the
# query protocol's order is reworked from the two. The first entry answers every precept for each case against the host;
# the second times every costed precept, alone and interleaved.
#
#     src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/monolith_run.sh [arch]
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../../../../.." && pwd)"
CUDA="/c/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.3"
OUT="$TOP/build/monolith/run"
mkdir -p "$OUT"

ARCH="${1:-}"
if [ -z "$ARCH" ]; then
    CAP="$(nvidia-smi --query-gpu=compute_cap --format=csv,noheader 2>/dev/null | head -1 | tr -d ' .')"
    ARCH="sm_${CAP:-86}"
fi

HOST_FLAGS=()
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        MSVC_BIN="$(ls -d "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC"/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
        [ -n "$MSVC_BIN" ] || MSVC_BIN="$(ls -d "/c/Program Files/Microsoft Visual Studio"/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
        [ -n "$MSVC_BIN" ] || { echo "  no host compiler nvcc accepts on this platform was found."; exit 1; }
        HOST_FLAGS=(-ccbin "$MSVC_BIN" -Xcompiler /Zc:preprocessor)
        ;;
esac

# NVIDIA's compiler builds the monolith once; its cubin is the answer key the part is asked to read back
"$CUDA/bin/nvcc" "${HOST_FLAGS[@]}" -cubin -arch="$ARCH" -O3 -o "$OUT/monolith.cubin" \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/monolith.cu" || { echo "  the monolith did not build"; exit 1; }

# the runner is host code that loads the cubin and asks the part for its entries; it links the CUDA driver alone
cc -std=c11 -O2 -Wall -I "$CUDA/include" -o "$OUT/monolith_run" "$HERE/monolith_run.c" \
    -L "$CUDA/lib/x64" -lcuda || { echo "  the runner did not link against the CUDA driver"; exit 1; }

"$OUT/monolith_run" "$(cygpath -m "$OUT/monolith.cubin")"
