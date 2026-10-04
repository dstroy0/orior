#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Write the meaning-matched vocabulary of the Salishan oracle tables as the input subgrouping.cu reads.
#
#   Usage:  python examples/Salishan/4_measure/subgrouping.py [OUTPUT] [PERMUTATIONS]
#
# The vocabulary is what subgrouping_check.py builds: per language, each meaning with the
# Dolgopolsky class strings of its forms and the paper each came from, read from every oracle table
# by corpus_rows.py. It is taken from that file and not restated here. Only a form's first two
# classes are written, since a match is two forms of one meaning agreeing on their first two classes.
# Nothing is counted or divided here: the program builds the matches, draws the shuffles and does
# every piece of arithmetic on the record machine.
#
# The input is text, tab separated. The head lines are "permutations", "seed", "least_shared",
# "least_meanings" and "p_below <numerator> <denominator>". Each language is then a line
# "language <branch> <name> <meanings>" and that many lines "<meaning>" followed by one
# "<classes>|<paper>" field per form.

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
import subgrouping_check  # noqa: E402

TARGET = os.path.join(ROOT, "build", "salishan_gold", "subgrouping.in")

# subgrouping_check.py's own settings: its default shuffle count is 200 and P0 was read at 2000, a
# pair is compared on 15 or more shared meanings, a language is held with 40 or more, and an outside
# pair fails criterion (d) below a shuffle p of 1/100. It seeds its shuffles with SEED.
PERMUTATIONS = 2000
SEED = 61
LEAST_SHARED = 15
LEAST_MEANINGS = 40
P_BELOW = (1, 100)


def main():
    out = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace", newline="")
    target = sys.argv[1] if len(sys.argv) > 1 else TARGET
    permutations = int(sys.argv[2]) if len(sys.argv) > 2 else PERMUTATIONS
    words = subgrouping_check.vocabulary(corpus_rows.rows(None))
    os.makedirs(os.path.dirname(target), exist_ok=True)
    held = 0
    with open(target, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("permutations\t%d\n" % permutations)
        handle.write("seed\t%d\n" % SEED)
        handle.write("least_shared\t%d\n" % LEAST_SHARED)
        handle.write("least_meanings\t%d\n" % LEAST_MEANINGS)
        handle.write("p_below\t%d\t%d\n" % P_BELOW)
        for language in sorted(words):
            branch = corpus_rows.BRANCHES.get(language)
            if branch is None:
                continue
            meanings = words[language]
            handle.write("language\t%s\t%s\t%d\n" % (branch, language, len(meanings)))
            for meaning in sorted(meanings):
                forms = sorted("%s|%s" % (shape[:2], stem) for shape, stem in meanings[meaning])
                if any(("\t" in one) or ("\n" in one) for one in forms + [meaning]):
                    out.write("  %s holds a tab or a line break in a meaning or a form\n" % language)
                    out.flush()
                    return 1
                handle.write("\t".join([meaning] + forms) + "\n")
            held += 1
    out.write("  %d languages written to %s\n" % (held, target))
    out.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
