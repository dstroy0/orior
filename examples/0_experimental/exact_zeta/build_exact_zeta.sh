#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds one zeta example of this directory on includes/exact_types into build/exact_zeta, then runs it with the
# arguments that follow its name. The compiler is CC where it is set, else cc.
#
# Run examples/0_experimental/exact_zeta/build_exact_zeta.sh zeros [height] [bits] for the zeros.
#
# The integers are 32768 bits wide, 1024 limbs. The tail of Euler-Maclaurin carries rationals near 9700 bits, and
# a product of two of them before its reduction near twice that. A value that does not fit stops the program with
# a message, and the program prints no wrong value.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../.." && pwd)"
EXACT="$TOP/includes/exact_types/integers"
# One folder per platform. A Linux shell and a Windows shell can share this tree, and neither runs the
# binary of the other.
OUT="$TOP/build/exact_zeta/$(uname -s | tr "[:upper:]" "[:lower:]")"
mkdir -p "$OUT"

if [ $# -lt 1 ]; then
    echo "usage: build_exact_zeta.sh zeros|gram|phase|certificate [arguments]"
    exit 2
fi
NAME="$1"
shift
case "$NAME" in
    zeros) PROGRAM=exact_zeta_zeros ;;
    gram) PROGRAM=exact_zeta_gram ;;
    phase) PROGRAM=exact_zeta_phase ;;
    certificate) PROGRAM=zeta_zero_certificate ;;
    *) echo "usage: build_exact_zeta.sh zeros|gram|phase|certificate [arguments]"; exit 2 ;;
esac

"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -DORIOR_EXACT_LIMBS=1024u -o "$OUT/$PROGRAM" "$HERE/$PROGRAM.c" \
    "$HERE/zeta_exact.c" "$HERE/zeta_search.c" "$HERE/zeta_shuffle.c" "$HERE/zeta_theta.c" "$EXACT/exact_integer_add.c" "$EXACT/exact_integer_decimal.c" \
    "$EXACT/exact_integer_divide.c" "$EXACT/exact_integer_gcd.c" "$EXACT/exact_integer_limbs.c" \
    "$EXACT/exact_integer_multiply.c" || exit 1
"$OUT/$PROGRAM" "$@"
