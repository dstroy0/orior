#!/bin/sh
# Regenerate each named paper from its generator and check it: finish, the residue against the paper,
# and the tier check of its examples.
#
# usage: sh check_papers.sh <stem> [stem ...]
cd "$(dirname "$0")" || exit 1
export PYTHONIOENCODING=utf-8
# Each paper's generator is the closed corpus's, and imports the tools from here.
GENERATORS="$(python -c 'import workdir; print(workdir.GENERATORS)')" || exit 1
SALISHAN_TOOLS="$(python -c 'import workdir; print(workdir.HERE)')" || exit 1
export SALISHAN_TOOLS
for stem in "$@"; do
    echo "== $stem"
    # A generator that fails leaves the oracle before it in place; nothing after it is checked.
    out=$(python "$GENERATORS/$stem.py" 2>&1)
    status=$?
    printf '%s\n' "$out" | grep -v " rows$"
    if [ "$status" -ne 0 ]; then
        echo "   the generator failed; the oracle was not rewritten"
        continue
    fi
    python finish.py "$stem" 2>&1 | tail -1
    python residue.py "$stem" 2>&1 | grep -E "forms the repair|written forms|language tokens|agrees|disagreements"
    python tier_check.py "$stem" | cut -c1-180 | head -40
done
