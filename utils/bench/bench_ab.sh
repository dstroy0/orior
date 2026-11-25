#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds bench_ab against orior and the tree's SHA-256, and runs it. With no files it reads every arm over the
# built in corpora; given files, it reads each one's spacing, units and salience instead.
#
#     utils/bench/bench_ab.sh [file...]
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../.." && pwd)"
SIFT="$TOP/src/cu/engine/nbody/orior"
source "$TOP/includes/exact_types/exact_types.sh"
OUT="$TOP/build/bench"
mkdir -p "$OUT"

cc "${FILE_DEFS_LTO_CC[@]}" -std=c11 -O2 -Wall -Wextra -I "$SIFT" "${EXACT_TYPES_FLAGS[@]}" -o "$OUT/bench_ab" "$HERE/bench_ab.c" \
    "$TOP/includes/codecs/sha256/sha256.c" "$SIFT/orior_core.c" "$SIFT/orior_field.c" "$SIFT/orior_steer.c" \
    "$SIFT/orior_steer_count.c" "$SIFT/orior_steer_plan.c" "$SIFT/scan.c" "${EXACT_TYPES_ARITHMETIC[@]}" \
    "${EXACT_TYPES_LIBRARIES[@]}" -lm || exit 1
"$OUT/bench_ab" "$@"
