"""Print the raw text layer codes around each hit of a string, page by page.

usage: python raw_near.py <stem> <string> [width]
"""
import os
import re
import sys

import pypdfium2 as pdfium

from page_text import CORPUS

stem, wanted = sys.argv[1], sys.argv[2]
width = int(sys.argv[3]) if len(sys.argv) > 3 else 6
document = pdfium.PdfDocument(os.path.join(CORPUS, "papers", stem + ".pdf"))
for number in range(len(document)):
    text = document[number].get_textpage().get_text_range()
    for hit in re.finditer(re.escape(wanted), text):
        near = text[hit.start():hit.end() + width]
        print(number + 1, hit.start(), repr(near), " ".join("%04X" % ord(one) for one in near))
