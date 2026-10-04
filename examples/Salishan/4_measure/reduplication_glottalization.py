#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Write the glottalized-resonant comparisons of the Salishan oracle tables as the input
# reduplication_glottalization.cu reads.
#
#   Usage:  python examples/Salishan/4_measure/reduplication_glottalization.py [OUTPUT]
#
# The comparisons are what reduplication_glottalization.py finds: per language, each doubled root
# once, with the verdict of each glottalized resonant in it and the paper it came from, the survey's
# own two papers left out. Table 4's class for each language is that file's TABLE_4. Both are taken
# from that file and not restated here. Nothing is counted or divided here: the program tallies the
# verdicts and papers, and reads each language, on the record machine.
#
# The input is text, tab separated. The head lines are "identical <numerator> <denominator>" and
# "split <numerator> <denominator>": a language reads IDENTICAL where its share of IDENTICAL verdicts
# is at least the first, and SPLIT where it is at most the second. Each language is then a line
# "language <name> <Table 4 class or -> <comparisons>" and that many lines "<paper> <verdicts>", the
# verdicts parted by a comma.

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
import reduplication_glottalization  # noqa: E402

TARGET = os.path.join(ROOT, "build", "salishan_gold", "reduplication_glottalization.in")

# reduplication_glottalization.py's own reading: IDENTICAL at a share of 0.75 or more, SPLIT at 0.25
# or less, BOTH between.
IDENTICAL = (3, 4)
SPLIT = (1, 4)


def main():
    out = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace", newline="")
    target = sys.argv[1] if len(sys.argv) > 1 else TARGET
    found = reduplication_glottalization.comparisons(corpus_rows.rows(None), False)
    table = reduplication_glottalization.TABLE_4
    os.makedirs(os.path.dirname(target), exist_ok=True)
    with open(target, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("identical\t%d\t%d\n" % IDENTICAL)
        handle.write("split\t%d\t%d\n" % SPLIT)
        for language in sorted(found, key=lambda one: (table.get(one, "~"), one)):
            handle.write("language\t%s\t%s\t%d\n" % (language, table.get(language, "-"), len(found[language])))
            for _pair, verdicts, stem in found[language]:
                handle.write("%s\t%s\n" % (stem, ",".join(verdicts)))
    out.write("  %d languages written to %s\n" % (len(found), target))
    out.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
