#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# who_table.sh: the who table, who.tsv beside this script, given every source the tree holds that it does not
# (who_table.py), in a query run of its own, query<n>-<datetime>.qry in build/engine/who/qry (qry_run.sh), whose writer
# writes the table in its place.
#
#     utils/maint/who/who_table.sh
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../.." && pwd)"
OUT="$TOP/build/engine"
source "$TOP/src/cu/transpiler/lstar/protocol/qry_run.sh"
qry_writer_built "$OUT"
run_started "$OUT/who/qry"
python "$HERE/who_table.py" "$TOP" "$HERE/who.tsv" | run_filed "$HERE/who.tsv"
