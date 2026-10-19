#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds ed25519_check and holds the tree's SHA-512 (src/cu/includes/codecs/sha512) to FIPS 180-4's examples and its
# Ed25519 (src/cu/includes/codecs/ed25519) to RFC 8032's test vectors. The compiler is CC where it is set, else cc.
#
#     utils/test/src/cu/includes/codecs/ed25519/ed25519_check.sh
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../../../../../.." && pwd)"
OUT="$TOP/build/ed25519"
mkdir -p "$OUT"

"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -o "$OUT/ed25519_check" "$HERE/ed25519_check.c" \
    "$TOP/src/cu/includes/codecs/ed25519/ed25519.c" "$TOP/src/cu/includes/codecs/sha512/sha512.c" \
    "$TOP"/src/cu/engine/runtime/scriptura/*.c || exit 1
"$OUT/ed25519_check"
