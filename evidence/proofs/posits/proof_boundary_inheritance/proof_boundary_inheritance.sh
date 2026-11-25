#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds proof_boundary_inheritance on the ring of examples/0_experimental/exact_navier_stokes and on
# includes/exact_types, into build/proof_boundary_inheritance, then runs it. The compiler is CC where it is set,
# else cc.
#
# Run evidence/proofs/posits/proof_boundary_inheritance/proof_boundary_inheritance.sh with no arguments.
#
# The integers are 8192 bits wide, 256 limbs. The decimal reading multiplies powers of 10^51, and that width
# holds them. A value that does not fit stops the program with a message, and the program prints no wrong value.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../../.." && pwd)"
RING="$TOP/examples/0_experimental/exact_navier_stokes"
source "$TOP/includes/exact_types/exact_types.sh"
# One folder per platform. A Linux shell and a Windows shell can share this tree, and neither runs the
# binary of the other.
OUT="$TOP/build/proof_boundary_inheritance/$(uname -s | tr "[:upper:]" "[:lower:]")"
mkdir -p "$OUT"

"${CC:-cc}" "${FILE_DEFS_LTO_CC[@]}" -std=c11 -O2 -Wall -Wextra -DORIOR_EXACT_LIMBS=256u "${EXACT_TYPES_FLAGS[@]}" \
    -o "$OUT/proof_boundary_inheritance" "$HERE/proof_boundary_inheritance.c" "$RING/torus_fields.c" \
    "${EXACT_TYPES_ARITHMETIC[@]}" "${EXACT_TYPES_LIBRARIES[@]}" || exit 1
"$OUT/proof_boundary_inheritance"
