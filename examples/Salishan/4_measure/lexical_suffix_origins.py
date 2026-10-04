#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Write the lexical suffixes and nouns of the Salishan oracle tables as the input lexical_suffix_origins.cu reads.
#
#   Usage:  python examples/Salishan/4_measure/lexical_suffix_origins.py [OUTPUT] [PERMUTATIONS]
#
# The suffixes and nouns are what lexical_suffix_origins.py collects: per language, each suffix as its
# segments with the nominal meanings it offers, and each noun as its segments with its one meaning, read
# from every oracle table by corpus_rows.py. They are taken from that file and not restated here.
# Nothing is counted or divided here: the program builds the relations, draws the shuffles and does
# every piece of arithmetic on the record machine.
#
# The input is text, tab separated. The head lines are "permutations", "seed", "least_suffixes" and
# "least_nouns". Each language is then a line "language <name> <suffixes> <nouns>", that many lines
# "suffix <segments> <meanings>" and that many lines "noun <segments> <meaning>", segments parted by a
# space and meanings by a comma. A segment is letters and their marks, and a meaning is a word of
# lexical_suffix_origins.NOMINAL. Neither holds a space, a comma, a tab or a line break.

import io
import os
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
# Walks up to the repository instead of counting directories to it.
while not os.path.isdir(os.path.join(ROOT, "src", "python")):
    ROOT = os.path.dirname(ROOT)
EXPERIMENTS = os.path.join(ROOT, "utils", "maint", "data", "salishan", "experiments")
sys.path.insert(0, EXPERIMENTS)
for _category in os.scandir(os.path.dirname(EXPERIMENTS)):
    if _category.is_dir():
        sys.path.insert(0, _category.path)

import corpus_rows  # noqa: E402
import lexical_suffix_origins  # noqa: E402

TARGET = os.path.join(ROOT, "build", "salishan_gold", "lexical_suffix_origins.in")

# lexical_suffix_origins.py's own settings: P1 was read at 5000 shuffles, and a language is held with
# 3 or more suffixes and 10 or more nouns. It seeds its shuffles with SEED.
PERMUTATIONS = 5000
SEED = 1998
LEAST_SUFFIXES = 3
LEAST_NOUNS = 10


def main():
    out = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace", newline="")
    target = sys.argv[1] if len(sys.argv) > 1 else TARGET
    permutations = int(sys.argv[2]) if len(sys.argv) > 2 else PERMUTATIONS
    suffixes, nouns = lexical_suffix_origins.collect(corpus_rows.rows(None))
    os.makedirs(os.path.dirname(target), exist_ok=True)
    held = 0
    with open(target, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("permutations\t%d\n" % permutations)
        handle.write("seed\t%d\n" % SEED)
        handle.write("least_suffixes\t%d\n" % LEAST_SUFFIXES)
        handle.write("least_nouns\t%d\n" % LEAST_NOUNS)
        for language in sorted(suffixes):
            own_suffixes, own_nouns = suffixes[language], nouns.get(language, {})
            lines = []
            for key in sorted(own_suffixes):
                lines.append("suffix\t%s\t%s" % (" ".join(key), ",".join(sorted(own_suffixes[key]))))
            for key in sorted(own_nouns):
                lines.append("noun\t%s\t%s" % (" ".join(key), own_nouns[key]))
            handle.write("language\t%s\t%d\t%d\n" % (language, len(own_suffixes), len(own_nouns)))
            handle.write("".join(one + "\n" for one in lines))
            held += 1
    out.write("  %d languages written to %s\n" % (held, target))
    out.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
