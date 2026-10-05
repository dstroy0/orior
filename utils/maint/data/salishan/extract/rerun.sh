#!/bin/sh
# Rebuild every finished paper from the engine through the residue check, to catch regressions
# after a change to paper_sift.py or finish.py. Prints one verdict line per paper, and the rows of
# its oracle the rebuild changed: the residue check holds every form to the page, and a row whose
# kind or who moved still passes it. The papers are those whose table file sets AUTHORS and LANG.
cd "$(dirname "$0")"
export PYTHONIOENCODING=utf-8
# The oracle tables are the public tree's, and the oracles saved to compare against go to the work
# directory, both as workdir.py resolves them.
ORACLES="$(python -c 'import workdir; print(workdir.ORACLES)')" || exit 1
PREV="$(python -c 'import workdir; print(workdir.WORK)')/oracle_prev" || exit 1
mkdir -p "$PREV"
run() {
    cp "$ORACLES/$1.oracle.tsv" "$PREV/$1.oracle.tsv"
    python paper_sift.py "$1" "$2" "$3" > /dev/null || echo "$1: paper_sift FAILED"
    python finish.py "$1" > /dev/null || echo "$1: finish FAILED"
    printf '%s: ' "$1"
    python residue.py "$1" | tail -1
    diff "$PREV/$1.oracle.tsv" "$ORACLES/$1.oracle.tsv" | grep '^[<>]' | cut -c1-160
}
TAB="$(printf '\t')"
python tables.py AUTHORS LANG | while IFS="$TAB" read -r stem authors lang; do
    run "$stem" "$authors" "$lang" < /dev/null
done
