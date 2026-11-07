#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Catalog: ANY-4-011
#
# The null permutation ratio of every symbol of every corpus, one row each, written to build/ratios.csv for
# evidence/sims/r/hypotheses/ratio_normality.R.
#
#   Usage:  python examples/any_corpus/4_measure/symbol_ratios.py corpus.sym [more.sym ...]
#
# A symbol is a byte. A symbol's spread is the coefficient of variation of the gaps between its occurrences,
# the population standard deviation over the mean, and its ratio is that spread in one fixed shuffle of the
# corpus over the spread in the corpus itself: 1 at the null, below 1 where the symbol clusters. A symbol
# occurring fewer than 32 times carries no usable gap statistic and gives no row. This is the measure of
# evidence/sims/r/departure.R and of examples/language/4_measure/language_constant.py, read per symbol and not
# summed into a half, and the R side tests the distribution the summaries rest on.

import io
import os
import random
import statistics
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
while not os.path.isdir(os.path.join(ROOT, "src", "cu", "engine")):
    ROOT = os.path.dirname(ROOT)
MIN_OCCURRENCES = 32
SHUFFLE_SEED = 0x51F7


def spreads(symbols):
    """Each symbol's gap coefficient of variation, for the symbols at or over the occurrence floor."""
    seen = {}
    for index, value in enumerate(symbols):
        seen.setdefault(value, []).append(index)
    out = {}
    for value, spots in seen.items():
        if len(spots) < MIN_OCCURRENCES:
            continue
        gaps = [spots[step] - spots[step - 1] for step in range(1, len(spots))]
        mean = statistics.fmean(gaps)
        if mean > 0.0:
            out[value] = statistics.pstdev(gaps) / mean
    return out


def ratios(symbols):
    """(symbol, occurrences, ratio) for every symbol both the corpus and its shuffle score."""
    live = spreads(symbols)
    shuffled = list(symbols)
    random.Random(SHUFFLE_SEED).shuffle(shuffled)
    dead = spreads(shuffled)
    counts = {}
    for value in symbols:
        counts[value] = counts.get(value, 0) + 1
    return [(value, counts[value], dead[value] / spread)
            for value, spread in sorted(live.items()) if value in dead and spread > 0.0]


def main():
    if len(sys.argv) < 2:
        print("usage: symbol_ratios.py corpus.sym [more.sym ...]")
        return 1
    out = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace", newline="")
    target = os.path.join(ROOT, "build", "ratios.csv")
    written = 0
    corpora = 0
    with open(target, "w", encoding="utf-8", newline="") as handle:
        handle.write("corpus,symbol,count,ratio\n")
        for path in sys.argv[1:]:
            if not os.path.isfile(path):
                out.write("  no corpus at %s\n" % path)
                continue
            with open(path, "rb") as corpus:
                symbols = corpus.read()
            name = os.path.splitext(os.path.basename(path))[0]
            rows = ratios(symbols)
            for value, count, ratio in rows:
                handle.write("%s,%d,%d,%.6f\n" % (name, value, count, ratio))
            out.write("  %-36s %d symbols scored\n" % (name[:36], len(rows)))
            written += len(rows)
            corpora += 1 if rows else 0
    out.write("\n  wrote %s with %d rows over %d corpora\n" % (target, written, corpora))
    out.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
