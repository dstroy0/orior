"""The ops of 24-Deal-ICSNL50_final-26: Amy Rose Deal on Nez Perce verb agreement, the full
paradigm of transitive agreement from systematic elicitation of four verbs: four restrictions on the
agreement affixes, two extensions of the cislocative and the reciprocal into the system, and the
cell 3PL on 3SG that no form fills in the perfect/perfective and the future.

The examples set a segmentation over its gloss, wrapping in pairs, and a translation; (9) sets the
surface form over the segmentation of its verb in parentheses. The generalizations and the template
set as examples, (2), (4), (6), (15) to (20) and (24) to (26), are notes. Tables 1 and 2, on pages
10 and 11, cross the subject with the object: each cell a row, Table 1's the prefixes and suffixes
around the stem's dot, Table 2's their glosses. The appendix gives the two paradigms elicited in
full, (1a) to (28a) for 'find' and (1b) to (28b) for 'call', each with the cell's subject and object
before its translation, 2sg/1sg:. The text layer sets the x with its circumflex as xˆ; the page
prints x̂.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Nez Perce"
AUTHORS = ["Amy Rose Deal"]
paper = gen.Paper("24-Deal-ICSNL50_final-26", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Bessie Scott", "Nez Perce teacher, thanked"),
         ("Florene Davis", "Nez Perce teacher, thanked"),
         ("Matthew Tucker", "thanked"),
         ("Harold Crook", "thanked; Crook (1999), Nez Perce stress"),
         ("Aoki", "Haruo Aoki, Nez Perce grammar (1970), texts (1979) and dictionary (1994)"),
         ("Rude", "Noel Rude, Nez Perce grammar and discourse (1985)"),
         ("Velten", "H. Velten, the Nez Perce verb (1943)"),
         ("Morvillo", "Anthony Morvillo, Grammatica linguae numipu (1891), the missionary period"),
         ("Smith", "A. B. Smith, the peculiarities of the Nez Percés language (1840), the missionary period"),
         ("Phinney", "Archie Phinney, Nez Percé texts (1934)"),
         ("Johannessen", "J. B. Johannessen, coordination (1998), unbalanced coordination"),
         ("Sigurðsson", "H. Á. Sigurðsson, person and number as separate probes, with Holmberg (2008)"),
         ("Holmberg", "A. Holmberg, with Sigurðsson (2008)"),
         ("Preminger", "O. Preminger, agreement as a fallible operation (2011)")]
LANGUAGES = [(LANGUAGE, "Sahaptian; Idaho")]
# The generalizations and the template set as examples: their printed lines, and whether each line
# is a row of its own.
DISPLAYS = {"2": 2, "4": 2, "6": (2, True), "15": 3, "16": 6, "17": 4, "18": 4, "19": 2, "20": 2,
            "24": 4, "25": (2, True), "26": 3}
# The text layer's damage on the table pages and the x̂ of the Nez Perce forms, as residue.py's
# CORRECTIONS for this paper put it back: the table cells are set closed up on the page, 3O-O.PL-•.
AS_PRINTED = (("xˆ", "x̂"), ("T able", "Table"), (":Results", ": Results"), ("[mo rphemes]", "[morphemes]"),
              ("[gl osses]", "[glosses]"), ("indicat ed", "indicated"), ("( •)", "(•)"), ("pre ﬁx", "preﬁx"),
              ("ﬁ", "fi"), ("ﬂ", "fl"), ("- •", "-•"), ("3 O", "3O"), ("3 S", "3S"), ("L -", "L-"),
              ("- S", "-S"), ("(REC )", "(REC)"))
CAPTION = re.compile(r"^T ?able (\d):")
# A line of a table: the subject heading its row, then its cells, a cell number and its form, or a
# dash where the paradigm has no cell.
ROW_HEAD = re.compile(r"^(\d[sp] S)\s+(.*)$")
CELL = re.compile(r"^(\d+[ab])\.\s+(.*)$")
# An example of the appendix, (13a) or (13b), and the cell's subject and object before its
# translation, 2sg/1sg: ‘You found me in the picture.’
APPENDIX_EXAMPLE = re.compile(r"^\((\d+[ab])\)\s+(.*)$")
CELL_TRANSLATION = re.compile(r"^(\d(?:sg|pl)/\d(?:sg|pl)):\s+(‘.*)$")


def as_printed(text):
    for damaged, whole in AS_PRINTED:
        text = text.replace(damaged, whole)
    return text


RUNNING = paper.running_numbers_set()


def after(number):
    number += 1
    while number <= paper.last and (not paper.text(number).strip() or paper.lines[number][2]
                                    or number in RUNNING):
        number += 1
    return number


def table_lines():
    """The lines of the two table pages: set at the size of the notes, the conventions of Table 2,
    3O 3rd person object preﬁx, would open footnote 3."""
    return {number for number in range(1, paper.last + 1) if paper.page(number) in (10, 11)}


FOUND = paper.page_footnotes(stops=table_lines())
# The paper numbers no note 16, and gen reads the notes in sequence, looking for 16 after 15; note
# 17, at the foot of page 19, is added by hand. Mark 18, on the translation of (23b), has no note on
# any page.
SEVENTEEN = paper.find(r"^17I suspect")
FOUND["17"] = ([SEVENTEEN, after(SEVENTEEN)], paper.page(SEVENTEEN))
paper.page_footnotes = lambda *args, **kwargs: FOUND


def table(first, where):
    """Table 1 or 2 to the foot of its page: the caption, the line heading the columns, each cell a
    row given its subject and its object, and the notes under it."""
    page = paper.page(first)
    name = "Table " + CAPTION.match(paper.text(first)).group(1)
    label = name + " line %d"
    paper.add(label % 1, A, "note", as_printed(paper.text(first)), "page %d, the caption" % page)
    header = after(first)
    objects = re.findall(r"\d(?:sg|pl) O", paper.text(header))
    paper.add(label % 2, A, "note", paper.text(header), "page %d, the line heading the columns, the object" % page)
    line, subject, column_of = after(header), None, {}
    while paper.page(line) == page and not paper.text(line).startswith("Notes on"):
        text = paper.text(line)
        head = ROW_HEAD.match(text)
        if head:
            subject, text = head.group(1), head.group(2)
            paper.add("%s, %s" % (name, subject), A, "note", subject, "page %d, the subject heading the row" % page)
        pieces = re.split(r"\s+(?=—|\d+[ab]\.\s)", text)
        # The two dashes of a row stand side by side, the subject on its own person in each number;
        # they are one row.
        dashes = [index for index, piece in enumerate(pieces) if piece == "—"]
        if dashes:
            paper.add("%s, %s on %s" % (name, subject, " and ".join(objects[one] for one in dashes)), A, "note",
                      " ".join("—" for _ in dashes), "page %d, no cell: the paradigm leaves out the reflexive" % page)
        for index, piece in enumerate(pieces):
            cell = CELL.match(piece)
            if not cell:
                continue
            number, form = cell.group(1), re.sub(r"\s+", "", as_printed(cell.group(2)))
            column_of.setdefault(number[:-1], index)
            on = "%s on %s" % (subject, objects[column_of[number[:-1]]])
            here = "%s cell %s" % (name, number)
            if form == "**":
                paper.add(here, A, "note", form, "page %d, %s, no form: the combination is ineffable, (22)" % (page, on))
            elif name == "Table 1":
                paper.add(here, L, "cited affix", form, "page %d, %s, the affixes around the stem (•)" % (page, on))
            else:
                paper.add(here, A, "gloss", form, "page %d, %s, the glosses of the affixes" % (page, on))
        line = after(line)
    notes, count = [], 0
    while paper.page(line) == page:
        text = as_printed(paper.text(line))
        if name == "Table 2" and notes:
            count += 1
            paper.add("%s, the conventions line %d" % (name, count), A, "note", re.sub(r"^(\S+) ", r"\1 ", text),
                      "page %d, a gloss convention of Table 2" % page)
        else:
            notes.append(text)
            if name == "Table 2":
                paper.add(label % 3, A, "note", text, "page %d, the notes under the table" % page)
        line = after(line)
    if name == "Table 1":
        paper.add(label % 3, A, "note", " ".join(notes), "page %d, the notes under both tables" % page)
    return line


def appendix_example(first, where):
    """(Na) or (Nb): a segmentation over its gloss, wrapping in pairs, then the cell and its
    translation."""
    opened = APPENDIX_EXAMPLE.match(paper.text(first))
    number, text, line = opened.group(1), opened.group(2), first
    here = "A.%s (%s) line %%d" % ("1" if number.endswith("a") else "2", number)
    count = 0
    while not CELL_TRANSLATION.match(text):
        count += 1
        paper.add(here % count, L, ("segmentation", "gloss")[(count - 1) % 2], as_printed(text),
                  "page %d" % paper.page(line))
        line = after(line)
        text = paper.text(line)
    cell = CELL_TRANSLATION.match(text)
    paper.add(here % (count + 1), A, "note", cell.group(1) + ":",
              "page %d, the paradigm cell, subject on object" % paper.page(line))
    paper.add(here % (count + 2), A, "translation", cell.group(2), "page %d" % paper.page(line))
    return after(line)


def descriptions(first, where):
    """The five descriptions of nees- quoted on page 3, each after its bullet and wrapping to the
    next: a note to each, to the paragraph after them."""
    line, quoted = first, []
    while not paper.text(line).startswith("This style of presentation"):
        text = paper.text(line)
        if text.startswith("•"):
            quoted.append([text[1:].strip(), line])
        else:
            quoted[-1][0] += " " + text
        line = after(line)
    for text, at in quoted:
        paper.add(where, A, "note", "• " + text, "page %d, a description of nees- quoted, set after a bullet" % paper.page(at))
    return line


# Each example opens on its segmentation over the gloss, and (9) on the surface form over the
# segmentation of its verb.
paper.opening = "auto"
blocks = {paper.find(r"^• plurality of object"): descriptions}
for number in range(1, paper.last + 1):
    text = paper.text(number)
    if CAPTION.match(text):
        blocks[number] = table
    elif APPENDIX_EXAMPLE.match(text) and paper.page(number) >= 17:
        blocks[number] = appendix_example
# (22) stands under a line that closes on two marks, (22).14, 15, and the prose reader runs the line
# on into it.
blocks[paper.find(r"^\(22\) a\. ke kaa")] = lambda first, where: paper.example(first)
HEADINGS = dict(paper.headings(1, paper.last))
# The appendix and its two paradigms, lettered: A, A.1 and A.2.
for pattern, label in ((r"^A Appendix: sample paradigms$", "A"), (r"^A\.1 ’iyaaq", "A.1"), (r"^A\.2 cewcewi", "A.2")):
    HEADINGS[paper.find(pattern)] = label
paper.standard(AUTHORS, NAMES, LANGUAGES, displays=DISPLAYS, blocks=blocks, headings=HEADINGS)
# Page 16 opens a line on [PL], the subject's feature, inside a sentence, and the prose reader takes
# the bracket for the start of a note of its own.
for index, row in enumerate(paper.rows):
    if row[2] == "note" and row[3].startswith("[PL] from the subject cannot be indexed on the verb, that"):
        paper.rows[index - 1][3] += " " + row[3]
        del paper.rows[index]
        break
for row in paper.rows:
    row[3] = row[3].replace("xˆ", "x̂").replace("ﬁ", "fi").replace("ﬂ", "fl")
    # (9) sets the surface form over the segmentation of its verb alone, in parentheses: a form
    # cited, the gloss under it glossing the whole sentence.
    if row[0] in ("(9a) line 2", "(9b) line 2"):
        row[2], row[4] = "cited form", row[4] + ", the verb of the line over it segmented"
paper.write()
