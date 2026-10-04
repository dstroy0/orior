"""Count, for one paper, the text layer lines the glyph positions space more and the ones they space
less, among lines holding the same letters.

usage: python space_census.py <stem> [examples]

A paper whose text layer drops spaces, Thispaperreexamines, has many lines of the first kind. One
whose text layer puts a space at every font change, n ɬeʔkepmxcín, has many of the second.
"""
import os
import sys

import pypdfium2 as pdfium

from page_text import CORPUS, gap_lines


def main():
    stem = sys.argv[1]
    show = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    document = pdfium.PdfDocument(os.path.join(CORPUS, "papers", stem + ".pdf"))
    by_letters = {}
    for one in gap_lines(document):
        by_letters.setdefault("".join(one.split()), one)
    with open(os.path.join(CORPUS, "papers", stem + ".txt"), encoding="utf-8") as handle:
        layer = [line.rstrip() for line in handle.read().split("\n")]
    more = fewer = same = unmatched = 0
    shown = []
    for line in layer:
        spaced = by_letters.get("".join(line.split()))
        if spaced is None:
            unmatched += 1
            continue
        own, theirs = len(line.split()), len(spaced.split())
        if theirs > own:
            more += 1
        elif theirs < own:
            fewer += 1
            if len(shown) < show:
                shown.append((line, spaced))
        else:
            same += 1
    print("%s: glyph positions space %d lines more, %d less, %d the same, %d without a match"
          % (stem, more, fewer, same, unmatched))
    for line, spaced in shown:
        print("  layer: %s\n  glyph: %s" % (line, spaced))


if __name__ == "__main__":
    main()
