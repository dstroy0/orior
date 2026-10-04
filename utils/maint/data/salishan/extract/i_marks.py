"""Measure the ink over each i on a page: the dot of i against the acute of í.

usage: python i_marks.py <stem> <page>

wlwlmelst and Janzen's font maps í to i and gives both the same box. The page still prints one with
a round dot and the other with a slanted stroke. Prints, for each i, the dark pixels above its
x-height and how wide and tall they spread, at 600 dpi, with the word around it.
"""
import os
import sys

import pypdfium2 as pdfium
import pypdfium2.raw as pdfium_c

from page_text import CORPUS

DPI = 600


def ink_over(image, box, size, page_height, scale):
    """Dark pixels over an i's x-height, as (count, width, height) in pixels."""
    left, bottom, right, top = box
    # The x-height sits about 0.47 em over the baseline; the mark is above 0.55 em.
    x0 = int(left * scale)
    x1 = int(right * scale)
    y0 = int((page_height - top - 0.05 * size) * scale)
    y1 = int((page_height - (bottom + 0.55 * size)) * scale)
    region = image.crop((x0, y0, x1, y1))
    pixels = region.load()
    dark = [(x, y) for x in range(region.width) for y in range(region.height) if pixels[x, y] < 128]
    if not dark:
        return 0, 0, 0
    xs = [one[0] for one in dark]
    ys = [one[1] for one in dark]
    return len(dark), max(xs) - min(xs) + 1, max(ys) - min(ys) + 1


def main():
    stem, number = sys.argv[1], int(sys.argv[2])
    document = pdfium.PdfDocument(os.path.join(CORPUS, "papers", stem + ".pdf"))
    page = document[number - 1]
    scale = DPI / 72.0
    image = page.render(scale=scale).to_pil().convert("L")
    page_height = page.get_height()
    textpage = page.get_textpage()
    count = textpage.count_chars()
    for index in range(count):
        if textpage.get_text_range(index, 1) != "i":
            continue
        box = textpage.get_charbox(index)
        size = pdfium_c.FPDFText_GetFontSize(textpage.raw, index) or 10
        ink, wide, tall = ink_over(image, box, size, page_height, scale)
        around = textpage.get_text_range(max(0, index - 4), min(4, index)) + "[i]" + \
            textpage.get_text_range(index + 1, min(4, count - index - 1))
        print("%5d  ink %4d  wide %3d  tall %3d  %s" % (index, ink, wide, tall,
                                                         " ".join(around.split())))


if __name__ == "__main__":
    main()
