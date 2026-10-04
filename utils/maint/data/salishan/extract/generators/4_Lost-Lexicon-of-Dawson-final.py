"""The ops of 4_Lost-Lexicon-of-Dawson-final: Jonathan Janzen's recreation of the Kwak̕wala wordlist
George M. Dawson printed in 1885, some seven hundred English words, each with Dawson's form and its
modern equivalent in the Um̓ista orthography.

The page is read by glyph rows (page_text.py rows), which sets the three columns of the list apart
by their gaps. After the introduction the list runs to the references in 24 numbered groups, 1
Persons to 24 Adjectives, Pronouns, Verbs, etc., under the column heads English, Dawson and Um̓ista.
Each entry is its English prompt as a translation, Dawson's form as a transcription under Dawson
(1885), and each modern form as a transcription. A word goes to the column whose left edge it
stands at on its page, and a line with no English prompt opening on a capital carries on the cells
of the entry above it: Crown of the / head, hel'-kiots-e-ya-pai- / e.
"""
import os
import re
import statistics
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Kwak̕wala"
AUTHORS = ["Jonathan Janzen"]
DAWSON = "Dawson (1885)"
paper = gen.Paper("4_Lost-Lexicon-of-Dawson-final", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("George M. Dawson", "geologist, author of the 1885 wordlist"),
         ("Dawson", "George M. Dawson, the 1885 wordlist"),
         ("Kwakwaka̱’wakw", "the people who speak Kwak̕wala"),
         ("Major J. W. Powell", "Introduction to the Study of Indian Languages"),
         ("Dr. Tolmie", "Comparative Vocabularies of the Indian tribes of British Columbia"),
         ("Tolmie", "Dr. Tolmie, Comparative Vocabularies"),
         ("FirstVoices", "the website of the modern forms"),
         ("Grubb", "David McC. Grubb, the Grubb Dictionary (1977)"),
         ("RDC", "the mother tongue speaker of Kwak̕wala the author worked with")]
LANGUAGES = [(LANGUAGE, "Wakashan, the language of the wordlist"), ("English", "the translations")]

# Note 1 ends on an e-mail address with no stop, and note 2's mark stands on a line of its own at
# the foot of page 2, over Many thanks to RDC, which can read as a page number. Where page_footnotes
# runs note 2 on into note 1, note 2 is parted from it at Many thanks.
FOUND = paper.page_footnotes()
PARTS, PAGE = FOUND["1"]
SPLIT = next((index for index, one in enumerate(PARTS) if paper.text(one).startswith("Many thanks to RDC")), None)
if SPLIT is not None:
    FOUND["1"], FOUND["2"] = (PARTS[:SPLIT], PAGE), (PARTS[SPLIT:], paper.page(PARTS[SPLIT]))
paper.page_footnotes = lambda *args, **kwargs: FOUND
FOOTNOTES = paper.page_footnotes()
SKIP = {one for parts, _ in FOOTNOTES.values() for one in parts} | set(paper.volume_header())
RUNNING = paper.running_numbers_set()
REFERENCES = paper.find(r"^References$")
LIST = paper.find(r"^English\s+Dawson\s+Um̓\s?ista$")
GROUP = re.compile(r"^(\d{1,2})\s+(\S.*)$")
COLUMNS = ("English", "Dawson", "Um̓ista")


def page_columns():
    """{page: [left edge of each column]}: the middle left edge of each cell of the lines on the
    page that the glyph rows set in three cells."""
    edges = {}
    for number in range(LIST + 1, REFERENCES):
        cells = re.split(r"\s{3,}", paper.spaced[number])
        positions = paper.word_positions(number) if len(cells) == 3 else None
        if not positions:
            continue
        index, found = 0, []
        for cell in cells:
            found.append(positions[index][0])
            index += len(cell.split())
        for column, left in enumerate(found):
            edges.setdefault(paper.page(number), [[], [], []])[column].append(left)
    return {page: [statistics.median(one) for one in lefts] for page, lefts in edges.items()}


EDGES = page_columns()


def cells(number):
    """The words of line number under each column, English, Dawson and Um̓ista."""
    edges = EDGES[paper.page(number)]
    found = [[], [], []]
    positions = paper.word_positions(number)
    if positions is None:
        print("# no positions:", number, paper.text(number), file=sys.stderr)
        return [paper.text(number), "", ""]
    for left, word in positions:
        column = max((one for one in range(3) if edges[one] <= left + 6), default=0)
        found[column].append(word)
    return [" ".join(one) for one in found]


# An English gloss in brackets after a modern form, (good writer), a form in square brackets, a
# modern word unlike Dawson's, and forms set side by side with or between them.
PIECE = re.compile(r"\[[^\]]*\]|\([^)]*\)|\bor\b|[^\s\[\]()]+(?:\s+(?!or\b)[^\s\[\]()]+)*")
# A Kwak̕wala word in round brackets after a modern form is a form too, alone, (lema̱nstu) after
# tła̱nx̱a, or with its meaning after a dash, (łoḵwa - strong): its letters are outside English's.
BRACKETED = re.compile(r"^\(([^\s()]*[^\x00-\x7f‘’“”][^\s()]*)(?:\s+[-–]\s+([^()]+))?\)$")


def modern_forms(text):
    """[(form, gloss, after)] for the Um̓ista cell text: each form with the English in brackets after
    it, and the form a bracketed one follows."""
    forms = []
    for piece in PIECE.findall(text):
        if piece == "or":
            continue
        bracketed = BRACKETED.match(piece)
        if bracketed and forms:
            meaning = ["(%s)" % bracketed.group(2)] if bracketed.group(2) else []
            forms.append([bracketed.group(1), meaning, forms[-1][0]])
        elif piece.startswith("(") and forms:
            forms[-1][1].append(piece)
        elif piece.startswith("("):
            forms.append(["", [piece], ""])
        else:
            forms.append([piece, [], ""])
    return [(form, " ".join(glosses), after) for form, glosses, after in forms]


# The Ankle form types ǥ, U+01E5, where Ankle bone below defines g̱; the form is kept as typed.
TYPED = {"x̱ax̱a̱ǥa̱nukwsidze'": "the print reads as g̱ (500 dpi, page 4) and Ankle bone below spells it g̱"}


def modern_gloss(form, page, english, after=""):
    gloss = "page %d, the modern form in the Um̓ista orthography" % page
    if after:
        gloss += ", in round brackets after %s" % after
    if form in TYPED:
        gloss += ", " + TYPED[form]
    if form.startswith("*"):
        gloss += ", marked *, a reproduction of Dawson's form not verified in modern use"
    if form.startswith("["):
        gloss += ", in square brackets, a modern word for the English unlike Dawson's form"
    if english:
        gloss += ", %s the modern meaning" % english
    return gloss


def entry_rows(entry):
    label, page = "Dawson #%d" % entry["count"], entry["page"]
    english, dawson, modern = entry["cells"]
    paper.add(label, A, "translation", english, "page %d, the English prompt" % page)
    for form in re.split(r"\s+or\s+", dawson) if dawson else []:
        paper.add(label, DAWSON, "transcription", form, "page %d, the form in %s" % (page, DAWSON))
    for form, english, after in modern_forms(modern):
        if form:
            paper.add(label, L, "transcription", form, modern_gloss(form, page, english, after))
        else:
            paper.add(label, A, "note", english, "page %d, in the Um̓ista column" % page)


def unfinished(english):
    """Whether an English prompt runs onto the next line: an open bracket, One half (in, or a last
    word that leads on, One hundred and."""
    return english.count("(") > english.count(")") or \
        english.split()[-1] in ("and", "of", "the", "in", "a", "for", "to", "with", "or")


def wordlist(start, where):
    """The list from its column heads to the references: a heading to each group and the rows of
    each entry."""
    paper.add("Dawson", A, "note", paper.text(start), "page %d, heads the columns" % paper.page(start))
    entry, count = None, 0
    for number in range(start + 1, REFERENCES):
        text = paper.text(number)
        if number in SKIP or paper.lines[number][2] or not text or number in RUNNING:
            continue
        group = GROUP.match(text)
        if group:
            if entry:
                entry_rows(entry)
                entry = None
            paper.add("Dawson group %s" % group.group(1), A, "heading", text,
                      "page %d, a group of the wordlist" % paper.page(number))
            continue
        found = cells(number)
        if entry is None or found[0] and (found[1] or found[2]) and not unfinished(entry["cells"][0]):
            if entry:
                entry_rows(entry)
            count += 1
            entry = {"count": count, "page": paper.page(number), "cells": found}
            continue
        # A line carrying on the cells above: a Dawson form broken after a hyphen runs on whole, and
        # so does an English compound, water- / ouzel.
        for column, piece in enumerate(found):
            if not piece:
                continue
            before = entry["cells"][column]
            entry["cells"][column] = before + piece if before.endswith("-") and column < 2 else \
                (before + " " + piece).strip()
    if entry:
        entry_rows(entry)
    return REFERENCES


HEADINGS = paper.headings(1, LIST - 1, skip=SKIP)
# Note 1 is marked on the author's name, Jonathan Janzen1, and is written with the notes on the title.
paper.standard(AUTHORS, NAMES, LANGUAGES, front_languages=1, blocks={LIST: wordlist}, headings=HEADINGS,
               notes_title=tuple(gen.TITLE_MARKS) + ("1",))
for row in paper.rows:
    if row[0] == "footnote 1" and row[4].endswith(", on the title"):
        row[4] = row[4][:-len(", on the title")] + ", on the author's name"
paper.write()
