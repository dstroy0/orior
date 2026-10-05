"""Print the text layer line and the wide glyph line holding a string, to see why they do not pair."""
import os
import sys

import pypdfium2 as pdfium

from page_text import CORPUS, gap_lines


def main():
    stem, wanted = sys.argv[1], sys.argv[2]
    document = pdfium.PdfDocument(os.path.join(CORPUS, "papers", stem + ".pdf"))
    with open(os.path.join(CORPUS, "papers", stem + ".txt"), encoding="utf-8") as handle:
        for line in handle.read().split("\n"):
            if wanted in line:
                print("LAYER", repr(line), repr("".join(line.split())))
    for line in gap_lines(document, wide=True):
        if wanted in "".join(line.split()):
            print("GLYPH", repr(line), repr("".join(line.split())))


if __name__ == "__main__":
    main()
