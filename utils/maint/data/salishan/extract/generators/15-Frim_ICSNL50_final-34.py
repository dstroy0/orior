"""The ops of 15-Frim_ICSNL50_final-34: Daniel J. Frim's morphemically glossed excerpt of "Star Story",
a Kwak’wala narrative George Hunt recorded and Boas published (1943; translated 1935a), in which a sea
otter drags hunters into the sky, where they become the Pleiades and Orion.

The introduction's examples (1) and (2), from Boas (1947:272), set each word group as a transcription
over its segmentation and gloss; a quoted gloss in bold under a word group, with its source, is a note
and a citation, and the quoted sentence that closes the example is its translation. The key to the
tiers is a note to its heading and to each tier it names.

The text is 80 units, each numbered and set in seven lettered tiers, all a unit's rows under "2 N":
a. the text in Boas's orthography and b. in NAPA are transcriptions, c. the segmentation, d. and e.
glosses (e. word by word), f. Boas's translation (1935a) and g. Frim's modified one. A tier the page
wraps runs on in its row; e. sets its words in columns and a column's second line runs on after the
first line's. The list of abbreviated suffixes is a note to each entry. The page draws the glottalized
letters as images; page_text reads them back from PAPER_IMAGES.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Kwak’wala"
AUTHORS = ["Daniel J. Frim"]
BOAS = "Franz Boas"
paper = gen.Paper("15-Frim_ICSNL50_final-34", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [(BOAS, "Franz Boas, who published the text (1943) and its translation (1935a)"),
         ("George Hunt", "recorded the narrative; Boas and Hunt (1905, 1906)"),
         ("Daisy Rosenblum", "taught the course the paper was prepared in; thanked; Rosenblum (2013, 2014)"),
         ("Judith Berman", "thanked; Berman (1983 to 1994)"), ("Patrick Littell", "thanked; Littell (2012)"),
         ("Patricia Shaw", "thanked"), ("Katie Sardinha", "a personal communication on -iʔ"),
         ("Galois", "Robert Galois, Kwakwaka’wakw settlements (1994)"),
         ("Codere", "Helen Codere, Kwakiutl traditional culture (1990); editor of Boas (1966)"),
         ("Lincoln", "Neville Lincoln, the North Wakashan comparative root list with Rath (1981)"),
         ("Rath", "John Rath, the North Wakashan comparative root list with Lincoln (1981)"),
         ("Anderson", "Stephen R. Anderson, Kwakwala syntax (1984)"),
         ("Black", "Anne Black, Aktionsart in Kwak’wala with Greene (2010)"),
         ("Greene", "H. Greene, Aktionsart in Kwak’wala with Black (2010)"),
         ("Levine", "Robert Levine, the Kwakwala passive (1980)"),
         ("Nicolson", "Marianne Nicolson, Kwakw’ala determiners with Werle (2009)"),
         ("Werle", "Adam Werle, Kwakw’ala determiners with Nicolson (2009)")]
LANGUAGES = [(LANGUAGE, "Wakashan, North Wakashan branch"), ("English", "the translations")]

RUNNING = paper.running_numbers_set()
AT_FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts} | set(paper.volume_header())
# A unit's number alone on its line, 1. to 80.
UNIT = re.compile(r"^(\d{1,2})\.$")
# A tier's letter and its text; the page can set a sign against the letter, c.?=sa in 25.
TIER = re.compile(r"^([a-g])\.\s*(.*)$")
# Each tier's who, kind and what it is.
TIERS = {"a": (L, "transcription", "Boas's orthography"), "b": (L, "transcription", "NAPA"),
         "c": (A, "segmentation", ""), "d": (A, "gloss", ""), "e": (A, "gloss", "word by word"),
         "f": (BOAS, "translation", "Boas (1935a)"), "g": (A, "translation", "modified")}
# An entry of the list of abbreviated suffixes opens on its label and a colon, 3.POSS: and
# LOC1, LOC2, LOC3:.
ENTRY = re.compile(r"^[A-Z0-9][A-Z0-9.]*(?:, [A-Z0-9.]+)*:\s")


def printed(number):
    return bool(paper.text(number).strip()) and not paper.lines[number][2] and number not in RUNNING \
        and number not in AT_FOOT


def after(number):
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def intro(start, where):
    """(1) or (2): each word group a transcription over its segmentation and gloss, a quoted gloss under
    a group a note with its source, and the quoted sentence closing the example its translation."""
    label = gen.EXAMPLE.match(paper.text(start)).group(1)
    state = {"count": 0}

    def row(who, kind, form, at, gloss=""):
        state["count"] += 1
        paper.add("(%s) line %d" % (label, state["count"]), who, kind, form, "page %d%s" % (paper.page(at), gloss))

    line, text, tier = start, gen.EXAMPLE.match(paper.text(start)).group(2), 0
    while True:
        if text.startswith("“"):
            # A quote whose parenthesis wraps runs on to the line that closes it, `(i.e. nominalised`
            # form of ‘being close to / surface of body’).
            at = line
            while text.count("(") > text.count(")"):
                line = after(line)
                text += " " + paper.text(line)
            said, rest = text[:text.index("”") + 1], text[text.index("”") + 1:]
            # The sentence closing the example ends on a stop after its source.
            closing = text.endswith(").")
            if closing:
                row(A, "translation", said, at)
            else:
                row(A, "note", said, at, ", Boas's gloss of the word group in bold")
            for piece in gen.pieces_of(rest):
                row(A, "citation" if gen.is_source(piece) else "note", piece, at,
                    ", at the right of the %s" % ("translation" if closing else "gloss"))
            if closing:
                return after(line)
        else:
            row(L if tier % 3 == 0 else A, ("transcription", "segmentation", "gloss")[tier % 3], text, line)
            tier += 1
        line = after(line)
        text = paper.text(line)


def key(start, where):
    """The key: a note to its heading and to each tier it names, the wrapped lines run on."""
    paper.add(where, A, "note", paper.text(start), "page %d, heading the key to the tiers" % paper.page(start))
    line, items = after(start), []
    while not UNIT.match(paper.text(line)):
        if TIER.match(paper.text(line)) or not items:
            items.append([paper.text(line), line])
        else:
            items[-1][0] += " " + paper.text(line)
        line = after(line)
    for text, at in items:
        paper.add(where, A, "note", text, "page %d, a tier of the key" % paper.page(at))
        paper.cited(where, text, [paper.page(at)])
        paper.mentions(where, text, NAMES, "name")
        paper.mentions(where, text, LANGUAGES, "language")
    return line


def unit(start, where):
    """A unit of the text: its seven tiers, each a row under 2 N, a tier's wrapped lines run on."""
    here = "2 %s" % UNIT.match(paper.text(start)).group(1)
    tiers, line = [], after(start)
    while line <= paper.last and not UNIT.match(paper.text(line)) and not gen.HEADING.match(paper.text(line)):
        opened = TIER.match(paper.text(line))
        if opened:
            tiers.append([opened.group(1), opened.group(2), line])
        else:
            tiers[-1][1] += " " + paper.text(line)
        line = after(line)
    for letter, text, at in tiers:
        who, kind, what = TIERS[letter]
        paper.add(here, who, kind, text, "page %d%s" % (paper.page(at), ", " + what if what else ""))
    return line


def abbreviations(start, where):
    """The list of abbreviated suffixes: a note to each entry, its wrapped lines run on."""
    line, items = start, []
    while line <= paper.last and not re.match(r"^References$", paper.text(line)):
        if ENTRY.match(paper.text(line)) or not items:
            items.append([paper.text(line), line])
        else:
            items[-1][0] += " " + paper.text(line)
        line = after(line)
    for text, at in items:
        paper.add(where, A, "note", text, "page %d, an entry of the list" % paper.page(at))
        paper.cited(where, text, [paper.page(at)])
        paper.mentions(where, text, NAMES, "name")
        paper.mentions(where, text, LANGUAGES, "language")
    return line


# gen reads a mark in a gloss or segmentation only where a space, a stop or the row's end follows it, or
# on a capital's label before its hyphen; Frim glues his to the morpheme, located20-ON.BEACH, m̓u25-gaʔł,
# k-əχsta-l̓is27=asa, c̓o-Gola=i ?37. The marks gen leaves are looked for in the units' rows, each after
# the one before it and glued to a letter, its mark above, a stop, a sign or a parenthesis, and three
# can share a row, one-CLASS40-FELLOW41=GEN RED42-rot.
GLUED = r"(?:(?<=[^\W\d_])|(?<=[̀-ͯ.?=)]))%s(?!\d)"
UNIT_ROW = re.compile(r"^2 \d+$")
gen_place = paper.place_footnotes


def place_footnotes(marks, write, placed=(), **rest):
    waiting = gen_place(marks, write, placed, **rest)
    if not waiting:
        return waiting
    last = max((index for index, row in enumerate(paper.rows) if row[0].startswith("footnote")), default=-1)
    out = paper.rows[:last + 1]
    for row in paper.rows[last + 1:]:
        out.append(row)
        at = 0
        while waiting and UNIT_ROW.match(row[0]) and row[2] in ("transcription", "segmentation", "gloss",
                                                                  "translation"):
            hit = re.compile(GLUED % re.escape(waiting[0])).search(row[3], at)
            if not hit:
                break
            held, paper.rows = paper.rows, []
            write(waiting[0])
            out.extend(paper.rows)
            paper.rows = held
            at = hit.end()
            waiting.pop(0)
    paper.rows = out
    return waiting


paper.place_footnotes = place_footnotes

blocks = {paper.find(r"^\(1\) "): intro, paper.find(r"^\(2\) "): intro, paper.find(r"^Key$"): key,
          paper.find(r"^3\.POSS: "): abbreviations}
for number in range(1, paper.last + 1):
    if UNIT.match(paper.text(number)) and printed(number):
        blocks[number] = unit
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)

# Note 18 wraps onto 1991:357). and gen.MARK reads its 19 as note 19's mark: the year goes back to note
# 18, and note 19 opens on its own mark's sentence.
for row in paper.rows:
    if row[0] == "footnote 18" and row[2] == "note" and row[3].endswith("(see Berman"):
        row[3] += " 1991:357)."
    if row[0] == "footnote 19" and row[2] == "note" and row[3].startswith("91:357). 19 "):
        row[3] = row[3][len("91:357). 19 "):]
# The references read the School District's entry on page 33 into Rosenblum (2014) before it, its first
# word no author's surname and comma.
for index, row in enumerate(paper.rows):
    if row[2] == "reference" and " School District 85 " in row[3]:
        cut = row[3].index(" School District 85 ")
        paper.rows.insert(index + 1, [row[0], row[1], row[2], row[3][cut + 1:], "page 33"])
        row[3] = row[3][:cut]
        break
# The italics reader drops the words whose letters PAPER_IMAGES rebuilds, k̓ʷɛm̓- and -im̓ in note 78,
# and keeps the hyphen after them as a cited form of its own; a cited form with no letter is dropped.
paper.rows = [row for row in paper.rows if row[2] != "cited form" or re.search(r"[^\W\d_]", row[3])]
paper.write()
