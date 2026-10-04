"""The ops of 03_ICSNL55_HDavis2_revised_final: Henry Davis on infinitives and raising in
St'át'imcets, the approximative predicate c̓íla, movement raising in infinitives and copy raising in
nominalized clauses.

The text layer holds each page as one line; the page text is read by glyph rows (page_text.py
rows). The examples set the sentence segmented over its gloss (opening = "segmentation"). The two
trees (45) and (46) are written a row a printed line, and Table 1 a row a line.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
paper = gen.Paper("03_ICSNL55_HDavis2_revised_final", authors="Henry Davis", language="St’át’imcets")
paper.opening = "segmentation"

NAMES = [("Carl Alexander", "Qway7án’ak, St'át'imcets speaker thanked on the title"),
         ("Michael Rochemont", "honored by the Memorial Workshop at UBC")]
LANGUAGES = [("St’át’imcets", "Northern Interior Salish (Lillooet), ISO 639-3 lil"),
             ("Nɬeʔkepmxcín", "Northern Interior Salish (Thompson River Salish)"),
             ("nɬeʔkepmxcín", "Thompson, as printed in lowercase"), ("Squamish", "Central Salish, Kroeber's example (1)"),
             ("English", "compared for raising and like"), ("Halkomelem", "Thompson 2012 on nominalization")]


def table_1(start, where):
    here = "Table 1"
    paper.add(here, A, "note", paper.text(start), "page %d, the table's caption" % paper.page(start))
    heads = ["the column heads", "finite complements: non-raising, copy raising, movement raising",
             "infinitival complements: non-raising, copy raising, movement raising"]
    for offset, why in enumerate(heads, 1):
        paper.add("%s line %d" % (here, offset + 1), A, "note", paper.text(start + offset),
                  "page %d, %s" % (paper.page(start), why))
    return start + 4


blocks = {paper.find(r"^Table 1: Patterns"): table_1}
paper.standard(["Henry Davis"], NAMES, LANGUAGES, front_languages=2,
               displays={"45": (r"^Now, let us turn", True), "46": (r"^The main claim", True)}, blocks=blocks)
# Footnote 10's (i) and (ii) are English, the comparison with seem like; (1) is Kroeber's Squamish
# example, from Kuipers.
for row in paper.rows:
    if row[0].startswith("footnote 10 (") and row[1] == gen.L:
        row[1] = A
    if row[0].startswith("(1) line") and row[1] == gen.L:
        row[1] = "Squamish"
paper.write()
