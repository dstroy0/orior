"""Print the gap before each glyph of a text-layer run, in em, for finding the word spaces a
respacing missed.

usage: python gap_probe.py <stem> <string>
"""
import os
import sys

import pypdfium2 as pdfium
import pypdfium2.raw as pdfium_c

import page_text

stem, wanted = sys.argv[1], sys.argv[2]
document = pdfium.PdfDocument(os.path.join(page_text.CORPUS, "papers", stem + ".pdf"))
print("calibrated share %.3f" % page_text.calibrated_share(document))
for number in range(len(document)):
    textpage = document[number].get_textpage()
    text = textpage.get_text_range()
    at = text.find(wanted)
    if at < 0:
        continue
    last = None
    out = []
    for index in range(at, at + len(wanted)):
        symbol = textpage.get_text_range(index, 1)
        box = textpage.get_charbox(index)
        size = pdfium_c.FPDFText_GetFontSize(textpage.raw, index) or 10
        gap = (box[0] - last[2]) / size if last else 0
        out.append("%s%.2f" % (symbol, gap))
        last = box
    print(number + 1, " ".join(out))
