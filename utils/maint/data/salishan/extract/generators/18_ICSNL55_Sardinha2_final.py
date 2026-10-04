"""The ops of 18_ICSNL55_Sardinha2_final: Katie Sardinha on the grammar of body-directed action verbs in
Kwak̕wala, reflexive and transitive sentences, implicit objects, and body part suffixes.

The interlinear examples are read by gen.Paper.example; (1) opens under a line that ends on a
footnote's mark set apart, case. 3, and is read as a block. The segmentation of a reduplicated verb,
CV~ t̕sux̱w -(x)t̕sana, is typed after the reading, and (6a)'s source is split from its comment.
Table 1 is three rows to each printed line, all of them Table 1 line k: the Verb cell a
transcription, the English Translation cell a translation, and the Morpheme Analysis cell a
segmentation with its gloss beside it a gloss row, a cell that wraps joined. Table 2 is a row to
each cell of each suffix's row: the suffix a segmentation, its translation, its phonology a note,
and its example derivation a phonemic row to each morpheme left of the arrow, a phonetic row for
the form right of it and a translation for its gloss, wrapped cells joined. Both tables center
their cells and are read by their quotes and hyphens. Table 3 is a note to each row, its wrapped
lines joined; the glossing abbreviations a note to each entry, its wrapped notes run on. The page's
bold and underline are not in the text layer.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Kwak̕wala"
AUTHORS = ["Katie Sardinha"]
paper = gen.Paper("18_ICSNL55_Sardinha2_final", authors=", ".join(AUTHORS), language=LANGUAGE)

SPEAKER = "Kwak̕wala speaker, the essay's elicitation is derived from work with her"
NAMES = [("Mildred Child", SPEAKER), ("Ruby Dawson Cranmer", SPEAKER), ("Julia Nelson", SPEAKER),
         ("Violet Bracic", SPEAKER), ("Boas", "Franz Boas, Kwakiutl grammar (1947) and its suffixes")]
LANGUAGES = [(LANGUAGE, "Northern Wakashan"), ("Wakashan", "the family, a keyword"),
             ("Salish", "neighbouring languages that mark transitive sentences on the verb"),
             ("English", "the translations, and its intensive pronouns")]
FOOTNOTES = paper.page_footnotes()
AT_FOOT = {one for parts, _ in FOOTNOTES.values() for one in parts}
REFERENCES = paper.find(r"^References$")
ABBREVIATIONS = paper.find(r"^Glossing Abbreviations$")
RUNNING = paper.running_numbers_set()
ENGLISH = {"the", "of", "and", "to", "in", "is", "that", "for", "as", "with", "are", "be", "by", "this",
           "which", "we", "on", "it", "not", "or", "from", "can", "an", "these", "has", "have"}


def prose(text):
    """Whether a line is running prose: nine words or more, three of them English function words."""
    words = text.split()
    return len(words) >= 9 and sum(1 for one in words if one.lower().strip(",.;:()") in ENGLISH) >= 3


def printed(number):
    return paper.text(number).strip() and not paper.lines[number][2] and number not in AT_FOOT \
        and number not in RUNNING


def groups(start):
    """The rows of a table from the line after start: runs of printed lines the page sets apart by
    an empty line, to the prose after the table. Returns the runs and the line after the table."""
    found, run, line = [], [], start + 1
    while line < REFERENCES:
        if line in AT_FOOT or line in RUNNING or paper.lines[line][2]:
            line += 1
            continue
        if not paper.text(line).strip():
            if run:
                found.append(run)
            run = []
        elif prose(paper.text(line)):
            break
        else:
            run.append(line)
        line += 1
    if run:
        found.append(run)
    return found, line


def table_lines(start, end):
    """The printed lines of a table from start to the line before end."""
    return [line for line in range(start, end) if printed(line)]


def run_on(text, more):
    """text with its next line after it, a space between. A line that breaks after the hyphen of
    non- keeps the space, as the flow does."""
    return text + " " + more


def caption(start, name):
    paper.add(name, A, "note", paper.text(start), "page %d, the table's caption" % paper.page(start))


def heads(pattern, start, name):
    """The column heads, a note. Returns their line."""
    head = paper.find(pattern, start)
    paper.add(name, A, "note", paper.text(head), "page %d, the column heads" % paper.page(head))
    return head


def opened(text):
    """Whether a gloss or translation in quotes is left open at the line's end."""
    return text.startswith("‘") and not re.search(r"’\W*\d{0,2}$", text)


def table_1(start, where):
    """Table 1: its caption and column heads a note each, then three rows to each printed line, the
    Verb cell a transcription, the English Translation cell a translation, the Morpheme Analysis
    cell a segmentation and its gloss a gloss row, all Table 1 line k. The page centers each cell,
    and a line is read by its quotes: a line with two or more opens a verb's row, its Verb cell
    the words before the first quote, its English Translation cell that quote to the closing quote
    a space follows, and its Morpheme Analysis cell the rest. A line with no quote under a gloss
    left open runs the gloss on, lus- `‘to uncover, to open / curtain or roof’`."""
    caption(start, "Table 1")
    head = heads(r"^Verb English Translation Morpheme Analysis$", start, "Table 1")
    end = next(line for line in range(head + 1, REFERENCES) if printed(line) and prose(paper.text(line)))
    count, held = 0, None
    for line in table_lines(head + 1, end):
        text = paper.text(line)
        if "‘" not in text and held is not None and opened(held[3]):
            held[3] += " " + text
            continue
        count += 1
        here, page = "Table 1 line %d" % count, "page %d, Table 1" % paper.page(line)
        analysis = text
        if text.count("‘") >= 2:
            verb, _, rest = text.partition("‘")
            close = re.search(r"’(?=\s)", rest)
            paper.add(here, L, "transcription", verb.strip(), page + ", the Verb cell")
            paper.add(here, A, "translation", "‘" + rest[:close.end()], page + ", the English Translation cell")
            analysis = rest[close.end():].strip()
        form, quote, gloss = analysis.partition("‘")
        paper.add(here, L, "segmentation", form.strip(), page + ", the Morpheme Analysis cell")
        held = None
        if quote:
            paper.add(here, L, "gloss", quote + gloss, page + ", the Morpheme Analysis cell, its gloss")
            held = paper.rows[-1]
    return end


def derivation(text, here, page):
    """A derivation, /kus-/ + /-(g̱)a̱m/ + /-(x)’id/ → [kusa̱md] ‘to face-shave’: a phonemic row to each
    morpheme left of the arrow, a phonetic row for the form right of it, a translation for its
    gloss."""
    left, _, right = text.partition("→")
    for morpheme in left.split(" + "):
        if morpheme.strip():
            paper.add(here, L, "phonemic", morpheme.strip(), page + ", left of the arrow")
    formed = re.match(r"^\s*(\[[^\]]*\])\s*(.*)$", right)
    if formed:
        paper.add(here, L, "phonetic", formed.group(1), page + ", right of the arrow")
        if formed.group(2):
            paper.add(here, A, "translation", formed.group(2), page + ", the derivation's gloss")


# A row of Table 2 with its lines joined: the suffix, its translation (two where the page gives two,
# ‘eye’ or ‘round opening’), its phonology and the derivation from the first slash on.
SUFFIX_ROW = re.compile(r"^(\S+)\s+(‘[^’]*’\d{0,2}(?: or ‘[^’]*’)?)\s+(.*?)\s+(/.*)$")


def table_2(start, where):
    """Table 2: its caption and column heads a note each, then to each suffix's row the suffix a
    segmentation, its translation, its phonology a note and its example a derivation, all Table 2
    row k. The page centers each cell; a row opens on the line that begins with its suffix's
    hyphen and holds each printed line to the next such line, the cells read off the joined text.
    The table ends at the paragraph under it, Verbs describing actions directed at the body."""
    caption(start, "Table 2")
    head = heads(r"^Suffix Translation Phonology Example$", start, "Table 2")
    end = paper.find(r"^Verbs describing actions directed at the body", head)
    rows = []
    for line in table_lines(head + 1, end):
        if paper.text(line).startswith("-"):
            rows.append([paper.text(line), line])
        else:
            rows[-1][0] = run_on(rows[-1][0], paper.text(line))
    for count, (text, line) in enumerate(rows, 1):
        here, page = "Table 2 row %d" % count, "page %d, Table 2" % paper.page(line)
        suffix, english, phonology, example = SUFFIX_ROW.match(text).groups()
        paper.add(here, L, "segmentation", suffix, page + ", the Suffix cell")
        paper.add(here, A, "translation", english, page + ", the Translation cell")
        paper.add(here, A, "note", phonology, page + ", the Phonology cell, its wrapped lines joined")
        derivation(example, here, page + ", the Example cell")
    return end


def table_3(start, where):
    """Table 3: its caption and column heads a note each, then a note to each row, its wrapped lines
    joined."""
    caption(start, "Table 3")
    head = heads(r"^Reflexive sentences Example #s$", start, "Table 3")
    runs, end = groups(head)
    for count, run in enumerate(runs, 1):
        text = paper.text(run[0])
        for line in run[1:]:
            text = run_on(text, paper.text(line))
        paper.add("Table 3 line %d" % count, A, "note", text,
                  "page %d, Table 3, a row, its wrapped lines joined" % paper.page(run[0]))
    return end


def example(start, where):
    return paper.example(start, REFERENCES - 1, AT_FOOT)


blocks = {paper.find(r"^Table 1: "): table_1, paper.find(r"^Table 2: "): table_2,
          paper.find(r"^Table 3: "): table_3, paper.find(r"^\(1\) Reflexive sentences:"): example}
# (14) and (15) name the root after the colon of their captions, body part suffixes: t̓sux̱w-.
paper.captions = {gen.EXAMPLE.match(paper.text(paper.find(r"^\(%s\) " % label))).group(2) for label in ("14", "15")}
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, appendix=r"^Glossing Abbreviations$")

# The segmentation of a reduplicated verb holds its template, CV~ t̕sux̱w -(x)t̕sana, whose capitals
# read as a gloss: a gloss row holding CV~ with a gloss row of the same example under it is the
# segmentation, and a segmentation over it the wrapped transcription, t̕sut̕sa̱x̱wa̱m lax̱ada la’sta̱’at̕si
# in (11e).
def example_of(row):
    """The label of the example a row stands in, (11e) for (11e) line 4."""
    return row[0].split(" line ")[0]


for index in range(1, len(paper.rows) - 1):
    row, over, under = paper.rows[index], paper.rows[index - 1], paper.rows[index + 1]
    if row[2] == "gloss" and re.search(r"(?:^|\s)CV~\s", row[3]) and example_of(under) == example_of(row) \
            and under[2] == "gloss":
        row[2] = "segmentation"
        if example_of(over) == example_of(row) and over[2] == "segmentation":
            over[2] = "transcription"
# (6a) sets its source right of the speaker's comment; the source is a note of its own, as the
# source on its own line under (6b) is.
for index, (where, who, kind, form, gloss) in enumerate(paper.rows):
    split = re.match(r"^(.*”)\s+(\([^()]*\))$", form)
    if kind == "speaker comment" and split:
        paper.rows[index:index + 1] = [[where, who, kind, split.group(1), gloss],
                                       [where, A, "note", split.group(2),
                                        gloss.split(",")[0] + ", at the right of the comment"]]
        break

# The glossing abbreviations: a heading, the column heads a note, and a note to each entry with its
# wrapped notes run on.
ENTRY = re.compile(r"^(?:[A-Z0-9][A-Z0-9.]*|[-=~]|\(\))\s")
paper.add("abbreviations", A, "heading", paper.text(ABBREVIATIONS), "page %d" % paper.page(ABBREVIATIONS))
count, held = 0, None
for line in range(ABBREVIATIONS + 1, paper.last + 1):
    if not printed(line):
        continue
    text = paper.text(line)
    if text == "Gloss Morphs Notes":
        count += 1
        paper.add("abbreviations line %d" % count, A, "note", text, "page %d, the column heads" % paper.page(line))
    elif ENTRY.match(text) or held is None:
        count += 1
        paper.add("abbreviations line %d" % count, A, "note", text,
                  "page %d, an entry, its wrapped notes run on" % paper.page(line))
        held = paper.rows[-1]
    else:
        held[3] += " " + text
paper.write()
