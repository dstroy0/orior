"""The tokens a glyph-row page text and the paper's text layer disagree on, between two lines.

usage: python layer_diff.py <stem> <first line text> <last line text>

The text layer of a word-over-gloss page sets each column's word, segmentation and gloss on lines of
their own, which keeps a gloss like cedar.shakes whole where the glyph rows can break it at a wide
gap, or run NEG and ??? together at a narrow one. Prints the tokens each side holds that the other
lacks, with their counts, between the first line opening on first and the next opening on last.
"""
import collections
import os
import sys

import page_text

HERE = os.path.dirname(os.path.abspath(__file__))
from workdir import PRIVATE  # noqa: E402


def region(lines, first, last):
    start = next(number for number, text in enumerate(lines) if text.strip().startswith(first))
    end = next(number for number, text in enumerate(lines) if number > start and text.strip().startswith(last))
    return lines[start:end]


def tokens(lines):
    counted = collections.Counter()
    for line in lines:
        if line.startswith("====="):
            continue
        counted.update(line.split())
    return counted


def main():
    stem, first, last = sys.argv[1:4]
    with open(os.path.join(PRIVATE, "pagetext", stem + ".txt"), encoding="utf-8") as handle:
        rows = region(handle.read().split("\n"), first, last)
    with open(os.path.join(page_text.CORPUS, "papers", stem + ".txt"), encoding="utf-8") as handle:
        layer = region(handle.read().split("\n"), first, last)
    from_rows, from_layer = tokens(rows), tokens(layer)
    print("glyph rows only:")
    for token, count in sorted((from_rows - from_layer).items()):
        print("  %s\t%d" % (token, count))
    print("text layer only:")
    for token, count in sorted((from_layer - from_rows).items()):
        print("  %s\t%d" % (token, count))


if __name__ == "__main__":
    main()
