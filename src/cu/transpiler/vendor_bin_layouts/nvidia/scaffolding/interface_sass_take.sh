#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Takes every instruction of a listing whose form the machine file lacks into it with its fields
# (interface_sass_probe_take.c). nvdisasm turns each taken form's bits over, the probe's own reading.
#
#     src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/interface_sass_take.sh <listing> [machine file] [architecture]
#     src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/interface_sass_take.sh --fields [machine file] [architecture]
#
# The second form gives every form the machine file holds its fields again and writes the file. It is one query run,
# query<n>-<datetime>.qry in build/interface_sass_take/qry (qry_run.sh), whose writer writes the machine file in its
# place, which therefore stands in the tree.
set -u

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../../.." && pwd)"
LISTING="${1:?a listing, as cuobjdump -sass or nvdisasm prints it, or --fields}"
MACHINE="${2:-$TOP/src/cu/transpiler/lstar/protocol/table/sm_86.khw}"
ARCH="${3:-SM86}"
TEST="$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding"
INTERFACE="$TOP/src/cu/transpiler/lstar/interface"
OUT="$TOP/build/interface_sass_take"
mkdir -p "$OUT/decode"
source "$TOP/src/cu/transpiler/lstar/protocol/qry_run.sh"
qry_writer_built "$OUT"
type -P nvdisasm > /dev/null || { echo "  no nvdisasm on the PATH: the CUDA toolkit's disassembler names the fields"; exit 1; }

INCLUDES=(-I "$TOP/src/cu/engine" -I "$INTERFACE" -I "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding"
          -I "$TOP/src/cu/includes/parsers" -I "$TEST")
OBJECTS=()
for source in "$INTERFACE/interface.c" "$INTERFACE"/platform/*.c "$INTERFACE/interface_names.c" "$TOP"/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_machine/*.c "${FILE_DEFS[@]}" \
              "$TOP"/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_assemble/*.c "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/interface_sass_probe_class.c" \
              "$TOP"/src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/interface_sass_probe_machine/*.c "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/interface_sass_probe_read.c" \
              "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/interface_sass_probe_take.c"; do
    object="$OUT/$(basename "$source" .c).o"
    cc -std=c11 -O2 -w "${INCLUDES[@]}" -c "$source" -o "$object" || exit 1
    OBJECTS+=("$object")
done
cc -o "$OUT/interface_sass_probe_take" "${OBJECTS[@]}" || exit 1
run_started "$OUT/qry"
if [ "$LISTING" = "--fields" ]; then
    "$OUT/interface_sass_probe_take" --fields "$MACHINE" "$ARCH" "$OUT/decode"
else
    "$OUT/interface_sass_probe_take" "$LISTING" "$MACHINE" "$ARCH" "$OUT/decode"
fi
