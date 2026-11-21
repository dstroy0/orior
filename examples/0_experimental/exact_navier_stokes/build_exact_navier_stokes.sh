#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds the two Navier-Stokes reports of this directory on includes/exact_types into build/exact_navier_stokes,
# then runs each. The compiler is CC where it is set, else cc.
#
# Run examples/0_experimental/exact_navier_stokes/build_exact_navier_stokes.sh to build and run both.
# Pass torus or cascade to run one of them.
#
# The integers are 1024 bits wide, 32 limbs. That holds 308 decimal digits, far past every value of the
# reports. A value that does not fit stops the program with a message, and the program prints no wrong value.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../.." && pwd)"
source "$TOP/includes/exact_types/exact_types.sh"
# One folder per platform. A Linux shell and a Windows shell can share this tree, and neither runs the
# binary of the other.
OUT="$TOP/build/exact_navier_stokes/$(uname -s | tr "[:upper:]" "[:lower:]")"
mkdir -p "$OUT"

WIDTH=(-DORIOR_EXACT_LIMBS=32u -DORIOR_EXACT_DIGITS=256u)
SOURCES=("$HERE/torus_fields.c" "${EXACT_TYPES_ARITHMETIC[@]}")

WHICH="${1:-both}"
STATUS=0
for report in exact_navier_stokes_on_torus exact_navier_stokes_cascade; do
    case "$WHICH" in
        both) ;;
        torus) [ "$report" = exact_navier_stokes_on_torus ] || continue ;;
        cascade) [ "$report" = exact_navier_stokes_cascade ] || continue ;;
        *) echo "usage: build_exact_navier_stokes.sh [torus|cascade]"; exit 2 ;;
    esac
    "${CC:-cc}" -std=c11 -O2 -Wall -Wextra "${WIDTH[@]}" "${EXACT_TYPES_FLAGS[@]}" -o "$OUT/$report" "$HERE/$report.c" \
        "${SOURCES[@]}" "${EXACT_TYPES_LIBRARIES[@]}" || exit 1
    "$OUT/$report" || STATUS=1
done
exit "$STATUS"
