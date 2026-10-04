"""The ops of 10-McClay_Birdstone_ICSNL50_final: E.K. McClay and Violet Birdstone on wh-questions in
Ktunaxa: they are formed by direct movement to the left periphery of the clause, obey the Coordinate
Structure, Adjunct Island and Complex NP constraints, and a wh-word takes a copula to be a predicate.

The gloss labels are set in small capitals, which the page text reads as capitals. Examples (1), (2),
(6) and (7) set their lettered parts side by side: each printed line is cut where part b stands on
the header line, and each part's tiers are read in turn.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Ktunaxa"
AUTHORS = ["E.K. McClay", "Violet Birdstone"]
paper = gen.Paper("10-McClay_Birdstone_ICSNL50_final", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Ross", "John R. Ross, constraints on variables in syntax (1967)"),
         ("Boas", "Franz Boas, Kutenai Tales (1918) and notes on the language (1927a)"),
         ("Morgan", "Lawrence R. Morgan, a description of the language (1991)"),
         ("Mast", "Susan J. Mast, aspects of Kutenai morphology (1988)"),
         ("Kootenai Culture Committee", "the Kootenai dictionary (1999)"),
         ("Martina Wiltschko", "thanked"), ("Henry Davis", "thanked"),
         ("Canestrelli", "Philippo Canestrelli (1927), cited by Mast"),
         ("Dryer", "Matthew S. Dryer, obviation in Kutenai and Algonquian (1992)"),
         ("Bobaljik & Wurmbrand", "Jonathan Bobaljik and Susi Wurmbrand, questions with declarative syntax (2014)"),
         ("Kroeber", "Paul D. Kroeber, Salish syntax (1999)")]
LANGUAGES = [(LANGUAGE, "isolate; British Columbia, Montana, Idaho"), ("Kutenai", LANGUAGE + ", Boas's spelling"),
             ("English", "echo-questions"),
             ("Salish", "Ktunaxa's neighbours, predicative wh-words"),
             ("Algonquian", "Dryer 1992, obviation")]


DOCUMENT = gen.pdfium.PdfDocument(gen.os.path.join(gen.CORPUS, "papers", paper.stem + ".pdf"))
GLYPH_ROWS = {}


def glyph_rows(page):
    """gen's glyph rows, each small capital read as page_text reads it: pdfium gives the gloss labels
    as codes of the Brill cipher, and word_positions would find no printed row for a gloss line."""
    if page not in GLYPH_ROWS:
        textpage = DOCUMENT[page - 1].get_textpage()
        glyphs = []
        for index in range(textpage.count_chars()):
            symbol = textpage.get_text_range(index, 1)
            if symbol and not symbol.isspace():
                left, bottom, right, top = textpage.get_charbox(index, loose=True)
                glyphs.append((bottom, left, gen.page_text.deciphered(symbol, textpage, index)))
        rows, baseline = [], None
        for bottom, left, symbol in sorted(glyphs, key=lambda one: -one[0]):
            if baseline is None or baseline - bottom > 2.5:
                rows.append([])
                baseline = bottom
            rows[-1].append((left, symbol))
        GLYPH_ROWS[page] = [sorted(row) for row in rows]
    return GLYPH_ROWS[page]


paper.glyph_rows = glyph_rows

# The Latin in situ is set in italics with the form before it, Leaving qaⱡa in situ, and is no part of it.
ITALICS = {page: ["qaⱡa" if run == "qaⱡa in situ" else run for run in runs] for page, runs in paper.italics().items()}
paper.italics = lambda: ITALICS
RUNNING = paper.running_numbers_set()
FOOTNOTES = paper.page_footnotes()
AT_FOOT = {one for parts, _ in FOOTNOTES.values() for one in parts} | set(paper.volume_header())
# Each side-by-side example's printed lines, its header line the first.
PAIRED = {"1": 4, "2": 4, "6": 4, "7": 5}
# A translation, with the label the page sets before it or none: intended:, Lit., (intended):.
TRANSLATION = re.compile(r"^(?:intended:|Lit\.|\(intended:?\):?)?\s*‘")
# The label, a note of its own; (24c) sets Lit. before a translation printed with no opening quote.
LABEL = re.compile(r"^(?:intended:|Lit\.|\(intended:?\):?)(?=\s)")


def printed(number):
    return paper.text(number).strip() and not paper.lines[number][2] and number not in RUNNING \
        and number not in AT_FOOT


def after(number):
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def glossed(number):
    """Whether line number is a gloss: it holds a label in small capitals, IND, OBV, COMP."""
    return number <= paper.last and bool(re.search(r"(?<![A-Za-z])[A-Z]{2,}", paper.text(number)))


def tiers(label, lines):
    """The rows of one side-by-side part from its printed pieces [(line, text)]: the transcription,
    its segmentation and gloss, the translation with any label before it, and what stands under it
    a note."""
    kinds = ["transcription", "segmentation", "gloss"]
    count = 0
    for line, text in lines:
        count += 1
        where, page = "(%s) line %d" % (label, count), "page %d, side by side" % paper.page(line)
        if kinds:
            paper.add(where, L, kinds.pop(0), text, page)
            continue
        before = LABEL.match(text).group(0) if LABEL.match(text) else ""
        if before:
            paper.add(where, A, "note", before, page + ", the label before the translation")
            count += 1
            where, text = "(%s) line %d" % (label, count), text[len(before):].strip()
        paper.add(where, A, "translation" if text.startswith("‘") else "note", text, page)


def interlinear(start, where):
    """A numbered example, whole or in lettered parts. A part's tiers run transcription, segmentation
    and gloss, wrapping in threes where a sentence runs over the line, until a line that is no tier:
    the one two below a wrapped transcription is a gloss. That line is the translation, a label
    before it or none, run on to its closing stop. A line under it opening on a parenthesis, or on
    Potential replies:, is a note, run on to its closing parenthesis. A lettered line opens the next
    part; any other line is the prose after the example."""
    opened = gen.EXAMPLE.match(paper.text(start))
    number, line, text = opened.group(1), start, opened.group(2)
    while True:
        sub = gen.SUB.match(text)
        label = number + (sub.group(1) if sub else "")
        text = sub.group(2).strip() if sub else text
        count, tier = 0, 0

        def row(who, kind, form, at, gloss=""):
            paper.add("(%s) line %d" % (label, count), who, kind, form, "page %d%s" % (paper.page(at), gloss))
        while not tier or not TRANSLATION.match(text) and (tier % 3 or glossed(after(after(line)))):
            count += 1
            row(L, ("transcription", "segmentation", "gloss")[tier % 3], text, line)
            tier += 1
            line = after(line)
            text = paper.text(line)
        at = line
        while not re.search(r"[’.?!]$", text):
            line = after(line)
            text += " " + paper.text(line)
        count += 1
        before = LABEL.match(text).group(0) if LABEL.match(text) else ""
        if before:
            row(A, "note", before, at, ", the label before the translation")
            count += 1
            text = text[len(before):].strip()
        row(A, "translation", text, at)
        line = after(line)
        text = paper.text(line)
        while text.startswith(("(", "Potential repl")) and not gen.EXAMPLE.match(text):
            at = line
            while text.count("(") > text.count(")"):
                line = after(line)
                text += " " + paper.text(line)
            count += 1
            row(A, "note", text, at, ", under the translation")
            line = after(line)
            text = paper.text(line)
        if not gen.SUB.match(text):
            return line


def quotation(start, where):
    """Mast's summary set as a block quotation, one note, to the paragraph under it."""
    end = paper.find(r"^It can also mark subordinate clauses", start)
    body = paper.joined(range(start, end))
    paper.add(where, A, "note", body, "page %d, a block quotation of Mast (1988:109)" % paper.page(start))
    paper.mentions(where, body, NAMES, "name")
    paper.mentions(where, body, LANGUAGES, "language")
    return end


def paired(start, where):
    """An example whose parts a and b stand side by side: every printed line cut at the left edge of
    b on the header line."""
    label = gen.EXAMPLE.match(paper.text(start)).group(1)
    words = paper.word_positions(start)
    cut = next(left for left, word in words if word == "b.") - 2
    parts = {"a": [], "b": []}
    line = start
    for count in range(PAIRED[label]):
        for left, word in paper.word_positions(line):
            if re.fullmatch(r"\(\d+\)|[ab]\.", word):
                continue
            side = parts["a" if left < cut else "b"]
            if side and side[-1][0] == line:
                side[-1][1] += " " + word
            else:
                side.append([line, word])
        line = after(line)
    for letter in "ab":
        tiers(label + letter, parts[letter])
    return line


blocks = {paper.find(r"^First, as Canestrelli \(1927:7\) notes"): quotation}
for number in range(1, paper.last + 1):
    opened = gen.EXAMPLE.match(paper.text(number))
    if printed(number) and opened:
        blocks[number] = paired if opened.group(1) in PAIRED else interlinear
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)

# The Kootenai Culture Committee's entry opens on no surname and initials, and gen runs it onto
# Dryer's above it.
for index, row in enumerate(paper.rows):
    if row[2] == "reference" and " Kootenai Culture Committee. (1999)" in row[3]:
        first, second = row[3].split(" Kootenai Culture Committee. (1999)")
        paper.rows[index:index + 1] = [[row[0], row[1], row[2], first, row[4]],
                                       [row[0], row[1], row[2], "Kootenai Culture Committee. (1999)" + second, row[4]]]
        break
paper.write()
