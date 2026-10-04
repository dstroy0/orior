"""The ops of 2009_Miyashita_Chatsis: Mizuki Miyashita and Annabelle Chatsis on building a first
Blackfoot language course at the University of Montana, a linguist and a native speaker together,
against a lack of materials, of a standard dialect, of one orthography and of teacher training.

Table 1 sets three phrases in Frantz's, Holterman's and Weatherwax's writing systems, each phrase's
English wrapping onto a second line under it. Table 2 is a sample conversation, a speaker's letter,
the Blackfoot and its English on each line. Table 3 is the paradigm of isina’si ‘busy’, two forms to
a line, each beside its English. The paper sets its Blackfoot words in italics.
"""
import re
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Blackfoot"
AUTHORS = ["Mizuki Miyashita", "Annabelle Chatsis"]
paper = gen.Paper("2009_Miyashita_Chatsis", authors=" and ".join(AUTHORS), language=LANGUAGE)
NAMES = [("Don Frantz", "thanked"), ("Wade Davies", "thanked"), ("John Douglas", "thanked"),
         ("Leora Bar-el", "thanked"), ("Rosella Many Bears", "thanked"),
         ("Frantz", "Donald G. Frantz, the Blackfoot Grammar (1991) and the orthography the course uses (1978)"),
         ("Lena Russell", "Blackfoot materials for the Kainai tribe (1997)"),
         ("Marvin Weatherwax", "notes with CDs and a DVD for the Blackfeet Community College (2007)"),
         ("Jack Holterman", "an amateur linguist of Browning, Montana, his own writing system"),
         ("Uhlenbeck", "A Concise Blackfoot Grammar (1938)"), ("Taylor", "A Grammar of Blackfoot (1969)"),
         ("Zepeda", "A Tohono O’odham Grammar (1983)"), ("Grenoble", "Lenore Grenoble (2009)")]
LANGUAGES = [(LANGUAGE, "Algonquian, taught at the University of Montana"),
             ("Tohono O’odham", "a Uto-Aztecan language of Southwestern Arizona and Northern Sonora"),
             ("Spanish", "a commonly taught language"), ("English", "one writing system over its dialects"),
             ("Russian", "an uncommonly taught language the authors observed"),
             ("Japanese", "an uncommonly taught language the authors observed")]


def cells(number):
    return [one for one in re.split(r"\s{3,}", paper.spaced[number].strip()) if one]


def systems(start, where):
    """Table 1: the header a note, then each phrase, its English a note joined from its two lines and
    its three definitions cited forms glossed by the phrase and the system."""
    page = paper.page(start)
    header = cells(start)
    paper.add("Table 1", A, "note", paper.text(start), "page %d, the table's header" % page)
    line = start + 1
    while not paper.text(line).startswith("Table 1."):
        row = cells(line)
        english = "%s %s" % (row[0], paper.text(line + 1))
        paper.add("Table 1", A, "note", row[0], "page %d, in italics, the first line of the phrase %s" % (page, english))
        paper.add("Table 1", A, "note", paper.text(line + 1),
                  "page %d, in italics, the second line of the phrase %s" % (page, english))
        for column, form in enumerate(row[1:], 1):
            paper.add("Table 1", L, "cited form", form, "page %d, %s, ‘%s’" % (page, header[column], english))
        line += 2
    paper.add("Table 1", A, "note", paper.text(line), "page %d, the table's caption" % page)
    return line + 1


def conversation(start, where):
    """Table 2: each line a speaker's words and their English, then the caption."""
    page = paper.page(start)
    line, count = start, 0
    while not paper.text(line).startswith("Table 2."):
        speaker, words, english = cells(line)
        count += 1
        here = "Table 2 line %d" % count
        paper.add(here, L, "transcription", words, "page %d, speaker %s" % (page, speaker.rstrip(":")))
        paper.add(here, A, "translation", english, "page %d, in italics" % page)
        line += 1
    paper.add("Table 2", A, "note", paper.text(line), "page %d, the table's caption" % page)
    return line + 1


def paradigm(start, where):
    """Table 3: each form a cited form with its English beside it, then the caption."""
    page = paper.page(start)
    line = start
    while not paper.text(line).startswith("Table 3."):
        row = cells(line)
        for at in range(0, len(row), 2):
            paper.add("Table 3", L, "cited form", row[at], "page %d, ‘%s’, in italics" % (page, row[at + 1]))
            paper.add("Table 3", A, "note", row[at + 1], "page %d, in italics, the English of %s" % (page, row[at]))
        line += 1
    paper.add("Table 3", A, "note", paper.text(line), "page %d, the table's caption" % page)
    return line + 1


def prose(start, where):
    """A paragraph with a line that opens on its list item (iii), which flow() reads as a paragraph
    of its own: the paragraph runs to the line that ends it."""
    last = paper.find(ENDS[start], start)
    body = paper.joined(range(start, last + 1))
    pages = sorted({paper.page(one) for one in range(start, last + 1)})
    paper.add(where, A, "note", body, "page %d" % pages[0])
    paper.cited(where, body, pages)
    paper.mentions(where, body, NAMES, "name")
    paper.mentions(where, body, LANGUAGES, "language")
    return last + 1


ENDS = {paper.find("^There are abundant resources"): r"our own materials for this course\.$",
        paper.find("^We learned several important things"): r"^Blackfoot in their everyday life\.$"}
blocks = {paper.find("^Phrases Frantz Holterman"): systems, paper.find("^A: kitáíkihpa"): conversation,
          paper.find("^nitsísina’si "): paradigm}
blocks.update({start: prose for start in ENDS})
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, references=r"^References:$")
rows = paper.rows
# The university, under both authors' names, and the abstract under it, one note.
lines = [index for index, row in enumerate(rows) if row[0] == "front" and row[2] == "note"
         and row[4] == "page 1, under Annabelle Chatsis"]
rows[lines[0]][4] = "page 1, under both authors"
if len(lines) > 1:
    rows[lines[1]][3] = " ".join(rows[index][3] for index in lines[1:])
    rows[lines[1]][4] = "page 1, the abstract"
    rows = [row for index, row in enumerate(rows) if index not in lines[2:]]
# cited() passes over forms defined in plain letters alone. Each follows the paragraph that sets it,
# after the cited forms cited() found there.
CITED = (("This makes Blackfoot language education", L, "siksikimi", "page 4, in italics, ‘coffee’ in the US"),
         ("A version that is widely used today", L, "ts", "page 4, in italics, an affricate, one symbol "
                                                                "z in Holterman's system"),
         ("A version that is widely used today", L, "z", "page 4, in italics, Holterman's symbol for ts"),
         ("A version that is widely used today", L, "ks", "page 4, in italics, x in Holterman's system"),
         ("A version that is widely used today", L, "x", "page 4, in italics, Holterman's symbol for ks"),
         ("In addition, in commonly taught languages", "English", "pen", "page 5, in italics, pronounced [pen] in "
                                                                         "Standard American English, [pɪn] in the "
                                                                         "Southern dialect"),
         ("In addition, in commonly taught languages", L, "mataki", "page 5, in italics, ‘potato’ in Canada"),
         ("In addition, in commonly taught languages", L, "pataki", "page 5, in italics, ‘potato’ in the US"))
for inside, who, form, gloss in CITED:
    at = next(index for index, row in enumerate(rows) if row[2] == "note" and row[3].startswith(inside))
    while at + 1 < len(rows) and rows[at + 1][2] == "cited form" and rows[at + 1][0] == rows[at][0]:
        at += 1
    rows[at + 1:at + 1] = [[rows[at][0], who, "cited form", form, gloss]]
paper.rows = rows
paper.write()
