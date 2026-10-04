#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Write the gold standard corpora and the screened papers as the input gold_readings.cu reads, every
# line as it is held.
#
#   Usage:  python examples/Salishan/4_measure/gold_readings.py [OUTPUT] [screened|sifted]
#
# The corpora are the ones language_check.py reads: each language's known-pure lines, keyed by
# BY_CORPUS, and the English the readers marked as translation. The papers are the ones it reads too:
# every paper whose front matter names a language the corpora cover, one text each, the nine papers
# whose readers made the corpora left out. Corpora, papers and lines are taken from that file and the
# modules it imports and not restated here. Nothing is counted or
# divided here: the program counts the byte pairs and does every piece of arithmetic on the record
# machine.
#
# Which lines of a paper are read is the source's:
#   screened  the lines language_check.py's English screen does not account for, the text the
#             published readings read (the default);
#   sifted    the lines sift_extract.py's tool extraction sorts nearer the pure corpus than English.
#
# The input is text. Its first line is "margin <numerator> <denominator>", the factor a distance has
# to clear a split-half distance by, and its second "least <pairs>", the fewest pairs a paper is read
# at. Each corpus is then a line "corpus <name> <lines>" and each paper a line
# "paper <stem> <language> <lines>", and that many lines of its text, byte for byte as
# language_check.py holds them. A space in a stem is written as an underscore. A held line is stripped
# and never empty. No line is blank.

import io
import os
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
# Walks up to the repository instead of counting directories to it.
while not os.path.isdir(os.path.join(ROOT, "src", "python")):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "utils", "maint", "data", "salishan", "orior_algorithmic_extraction"))

import language_check  # noqa: E402
import sift_extract  # noqa: E402

TARGET = os.path.join(ROOT, "build", "salishan_gold", "gold_readings.in")

# language_check.py reads a paper where its gap clears its own split-half distance, a factor of one,
# and the chapter reads a pair of corpora where their distance clears the larger split-half distance
# of the two by the same factor.
MARGIN = (1, 1)

# The fewest pairs language_check.py reads a paper at.
LEAST = 2000


def block(handle, out, head, lines):
    """One corpus or paper: its head line, then its lines, refused where a line holds a break."""
    if any(("\n" in one) or ("\r" in one) for one in lines):
        out.write("  %s holds a line break inside a line\n" % head)
        return False
    handle.write("%s %d\n" % (head, len(lines)))
    for one in lines:
        handle.write(one + "\n")
    return True


def main():
    out = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace", newline="")
    target = sys.argv[1] if len(sys.argv) > 1 else TARGET
    source = sys.argv[2] if len(sys.argv) > 2 else "screened"
    if source not in ("screened", "sifted"):
        out.write("  the source is screened or sifted, not %s\n" % source)
        out.flush()
        return 1
    corpora = language_check.language_texts()
    corpora["English"] = language_check.english_texts()
    if len(corpora) < 3:
        out.write("  no gold standard corpora under build/corpora\n")
        out.flush()
        return 1

    screen_counts, screen_total, _ = language_check.english_reference()
    cut = language_check.calibrated_cut(screen_counts, screen_total)
    if source == "sifted":
        english = sift_extract.english_reference()
        language = sift_extract.language_reference()

    os.makedirs(os.path.dirname(target), exist_ok=True)
    with open(target, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("margin %d %d\n" % MARGIN)
        handle.write("least %d\n" % LEAST)
        for name in sorted(corpora):
            lines = [one for one in corpora[name] if one]
            if not block(handle, out, "corpus %s" % name, lines):
                out.flush()
                return 1
            out.write("  %-16s %d lines\n" % (name, len(lines)))
        papers = 0
        for path in language_check.paper_texts():
            stem = os.path.basename(path)[:-4]
            if stem in language_check.READ:
                continue
            says = language_check.attribution(language_check.named_in(path))
            if says not in corpora:
                continue
            if source == "screened":
                noise = language_check.paper_lines(path, screen_counts, screen_total, cut)
            else:
                noise = [held[4] for held in sift_extract.found_in(path, english, language) if held[1] == "language"]
            stem = stem.replace(" ", "_")
            if not block(handle, out, "paper %s %s" % (stem, says), [one for one in noise if one]):
                out.flush()
                return 1
            papers += 1
    out.write("  %d papers name a language the corpora cover, read from their %s lines\n" % (papers, source))
    out.write("  wrote %s\n" % target)
    out.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
