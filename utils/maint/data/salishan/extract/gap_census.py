"""The glyph gaps of a paper inside the text layer's words and at its spaces, in em.

usage: python gap_census.py <stem>

Two neighboring glyphs on one baseline with no space between them in the text layer are one word,
and the gap between their boxes is a gap inside a word. With a space between them, the gap is a word
space, or a space the layer inserted inside a word. Prints percentiles of each, the author's own
setting of the page, from which a word-space threshold for this paper can be read.
"""
import os
import sys

import pypdfium2 as pdfium
import pypdfium2.raw as pdfium_c

from page_text import CORPUS


def gaps(document):
    inside, spaced = [], []
    for number in range(len(document)):
        textpage = document[number].get_textpage()
        last = None
        space = False
        for index in range(textpage.count_chars()):
            symbol = textpage.get_text_range(index, 1)
            if symbol in ("\r", "\n"):
                last, space = None, False
                continue
            if symbol == " ":
                space = True
                continue
            if not symbol or not symbol.isalpha():
                last, space = None, False
                continue
            box = textpage.get_charbox(index)
            size = pdfium_c.FPDFText_GetFontSize(textpage.raw, index) or 10
            if last is not None and abs(box[1] - last[1]) < 0.3 * size and box[0] >= last[0]:
                gap = (box[0] - last[2]) / size
                (spaced if space else inside).append(gap)
            last, space = box, False
    return inside, spaced


def percentile(values, share):
    values = sorted(values)
    return values[min(len(values) - 1, int(share * len(values)))] if values else float("nan")


def main():
    document = pdfium.PdfDocument(os.path.join(CORPUS, "papers", sys.argv[1] + ".pdf"))
    inside, spaced = gaps(document)
    print("inside words %d gaps: p50 %.3f p99 %.3f p99.9 %.3f max %.3f"
          % (len(inside), percentile(inside, 0.5), percentile(inside, 0.99),
             percentile(inside, 0.999), max(inside)))
    print("at spaces    %d gaps: p1 %.3f p5 %.3f p10 %.3f p50 %.3f"
          % (len(spaced), percentile(spaced, 0.01), percentile(spaced, 0.05),
             percentile(spaced, 0.10), percentile(spaced, 0.5)))
    edges = [0.05 + 0.01 * step for step in range(30)]
    for edge in edges:
        above = sum(1 for one in inside if one > edge)
        below = sum(1 for one in spaced if one <= edge)
        print("  %.2f  inside above %4d  spaces at or below %4d" % (edge, above, below))


if __name__ == "__main__":
    main()
