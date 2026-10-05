"""Count the runs of one-word lines in a text layer, the mark of tiers set one cell to a line.

usage: python stacked_census.py <stem> [stem ...]

A text layer that sets each word of an example over its gloss gives dim / PROSP / hadiks / swim, a
run of lines of one token each. Prints the number of runs of six or more such lines per paper.
"""
import os
import re
import sys

from page_text import CORPUS


LABEL = re.compile(r"^[=\-]?(?:[0-9]*[A-Z][A-Z0-9]*[.\-=]?)*[0-9]*[A-Z]{2,}[A-Z0-9.\-=]*$")


def runs(lines, least=6):
    """Runs of one-word lines, a third of them or more gloss labels, PROSP or 1SG.PRON."""
    count = length = labels = 0
    for line in lines + [""]:
        if len(line.split()) == 1 and len(line.strip()) < 25:
            length += 1
            labels += bool(LABEL.match(line.strip()))
            continue
        count += length >= least and labels * 3 >= length
        length = labels = 0
    return count


def main():
    for stem in sys.argv[1:]:
        with open(os.path.join(CORPUS, "papers", stem + ".txt"), encoding="utf-8") as handle:
            print("%-40s %d" % (stem, runs(handle.read().split("\n"))))


if __name__ == "__main__":
    main()
