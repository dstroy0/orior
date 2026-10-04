"""The ops of 3_Inman_2018: David Inman on predicates at the syntax-semantics boundary in
Nuuchahnulth: verbs, adjectives and common nouns are syntactic predicates and one-place (or more)
semantic predications, proper nouns zero-place predications, shown by the article =ʔiˑ, which a
proper noun never takes, and the predicate linker -(q)ḥ, which joins two predicates under the
subject of the clausal clitics.

The page is read by glyph rows (page_text.py rows), which sets each tier of an example on one line
and reads the small capitals of the glosses from their font's cipher. An example is the sentence,
its segmentation over its gloss, and the translation with the dialect and speaker at its right, a
citation. A numbered semantic representation, (4) SEE(x, y), is a formula. The appendix after the
references is its heading, a note to each paragraph, and Table 1 a caption note and a note to each
abbreviation with its name and description.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Nuuchahnulth"
AUTHORS = ["David Inman"]
paper = gen.Paper("3_Inman_2018", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Fidelia Haiyupis", "Nuuchahnulth speaker, Northern dialect"),
         ("Julia Lucas", "tupaat Julia Lucas, Nuuchahnulth speaker, Central dialect"),
         ("Marjorie Touchie", "Nuuchahnulth speaker, Barkley dialect"),
         ("Bob Mundy", "Nuuchahnulth speaker, Barkley dialect"),
         ("Adam Werle", "first proposed the predicate linker; the spreadsheet of the Nootka Texts"),
         ("Matthew Davidson", "the searchable database of the Nootka Texts"),
         ("Werle", "Adam Werle, the four dialect groups (2013)"),
         ("Sapir", "Edward Sapir, the Nootka Texts (1939)"), ("Swadesh", "Morris Swadesh (1938, 1939)"),
         ("Jacobsen", "William H. Jacobsen, noun and verb in Nootkan (1979)"),
         ("Wojdak", "Rachel Wojdak, category neutrality (2001)"),
         ("Virginia Woolfe", "a literary style"), ("William Faulkner", "a literary style")]
LANGUAGES = [(LANGUAGE, "Wakashan, the west coast of Vancouver Island, ISO 639-3 nuk"),
             ("Wakashan", "the family"), ("English", "the translations"),
             ("Ancient Greek", "participial phrases as a literary style"),
             ("Salish", "neighboring languages, predicate-initial and predicate-flexible")]

FOOTNOTES = paper.page_footnotes()
SKIP = {one for parts, _ in FOOTNOTES.values() for one in parts} | set(paper.volume_header())
REFERENCES = paper.find(r"^References$")
APPENDIX = paper.find(r"^A IGT Conventions$")
TABLE = paper.find(r"^Table 1: Non-standard abbreviations$")
# A semantic representation opens on a predication in capitals or the existential.
FORMULA = re.compile(r"^[∃A-Z]")


def formula(start, where):
    """A numbered semantic representation: one formula row."""
    label, text = gen.EXAMPLE.match(paper.text(start)).groups()
    paper.add("(%s) line 1" % label, A, "formula", text, "page %d" % paper.page(start))
    return start + 1


def example(start, where):
    """An example by gen's reader, and the translation of a starred sentence after Intended:,
    which the reader leaves to the prose for its many words past a short sentence's tiers."""
    after = paper.example(start, skip=SKIP)
    text = paper.text(after)
    if not text.startswith("Intended:"):
        return after
    label = gen.EXAMPLE.match(paper.text(start)).group(1)
    count = sum(1 for row in paper.rows if row[0].startswith("(%s) line" % label))
    said, pieces = gen.split_translation(text)
    for kind, form in [("translation", said)] + [("citation", piece) for piece in pieces]:
        count += 1
        paper.add("(%s) line %d" % (label, count), A, kind, form, "page %d" % paper.page(after))
    return after + 1


def before(number):
    """The line of text over line number, past page breaks, page numbers and footnotes."""
    number -= 1
    while number > 1 and (paper.lines[number][2] or not paper.text(number) or number in SKIP
                          or re.match(r"^\d+$", paper.text(number))):
        number -= 1
    return paper.text(number)


# A number opens an example where the line over it closes a sentence, a source or a footnote mark:
# (3) predicates. on page 3 carries on the sentence over it.
blocks = {}
for number in range(1, REFERENCES):
    found = gen.EXAMPLE.match(paper.text(number))
    if found and number not in SKIP and re.search(r"[.:)’\d]$", before(number)):
        blocks[number] = formula if FORMULA.match(found.group(2) or "") else example
HEADINGS = paper.headings(1, REFERENCES - 1, skip=SKIP)
# The author line prints the surname first, Inman, David.
paper.standard(["Inman, David"], NAMES, LANGUAGES, front_languages=1, blocks=blocks, headings=HEADINGS,
               appendix=r"^A IGT Conventions$")
for row in paper.rows:
    if row[2] == "name" and row[3] == "Inman, David":
        row[3], row[4] = "David Inman", "author, printed Inman, David"
    # The dialect and speaker at the right of a translation or under it, (N, Fidelia Haiyupis).
    if row[2] in ("note", "citation") and re.match(r"^\([QNCB], [^()]+\)$", row[3]):
        row[2] = "citation"
        row[4] = re.sub(r", (at the right of|under) the translation$|$",
                        lambda found: ", the dialect and speaker %s the translation"
                        % (found.group(1) or "at the right of"), row[4], count=1)
    # (22) segments its first word with the length template LS in capitals, which the reader takes
    # for a gloss.
    if row[0] == "(22) line 2":
        row[2] = "segmentation"

# Footnote 3's mark stands on (13), ʔuḥʔiiš3; the reader places the note at STRG.3 in (9)'s gloss.
held = [row for row in paper.rows if row[0] == "footnote 3"]
paper.rows[:] = [row for row in paper.rows if row[0] != "footnote 3"]
at = next(index for index, row in enumerate(paper.rows) if row[0] == "(13) line 1") + 1
paper.rows[at:at] = held

# (27) and (28) set a reading the consultant rejected, in italics, at the right of the translation.
for label, reading, judged in (("27", "*He/she/they found something.", "a reading the consultant rejects"),
                               ("28", "?? Someone found it.", "a reading the consultant doubts")):
    index, row = next((index, row) for index, row in enumerate(paper.rows)
                      if row[0].startswith("(%s) line" % label) and row[2] == "translation")
    row[3] = row[3][:row[3].index("’") + 1]
    paper.rows.insert(index + 1, [row[0], A, "note", reading, "%s, %s, at the right of the translation"
                                  % (row[4], judged)])
    count = 0
    for one in paper.rows:
        if one[0].startswith("(%s) line" % label):
            count += 1
            one[0] = "(%s) line %d" % (label, count)

# The appendix: its heading and a note to each paragraph, with Table 1 where the page sets it, its
# caption and column heads each a note and each abbreviation a notation. The rows of the table set
# a description over two lines centered on the abbreviation, and the glyph rows run the two lines
# of D1, D2, D3, D4 into each other: each row is read off a render of page 16.
ABBREVIATIONS = [
    ("IN", "inceptive", "the inceptive aspect"),
    ("MO", "momentaneous", "the momentaneous aspect, similar to perfective but may indicate the start of an "
     "event rather than its completion"),
    ("DR", "durative", "the durative aspect"),
    ("GRAD", "graduative", "the graduative aspect, similar to English progressive"),
    ("NOW", "now", "indicates the beginning of the next event in a sequence"),
    ("STRG", "strong mood", "strong claim to factual status, non-Barkley sound"),
    ("REAL", "real mood", "strong claim to factual status, Barkley sound only"),
    ("NEUT", "neutral mood", "no claim to factual status or a continuation of previous factual claim"),
    ("HRSY", "hearsay mood", "the status of the event is based on hearsay"),
    ("INFR", "inferential", "the status of the event is inferred from other information"),
    ("X", "—", "a semantically empty object (ʔu) that certain suffixes must attach to"),
    ("ART", "article", "the article"),
    ("D1, D2, D3, D4", "deictic (1, 2, 3, 4)",
     "a demonstrative deictic, with 1 being closest to the speaker and 4 furthest away")]
AFTER_TABLE = paper.find(r"^There are other morphemes", TABLE)
paper.add("appendix", A, "heading", paper.text(APPENDIX), "page %d" % paper.page(APPENDIX))
paragraph, at = [], None


def flush():
    if paragraph:
        paper.add("appendix", A, "note", " ".join(paragraph), "page %d" % at)
    del paragraph[:]


for number in range(APPENDIX + 1, paper.last + 1):
    text = paper.text(number)
    if paper.lines[number][2] or not text or number in SKIP or re.match(r"^\d+$", text) or \
            TABLE < number < AFTER_TABLE:
        continue
    if number == TABLE:
        flush()
        page = paper.page(TABLE)
        paper.add("Table 1 line 1", A, "note", text, "page %d, the caption" % page)
        paper.add("Table 1 line 2", A, "note", "Abbreviation Full Name Description", "page %d, heads the columns" % page)
        for count, (label, name, description) in enumerate(ABBREVIATIONS, 3):
            paper.add("Table 1 line %d" % count, A, "notation", label, "page %d, %s, %s" % (page, name, description))
            # X's description names the empty object in italics, (ʔu).
            if label == "X":
                paper.add("Table 1 line %d" % count, L, "cited form", "ʔu", "page %d, in italics, the empty object X" % page)
        continue
    if re.match(r"^(In addition|There are other|Table 1 gives)", text):
        flush()
    if not paragraph:
        at = paper.page(number)
    paragraph.append(text)
flush()
paper.write()
