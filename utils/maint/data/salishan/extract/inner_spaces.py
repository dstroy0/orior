"""List the spaces a page text sets inside a word, with the glyph gap at each, for CORRECTIONS.

usage: python inner_spaces.py <stem> [max gap in em, default 0.1]

For each page, each pair of neighboring page-text tokens with a letter outside ASCII in either is
looked up on the glyph line holding them both: where the gap between the last glyph of the first
and the first glyph of the second is under the threshold, the page sets them as one word and the
space is the text layer's. Prints the spaced and the joined text with the gap, once per distinct
pair, for a person to check before it goes into the CORRECTIONS of the paper's table file.
"""
import os
import re
import sys

import pypdfium2 as pdfium
import pypdfium2.raw as pdfium_c

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from workdir import CORPUS, PRIVATE  # noqa: E402
import residue  # noqa: E402

stem = sys.argv[1]
limit = float(sys.argv[2]) if len(sys.argv) > 2 else 0.1
repair = residue.paper_repair(stem)
with open(os.path.join(PRIVATE, "pagetext", stem + ".txt"), encoding="utf-8") as handle:
    pages = re.split(r"===== page \d+ =====", handle.read())[1:]
document = pdfium.PdfDocument(os.path.join(CORPUS, "papers", stem + ".pdf"))
seen = set()
for number, text in enumerate(pages, 1):
    textpage = document[number - 1].get_textpage()
    # The glyph stream of the page with each glyph's box and font size, marks and spaces left out.
    glyphs = []
    for index in range(textpage.count_chars()):
        symbol = textpage.get_text_range(index, 1)
        if not symbol or symbol.isspace() or symbol in ("\r", "\n", "￾"):
            continue
        glyphs.append((symbol, textpage.get_charbox(index), pdfium_c.FPDFText_GetFontSize(textpage.raw, index) or 10))
    stream = "".join(one[0] for one in glyphs)
    for line in text.split("\n"):
        tokens = repair(line).split()
        for first, second in zip(tokens, tokens[1:]):
            if all(ord(one) < 128 for one in first + second):
                continue
            joined = first + second
            found = stream.find(joined)
            if found < 0:
                continue
            last = glyphs[found + len(first) - 1]
            nxt = glyphs[found + len(first)]
            # A combining mark has no box of its own worth measuring; step back to its letter.
            back = found + len(first) - 1
            while 0x0300 <= ord(glyphs[back][0][0]) <= 0x036F and back > found:
                back -= 1
            gap = (nxt[1][0] - glyphs[back][1][2]) / nxt[2]
            if abs(nxt[1][1] - glyphs[back][1][1]) > 0.5 * nxt[2]:
                continue
            key = (first, second)
            if gap < limit and key not in seen:
                seen.add(key)
                print("p%-3d %6.3f  %s %s  ->  %s" % (number, gap, first, second, joined))
