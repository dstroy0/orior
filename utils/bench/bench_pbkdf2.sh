#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Holds the PBKDF2 of includes/codecs/pbkdf2 to its published vectors, then times one BIP-39 seed derivation. The
# compiler is CC where it is set, else cc.
#
# Run utils/bench/bench_pbkdf2.sh with no arguments. Pass --fetch to fetch the vector files again.
#
# The vector check runs first. Where it fails, the script stops, and reports no rate.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../.." && pwd)"
OUT="$TOP/build/bench"
mkdir -p "$OUT"

if ! bash "$TOP/utils/test/includes/codecs/pbkdf2/pbkdf2_check.sh" "$@"; then
    echo "  REFUSED: a published vector fails. Fix the disagreement before any rate."
    exit 1
fi

"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -o "$OUT/bench_pbkdf2" "$HERE/bench_pbkdf2.c" \
    "$TOP/includes/codecs/pbkdf2/pbkdf2.c" "$TOP/includes/codecs/sha512/sha512.c" || exit 1
"$OUT/bench_pbkdf2"
