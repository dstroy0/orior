"""Print every character of a page between two indexes, with its box, empty ones included.

usage: python glyph_run.py <stem> <page> <from index> <to index>
"""
import os
import sys

import pypdfium2 as pdfium
import pypdfium2.raw as pdfium_c

from page_text import CORPUS


def main():
    stem, number, start, end = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
    document = pdfium.PdfDocument(os.path.join(CORPUS, "papers", stem + ".pdf"))
    textpage = document[number - 1].get_textpage()
    for index in range(start, end):
        symbol = textpage.get_text_range(index, 1)
        code = pdfium_c.FPDFText_GetUnicode(textpage.raw, index)
        left, bottom, right, top = textpage.get_charbox(index)
        size = pdfium_c.FPDFText_GetFontSize(textpage.raw, index) or 10
        print("%5d  %-4r U+%04X  left %.1f width %.2f em  bottom %.1f top %.1f"
              % (index, symbol, code, left, (right - left) / size, bottom, top))


if __name__ == "__main__":
    main()
