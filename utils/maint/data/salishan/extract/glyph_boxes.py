"""Print the box of every character of one kind on a page, with the font size and its neighbors.

usage: python glyph_boxes.py <stem> <page> <character> [limit]

For a text layer that maps two glyphs to one character: the letter ƛ and the comma above that the
font of wlwlmelst and Janzen's paper also calls ƛ. The two differ in width and in height.
"""
import os
import sys

import pypdfium2 as pdfium
import pypdfium2.raw as pdfium_c

from page_text import CORPUS


def main():
    stem, number, wanted = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    limit = int(sys.argv[4]) if len(sys.argv) > 4 else 40
    document = pdfium.PdfDocument(os.path.join(CORPUS, "papers", stem + ".pdf"))
    textpage = document[number - 1].get_textpage()
    count = textpage.count_chars()
    shown = 0
    for index in range(count):
        if textpage.get_text_range(index, 1) != wanted:
            continue
        left, bottom, right, top = textpage.get_charbox(index)
        size = pdfium_c.FPDFText_GetFontSize(textpage.raw, index) or 10
        before = textpage.get_text_range(max(0, index - 3), min(3, index))
        after = textpage.get_text_range(index + 1, min(3, count - index - 1))
        print("%4d  width %.2f em  bottom %.1f  top %.1f  size %.1f  %s[%s]%s"
              % (index, (right - left) / size, bottom, top, size, before, wanted, after))
        shown += 1
        if shown >= limit:
            break


if __name__ == "__main__":
    main()
