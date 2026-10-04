#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Write the gold standard corpora as the input gold_readings.cu reads, every line as it is held.
#
#   Usage:  python examples/Salishan/4_measure/gold_readings.py [OUTPUT]
#
# The corpora are the ones language_check.py reads: each language's known-pure lines, keyed by
# BY_CORPUS, and the English the readers marked as translation. They are taken from that file and not
# restated here, and the published readings and this program read the same selection. Nothing is
# counted or divided here. The program counts the byte pairs and does every piece of arithmetic on
# the record machine.
#
# The input is text. Its first line is "margin <numerator> <denominator>", the factor a distance has
# to clear a split-half distance by. Each corpus is then a line "corpus <name> <lines>" and that many
# lines of its text, byte for byte as language_check.py holds them. A held line is stripped and never
# empty. No line of a corpus is blank.

import io
import os
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
# Walks up to the repository instead of counting directories to it.
while not os.path.isdir(os.path.join(ROOT, "src", "python")):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "utils", "maint", "data", "salishan", "orior_algorithmic_extraction"))

from language_check import english_texts, language_texts  # noqa: E402

TARGET = os.path.join(ROOT, "build", "salishan_gold", "gold_readings.in")

# The chapter reads a pair of profiles where their distance clears the larger split-half distance
# of the two, a factor of one.
MARGIN = (1, 1)


def main():
    out = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace", newline="")
    target = sys.argv[1] if len(sys.argv) > 1 else TARGET
    corpora = language_texts()
    corpora["English"] = english_texts()
    if len(corpora) < 3:
        out.write("  no gold standard corpora under build/corpora\n")
        return 1

    os.makedirs(os.path.dirname(target), exist_ok=True)
    with open(target, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("margin %d %d\n" % MARGIN)
        for name in sorted(corpora):
            lines = [one for one in corpora[name] if one]
            if any(("\n" in one) or ("\r" in one) for one in lines):
                out.write("  %s holds a line break inside a line\n" % name)
                return 1
            handle.write("corpus %s %d\n" % (name, len(lines)))
            for one in lines:
                handle.write(one + "\n")
            out.write("  %-16s %d lines\n" % (name, len(lines)))
    out.write("  wrote %s\n" % target)
    out.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
