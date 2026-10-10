#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds and runs krs_write (src/cu/transpiler/lstar/parser/krs_write.c): sass.krs and sm_86.krs written beside
# sm_86.khw from the forms the part answered for. Nothing reaches the part and nothing of a vendor's toolchain is run:
# the vendor's writer reads the machine file, and the protocol reads which word each form writes off the word web.
# The parameters are named as cu.krs names the same forms.
#
#     src/cu/transpiler/vendor_bin_layouts/nvidia/krs_write.sh
set -u

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../.." && pwd)"
OUT="$TOP/build/engine"
WORK="$OUT/krs"
mkdir -p "$WORK"
PARSER="$TOP/src/cu/transpiler/lstar/parser"
INTERFACE="$TOP/src/cu/transpiler/lstar/interface"
COHERENCE="$TOP/src/cu/transpiler/lstar/protocol/table"
LAYOUTS="$TOP/src/cu/transpiler/vendor_bin_layouts"

BINARY="$OUT/krs_write"
cc -std=c11 -O2 -Wall -Wextra -I "$TOP/src/cu/engine" -o "$BINARY" "$PARSER/krs_write.c" \
    "$INTERFACE/interface.c" "$INTERFACE/interface_names.c" ||
    { echo "  build failed: krs_write did not compile"; exit 1; }

# the vendor's writer, a program the protocol runs through the interface to read the machine file it laid out
WRITER="$OUT/khw_machine_write"
cc -std=c11 -O2 -Wall -o "$WRITER" "$LAYOUTS/nvidia/khw_machine_write.c" \
    "$LAYOUTS/nvidia/cubin_write.c" "$LAYOUTS/container_write.c" "$LAYOUTS/container_pattern.c" \
    "$LAYOUTS/container_layout.c" "$LAYOUTS/nvidia/sass_assemble.c" "$LAYOUTS/nvidia/sass_machine.c" ||
    { echo "  build failed: khw_machine_write did not compile"; exit 1; }

"$BINARY" sass sm_86 "$COHERENCE/sm_86.khw" "$LAYOUTS/nvidia/elf64_nvidia.tsv" "$LAYOUTS/nvidia/mnemonic_nvidia.tsv" \
    "$WORK" "$WRITER" "$COHERENCE/cu.krs"
