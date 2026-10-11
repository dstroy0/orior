#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# who_table.sh: the who table, who.tsv beside this script, given every source the tree holds that it does not
# (who_table.py), in a query run of its own, query<n>-<datetime>.qry in build/engine/who/qry (qry_run.sh), whose writer
# writes the table in its place.
#
#     src/cu/types/file_defs/who/who_table.sh
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../../../.." && pwd)"
OUT="$TOP/build/engine"
source "$TOP/src/cu/types/file_defs/qry/qry_run.sh"
qry_writer_built "$OUT"
run_started "$OUT/who/qry"
run_read "$HERE/who.tsv"
python "$HERE/who_table.py" "$TOP" "$HERE/who.tsv" | run_filed "$HERE/who.tsv"
