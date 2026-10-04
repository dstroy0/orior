"""The ops of 01_ICSNL55_Beck_final: David Beck on Lushootseed numerals, the plain, human and
temporal-iterative series and the numeral phrase.

The page text is the text layer closed up from the glyph rows (page_text.py closeup): the layer
sets spaces inside words, fr om, x̌ ʷəlačiʔ. Tables 1 to 3 print two numerals to a line, each its
number and its form; Table 4 prints two forms to a line with their glosses. The examples are the
interlinear layout gen.Paper.example reads.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
paper = gen.Paper("01_ICSNL55_Beck_final", authors="David Beck", language="Lushootseed")
add, text, page, find = paper.add, paper.text, paper.page, paper.find

NAMES = [("Thom Hess", "Thomas M. Hess, thanked on the title; Hess & Hilbert 1976, Beck & Hess 2014, 2015"),
         ("Vi Hilbert", "Vi Taqʷšəblu Hilbert, thanked on the title; her recordings are in the corpus"),
         ("Leon Metcalf", "collected recordings in the Lushootseed corpus"),
         ("Louise Anderson", "speaker of example (1)"), ("Mary Sampson Willup", "speaker of example (2)"),
         ("Agnes James", "speaker of example (3)"), ("Harry Moses", "speaker of examples (5), (8), (16), (27), (28)"),
         ("Edward Sam", "speaker of examples (6), (15), (17)"), ("Martha Lamont", "speaker of many of the examples"),
         ("Martin Sampson", "speaker of example (11)"), ("Alice Williams", "speaker of examples (12), (20)"),
         ("Julia Siddle", "speaker of example (18), Basket Ogress"), ("Dora Solomon", "speaker of example (24)"),
         ("Edward Sapir", "proposed the areal form *moos ‘four’")]
LANGUAGES = [("Lushootseed", "Central Salish, of Puget Sound"),
             ("Northern Lushootseed", "NL in the tables"), ("Southern Lushootseed", "SL in the tables"),
             ("Proto-Salishan", "the reconstructed ancestor; *ʔupán-akis(t) ‘ten’ (Kinkade 2002)"),
             ("Snoqualmie-Duwamish", "the Southern Lushootseed forms of Tweddell 1950"),
             ("Halkomelem", "Central Salish, whose numeral classifiers are compared"),
             ("hən’q’əmin’əm’", "Downriver Halkomelem, as printed"), ("English", "compared in the numerals for centuries")]

# The front matter.
TITLE = find(r"^Lushootseed Numerals")
add("front", A, "title", text(TITLE).rstrip("* "), "page 1, carries footnote *")
add("front", A, "name", text(TITLE + 1), "author")
add("front", A, "note", text(TITLE + 2), "page 1, the author's affiliation")
KEYWORDS = find(r"^Keywords:")
add("front", A, "note", paper.joined(range(TITLE + 3, KEYWORDS)), "page 1, the abstract")
add("front", A, "note", text(KEYWORDS), "page 1, the keywords")
add("front", A, "note", paper.joined(range(1, TITLE)), "page 1, the volume's header")
for name, why in LANGUAGES[:3]:
    add("front", A, "language", name, why)
    paper.mentioned.add(("language", name))

# The footnotes, each from its mark to the end of its page.
REFERENCES = find(r"^References$")
FOOTNOTES = {}
for marks, first in ((["*", "1"], r"^\* Numberless"), (["2", "3", "4"], r"^2 Tweddell"),
                     (["5"], r"^5 The forms higher"), (["6"], r"^6 The number four")):
    FOOTNOTES.update(paper.footnotes(marks, start=find(first)))
FOOTNOTE_LINES = {one for parts, _ in FOOTNOTES.values() for one in parts}


def footnote(mark):
    parts, at = FOOTNOTES[mark]
    where = "footnote %s" % mark
    body = gen.Paper.body_of(paper.joined(parts), mark)
    add(where, A, "note", body, "page %d, footnote %s%s" % (at, mark, ", on the title" if mark == "*" else ""))
    paper.cited(where, body, [at])
    paper.mentions(where, body, NAMES, "name")
    paper.mentions(where, body, LANGUAGES, "language")


footnote("*")

# The tables.
SERIES = {"1": "the plain series", "2": "the human series", "3": "the temporal-iterative series"}
PAIR = re.compile(r"^(\d{1,4}) (.+?) (\d{1,4}) (.+)$")
DAYS = re.compile(r"^(\S+) (‘[^’]+’\*?) (\S+) (‘[^’]+’.*)$")
LAST_DAY = re.compile(r"^— (\S+) (‘[^’]+’.*)$")


def table(start, where):
    label = re.match(r"^Table (\d+):", text(start)).group(1)
    here = "Table %s" % label
    add(here, A, "note", text(start), "page %d, the table's caption" % page(start))
    count = 1
    number = start + 1
    while True:
        line = text(number)
        if not line or paper.lines[number][2]:
            number += 1
            continue
        pair, days, last_day = PAIR.match(line), DAYS.match(line), LAST_DAY.match(line)
        if label != "4" and pair:
            count += 1
            for value, form in ((pair.group(1), pair.group(2)), (pair.group(3), pair.group(4))):
                add("%s line %d" % (here, count), L, "transcription", form,
                    "page %d, %s, %s" % (page(number), value, SERIES[label]))
        elif label == "4" and (days or last_day):
            count += 1
            cells = [days.group(1, 2), days.group(3, 4)] if days else [last_day.group(1, 2)]
            for form, gloss in cells:
                add("%s line %d" % (here, count), L, "transcription", form,
                    "page %d, %s" % (page(number), "the day of the week" if form.endswith("il") else "the count of days"))
                add("%s line %d" % (here, count), A, "translation", gloss, "page %d" % page(number))
        else:
            break
        number += 1
    # The notes under the table, each from its mark to a line that closes on a stop before the
    # next paragraph, a blank line, a footnote or the page's end. The notes are indented as the
    # paragraphs are.
    notes = []
    starts = paper.paragraph_starts()
    while True:
        line = text(number)
        if line.startswith(("*", "†")):
            notes.append([number])
        elif notes and line and not paper.lines[number][2] and number not in FOOTNOTE_LINES \
                and not (text(notes[-1][-1]).endswith(".") and number in starts):
            notes[-1].append(number)
        else:
            break
        number += 1
    for parts in notes:
        body = paper.joined(parts)
        mark = body[0]
        add("%s note %s" % (here, mark), A, "note", body, "page %d, the note to the table's %s" % (page(parts[0]), mark))
        paper.cited("%s note %s" % (here, mark), body, [page(parts[0])])
        paper.mentions(here, body, NAMES, "name")
    return number


TABLES = {number: table for number in range(1, REFERENCES) if re.match(r"^Table \d+:", text(number))}
HEADINGS = paper.headings(KEYWORDS + 1, REFERENCES - 1, skip=FOOTNOTE_LINES)
if "debug" in sys.argv:
    print(HEADINGS, sorted(TABLES))

# The body; a footnote's rows follow the paragraph its mark stands in.
paper.flow(KEYWORDS + 1, REFERENCES - 1, "front", headings=HEADINGS, skip=FOOTNOTE_LINES, blocks=TABLES,
           names=NAMES, languages=LANGUAGES)
out, placed = [], {"*"}
for row in paper.rows:
    out.append(row)
    if row[2] in ("note", "heading") and not row[0].startswith("footnote"):
        for mark in sorted(FOOTNOTES, key=lambda one: (len(one), one)):
            if mark not in placed and re.search(r"[^\s\d(]%s(?:\s|$)" % re.escape(mark), row[3]):
                placed.add(mark)
                held, paper.rows = paper.rows, []
                footnote(mark)
                out.extend(paper.rows)
                paper.rows = held
paper.rows = out
# The speakers named in the examples' citations.
out = []
for row in paper.rows:
    out.append(row)
    if row[2] == "citation":
        held, paper.rows = paper.rows, []
        paper.mentions(row[0], row[3], NAMES, "name")
        out.extend(paper.rows)
        paper.rows = held
paper.rows = out
missing = [one for one in FOOTNOTES if one not in placed]
if missing:
    print("# footnotes not placed:", missing, file=sys.stderr)

add("references", A, "heading", "References", "page %d" % page(REFERENCES))
for entry, at in paper.references(REFERENCES + 1, paper.last):
    add("references", A, "reference", entry, "page %d" % at)
paper.write()
