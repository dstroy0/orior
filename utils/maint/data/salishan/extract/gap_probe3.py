"""Print the glyph-gap lines of one page beside nothing else, to compare with the text layer.

usage: python gap_probe3.py <stem> <page> [share]
"""
import os
import sys

import pypdfium2 as pdfium

from page_text import CORPUS, gap_lines


class OnePage(object):
    def __init__(self, document, number):
        self.document = document
        self.number = number

    def __len__(self):
        return 1

    def __getitem__(self, index):
        return self.document[self.number - 1]


stem, number = sys.argv[1], int(sys.argv[2])
share = float(sys.argv[3]) if len(sys.argv) > 3 else 0.18
document = pdfium.PdfDocument(os.path.join(CORPUS, "papers", stem + ".pdf"))
for line in gap_lines(OnePage(document, number), share):
    print(line)
