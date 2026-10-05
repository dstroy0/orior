"""Print the words of a page with their x-positions, one printed line to a row.

usage: python table_cells.py <stem> <page> [first words of a line]

A table's text layer runs its cells together with spaces that do not keep the columns: the
violation marks of an OT tableau come out as *! * with nothing to say which constraint each falls
under. The glyph boxes do keep them. word_lines() gives each printed line of a page as a list of
(left, right, word), and column_of() puts a word under the column head whose middle is nearest.
With first words, only the lines from the one opening on them to the next blank gap are printed.
"""
import os
import sys

import pypdfium2 as pdfium
import pypdfium2.raw as pdfium_c

from workdir import CORPUS  # noqa: E402

# Two glyphs on one line are two words where the gap between their boxes is wider than this share
# of the font size. The cells of a tableau stand an em and more apart.
WORD_GAP = 0.3


def word_lines(stem, page):
    """The printed lines of a page, top to bottom, each a list of (left, right, word)."""
    document = pdfium.PdfDocument(os.path.join(CORPUS, "papers", stem + ".pdf"))
    textpage = document[page - 1].get_textpage()
    glyphs = []
    for index in range(textpage.count_chars()):
        symbol = textpage.get_text_range(index, 1)
        if not symbol or symbol in ("\r", "\n", " ", "￾"):
            continue
        box = textpage.get_charbox(index)
        size = pdfium_c.FPDFText_GetFontSize(textpage.raw, index) or 10
        glyphs.append((box, size, symbol))
    # A line is the glyphs whose baselines stand within a third of the font size of each other. A
    # combining mark joins the line of the letter before it wherever its own box sits.
    lines = []
    for box, size, symbol in glyphs:
        combining = 0x0300 <= ord(symbol) <= 0x036F
        if combining and lines:
            lines[-1][1].append((box, size, symbol, True))
            continue
        for baseline, members in lines:
            if abs(baseline - box[1]) < 0.35 * size:
                members.append((box, size, symbol, False))
                break
        else:
            lines.append((box[1], [(box, size, symbol, False)]))
    out = []
    for baseline, members in sorted(lines, key=lambda one: -one[0]):
        # The stream order keeps a mark after its letter, and the letters sort by x and each mark
        # goes after the letter it followed in the stream.
        ordered = []
        for box, size, symbol, combining in members:
            if combining and ordered:
                ordered[-1][2] += symbol
            else:
                ordered.append([box, size, symbol])
        ordered.sort(key=lambda one: one[0][0])
        words = []
        for box, size, symbol in ordered:
            if words and box[0] - words[-1][1] <= WORD_GAP * size:
                words[-1][1] = max(words[-1][1], box[2])
                words[-1][2] += symbol
            else:
                words.append([box[0], box[2], symbol])
        out.append((baseline, [tuple(word) for word in words]))
    return out


def column_of(word, heads):
    """The index of the head, a (left, right, text), whose middle stands nearest the word's."""
    middle = (word[0] + word[1]) / 2
    return min(range(len(heads)), key=lambda at: abs((heads[at][0] + heads[at][1]) / 2 - middle))


if __name__ == "__main__":
    stem, page = sys.argv[1], int(sys.argv[2])
    opening = sys.argv[3] if len(sys.argv) > 3 else None
    printing = opening is None
    for baseline, words in word_lines(stem, page):
        text = " ".join(word[2] for word in words)
        if opening and text.startswith(opening):
            printing = True
        if printing:
            print("%6.1f  %s" % (baseline, "  ".join("%s@%.0f-%.0f" % (w[2], w[0], w[1]) for w in words)))
