# Context for Henry Davis, Central Salish from a Nooksack Perspective. The paper compares sixteen
# grammatical traits across seven Central Salish languages in sixteen tables, each a row of language
# heads over rows of traits; the text layer ran every table into the prose around it. Each table is
# read again from the glyph positions (grid_table.py), a cell under the head its middle stands
# nearest: the cells that are affixes or particles are cited forms of their column's language, and a
# table of marks (√, U+2014, yes, no) is a row a trait with each language's mark in the gloss. The
# examples are Nooksack but for (4) and (8), Upriver Halkomelem, (9), Squamish, (29) and (33),
# nɬeʔkepmxcín, and (30) and (34), St'át'imcets, each named with its source on the page.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from workdir import WORK  # noqa: E402
sys.path.insert(0, HERE)
import grid_table  # noqa: E402

STEM = "ICSNL59_Davis_final"
AUTHORS = "Henry Davis"
NK = "Nooksack"
UH = "Upriver Halkomelem"
HL = "Halkomelem"
SQ = "Squamish"
LU = "Lushootseed"
TH = "nɬeʔkepmxcín"
LI = "St’át’imcets"

TITLE = "Central Salish from a Nooksack Perspective"
BYLINE = "Henry Davis, University of British Columbia"
VOLUME = "59"

with open(os.path.join(WORK, STEM + ".draft.tsv"), encoding="utf-8") as handle:
    DRAFT = [one.rstrip("\n").split("\t") for one in handle][1:]

# The column heads, as §2 and the tables define them.
HEADS = {"LU": LU, "NK": NK, "UH": UH, "NSS": "Northern Straits Salish", "SS": "Straits Salish",
         "SQ": SQ, "SE": "Sechelt", "CX": "Comox"}


def chained(anchor, rows):
    """Each row after the one before it, the first after anchor."""
    out = []
    for row in rows:
        out.append((anchor, row))
        anchor = (row[0], row[3])
    return out


def read(page, bottom, top=None):
    """The heads and cells of the table on page whose head line stands under top."""
    heads, rows = grid_table.rows(STEM, page, "LU", bottom, top)
    return heads, [cells for label, cells in rows]


def forms(number, page, caption, labels, bottom, top=None, fixes=None):
    """A table of affixes or particles: the caption, then a row a trait and a cited form a cell,
    given to the language of the cell's column. A dash is an empty cell."""
    heads, cells = read(page, bottom, top)
    where = "Table %d" % number
    out = [(where, AUTHORS, "note", caption, "page %d, the caption" % page)]
    for label, row in zip(labels, cells):
        out.append((where, AUTHORS, "note", label, "page %d, the head of a row" % page))
        for head, cell in zip(heads, row):
            cell = (fixes or {}).get((label, head), cell)
            if cell in ("", "—", "_—"):
                continue
            kind = "cited affix" if re.search(r"^\(?[-=]|=\)?$", cell) else "cited form"
            out.append(("%s %s" % (where, head), HEADS[head], kind, cell,
                        "page %d, %s, the %s column" % (page, label, head)))
    return out


def marks(number, page, caption, labels, bottom, top=None):
    """A table of marks: the caption, then a row a trait, its form the trait and the marks as the
    page sets them, the gloss naming each language's mark."""
    heads, cells = read(page, bottom, top)
    where = "Table %d" % number
    out = [(where, AUTHORS, "note", caption, "page %d, the caption" % page)]
    for label, row in zip(labels, cells):
        printed = " ".join(cell.replace("_", "") for cell in row)
        out.append((where, AUTHORS, "note", ("%s %s" % (label, printed)).strip(),
                    "page %d, %s" % (page, ", ".join("%s %s" % (head, cell.replace("_", "")) for head, cell in zip(heads, row)))))
    return out


TABLE_1 = marks(1, 5, "Table 1: Imperfective marking across Central Salish",
                ("C1 reduplication", "‘actual’", "auxiliary", "prefix/proclitic"), 330)
TABLE_2 = forms(2, 7, "Table 2: Third person transitive subject marking across Central Salish",
                ("main clause", "subordinate clause", "subjunctive clause", "nominalized clause"), 240,
                fixes={(label, head): None for label in () for head in ()})
TABLE_3 = marks(3, 9, "Table 3: *3 > 2 in Central Salish", ("*3 > 2",), 340)
TABLE_4 = marks(4, 10, "Table 4: Passive in Central Salish", ("non-promotional", "promotional"), 240)
TABLE_5 = marks(5, 11, "Table 5: Distribution of the oblique determiner across Central Salish",
                ("ergative proper noun subjects", "oblique proper noun subjects", "oblique common noun subjects"), 380)
TABLE_6 = forms(6, 12, "Table 6: The general oblique marker/preposition across Central Salish",
                ("oblique marker",), 440)
TABLE_7 = marks(7, 12, "Table 7: Pre-predicative subjects in Central Salish", ("SV(O)",), 200, top=230)
TABLE_8 = marks(8, 13, "Table 8: Distribution of post-verbal DPs in Central Salish", ("",), 490)
TABLE_9 = forms(9, 14, "Table 9: Intransitive Markers across Central Salish",
                ("active intransitive", "developmental", "autonomous", "middle"), 530)
# The relic rows of Table 10 set a raised hyphen inside the parenthesis and a raised y after x; the
# page at 250 dpi prints (-ns), (-ləs), (-aš ~ axy) and (-əxy), and the text layer codes the y on
# the line.
TABLE_10 = forms(10, 15, "Table 10: Transitivizers across Central Salish",
                 ("control", "limited control", "causative", "(transitive)", "(purposive)"), 390,
                 fixes={("(transitive)", "NK"): "(-ns)", ("(transitive)", "UH"): "(-ləs)",
                        ("(purposive)", "NK"): "(-aš ~ axy )", ("(purposive)", "UH"): "(-əxy)",
                        ("(purposive)", "NSS"): "-as ~ -əs", ("(purposive)", "SE"): "(-aš ~ iš)"})
TABLE_11 = forms(11, 17, "Table 11: Applicatives across Central Salish",
                 ("redirective", "relational", "indirective"), 600,
                 fixes={("redirective", "NK"): "-ši-t ~ -xyi-t"})
TABLE_12 = marks(12, 18, "Table 12: Clausal negation across Central Salish", ("Type A", "Type B", "Type B’", "Type C"), 370)
TABLE_13 = marks(13, 19, "Table 13: Invariant and alternating independent pronoun systems in Central Salish", ("",), 400)
TABLE_14 = marks(14, 20, "Table 14: Use of 1st- and 2nd- person independent pronouns as arguments in Central Salish",
                 ("free use of independent pronouns as arguments",), 640)
TABLE_15 = forms(15, 20, "Table 15: Form of the clefting predicate and the 3rd-person independent pronoun in Central Salish",
                 ("clefting particle", "3rd-person independent pronoun"), 200, top=270,
                 fixes={("3rd-person independent pronoun", "NK"): "ƛ̓u", ("3rd-person independent pronoun", "UH"): "ƛ̓a",
                        ("3rd-person independent pronoun", "SQ"): "— (DEM)", ("3rd-person independent pronoun", "SE"): "niɬ (?) (DEM)",
                        ("3rd-person independent pronoun", "CX"): "— (DEM)"})
TABLE_16 = marks(16, 21, "Table 16: Grammatical variation across Central Salish from a NK perspective", (
    "imperfective auxiliary", "oblique proper noun determiner", "no oblique marker",
    "3 ergative suffix in indicative clauses", "no 3 ergative suffix in subjunctive clauses",
    "3 ergative suffix in nominalized clauses", "*3 > 2", "promotional passive", "SV(O) word order",
    "unmarked VOS word order", "use of *-xi-t redirective", "use of -ni-t rather than -min-t",
    "pattern B negation", "alternating independent pronouns", "free use of independent pronouns as arguments",
    "use of ƛ̓u/ƛ̓a as clefting predicate"), 200)
TABLES = (TABLE_1, TABLE_2, TABLE_3, TABLE_4, TABLE_5, TABLE_6, TABLE_7, TABLE_8, TABLE_9, TABLE_10,
          TABLE_11, TABLE_12, TABLE_13, TABLE_14, TABLE_15, TABLE_16)

# Figure 1, the family tree of Central Salish, a node a row with its branch in the gloss.
FIGURE = [("Figure 1", AUTHORS, "note", "Figure 1: Central Salish", "page 3, the caption of the tree")]
for _node, _under in (("*Proto-Central Salish", "the root, carrying footnote 3"),
                      ("North Georgia", "under *Proto-Central Salish"), ("South Georgia", "under *Proto-Central Salish"),
                      ("Puget", "under *Proto-Central Salish"), ("Comox-Sliammon", "under North Georgia"),
                      ("Pentlatch", "under North Georgia"), ("Sechelt", "under North Georgia"),
                      ("Squamish", "under South Georgia"), ("Nooksack", "under South Georgia, in bold italics"),
                      ("Halkomelem", "under South Georgia, with Straits"), ("Straits", "under South Georgia, with Halkomelem"),
                      ("Northern Straits", "under Straits"), ("Klallam", "under Straits"),
                      ("Lushootseed", "under Puget"), ("Twana", "under Puget")):
    FIGURE.append(("Figure 1", AUTHORS, "language", _node, "page 3, a node of the tree, %s" % _under))

