#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# The host swept for the run's buffer: its candidates timed once more (qry_sweep.c) in a run of their own in
# build/qry_sweep/qry, its blob `sweep`, and qry_host.h written beside qry_buffer.h from the median of every sweep kept
# there, once there are enough of them to say. A sweep run a few times over some days, while the host is otherwise
# idle, settles the medians (qry_kept.sh runs one each time it runs); every build of the buffer after takes them.
# qry_host.h is put in place only where what it says changes, and the builds that read it build again only then.
#
#     src/cu/types/file_defs/qry/qry_sweep.sh
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../../../.." && pwd)"
OUT="$TOP/build/qry_sweep"
mkdir -p "$OUT"
source "$HERE/qry_run.sh"
qry_writer_built "$OUT"
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/qry_sweep" "$HERE/qry_sweep.c" "$HERE"/buffer/*.c ||
    { echo "  qry_sweep did not compile"; exit 1; }

unset QRY
run_started "$OUT/qry"
"$OUT/qry_sweep" time "$OUT" | "$QRY_WRITER" hand sweep
[ "${PIPESTATUS[0]}" -eq 0 ] || { echo "  the sweep did not run"; exit 1; }
# the run ended here, its .qry dumped, for the pick to read it beside the sweeps before it
run_ended
trap - EXIT
runs=()
for run in "$OUT"/qry/query*.qry; do
    [ -f "$run" ] && runs+=("$run")
done
"$OUT/qry_sweep" pick "${runs[@]}" > "$HERE/qry_host.h.next"
picked=$?
if [ "$picked" -ne 0 ]; then
    rm -f "$HERE/qry_host.h.next"
    [ "$picked" -eq 3 ] && exit 0
    exit 1
fi
if cmp -s "$HERE/qry_host.h.next" "$HERE/qry_host.h"; then
    rm -f "$HERE/qry_host.h.next"
    echo "  qry_host.h holds as it was, from ${#runs[@]} sweeps"
else
    mv -f "$HERE/qry_host.h.next" "$HERE/qry_host.h"
    echo "  qry_host.h written from ${#runs[@]} sweeps:"
fi
grep '^#define' "$HERE/qry_host.h" | sed 's/^/    /'
