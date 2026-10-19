#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds ksg_check and holds a file's seal (src/cu/includes/file_defs/ksg) to what ksg.h says it is: its folded root
# against RFC 6962's root of its leaves, pieces of every size, one changed line, no bytes, and a long line. The
# compiler is CC where it is set, else cc.
#
#     utils/test/src/cu/includes/file_defs/ksg/ksg_check.sh
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../../../../../.." && pwd)"
OUT="$TOP/build/ksg"
mkdir -p "$OUT"
source "$TOP/src/cu/includes/file_defs/file_defs.sh"

"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -o "$OUT/ksg_check" "$HERE/ksg_check.c" \
    "$TOP/src/cu/includes/file_defs/ksg/ksg_seal.c" "${FILE_DEFS_MMGR[@]}" || exit 1
"$OUT/ksg_check" || exit 1

# the ksg program on the tree's sm_86.gsm: sealed, held, a relation on line 5 changed and named, and held once more as
# it was
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) RANDOMNESS=(-lbcrypt) ;;
    *) RANDOMNESS=() ;;
esac
"${CC:-cc}" -std=c11 -O2 -Wall -o "$OUT/ksg" "$TOP/src/cu/includes/file_defs/ksg/ksg.c" \
    "$TOP/src/cu/includes/file_defs/ksg/ksg_approvals.c" "$TOP/src/cu/includes/file_defs/ksg/ksa.c" \
    "$TOP/src/cu/includes/file_defs/ksg/ksg_seal.c" "$TOP/src/cu/includes/codecs/ed25519/ed25519.c" \
    "$TOP/src/cu/includes/codecs/sha512/sha512.c" "$TOP"/src/cu/includes/file_defs/qry/writer/platform/*.c \
    "${FILE_DEFS_MMGR[@]}" "${RANDOMNESS[@]}" || exit 1
TRIAL="$OUT/trial"
rm -rf "$TRIAL"
mkdir -p "$TRIAL"
cp "$TOP/src/cu/transpiler/lstar/protocol/table/sm_86.gsm" "$TRIAL/sample.gsm"
failed=0
"$OUT/ksg" seal "$TRIAL/sample.gsm" > "$TRIAL/said.txt" || failed=1
"$OUT/ksg" check "$TRIAL/sample.gsm" > "$TRIAL/said.txt" || failed=1
sed -i '5s/->/->1/' "$TRIAL/sample.gsm"
"$OUT/ksg" check "$TRIAL/sample.gsm" > "$TRIAL/said.txt" && failed=1
grep -q "line 5 differs" "$TRIAL/said.txt" || failed=1
[ "$(grep -c "^  line " "$TRIAL/said.txt")" = 1 ] || failed=1
sed -i '5s/->1/->/' "$TRIAL/sample.gsm"
"$OUT/ksg" check "$TRIAL/sample.gsm" > "$TRIAL/said.txt" || failed=1
if [ "$failed" = 0 ]; then
    echo "  ok   a sealed .gsm holds, its line 5 changed is named alone, and it holds again as it was"
else
    echo "  FAIL a sealed .gsm held, named its changed line alone and held again"
fi

# its approvals: an approver's keys made, the seal approved, held with the approver asked for; a second key that did
# not approve asked for and refused; the file changed and sealed again, its approval no longer holding; and two
# approvers both holding
approved=0
KEYS="$OUT/keys"
rm -rf "$KEYS"
mkdir -p "$KEYS"
"$OUT/ksg" keys ed25519 "$KEYS/approver.ksk" "$KEYS/approver.kpk" > "$TRIAL/said.txt" || approved=1
"$OUT/ksg" keys ed25519 "$KEYS/other.ksk" "$KEYS/other.kpk" > "$TRIAL/said.txt" || approved=1
"$OUT/ksg" keys rsa "$KEYS/no.ksk" "$KEYS/no.kpk" > "$TRIAL/said.txt" && approved=1
"$OUT/ksg" approve "$KEYS/approver.ksk" "$TRIAL/sample.gsm" > "$TRIAL/said.txt" || approved=1
"$OUT/ksg" check --approver "$KEYS/approver.kpk" "$TRIAL/sample.gsm" > "$TRIAL/said.txt" || approved=1
grep -q ": holds" "$TRIAL/said.txt" || approved=1
"$OUT/ksg" check --approver "$KEYS/other.kpk" "$TRIAL/sample.gsm" > "$TRIAL/said.txt" && approved=1
grep -q "no approval holds by the approver" "$TRIAL/said.txt" || approved=1
cp "$TRIAL/sample.gsm" "$TRIAL/approved.gsm"
sed -i '5s/->/->1/' "$TRIAL/sample.gsm"
"$OUT/ksg" seal "$TRIAL/sample.gsm" > "$TRIAL/said.txt" || approved=1
"$OUT/ksg" check --approver "$KEYS/approver.kpk" "$TRIAL/sample.gsm" > "$TRIAL/said.txt" && approved=1
grep -q ": does not hold" "$TRIAL/said.txt" || approved=1
cp "$TRIAL/approved.gsm" "$TRIAL/sample.gsm"
"$OUT/ksg" seal "$TRIAL/sample.gsm" > "$TRIAL/said.txt" || approved=1
"$OUT/ksg" approve "$KEYS/approver.ksk" "$TRIAL/sample.gsm" > "$TRIAL/said.txt" || approved=1
"$OUT/ksg" approve "$KEYS/other.ksk" "$TRIAL/sample.gsm" > "$TRIAL/said.txt" || approved=1
"$OUT/ksg" check --approver "$KEYS/approver.kpk" --approver "$KEYS/other.kpk" "$TRIAL/sample.gsm" \
    > "$TRIAL/said.txt" || approved=1
[ "$(grep -c ": holds" "$TRIAL/said.txt")" = 2 ] || approved=1
if [ "$approved" = 0 ]; then
    echo "  ok   approved, held for its approver, refused for one that did not approve, refused once the file changed" \
         "and was sealed again, and held for two approvers"
else
    echo "  FAIL the approvals held and were refused as they should be"
fi
[ "$failed" = 0 ] && [ "$approved" = 0 ]
