#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# kdecode.sh: kdecode (kdecode.c) built into build/engine where its sources are newer, and run over the files given:
# each k-file of records written for a person to read, each who named through the who table, or the rows of a table of
# each run given, raw or decoded through the table's oracle.
#
#     src/cu/types/file_defs/kdecode.sh [--raw] [--from <record>] [--count <records>] <file>...
#     src/cu/types/file_defs/kdecode.sh [--raw] [--from <key>] [--to <key>] [--count <rows>] --table <table> <run.qry>...
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../../.." && pwd)"
source "$TOP/src/cu/types/file_defs/file_defs.sh"
OUT="$TOP/build/engine"
mkdir -p "$OUT"
BINARY="$OUT/kdecode"
SOURCES=("$HERE/kdecode.c" "$HERE"/kdecode/*.c "${FILE_DEFS[@]}")
built="$BINARY"
[ -f "$built.exe" ] && built="$built.exe"
stale=0
[ -f "$built" ] || stale=1
for source in "${SOURCES[@]}" "$HERE/kqr/kqr.h" "$HERE/krep_records.h" "$HERE/qry/qry.h" "$HERE/qry/qry_buffer.h"; do
    [ "$source" -nt "$built" ] && stale=1
done
if [ "$stale" -eq 1 ]; then
    cc -std=c11 -O2 -Wall -Wextra -o "$BINARY" "${SOURCES[@]}" || { echo "  build failed: kdecode did not compile"; exit 2; }
fi
"$BINARY" "$@"
