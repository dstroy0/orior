"""The ops of 2010_vanEijk: Jan P. van Eijk's ‘Cherchez la femme:’ A Lillooet lexical treasure hunt,
the derivations of the two Lillooet words for ‘woman’, northern s.múlhats and southern s.yáqtsa7.

The paper sets its Lillooet words in the practical orthography and in italics, and gen reads each as
a cited form. Section 2 is a list of entries, each opening on its forms and running on as a
paragraph. The author's name and address close the paper under the references.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
AUTHORS = ["Jan P. van Eijk"]
paper = gen.Paper("2010_vanEijk", authors=AUTHORS[0], language="Lillooet")
NAMES = [("Kuipers", "Aert H. Kuipers, the Shuswap word-list (1975) and the Salish Etymological Dictionary (2002)"),
         ("Timmers", "Jan A. Timmers, the Sechelt word-list (1977)"),
         ("Thompson and Thompson", "Laurence C. and M. Terry Thompson, the Thompson River Salish Dictionary (1996)"),
         ("Turner, Thompson, Thompson, and York", "Thompson Ethnobotany (1990)"),
         ("Sonja", "the author's wife"), ("Annigje van Eijk – van der Wilt", "the author's mother"),
         ("Angeline van Leeuwen – de Jong", "the author's mother-in-law")]
LANGUAGES = [("Lillooet", "Northern Interior Salish, St’át’imc"), ("St’át’imc", "Lillooet, in its own name"),
             ("Sechelt", "Central Salish, s.yáqcuw ‘wife’"), ("Proto-Salish", "the source of s.múlhats"),
             ("Thompson", "Northern Interior Salish, Nlaka’pamux"), ("Nlaka’pamux", "Thompson, in its own name"),
             ("Tillamook", "Tillamook Salish, shares s.múlhats"), ("Shuswap", "Northern Interior Salish, núxwenxw")]
paper.standard(AUTHORS, NAMES, LANGUAGES)
# The title carries footnote 1 on its last word, hunt1, and place_footnotes finds no mark for it.
title = next(row for row in paper.rows if row[2] == "title")
title[3], title[4] = title[3][:-1], "page 1, carries footnote 1"
rows, paper.rows = paper.rows, []
paper.footnote("1", list(range(34, 39)), 1, names=NAMES, languages=LANGUAGES)
at = rows.index(title) + 1
rows[at:at] = paper.rows
# The abstract, three lines under the university, is one note.
lines = [index for index, row in enumerate(rows) if row[0] == "front" and row[2] == "note"
         and row[4] == "page 1, under Jan P. van Eijk"][1:]
rows[lines[0]][3] = " ".join(rows[index][3] for index in lines)
rows[lines[0]][4] = "page 1, the abstract"
rows = [row for index, row in enumerate(rows) if index not in lines[1:]]
# The reference reader runs the entries a ditto dash opens, U+2014 U+2014 ., into the one before them, and parts
# Turner et al. (1990) at its first line's end. Each entry is its printed lines; the author's name
# and address under them close the paper, a note.
ENTRIES = [(149, 150), (151, 152), (153, 155), (156, 157), (158, 160), (161, 161), (162, 163), (164, 167),
           (168, 168)]
rows = [row for row in rows if row[2] != "reference"]
for first, last in ENTRIES:
    rows.append(["references", A, "reference", paper.joined(range(first, last + 1)), "page 4"])
rows.append(["references", A, "note", paper.joined([169, 170]), "page 4, the author's name and address"])
# cited() ends a gloss at the first closing quote, and an apostrophe inside it, man’s, reads as one.
# These glosses are given whole.
GLOSSES = {"yaqts7-áw’s": "‘man’s female relatives.’", "qaycw-áw’s": "‘woman’s male relatives.’",
           "yaqts7-án-tsut": "‘to do s.t. like a woman (i.e., a woman doing a man’s job, but not being good at it).’",
           "k’uk’wm’it-án-tsut": "‘to act like a child (s.k’úk’wm’it),’",
           "n.múlhats-cen": "‘leafstalk of hákwa7 ‘cow-parsnip’ (“Indian rhubarb”).’"}
for row in rows:
    if row[2] == "cited form" and row[3] in GLOSSES:
        row[4] = "page %s, in italics, %s" % (row[4].split(",")[0].split()[1], GLOSSES[row[3]])
# cited() passes over s.qaycw, defined in plain letters alone, and over the two forms the page breaks
# at a hyphen, whose italic runs hold a U+FFFE where the line ends. Each follows the form before it.
for after, form, page, gloss in (("n.s.yáqtsa7", "s.qaycw", 2, "‘(1) man, (2) woman’s brother, nephew or male cousin’"),
                                 ("yaqca7-mánst", "nexw-nexw-mánst", 2, ""),
                                 ("-tsut", "qaycw-án-tsut", 3, "‘to do s.t. like a man (i.e., a man doing a "
                                                               "woman’s job, but not being good at it),’")):
    at = next(index for index, row in enumerate(rows) if row[2] == "cited form" and row[3] == after)
    rows[at + 1:at + 1] = [[rows[at][0], rows[at][1], "cited form", form,
                            "page %d, in italics%s" % (page, ", " + gloss if gloss else "")]]
paper.rows = rows
paper.write()
