"""The ops of 2011_Matheson: Andrew Matheson on universal and existential quantification in
Ktunaxa, a quantifier on the subject an adjunct to its DP and a quantifier preverb heading its own
projection over the VP and the object, with Glougie's Blackfoot preverbal quantifiers alongside.

The examples set a segmented line over its gloss and a translation in quotes; (6) and (7) open on a
caption, Mass and Count, and (12) and (13) are Blackfoot, from Glougie (2000). Tables A to D set
Ktunaxa forms in columns beside their English: each form is a cited form with its row's English and
column in its gloss, and each table's caption and header a note. Trees A and B are drawings; their
captions are notes.
"""
import re
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Ktunaxa"
AUTHORS = ["Andrew Matheson"]
paper = gen.Paper("2011_Matheson", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Dryer", "Matthew Dryer, preverbs in Kutenai and Algonquian (2002)"),
         ("Glougie", "Jennifer R. S. Glougie, Blackfoot quantifiers (2000)"), ("Ladd", "Robert D. Ladd (2008)"),
         ("Vi Birdstone", "the author's consultant"), ("Martina Wiltschko", "thanked for guidance and support")]
LANGUAGES = [(LANGUAGE, "a language isolate of the Kootenay Mountains, British Columbia"),
             ("Blackfoot", "Algonquian, Glougie's preverbal quantifiers"),
             ("Algonquian", "the family east of Ktunaxa"), ("Salish", "the family west of Ktunaxa"),
             ("English", "a mass and count distinction like Ktunaxa's")]


def cells(number):
    return [one for one in re.split(r"\s{3,}", paper.spaced[number].strip()) if one]


def table(start, where):
    """A table: its caption and header notes, then each row's Ktunaxa forms cited forms, glossed by
    the row's English and the column each stands in. Table C sets its six columns' English on the
    line over the forms, under its two headers, Mass and Count."""
    name = paper.text(start)
    page = paper.page(start)
    paper.add(name, A, "note", name, "page %d, the table's caption" % page)
    header = cells(start + 1)
    paper.add(name, A, "note", paper.text(start + 1), "page %d, the table's header" % page)
    line = start + 2
    if name == "Table C":
        english = cells(line)
        paper.add(name, A, "note", paper.text(line), "page %d, the English of each column" % page)
        forms = cells(line + 1)
        for column, (form, gloss) in enumerate(zip(forms, english)):
            paper.add(name, L, "cited form", form, "page %d, %s, %s" % (page, header[column // 3], gloss))
        return line + 2
    while line <= paper.last and not paper.lines[line][2] and len(cells(line)) == len(header):
        row = cells(line)
        for column, form in enumerate(row[:-1]):
            paper.add(name, L, "cited form", form, "page %d, %s, %s" % (page, header[column], row[-1]))
        paper.add(name, A, "note", row[-1], "page %d, the English of the row" % page)
        line += 1
    return line


def tree(start, where):
    paper.add(paper.text(start), A, "note", paper.text(start), "page %d, the caption of a tree drawn as a picture"
              % paper.page(start))
    return start + 1


def captioned(start, where):
    """(6) and (7): the caption, Mass or Count, a note, then the segmented lines and their glosses in
    turn to the translation."""
    label, caption = gen.EXAMPLE.match(paper.text(start)).groups()
    paper.add("(%s)" % label, A, "note", caption, "page %d, the caption" % paper.page(start))
    line, count = start + 1, 0
    while True:
        text = paper.text(line)
        count += 1
        if text.startswith("‘"):
            paper.add("(%s) line %d" % (label, count), A, "translation", text, "page %d" % paper.page(line))
            return line + 1
        paper.add("(%s) line %d" % (label, count), L, "gloss" if count % 2 == 0 else "transcription", text,
                  "page %d" % paper.page(line))
        line += 1


blocks = {paper.find("^Table %s$" % letter): table for letter in "ABCD"}
blocks.update({179: tree, 192: tree, 110: captioned, 114: captioned})
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)
# The abstract, under the university, is one note.
lines = [index for index, row in enumerate(paper.rows) if row[0] == "front" and row[2] == "note"
         and row[4] == "page 1, under Andrew Matheson"][1:]
if lines:
    paper.rows[lines[0]][3] = " ".join(paper.rows[index][3] for index in lines)
    paper.rows[lines[0]][4] = "page 1, the abstract"
    paper.rows = [row for index, row in enumerate(paper.rows) if index not in lines[1:]]
for row in paper.rows:
    # (1) wraps its sentence onto a second pair of lines; the wrapped half is the same tier as the
    # first. (12) and (13) are Glougie's Blackfoot.
    if row[0] == "(1) line 3":
        row[2] = "transcription"
    if re.match(r"\(1[23]\) line", row[0]) and row[1] == L:
        row[1] = "Blackfoot"
# Table D sets two verbal forms in one cell, asniɬ, (xaȼniɬ), under two (both): each is its own row.
at = next(index for index, row in enumerate(paper.rows) if row[3] == "asniɬ, (xaȼniɬ)")
paper.rows[at][3], paper.rows[at][4] = "asniɬ", "page 4, VERBAL MOD, two"
paper.rows[at + 1:at + 1] = [paper.rows[at][:3] + ["xaȼniɬ", "page 4, VERBAL MOD, (both), set in parentheses"]]
paper.write()
