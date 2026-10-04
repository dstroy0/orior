#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Writes the Salishan research paper's generated files from the checks that measure them, before
# build_theory.sh sets the paper.
#
#   Usage:  bash theory/theory/Salishan/generate.sh
#
# Four generators, in order:
#   pure_corpus_index.py   the chapter "Whose words these are", from the config and the tables;
#   corpus_derivation.py   the chapter "Corpus derivation" and its figure;
#   gold_readings.sh       the gold standard corpora and the papers read exactly on the record machine;
#   instrument_figures.py  the figures of the chapter "The corpus under the instrument".
# Each prints "  wrote <path>" for every file it rewrites, and build_theory.sh shows those.
#
# The corpora and papers they read are under build/, which is not tracked. Where build/corpora is
# absent the generated files stand as they are tracked, and this says so and succeeds.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../.." && pwd)"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) PYTHON=python ;;
    *) PYTHON=python3 ;;
esac
export PYTHONIOENCODING=utf-8

if [ ! -d "$TOP/build/corpora" ]; then
    echo "  Salishan: no build/corpora, the generated files stand as tracked"
    exit 0
fi

"$PYTHON" "$TOP/utils/maint/data/salishan/hand_extraction/pure_corpus_index.py" || exit 1
"$PYTHON" "$TOP/utils/maint/data/salishan/corpus_derivation.py" || exit 1
mkdir -p "$TOP/build/salishan_gold"
bash "$TOP/examples/Salishan/4_measure/gold_readings.sh" > "$TOP/build/salishan_gold/gold_readings.log" 2>&1 || {
    echo "  Salishan: gold_readings failed, see build/salishan_gold/gold_readings.log"
    exit 1
}
tail -n 1 "$TOP/build/salishan_gold/gold_readings.log"
"$PYTHON" "$TOP/utils/maint/data/salishan/instrument_figures.py" || exit 1
