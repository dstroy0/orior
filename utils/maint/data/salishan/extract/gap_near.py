"""Print the gap before each glyph around every place a word stands in the text layer, in em, for
reading whether the page sets a space the rows text lost.

usage: python gap_near.py <stem> <word> [glyphs before] [glyphs after]
"""
import os
import sys

import pypdfium2 as pdfium
import pypdfium2.raw as pdfium_c

import page_text

stem, wanted = sys.argv[1], sys.argv[2]
before = int(sys.argv[3]) if len(sys.argv) > 3 else 12
after = int(sys.argv[4]) if len(sys.argv) > 4 else 12
document = pdfium.PdfDocument(os.path.join(page_text.CORPUS, "papers", stem + ".pdf"))
for number in range(len(document)):
    textpage = document[number].get_textpage()
    text = textpage.get_text_range()
    at = text.find(wanted)
    while at >= 0:
        last = None
        out = []
        for index in range(max(0, at - before), min(len(text), at + len(wanted) + after)):
            symbol = textpage.get_text_range(index, 1)
            if symbol in "\r\n":
                out.append("|")
                last = None
                continue
            box = textpage.get_charbox(index)
            size = pdfium_c.FPDFText_GetFontSize(textpage.raw, index) or 10
            gap = (box[0] - last[2]) / size if last else 0
            out.append("%s%.2f" % (symbol, gap))
            last = box
        print(number + 1, " ".join(out))
        at = text.find(wanted, at + 1)
