"""Print the gap before each glyph of every occurrence of a string on a page, in em.

usage: python gap_of.py <stem> <page> <string>

For a word the glyph rows glue, ofthe for of the: the gap between f and t shows whether the page
leaves a word space there that the share in row_lines misses.
"""
import os
import sys

import pypdfium2 as pdfium
import pypdfium2.raw as pdfium_c

from page_text import glyph_size, paper_document

sys.stdout.reconfigure(encoding="utf-8")


def main():
    stem, number, wanted = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    # The paper as page_text reads it, its fonts mended or mapped, and a ciphered face reads as letters.
    document = paper_document(stem)[0]
    textpage = document[number - 1].get_textpage()
    glyphs = []
    for index in range(textpage.count_chars()):
        symbol = textpage.get_text_range(index, 1)
        if not symbol or symbol in ("\r", "\n", " "):
            continue
        box = textpage.get_charbox(index)
        size = glyph_size(textpage, index) or 10
        glyphs.append((symbol, box, size))
    text = "".join(one[0] for one in glyphs)
    start = text.find(wanted)
    while start >= 0:
        cells = []
        for at in range(max(1, start), start + len(wanted)):
            symbol, box, size = glyphs[at]
            gap = (box[0] - glyphs[at - 1][1][2]) / size
            cells.append("%s %+.2f" % (symbol, gap))
        print("  ".join(cells))
        start = text.find(wanted, start + 1)


if __name__ == "__main__":
    main()
