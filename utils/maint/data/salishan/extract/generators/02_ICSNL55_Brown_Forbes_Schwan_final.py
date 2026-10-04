"""The ops of 02_ICSNL55_Brown_Forbes_Schwan_final: Colin Brown, Clarissa Forbes and Michael David
Schwan on the transitive vowel of Tsimshianic, in Gitksan and Nisga'a (Interior) and Sm'algyax
(Maritime, Coast Tsimshian).

The page text is read by glyph rows (page_text.py rows): the examples set each word over its
segmentation and gloss, which the text layer reads a column at a time. Tables 2 and 3 are printed
a quarter turn round and kept as the text layer reads them, with the spaces it puts at the bold
letters taken out in residue.py CORRECTIONS. Their cells span one or both clause-type columns as
the page shades them, read off renders turned upright. Each example's language comes from its
tag, G, N or CT, or from the consultant's initials.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
paper = gen.Paper("02_ICSNL55_Brown_Forbes_Schwan_final", authors="Colin Brown, Clarissa Forbes, Michael David Schwan",
                  language="Gitksan")
add, text, page, find = paper.add, paper.text, paper.page, paper.find
# Footnote ∗ thanks the consultants in two languages, 'Wii t'isim ha'miiyaa 'nuu'm to the Gitksan
# ones and 'ap luk'wil t'oyaxsmt 'nüüsm to the Sm'algyax ones.
FORM_LANGUAGE = {"Wii t’isim ha’miiyaa ’nuu’m": "Gitksan", "ap luk’wil t’oyaxsmt ’nüüsm": "Sm’algyax"}
paper.form_language = lambda run: FORM_LANGUAGE.get(run, L if gen.orthographic(run) else None)

NAMES = [("Barbara Sennott", "Gitksan consultant (BS)"), ("Vince Gogag", "Gitksan consultant (VG)"),
         ("Hector Hill", "Gitksan consultant (HH)"), ("Jeanne Harris", "Gitksan consultant (JH)"),
         ("Velna Nelson", "Sm'algyax consultant (VN)"), ("Beatrice Robinson", "Sm'algyax consultant (BR)"),
         ("Ellen Mason", "Sm'algyax consultant (EM)"), ("Margaret Anderson", "of the UBC Gitksan Research Lab"),
         ("Fumiko Sasama", "of the UBC Gitksan Research Lab; Sasama 2001"), ("Henry Davis", "PI of SSHRC Insight Grant 435-2015-1694")]
LANGUAGES = [("Gitksan", "Interior Tsimshianic (G)"), ("Nisga’a", "Interior Tsimshianic (N)"),
             ("Sm’algyax", "Maritime Tsimshianic, Coast Tsimshian (CT)"), ("Coast Tsimshian", "Sm'algyax (CT)"),
             ("Sgüüxs", "Maritime Tsimshianic, Southern Tsimshian (ST)"), ("Southern Tsimshian", "Sgüüxs (ST)"),
             ("Tsimshianic", "the family, Interior and Maritime branches")]
TAGS = {"G": "Gitksan", "N": "Nisga’a", "CT": "Sm’algyax", "G+CT": "Gitksan and Sm’algyax"}
SPEAKERS = {"BS": "Gitksan", "VG": "Gitksan", "HH": "Gitksan", "JH": "Gitksan",
            "VN": "Sm’algyax", "BR": "Sm’algyax", "EM": "Sm’algyax"}

# The front matter: the title, each author over their university, the abstract and keywords.
TITLE = find(r"^Clause-type, Transitivity")
add("front", A, "title", text(TITLE).rstrip("∗* "), "page 1, carries footnote ∗")
for number in (TITLE + 1, TITLE + 3, TITLE + 5):
    add("front", A, "name", text(number), "author, %s" % text(number + 1))
    add("front", A, "note", text(number + 1), "page 1, the affiliation of %s" % text(number))
KEYWORDS = find(r"^Keywords:")
add("front", A, "note", paper.joined(range(TITLE + 7, KEYWORDS)), "page 1, the abstract")
add("front", A, "note", text(KEYWORDS), "page 1, the keywords")
add("front", A, "note", paper.joined(paper.volume_header()), "page 1, the volume's header")
for name, why in LANGUAGES[:3]:
    add("front", A, "language", name, why)
    paper.mentioned.add(("language", name))

FOOTNOTES = paper.page_footnotes()
FOOTNOTE_LINES = {one for parts, _ in FOOTNOTES.values() for one in parts} | set(paper.volume_header())
REFERENCES = find(r"^References$")


def footnote(mark):
    parts, at = FOOTNOTES[mark]
    paper.footnote(mark, parts, at, NAMES, LANGUAGES,
                   gloss="page %d, footnote %s%s" % (at, mark, ", on the title" if mark == "∗" else ""))


footnote("∗")

# Table 1: each type's triggers, a line to a language.
TABLE_1_ROW = re.compile(r"^(?:(Clausal Subordination|Aspectual Markers|Temporal Morphemes|Syntactically Determined) )?"
                         r"(G\+CT|G|CT) (.+)$")


def table_1(start, where):
    add("Table 1", A, "note", text(start), "page %d, the table's caption" % page(start))
    add("Table 1", A, "note", text(start + 1), "page %d, the column heads" % page(start))
    number, kind, count = start + 2, None, 2
    while True:
        row = TABLE_1_ROW.match(text(number))
        if not row:
            return number
        count += 1
        here = "Table 1 line %d" % count
        if row.group(1):
            kind = row.group(1)
            add(here, A, "note", kind, "page %d, the type of trigger" % page(number))
        add(here, A, "note", row.group(2), "page %d, the language tag, %s" % (page(number), TAGS[row.group(2)]))
        who = TAGS[row.group(2)] if row.group(2) != "G+CT" else A
        add(here, who, "transcription" if who != A else "note", row.group(3),
            "page %d, %s triggers of the Dependent order" % (page(number), kind.lower()))
        number += 1


# Tables 2 and 3: a row a stem shape, each cell a form under one clause-type column, I or D, or
# spread over both where the stem has no contrast. SPANS gives each row's cells their widths.
TABLES = {
    "2": ("Gitksan", ["Obstruent", "-si’m", "Sonorant", "-diit"], {
        "Obstruent": "1 1 1 1 2 2", "Sibilant": "1 1 2 2 2", "Sonorant (stressed syl)": "1 1 1 1 2 2",
        "Sonorant (unstressed syl)": "2 2 2 2", "Vowel": "1 1 1 1 1 1 2", "Obstruent + T": "1 1 1 1 2 2",
        "Sonorant + T": "2 2 2 2", "Vowel + T": "1 1 1 1 1 1 1 1"}),
    "3": ("Sm’algyax", ["Obstruent", "-sm", "Sonorant", "Vowel"], {
        "Obstruent": "1 1 1 1 2 2", "Obs-Stop": "2 2 2 2", "S-final": "1 1 2 2 2", "Sonorant": "2 2 2 2",
        "Vowel": "1 1 1 1 1 1 2", "Obstruent + T": "1 1 1 1 2 2", "Sonorant + T": "1 1 1 1 2 2",
        "Vowel + T": "1 1 2 2 2"}),
}


def stem_table(start, where):
    label = re.match(r"^Table (\d+):", text(start)).group(1)
    who, suffixes, spans = TABLES[label]
    here = "Table %s" % label
    add(here, A, "note", text(start), "page %d, the table's caption; the page is turned a quarter round" % page(start))
    add(here, A, "note", text(start + 1), "page %d, the suffix heads" % page(start))
    add(here, A, "note", text(start + 2), "page %d, the clause-type heads, I Independent and D Dependent" % page(start))
    number, count = start + 3, 3
    while True:
        line = text(number)
        shape = next((one for one in sorted(spans, key=len, reverse=True) if line.startswith(one + " ")), None)
        if not shape:
            return number
        count += 1
        cells = line[len(shape) + 1:].split()
        widths = [int(one) for one in spans[shape].split()]
        if len(cells) != len(widths):
            raise SystemExit("Table %s, %s: %d cells for %d spans" % (label, shape, len(cells), len(widths)))
        row_where = "Table %s line %d" % (label, count)
        add(row_where, A, "note", shape, "page %d, the stem shape" % page(number))
        column = 0
        for cell, width in zip(cells, widths):
            suffix = suffixes[column // 2]
            if width == 2:
                clause = "I and D alike, no contrast (shaded)"
            else:
                clause = "I, Independent" if column % 2 == 0 else "D, Dependent"
            add(row_where, who, "transcription", cell,
                "page %d, stem shape %s, column %s, %s" % (page(number), shape, suffix, clause))
            column += width
        number += 1


# The displays set as examples: a template, a list, a generalization, and their printed lines.
DISPLAYS = {"2": 2, "8": 2, "9": 3, "27": 4, "28": 2, "29": 3, "48": 6, "49": 2, "51": 4, "54": 3}
BLOCKS = {}
for number in range(1, REFERENCES):
    opened = gen.EXAMPLE.match(text(number))
    if opened and opened.group(1) in DISPLAYS and number not in FOOTNOTE_LINES:
        BLOCKS[number] = (lambda count: lambda start, where: paper.display(start, count))(DISPLAYS[opened.group(1)])
    if text(number).startswith("Table 1:"):
        BLOCKS[number] = table_1
    if re.match(r"^Table [23]:", text(number)):
        BLOCKS[number] = stem_table
HEADINGS = paper.headings(KEYWORDS + 1, REFERENCES - 1, skip=FOOTNOTE_LINES)
if "debug" in sys.argv:
    print(HEADINGS)

paper.flow(KEYWORDS + 1, REFERENCES - 1, "front", headings=HEADINGS, skip=FOOTNOTE_LINES, blocks=BLOCKS,
           names=NAMES, languages=LANGUAGES)
missing = paper.place_footnotes([one for one in FOOTNOTES if one != "∗"], footnote)
if missing:
    print("# footnotes not placed:", missing, file=sys.stderr)

# Each example's language: the tag at the right of a translation, G, N or CT, or the initials of
# the consultant, for every lettered part of the example.
untagged = paper.languages_by_tag({one: TAGS[one] for one in ("G", "N", "CT")}, SPEAKERS)
if untagged:
    print("# examples with no language tag:", untagged, file=sys.stderr)

add("references", A, "heading", "References", "page %d" % page(REFERENCES))
for entry, at in paper.references(REFERENCES + 1, paper.last):
    add("references", A, "reference", entry, "page %d" % at)
paper.write()
