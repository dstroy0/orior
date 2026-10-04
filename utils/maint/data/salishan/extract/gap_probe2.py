"""Print each glyph with its gap to the one before, over the font size, for the page line holding a string.

usage: python gap_probe2.py <stem> <page> <string>
"""
import os
import sys

import pypdfium2 as pdfium
import pypdfium2.raw as raw

import page_text

stem, page, needle = sys.argv[1], int(sys.argv[2]), sys.argv[3]
document = pdfium.PdfDocument(os.path.join(page_text.CORPUS, "papers", stem + ".pdf"))
textpage = document[page - 1].get_textpage()
text = textpage.get_text_range()
start = text.find(needle)
count = textpage.count_chars()
# get_text_range and the char index agree where the page has no generated characters before the string.
start = next(index for index in range(count)
             if textpage.get_text_range(index, len(needle)) == needle)
last = None
for index in range(start, start + len(needle) + 20):
    symbol = textpage.get_text_range(index, 1)
    box = textpage.get_charbox(index)
    size = raw.FPDFText_GetFontSize(textpage.raw, index)
    gap = (box[0] - last[2]) / size if last else 0
    print(repr(symbol), "%.3f" % gap, "size %.1f" % size, "bottom %.1f" % box[1])
    if box[2] > box[0]:
        last = box
