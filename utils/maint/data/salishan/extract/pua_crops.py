"""Crop the first occurrences of each private-use glyph in a paper out of a 600 dpi render.

usage: python pua_crops.py <stem> <out dir> [per glyph]

A font can map a letter it draws to the private use area, U+F729 for a glottalized letter, and a
text layer then drops it. Each crop holds the glyph and the word around it, named for its code
point and page, and the pdfium text around it is printed beside the file name, for reading the
glyph by eye and writing the page's letter back.
"""
import os
import sys

import pypdfium2 as pdfium

from page_text import CORPUS


def main():
    stem, out = sys.argv[1], sys.argv[2]
    per_glyph = int(sys.argv[3]) if len(sys.argv) > 3 else 2
    os.makedirs(out, exist_ok=True)
    scale = 600 / 72.0
    document = pdfium.PdfDocument(os.path.join(CORPUS, "papers", stem + ".pdf"))
    taken = {}
    for number in range(len(document)):
        page = document[number]
        height = page.get_height()
        textpage = page.get_textpage()
        text = textpage.get_text_range()
        image = None
        for index, symbol in enumerate(text):
            if not 0xE000 <= ord(symbol) <= 0xF8FF or taken.get(symbol, 0) >= per_glyph:
                continue
            taken[symbol] = taken.get(symbol, 0) + 1
            if image is None:
                image = page.render(scale=scale).to_pil()
            # The word around the glyph, to the spaces or line ends on either side.
            start = index
            while start > 0 and not text[start - 1].isspace():
                start -= 1
            end = index
            while end < len(text) and not text[end].isspace():
                end += 1
            boxes = [textpage.get_charbox(at) for at in range(start, end)]
            boxes = [box for box in boxes if box[2] > box[0]]
            left = min(box[0] for box in boxes)
            bottom = min(box[1] for box in boxes)
            right = max(box[2] for box in boxes)
            top = max(box[3] for box in boxes)
            crop = image.crop((int(max(0, left - 20) * scale), int((height - top - 6) * scale),
                               int((right + 20) * scale), int((height - bottom + 6) * scale)))
            name = os.path.join(out, "u%04X_p%d_%d.png" % (ord(symbol), number + 1, taken[symbol]))
            crop.save(name)
            print("%s  %s" % (name, ascii(text[start:end])))


if __name__ == "__main__":
    main()
