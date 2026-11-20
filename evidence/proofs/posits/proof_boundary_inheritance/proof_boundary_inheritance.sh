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
EXACT="$TOP/includes/exact_types/integers"
# One folder per platform. A Linux shell and a Windows shell can share this tree, and neither runs the
# binary of the other.
OUT="$TOP/build/proof_boundary_inheritance/$(uname -s | tr "[:upper:]" "[:lower:]")"
mkdir -p "$OUT"

"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -DORIOR_EXACT_LIMBS=256u -o "$OUT/proof_boundary_inheritance" \
    "$HERE/proof_boundary_inheritance.c" "$RING/torus_fields.c" "$EXACT/exact_integer_add.c" \
    "$EXACT/exact_integer_decimal.c" "$EXACT/exact_integer_divide.c" "$EXACT/exact_integer_gcd.c" \
    "$EXACT/exact_integer_limbs.c" "$EXACT/exact_integer_multiply.c" || exit 1
"$OUT/proof_boundary_inheritance"
