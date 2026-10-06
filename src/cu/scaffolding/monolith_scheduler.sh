#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Writes monolith_scheduler.md: the scheduler's bits NVIDIA's compiler writes over every CUDA source of the tree,
# gathered by operation (monolith_scheduler.c). Each source is built by NVIDIA's compiler to a cubin for one part, and
# a source it does not build is passed over and counted. Nothing is run on the part.
#
#     src/cu/scaffolding/monolith_scheduler.sh [arch]
set -u

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
HERE="$TOP/src/cu/scaffolding"
CUBIN="$TOP/src/cu/types/file_defs/readers"
KRS_C="$TOP/src/cu/types/file_defs/readers"
INT="$TOP/src/cu/transpiler/lstar/interface"
OUT="$TOP/build/monolith/scheduler"
mkdir -p "$OUT/cubins"

ARCH="${1:-}"
if [ -z "$ARCH" ]; then
    CAP="$(nvidia-smi --query-gpu=compute_cap --format=csv,noheader 2>/dev/null | head -1 | tr -d ' .')"
    ARCH="sm_${CAP:-86}"
fi
MACHINE="$TOP/src/cu/transpiler/lstar/coherence/$ARCH"
[ -f "$MACHINE" ] || { echo "  no machine file for $ARCH"; exit 1; }

HOST_FLAGS=()
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        MSVC_BIN="$(ls -d "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC"/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
        if [ -z "$MSVC_BIN" ]; then
            MSVC_BIN="$(ls -d "/c/Program Files/Microsoft Visual Studio"/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64 2>/dev/null | tail -1)"
        fi
        if [ -z "$MSVC_BIN" ]; then
            echo "  no host compiler nvcc accepts on this platform was found."
            exit 1
        fi
        HOST_FLAGS=(-ccbin "$MSVC_BIN" -Xcompiler /Zc:preprocessor)
        ;;
esac

# every source built to a cubin, each named for its path; a cubin already newer than its source is kept
built=0
passed=0
CUBINS=()
while IFS= read -r source; do
    name="$(printf '%s' "${source#"$TOP"/}" | tr '/' '_')"
    cubin="$OUT/cubins/${name%.cu}.cubin"
    if [ ! -f "$cubin" ] || [ "$source" -nt "$cubin" ]; then
        if ! nvcc "${HOST_FLAGS[@]}" -cubin -arch="$ARCH" -O3 -I "$TOP/src/cu/engine" -I "$TOP/src/cu/engine" \
            -I "$(dirname "$source")" -o "$cubin" "$source" > /dev/null 2>&1; then
            rm -f "$cubin"
            passed=$(( passed + 1 ))
            continue
        fi
    fi
    built=$(( built + 1 ))
    CUBINS+=("$cubin")
done < <(find "$TOP/src/cu" -name '*.cu' | sort)

cc -std=c11 -O1 -Wall -I "$TOP/src/cu/engine" -I "$TOP/src/cu/engine" -I "$CUBIN" -I "$KRS_C" -I "$INT" \
    -o "$OUT/monolith_scheduler" "$TOP/src/cu/scaffolding/monolith_scheduler.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_assemble.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_machine.c" \
    "$INT/interface.c" "$INT/interface_names.c" || exit 1

echo "  sources built $built, passed over $passed"
"$OUT/monolith_scheduler" "$MACHINE" "$TOP/src/cu/scaffolding/monolith_scheduler.md" "${CUBINS[@]}"
