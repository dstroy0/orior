"""Print, for each space character around a word in the text layer, whether pdfium generated it
or the page's content stream holds it.

usage: python space_kind.py <stem> <word>
"""
import os
import sys

import pypdfium2 as pdfium
import pypdfium2.raw as pdfium_c

import page_text

stem, wanted = sys.argv[1], sys.argv[2]
document = pdfium.PdfDocument(os.path.join(page_text.CORPUS, "papers", stem + ".pdf"))
for number in range(len(document)):
    textpage = document[number].get_textpage()
    text = textpage.get_text_range()
    at = text.find(wanted)
    while at >= 0:
        out = []
        for index in range(max(0, at - 12), min(len(text), at + len(wanted) + 12)):
            symbol = textpage.get_text_range(index, 1)
            if symbol == " ":
                out.append("[gen]" if pdfium_c.FPDFText_IsGenerated(textpage.raw, index) == 1 else "[real]")
            else:
                out.append(symbol)
        print(number + 1, "".join(out))
        at = text.find(wanted, at + 1)
