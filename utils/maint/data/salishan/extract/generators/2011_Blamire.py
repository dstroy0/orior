"""The ops of 2011_Blamire: Emily Blamire on the order of preverb strings in Ktunaxa, where some
preverbs stand in free variation and others in a fixed order, explained by a scope system after Rice
(2000) in which the two preverbs nearest the verb base form a constituent before the third applies.

Each example sets its words over their segmentation and gloss and a translation in quotes; a sentence
too long for one line wraps its last words, Martina or kyukyits, onto a second set of three lines.
A label over a pair of examples, (tsxaɬ ~ qa), names the preverbs the pair orders. Tables 1 to 4 each
list the six orders of three preverbs with the consultant's judgement and the prediction; the caption
under each opens on the sentence the rows reorder, its segmentation, gloss and translation under it.
"""
import re
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Ktunaxa"
AUTHORS = ["Emily Blamire"]
paper = gen.Paper("2011_Blamire", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Dryer", "Matthew S. Dryer, preverbs in Kutenai and Algonquian (2002)"),
         ("Cinque", "Guglielmo Cinque, adverbs and functional heads (1999)"),
         ("Rice", "Karen Rice, morpheme order and semantic scope in the Athapaskan verb (2000)"),
         ("Morgan", "Lawrence Morgan, a description of the Kutenai language (1991)"),
         ("Martina", "the subject of (3), (4), (11), (12), (17) and (18)"),
         ("Adam", "the subject of (25)"),
         ("Vi Birdstone", "the author's consultant, thanked"),
         ("Martina Wiltschko", "thanked for guidance and advice"), ("Rebecca Laturnus", "thanked")]
LANGUAGES = [(LANGUAGE, "a language isolate of south-eastern British Columbia, northern Idaho and "
              "north-western Montana"),
             ("English", "its adverbs roughly comparable to Ktunaxa preverbs"),
             ("Athabaskan", "Rice's scope system for its verbs")]
QUOTE = re.compile(r"^[‘“\"]")


def interlinear(label, text, line, page):
    """The tiers from text, the words, down to the translation: three lines, the words over their
    segmentation and gloss, then three more where the sentence wraps, then the translation."""
    tiers = [text]
    while not QUOTE.match(paper.text(line)):
        tiers.append(paper.text(line))
        line += 1
    kinds = ("transcription", "segmentation", "gloss")
    for count, tier in enumerate(tiers):
        paper.add("%s line %d" % (label, count + 1), L, kinds[count % 3], tier, "page %d" % page)
    paper.add("%s line %d" % (label, len(tiers) + 1), A, "translation", paper.text(line), "page %d" % page)
    return line + 1


def example(start, where):
    label, text = gen.EXAMPLE.match(paper.text(start)).groups()
    return interlinear("(%s)" % label, text, start + 1, paper.page(start))


def pair(start, where):
    paper.add(paper.text(start), A, "note", paper.text(start),
              "page %d, the label over the pair of examples under it, the preverbs they order"
              % paper.page(start))
    return start + 1


def cells(number):
    return [one for one in re.split(r"\s{3,}", paper.spaced[number].strip()) if one]


def table(start, where):
    """A table: its header a note, each row's order of preverbs a cited form with the judgement and
    the prediction beside it in its gloss, then the caption and the sentence it opens on."""
    page = paper.page(start)
    caption = paper.find(r"^Table \d\. ", start)
    name = re.match(r"^(Table \d)\. ", paper.text(caption)).group(1)
    header = cells(start)
    paper.add(name, A, "note", paper.text(start), "page %d, the table's header" % page)
    for line in range(start + 1, caption):
        found = re.match(r"^([a-f]\))\s+(.*)$", paper.spaced[line].strip())
        row = [found.group(2).split("   ")[0]] + cells(line)[1:]
        paper.add(name, L, "cited form", row[0].strip(), "page %d, row %s, %s" % (
            page, found.group(1), ", ".join("%s %s" % pair for pair in zip(header[1:], row[1:]))))
        paper.add(name, A, "note", " ".join(row[1:]), "page %d, row %s, the cells %s" % (
            page, found.group(1), " and ".join(header[1:])))
    paper.add(name, A, "note", name + ".", "page %d, the table's caption, over the sentence its rows order" % page)
    return interlinear(name, re.sub(r"^Table \d\.\s+", "", paper.text(caption)), caption + 1, page)


def display(start, where):
    """Dryer's formula for the verbal complex, set apart over two lines."""
    paper.add(where, A, "note", paper.joined([start, start + 1]), "page %d, set as a display" % paper.page(start))
    return start + 2


def prose(start, where):
    """The paragraph whose third line opens on (21) and (22), prose that names the examples under it,
    with its cited forms, names and languages."""
    last = paper.find(r"^expected order: tsxaɬ isiɬ ts’iɬ\.$", start)
    body = paper.joined(range(start, last + 1))
    paper.add(where, A, "note", body, "page %d" % paper.page(start))
    paper.cited(where, body, [paper.page(start)])
    paper.mentions(where, body, NAMES, "name")
    return last + 1


blocks = {paper.find(r"^V\.C\.= "): display, paper.find(r"^Consider the preverbs tsxaɬ, isiɬ, and ts’iɬ"): prose}
for number in range(1, paper.last + 1):
    text = paper.text(number)
    if re.match(r"^\(\d+\)\s{3}", paper.spaced[number].strip()):
        blocks[number] = example
    elif re.match(r"^\([a-zʔɬ’]+ [~>] [a-zʔɬ’]+\)$", text):
        blocks[number] = pair
    elif re.match(r"^(PRVB|Preverb) order", text):
        blocks[number] = table
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)
rows = paper.rows
# The abstract, under the university, is one note.
lines = [index for index, row in enumerate(rows) if row[0] == "front" and row[2] == "note"
         and row[4] == "page 1, under Emily Blamire"][1:]
if lines:
    rows[lines[0]][3] = " ".join(rows[index][3] for index in lines)
    rows[lines[0]][4] = "page 1, the abstract"
    rows = [row for index, row in enumerate(rows) if index not in lines[1:]]
# The italic runs that take in the words beside them: tsxaɬ, isiɬ (‘very’) on page 6, where tsxaɬ
# has a row of its own already and isiɬ takes its gloss, and tsxaɬ isiɬ saniɬ, which runs on into
# the sentence after it, Table 2.
drop = []
for index, row in enumerate(rows):
    if row[2] != "cited form":
        continue
    if row[3] == "tsxaɬ, isiɬ" and row[4].startswith("page 6"):
        row[3], row[4] = "isiɬ", "page 6, in italics, ‘very’"
        drop.append(next(one for one in range(index + 1, len(rows)) if rows[one][3] == "isiɬ"))
    elif row[3] == "tsxaɬ isiɬ saniɬ. Table 2":
        row[3] = "tsxaɬ isiɬ saniɬ"
rows = [row for index, row in enumerate(rows) if index not in drop]
# Figures 1 and 2 are tree drawings the layer does not carry; their captions are notes.
for row in rows:
    if row[2] == "note" and row[3].startswith("Figure "):
        row[0], row[4] = row[3].split(".")[0], row[4] + ", the figure's caption; the figure is a tree drawing"
paper.rows = rows
paper.write()
