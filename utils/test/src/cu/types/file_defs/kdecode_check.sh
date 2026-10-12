#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# kdecode_check.sh: kdecode's select over a run's tables (kdecode.c) held to what the run holds, on the run the checks
# of R, the who and the rows wrote (chain_check.sh):
#
#   files    a file's row decoded names the blob of its bytes by its key and that blob's name, and its path by its
#            text, the two the same
#   raw      a table raw and decoded is the same rows, by the same keys in the same order
#   range    --from and --count select the row of a key, and --from and --to the rows from one key through another
#   texts    a text the run numbers stands in its table once, decoded to its bytes
#   asks     a k-file's table opens with its head, the kind and the file's name, and a record put is named by its
#            format's oracle
#   links    a link a file's write makes is from the blob of the file's bytes
#   bare     a table with no oracle and no head is given raw, decoded or not
#   oracles  the run holds the oracle of every table it holds, put in as it ended, and is decoded through them with no
#            tree to read an oracle from
#   run      a run given with no table is told to name one
#
#     utils/test/src/cu/types/file_defs/kdecode_check.sh <run.qry>
set -u

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../../.." && pwd)"
KDECODE="$TOP/src/cu/types/file_defs/kdecode.sh"
RUN="$1"
checks=0
failed=0

check_that()
{
    checks=$((checks + 1))
    if [ "$1" -ne 0 ]; then
        failed=$((failed + 1))
        echo "  FAILED: $2"
    fi
}

# the text `$1` as lines, for a command that reads them
lines()
{
    printf '%s\n' "$1"
}

# files
files="$(bash "$KDECODE" --table files "$RUN")"
put="$(lines "$files" | grep -F 'build/engine/kdm_check_put.kdm]' | head -1)"
blob="$(lines "$put" | cut -d' ' -f3)"
[ -n "$put" ] && [ "${blob#*:}" = "build/engine/kdm_check_put.kdm" ]
check_that $? "files: the row of kdm_check's file names its bytes' blob by the file's path, and its path the same"

# raw
raw="$(bash "$KDECODE" --raw --table files "$RUN")"
decoded_keys="$(lines "$files" | cut -d' ' -f1)"
raw_keys="$(lines "$raw" | while read -r key _; do [ -n "$key" ] && printf '%d\n' "0x$key"; done)"
[ -n "$files" ] && [ "$decoded_keys" = "$raw_keys" ]
check_that $? "raw: the files table raw and decoded is the same rows by the same keys in the same order"

# range
first="$(lines "$decoded_keys" | sed -n 1p)"
second="$(lines "$decoded_keys" | sed -n 2p)"
third="$(lines "$decoded_keys" | sed -n 3p)"
one="$(bash "$KDECODE" --table files --from "$second" --count 1 "$RUN")"
[ -n "$second" ] && [ "$(lines "$one" | cut -d' ' -f1)" = "$second" ] && [ "$(lines "$one" | wc -l)" -eq 1 ]
check_that $? "range: --from a key and --count 1 is that key's row alone"
span="$(bash "$KDECODE" --table files --from "$first" --to "$third" "$RUN" | cut -d' ' -f1 | tr '\n' ' ')"
[ "$span" = "$first $second $third " ]
check_that $? "range: --from one key --to another is the rows from the one through the other"
after="$(bash "$KDECODE" --table files --from "$((third + 1))" --to "$third" "$RUN" 2>&1)"
[ -n "$third" ] && [ -z "$after" ]
check_that $? "range: a range whose first key is past its last holds no row"

# texts
said="$(bash "$KDECODE" --table texts "$RUN" | grep -cF ' a text the check numbers twice]')"
[ "$said" -eq 1 ]
check_that $? "texts: a text the rows check numbers twice stands in the table once, decoded to its bytes"

# asks
asks="$(bash "$KDECODE" --table asks "$RUN")"
head_row="$(lines "$asks" | sed -n 1p)"
[ "$(lines "$head_row" | cut -d' ' -f2-5)" = "head KQR 1 build/engine/kqr_check.kqr" ]
check_that $? "asks: the table opens with the head of R's file, its kind, its version and its name"
lines "$asks" | grep -qE '^[0-9]+ put [0-9]+ [0-9a-f]+ (kqr|cycle|ask|sample|pair|comment) '
check_that $? "asks: a record put is named by its format's oracle"

# links
link="$(bash "$KDECODE" --table links --count 1 "$RUN")"
from="$(lines "$link" | cut -d' ' -f3)"
lines "$files" | grep -qE "^[0-9]+ file ${from%%:*}:" && lines "$link" | grep -qF ' written_from '
check_that $? "links: a file's link is from the blob of its bytes, written_from"

# bare
bare="$(bash "$KDECODE" --table check_rows "$RUN")"
[ -n "$bare" ] && [ "$bare" = "$(bash "$KDECODE" --raw --table check_rows "$RUN")" ]
check_that $? "bare: a table with no oracle and no head is given raw, decoded or not"

# oracles: a top of the tree that holds no oracle, so that every oracle read is the run's
NO_TREE="$TOP/build/engine/kdecode_no_tree"
mkdir -p "$NO_TREE"
held_links="$(QRY_TOP="$NO_TREE" bash "$KDECODE" --table links --count 1 "$RUN")"
held_asks="$(QRY_TOP="$NO_TREE" bash "$KDECODE" --table asks "$RUN")"
lines "$held_links" | grep -qF ' written_from ' &&
    [ "$(lines "$held_asks" | sed -n 1p | cut -d' ' -f2)" = "head" ] &&
    lines "$held_asks" | grep -qE '^[0-9]+ put [0-9]+ [0-9a-f]+ (kqr|cycle|ask|sample|pair|comment) '
check_that $? "oracles: the run holds the oracles of its tables and is decoded through them with no tree's oracle"

# run
bash "$KDECODE" "$RUN" > /dev/null 2> "$TOP/build/engine/kdecode_check.err"
status=$?
[ "$status" -eq 1 ] && grep -qF 'is a run: name its table with --table' "$TOP/build/engine/kdecode_check.err"
check_that $? "run: a run given with no table is told to name one"

echo "kdecode_check: $checks checks, $failed failed"
[ "$failed" -eq 0 ]
