#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# measuring_stick_read.sh: measuring_stick_read (measuring_stick_read.c) built into build/measuring_stick where its
# sources are newer, and run with the arguments given, in the run of the script that runs it (qry_run.sh), whose writer
# writes the record. Its exit is the program's.
#
#     measuring_stick_read.sh <nvcc listing> <manifest> <sass.krs> <record> [<engine.tsv> <engine directory>]
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../../../.." && pwd)"
source "$TOP/src/cu/types/file_defs/file_defs.sh"
OUT="${BUILD_OUT:-$TOP/build/measuring_stick}"
mkdir -p "$OUT"
BINARY="$OUT/measuring_stick_read"
SOURCES=("$HERE/measuring_stick_read.c" "${FILE_DEFS[@]}")
built="$BINARY"
[ -f "$built.exe" ] && built="$built.exe"
stale=0
[ -f "$built" ] || stale=1
for source in "${SOURCES[@]}" "$TOP/src/cu/types/file_defs/klang.h" "$TOP/src/cu/types/file_defs/ktext.h" \
    "$TOP/src/cu/types/file_defs/qry/qry_buffer.h"; do
    [ "$source" -nt "$built" ] && stale=1
done
if [ "$stale" -eq 1 ]; then
    cc -std=c11 -O2 -Wall -Wextra -o "$BINARY" "${SOURCES[@]}" ||
        { echo "  build failed: measuring_stick_read did not compile"; exit 2; }
fi
"$BINARY" "$@"
