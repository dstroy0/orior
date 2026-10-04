"""Print one page as page_text.row_lines reads it, rows by glyph position.

usage: python rows_probe.py <stem> <page>
"""
import os
import sys

import pypdfium2 as pdfium

from page_text import CORPUS, OnePage, row_lines


def main():
    stem, number = sys.argv[1], int(sys.argv[2])
    document = pdfium.PdfDocument(os.path.join(CORPUS, "papers", stem + ".pdf"))
    for line in row_lines(OnePage(document, number - 1), [])[0]:
        print(line)


if __name__ == "__main__":
    main()
