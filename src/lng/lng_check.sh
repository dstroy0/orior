#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# lng_check.sh: lng_check (lng_check.c) built into build/lng where its sources are newer, and run from the top of the
# tree: the three language tables held against the tree and against each other. Its exit is lng_check's.
#
#     src/lng/lng_check.sh
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../.." && pwd)"
source "$TOP/src/cu/includes/file_defs/file_defs.sh"
OUT="$TOP/build/lng"
mkdir -p "$OUT"
BINARY="$OUT/lng_check"
SOURCES=("$HERE/lng_check.c" "$HERE/lng.c" "${FILE_DEFS[@]}")
built="$BINARY"
[ -f "$built.exe" ] && built="$built.exe"
stale=0
[ -f "$built" ] || stale=1
for source in "${SOURCES[@]}" "$HERE/lng.h" "$TOP/src/cu/includes/file_defs/klang/klang.h" "$TOP/src/cu/includes/file_defs/kfile_lines/kfile_lines.h" \
    "$TOP/src/cu/includes/file_defs/qry/writer/qry_member.h"; do
    [ "$source" -nt "$built" ] && stale=1
done
if [ "$stale" -eq 1 ]; then
    cc -std=c11 -O2 -Wall -Wextra -o "$BINARY" "${SOURCES[@]}" || { echo "  build failed: lng_check did not compile"; exit 2; }
fi
cd "$TOP" || exit 2
"$BINARY"
