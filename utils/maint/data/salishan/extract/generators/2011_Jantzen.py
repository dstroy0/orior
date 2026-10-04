"""The ops of 2011_Jantzen: Jonathan Janzen's Phrase- and word-level prosody in Kwak̓wala. The enclitics
of case, location, determiners, visibility and tense lean on the root before them for stress while
they stand in the syntactic constituent of the root after them; a pause falls at the syntactic break
without resetting pitch or intensity on the enclitic; and morphemes Boas took for suffixes, gánəm and
kás-dzi, carry stress of their own and are words.

Page text read by glyph rows; page_text's PRIVATE_USE reads the glottalized letters of the Aboriginal
Serif codes, and PAPER_IMAGES the ƛ̓ drawn as images on pages 4 and 5.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Kwak̓wala"
AUTHORS = ["Jonathan Janzen"]
paper = gen.Paper("2011_Jantzen", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Henry Davis", "taught the 2010 Field Methods course at the University of British Columbia"),
         ("Boas", "Franz Boas, the Kwakiutl grammar (1947)"),
         ("Ladd", "Robert D. Ladd, Intonational Phonology (2008)"),
         ("Chung", "Yunhee Chung, the Kwak'wala nominal domain (2007)"),
         ("Anderson", "Stephen R. Anderson, Kwakwala syntax and Government-Binding theory (1984)")]
LANGUAGES = [(LANGUAGE, "Wakashan, North Wakashan branch, the language of the paper"),
             ("Kwak’wala", "Kwak̓wala, as the title spells it")]
FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
RUNNING = paper.running_numbers_set()


def printed(number):
    return bool(paper.text(number)) and not paper.lines[number][2] and number not in RUNNING \
        and number not in FOOT


def after(number):
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def example(first, where):
    """An example of the transcription, its constituents in brackets, over its gloss, the two
    wrapping to a second pair of lines in (1), (5) to (7), (9) and (20), and the quoted translation
    closing it."""
    label, text = gen.EXAMPLE.match(paper.text(first)).groups()
    line, count = first, 0
    while True:
        count += 1
        here = "(%s) line %d" % (label, count)
        if text.startswith("‘"):
            paper.add(here, A, "translation", text, "page %d" % paper.page(line))
            return after(line)
        if count % 2:
            paper.add(here, L, "transcription", text, "page %d, the constituents in brackets" % paper.page(line))
        else:
            paper.add(here, A, "gloss", text, "page %d" % paper.page(line))
        line = after(line)
        text = paper.text(line)


def syllables(first, where):
    """The starred parse of (14)'s kás-dzi -χa bəgʷánəm with stress on the enclitic, its feet in
    round brackets over a line of its syllables."""
    paper.add("display line 1", L, "transcription", paper.text(first),
              "page %d, the starred parse, the enclitic stressed" % paper.page(first))
    paper.add("display line 2", A, "note", paper.text(first + 1), "page %d, its syllables" % paper.page(first + 1))
    return after(first + 1)


BLOCKS = {}
at = 1
for label in range(1, 21):
    at = paper.find(r"^\(%d\)\s" % label, at)
    BLOCKS[at] = example
    at += 1
BLOCKS[paper.find(r"^\*\(kás")] = syllables
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS, appendix=r"^Jonathan Janzen$")
tail = paper.find(r"^Jonathan Janzen$", paper.find(r"^References$"))
for number in range(tail, paper.last + 1):
    if printed(number):
        paper.add("end", A, "note", paper.text(number), "page %d, the author's address, under the references" %
                  paper.page(number))
# Footnote 1's mark stands on the heading of section 5, Prosodic Words1; the note follows the heading.
for row in paper.rows:
    if row[2] == "heading" and row[3] == "5 Prosodic Words1":
        row[3] = "5 Prosodic Words"
        row[4] += ", carries footnote 1"
# Chung (2007)'s entry runs on to a line after its editors, Vancouver, BC:, which the reference reader
# takes for an entry; every entry opens on a surname and a comma.
joined = []
for row in paper.rows:
    if row[2] == "reference" and not re.match(r"^[A-Z][\w’'\-]+, [A-Z][a-z.]", row[3]) and joined[-1][2] == "reference":
        joined[-1][3] += " " + row[3]
    else:
        joined.append(row)
paper.rows = joined
paper.write()
