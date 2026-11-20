#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds one zeta example of this directory on includes/exact_types into build/exact_zeta, then runs it with the
# arguments that follow its name. The compiler is CC where it is set, else cc.
#
# Run examples/0_experimental/exact_zeta/build_exact_zeta.sh zeros [height] [bits] for the zeros.
#
# Each program has its own width. The tail of Euler-Maclaurin carries rationals near 9700 bits, and a product of two
# of them before its reduction near twice that. Thus a program that sums zeta by Euler-Maclaurin uses 32768 bits,
# 1024 limbs. A value that does not fit stops the program with a message, and the program prints no wrong value.
#
# Riemann-Siegel reads theta at small t, where the series of Stirling needs N = 64 at 64 places. Its terms carry
# |w|^(2j-1) for j up to 2N, near 180000 bits. That program uses 262144 bits, 8192 limbs. Each value then holds
# 32 KiB, and the program asks for a stack of 512 MiB.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../.." && pwd)"
EXACT="$TOP/includes/exact_types/integers"
# One folder per platform. A Linux shell and a Windows shell can share this tree, and neither runs the
# binary of the other.
OUT="$TOP/build/exact_zeta/$(uname -s | tr "[:upper:]" "[:lower:]")"
mkdir -p "$OUT"

USAGE="usage: build_exact_zeta.sh zeros|gram|phase|certificate|arrival|spherical|riemann_siegel [arguments]"
if [ $# -lt 1 ]; then
    echo "$USAGE"
    exit 2
fi
NAME="$1"
shift
STACK=0
case "$NAME" in
    zeros) PROGRAM=exact_zeta_zeros LIMBS=1024u ;;
    gram) PROGRAM=exact_zeta_gram LIMBS=1024u ;;
    phase) PROGRAM=exact_zeta_phase LIMBS=1024u ;;
    certificate) PROGRAM=zeta_zero_certificate LIMBS=1024u ;;
    arrival) PROGRAM=exact_zeta_arrival LIMBS=32u ;;
    spherical) PROGRAM=exact_zeta_spherical LIMBS=256u ;;
    riemann_siegel) PROGRAM=exact_zeta_riemann_siegel LIMBS=8192u STACK=536870912 ;;
    *) echo "$USAGE"; exit 2 ;;
esac

SOURCES=("$HERE/zeta_exact.c" "$HERE/zeta_gram_walk.c" "$HERE/zeta_riemann_siegel.c" "$HERE/zeta_search.c"
    "$HERE/zeta_shuffle.c" "$HERE/zeta_theta.c"
    "$EXACT/exact_integer_add.c" "$EXACT/exact_integer_decimal.c" "$EXACT/exact_integer_divide.c"
    "$EXACT/exact_integer_gcd.c" "$EXACT/exact_integer_limbs.c" "$EXACT/exact_integer_multiply.c")
# A width below 4096 bits declares its own floor of decimal digits, as exact_integer_widths.h asks.
DIGITS=()
if [ "${LIMBS%u}" -lt 128 ]; then
    DIGITS=(-DORIOR_EXACT_DIGITS=$(( ${LIMBS%u} * 8 ))u)
fi
# A Windows linker sets the stack of the program. Elsewhere the shell sets it, up to its hard limit.
LINK=()
if [ "$STACK" -gt 0 ]; then
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*) LINK=(-Wl,--stack,"$STACK") ;;
        *) ulimit -s unlimited 2>/dev/null || ulimit -s "$(ulimit -H -s)" ;;
    esac
fi
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -DORIOR_EXACT_LIMBS="$LIMBS" "${DIGITS[@]}" -o "$OUT/$PROGRAM" \
    "$HERE/$PROGRAM.c" "${SOURCES[@]}" "${LINK[@]}" -lm || exit 1
# The programs read and write paths from the top of the tree.
cd "$TOP" || exit 1
"$OUT/$PROGRAM" "$@"
