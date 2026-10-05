#!/bin/sh
# Runs the same failing prose tests from HEAD's utils/maint/prose and from the working tree's copy, each
# in a scratch root under the work directory that carries the roots docs_check reads, and prints each
# result.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(git -C "$HERE" rev-parse --show-toplevel)" || exit 1
OUT="$(cd "$HERE" && python -c 'import workdir; print(workdir.WORK)')/cmp"
rm -rf "$OUT"
for side in head work; do
    mkdir -p "$OUT/$side/src" "$OUT/$side/docs" "$OUT/$side/examples" "$OUT/$side/theory" "$OUT/$side/utils/test"
done
cd "$REPO" || exit 1
git archive HEAD utils/maint/prose | tar -x -C "$OUT/head"
mkdir -p "$OUT/work/utils/maint"
cp -r "$REPO/utils/maint/prose" "$OUT/work/utils/maint/prose"
for side in head work; do
    echo "== $side =="
    cd "$OUT/$side/utils/maint/prose" || exit 1
    python -m pytest test_docs_check_tiers.py test_docs_check_exclusions.py -q -p no:cacheprovider --color=no 2>&1 | grep -E "^FAILED|passed|failed|rror"
done
