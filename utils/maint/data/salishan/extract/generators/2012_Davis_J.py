"""The ops of 2012_Davis_J: John Hamilton Davis's The two particles "ga" in Mainland Comox, on the
nonconfrontational enclitic ga of ’Ay’ajothem in requests, statements and questions, the
homophonous subordinating ga against ’ot "if", their Russian parallels with бы, and the intent and
result transitivizers.

The paper sets its examples inline, numbered in the prose, and its grammatical analysis as
paragraphs with wider margins: every line of one stands at 180 points, where a plain paragraph
indents only its first line from 144. The text layer keeps no margin, and each line's printed row is
found by its glyphs and its left and right edges read off it. A wide paragraph's lines stop by 432
points; a plain paragraph's first line runs to the right margin. FirstNationsNew's č̓, k̓ and ƛ̓ are
mapped by page_text's PRIVATE_USE. The italic reader leaves the leading glottal stop of ’ewk’w,
’imash, ’ot and the rest upright; each run takes it where the page sets it. Glosses are in
double quotes. §6 is a table of the particles by discourse context, §9 the English and Russian of
(35) to (41), and §12 a reading list followed by the consultants, thanks and support.
"""
import difflib
import re
import os
import sys
import unicodedata

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402
import page_text  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Mainland Comox"
AUTHORS = ["John Hamilton Davis"]
paper = gen.Paper("2012_Davis_J", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Bill Galligos", "consultant; called the language ’Ay’ajothem"),
         ("Mary George", "consultant, Mrs Mary George; sentences (6) to (14), (20), (21), (26), (35), (38)"),
         ("Tommy Paul", "consultant; the Transformer story, Thanch and the Octopus, Mink"),
         ("Noel George Harry", "consultant; the story of killing the wind"),
         ("Ambrose Wilson", "consultant; the story of T’ichewaxanam"),
         ("Jimi Wilson", "consultant; volunteered the spelling <wh>"),
         ("Rhonda Weir", "the author's late fiancée, who named ga the nonconfrontational particle"),
         ("Christine Harry", "fainted, in Mrs Mary George's story"),
         ("Elena Vedernikova", "thanked for composing the Russian sentences of §9"),
         ("Mark Mandel", "thanked"), ("Kristin Denham", "thanked"),
         ("Geoffrey Noel O’Grady", "administered the Canada Council grant, 1969 to 1972"),
         ("Wallace Chafe", "administered the Survey of California and Other Indian Languages; Chafe and Nichols (1986)"),
         ("Wayne Suttles", "personal communication, on Musqueam"),
         ("Franz Boas", "noted tho as “to, toward” in Island Comox"),
         ("Thanch", "kills the Octopus, in Tommy Paul's story"),
         ("Transformer", "in Tommy Paul's story"), ("Mink", "in Tommy Paul's story"),
         ("Dipper Bird", "in Tommy Paul's story of Mink"), ("Raven", "in (42)"),
         ("T’ichewaxanam", "the story told by Ambrose Wilson"),
         ("Baker", "Mona Baker, In Other Words (2011)"), ("Carrithers", "Michael Carrithers (2005)"),
         ("Nichols", "Johanna Nichols, with Chafe (1986)"), ("Grossman", "Edith Grossman (2011)"),
         ("Narrog", "Heiko Narrog, with Heine (2011)"), ("Heine", "Bernd Heine, with Narrog (2011)"),
         ("Munday", "Jeremy Munday (2008)"), ("Pym", "Anthony Pym (2009)")]
LANGUAGES = [(LANGUAGE, "the paper's language, ’Ay’ajothem [ʔayʔaǰoθəm] ~ [ʔayʔaǰuθəm]"),
             ("’Ay’ajothem", "Mainland Comox, as Bill Galligos called it"),
             ("Sliammon", "where the author worked, 1969 to 1979"),
             ("Homalco", "the band of three consultants of the 1978 paper"),
             ("Thalholhtwh", "Island Comox, Boas's tho"), ("Island Comox", "Thalholhtwh"),
             ("Pentlatch", "-olmesh"), ("Salish", "other Salish languages"),
             ("German", "discourse particles; schlecht, ob and wenn, the ich-laut and ach-laut"),
             ("English", "the discourse phrases and translations"), ("Japanese", "ogenki desu ka?"),
             ("Russian", "clauses with бы, §9"), ("Chinese", "jia “family”"), ("Pinyin", "the spelling <ia>"),
             ("Mongolian", "Лхагва “Wednesday”"), ("Nahuatl", "the spelling <tl>"),
             ("Musqueam", "the change to [tθ], Wayne Suttles")]
# Italic runs of other languages, and the italic title of a journal.
FOREIGN = {"schlecht": "German", "ob": "German", "wenn": "German", "falls": "German", "ich": "German",
           "ach": "German", "ogenki desu ka": "Japanese", "jia": "Chinese", "tlapatería": "Mexican",
           "The Journal of the Royal Anthropological Institute": None,
           "’Ay’ajothem": None, "’Ay’ajothem ga": None}
FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
SKIP = FOOT | set(paper.volume_header()) | paper.running_numbers()


def key(text):
    return [one for one in unicodedata.normalize("NFD", text)
            if not one.isspace() and not unicodedata.combining(one) and unicodedata.category(one) != "Lm"]


ROWS = {}


def page_rows(page):
    """The printed rows of page, each (left edge, right edge, letters), top to bottom."""
    if page not in ROWS:
        textpage = page_text.paper_document(paper.stem)[0][page - 1].get_textpage()
        glyphs = []
        for index in range(textpage.count_chars()):
            symbol = textpage.get_text_range(index, 1)
            if symbol and not symbol.isspace():
                left, bottom, right, top = textpage.get_charbox(index, loose=True)
                glyphs.append((bottom, left, right, symbol))
        rows, baseline = [], None
        for bottom, left, right, symbol in sorted(glyphs, key=lambda one: -one[0]):
            if baseline is None or baseline - bottom > 2.5:
                rows.append([])
                baseline = bottom
            rows[-1].append((left, right, symbol))
        ROWS[page] = [(min(one[0] for one in row), max(one[1] for one in row),
                       key("".join(one[2] for one in sorted(row)))) for row in rows]
    return ROWS[page]


def edges(number):
    """The left and right edges of the printed row that holds line number, found by its letters."""
    line = key(paper.text(number))
    best, ratio = None, 0.0
    for left, right, letters in page_rows(paper.page(number)):
        if len(letters) < 3:
            continue
        found = difflib.SequenceMatcher(None, line, letters, autojunk=False).ratio()
        if found > ratio:
            best, ratio = (left, right), found
    return best


def body_lines(first, last):
    return [one for one in range(first, last + 1)
            if one not in SKIP and not paper.lines[one][2] and paper.text(one).strip()]


HEADINGS = paper.headings(14, 538, skip=SKIP)
TABLE, TRANSLATIONS, READING, BACK = 286, 398, 541, 552
BLOCKED = set(range(TABLE, 294)) | set(range(TRANSLATIONS, 416)) | set(range(READING, paper.last + 1))
# Each line's place: wide, a line of an analysis; start, a plain paragraph's first line; or plain.
LINES = [one for one in body_lines(14, paper.last) if one not in HEADINGS and one not in BLOCKED]
PLACE = {}
for at, number in enumerate(LINES):
    left, right = edges(number)
    after = edges(LINES[at + 1]) if at + 1 < len(LINES) else (144.0, 0.0)
    # A plain paragraph's first line can stop short where its next word would not fit, The parting
    # greeting ... say on page 8 and In this paper ... pedagogical on page 14, and the line after it
    # carries its sentence on in lower case; the last line of an analysis ends its sentence, or
    # stops without one before a capital, "every" ... "everywhere" on page 13.
    ended = re.search(r"[.!?:;][”’)]*$", paper.text(number))
    carried = at + 1 < len(LINES) and re.match(r"^\W*[a-z]", paper.text(LINES[at + 1]))
    if left < 170:
        PLACE[number] = "plain"
    elif after[0] < 170 and (right > 434 or (not ended and carried)):
        PLACE[number] = "start"
    else:
        PLACE[number] = "wide"
STARTS, WIDE = set(), set()
previous = None
for number in LINES:
    place = PLACE[number]
    # An analysis can hold two paragraphs, the second after a line that stops well short, the
    # clause introduced by ... ga after the result transitive suffix -ewh. on page 9.
    split = place == "wide" and PLACE.get(previous) == "wide" and edges(previous)[1] < 390 and \
        re.search(r"[.!?][”’)]*$", paper.text(previous))
    if place == "start" or (place == "wide" and PLACE.get(previous) != "wide") or split or \
            (place == "plain" and PLACE.get(previous) == "wide") or previous in HEADINGS or \
            (previous is not None and number - previous > 1 and any(one in HEADINGS for one in range(previous, number))):
        STARTS.add(number)
        if place == "wide":
            WIDE.add(number)
    previous = number
paper._starts = STARTS
if "--places" in sys.argv:
    for number in LINES:
        print(number, PLACE[number], "*" if number in STARTS else " ", "%.0f %.0f" % edges(number), paper.text(number)[:60])
    sys.exit()


def form_language(run):
    return FOREIGN.get(run, L)


paper.form_language = form_language
# A run whose page sets a glottal stop upright before it, ’ewk’w and ’imash, takes it: each run is
# found on its page in order, where the page sets it as a word.
for page, runs in paper.italics().items():
    text = paper.joined(one for one in range(1, paper.last + 1) if paper.page(one) == page)
    place = 0
    for index, run in enumerate(runs):
        pattern = r"(?<![\w’])(’?)%s(?![\w])" % re.escape(run)
        found = re.compile(pattern).search(text, place) or re.search(pattern, text)
        if not found:
            continue
        if found.group(1):
            runs[index] = "’" + run
        place = found.end()


def table(first, where):
    """§6: the particles by discourse context, the heads and a row each."""
    heads = paper.text(first).split()
    paper.add(where, A, "note", paper.text(first), "page %d, the table's heads" % paper.page(first))
    for number in range(first + 1, first + 8):
        head, *cells = paper.text(number).split()
        paper.add(where, A, "note", head, "page %d, a row of the table" % paper.page(number))
        for column, cell in zip(heads, cells):
            if cell == "—":
                paper.add(where, A, "note", cell, "page %d, %s under %s, none" % (paper.page(number), head, column))
            else:
                paper.add(where, L, "cited form", cell, "page %d, %s under %s" % (paper.page(number), head, column))
    return first + 8


def translations(first, where):
    """§9: (35) to (41) in English and in Russian, under the two headings of the list."""
    number = first
    while number < 416:
        text = paper.text(number)
        page = paper.page(number)
        if not text or paper.lines[number][2] or number in SKIP:
            pass
        elif text.startswith("translations of"):
            paper.add(where, A, "note", text, "page %d, a heading of the list of translations" % page)
        elif re.search(r"[А-яЁё]", text):
            paper.add(where, "Russian", "translation", text, "page %d, (%s) in Russian" % (page, label))
        else:
            opened = re.match(r"^\((\d+)\)\s*", text)
            label = opened.group(1) if opened else label
            paper.add(where, A, "note", text, "page %d, (%s) in English" % (page, label))
        number += 1
    return 416


def reading(first, where):
    """§12: the reading list, an entry a row."""
    for entry, at in paper.references(first, 551, skip=SKIP):
        paper.add(where, A, "reference", entry, "page %d" % at)
        paper.mentions(where, entry, NAMES, "name")
    return 552


def back(first, where):
    """The consultants, the thanks, the paper's occasion, its support and the author's e-mail,
    each set between rules."""
    parts = [(553, 555, "the consultants"), (557, 559, "the thanks"), (561, 561, "the paper's occasion"),
             (562, 566, "the research's support")]
    for start, end, what in parts:
        body = paper.joined(range(start, end + 1))
        paper.add("end", A, "note", body, "page %d, %s" % (paper.page(start), what))
        paper.mentions("end", body, NAMES, "name")
    paper.add("end", A, "note", paper.text(567), "page %d, the author's e-mail" % paper.page(567))
    return paper.last + 1


paper.standard(AUTHORS, NAMES, LANGUAGES, references=r"^\uffff$", headings=HEADINGS,
               blocks={TABLE: table, TRANSLATIONS: translations, READING: reading, BACK: back})
rows = paper.rows

# The abstract, set with no heading under the author's town, is one note.
lines = [index for index, row in enumerate(rows) if row[0] == "front" and row[2] == "note"
         and row[4] == "page 1, under John Hamilton Davis"][1:]
if lines:
    rows[lines[0]][3] = " ".join(rows[index][3] for index in lines)
    rows[lines[0]][4] = "page 1, the abstract"
paper.rows = rows = [row for index, row in enumerate(rows) if index not in lines[1:]]

# An analysis set with wider margins says so; a cited form takes the gloss in double quotes the
# paragraph gives right after it, past its bracketed pronunciation.
wide = [paper.text(one) for one in WIDE]
body = ""
for row in rows:
    if row[2] == "note" and any(row[3].startswith(one) for one in wide):
        row[4] += ", an analysis set with wider margins"
    if row[2] == "note":
        body = row[3]
    elif row[2] == "cited form" and "in italics" in row[4] and "‘" not in row[4]:
        found = re.search(r"(?<![\w’])%s(?![\w’])(?:\??\s*\[[^\]]*\])?\s*(“[^”]*”)" % re.escape(row[3]), body)
        if found:
            row[4] += ", " + found.group(1)
# The question suffix -a is set in italics twice in one analysis on page 7, the second time without
# its hyphen, and reads as one form.
seen = set()
kept = []
for row in rows:
    if row[2] == "cited form" and "in italics" in row[4]:
        if row[3] in seen:
            continue
        seen.add(row[3])
    kept.append(row)
paper.rows = kept
# CV+lhuk'w "flying" on page 13 sets the reduplicant CV+ upright against the italic root; the form
# the page prints is the word.
for row in kept:
    if row[2] == "cited form" and row[3] == "lhuk’w":
        row[3] = "CV+lhuk’w"
        row[4] = row[4].replace("in italics", "in italics after an upright CV+")
paper.write()
