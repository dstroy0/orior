"""The ops of Mellesmoen_Dim_final: Gloria Mellesmoen's All the small things, diminutive
reduplication in ʔayʔaǰuθəm reanalyzed as -C1- infixation after the root's first vowel, driven by a
gradient ALIGN-Lred under ALIGN-Lroot, MAX-M, *CCC, *COMPLEX, MAX, DEP and FT-BIN, with -C1V- and
-[i]C1- where -C1- would make a worse form.

The page text is read by glyph rows (page_text.py rows). The word lists (1), (4), (7), (11), (13),
(14), (16) and (17) set a base form and its gloss, then the diminutive and its gloss, on each
lettered line: each form a transcription row, each gloss a translation row. (16) and (17) set the
reduplicant of the prefixing account in bold on the diminutive, named in its gloss. The constraints
(2), (5) and (9) are a note to each printed line. Table 1 and the tableaux (3), (6), (8), (10) and
(12) are ruled grids read cell by cell by page_text.ruled_tables; Table 1 has no rules of its own
and is read from edges measured off the page. Each tableau's input is a phonemic row and its heads,
set on their side, a note; each candidate a phonemic row naming its reduplicant, set in bold, and
the hand beside it, and each violation mark a note under its constraint. (15) sets its six forms in
two columns, each a form over its segmentation and gloss. The subscript 1 of C1 and the subscript
root and red of ALIGN-Lroot and ALIGN-Lred are read on the line.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402
import page_text  # noqa: E402

A, L = gen.A, gen.L
STEM = "Mellesmoen_Dim_final"
LANGUAGE = "ʔayʔaǰuθəm"
AUTHORS = ["Gloria Mellesmoen"]
paper = gen.Paper(STEM, authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Joanne Francis", "the author's consultant, thanked in the note on the title"),
         ("Gunnar Hansson", "thanked in the note on the title"),
         ("Douglas Pulleyblank", "thanked in the note on the title"),
         ("Watanabe", "Honoré Watanabe, a morphological description of Sliammon (2003)"),
         ("Blake", "S. J. Blake, schwa in Sliammon Salish (2000)"),
         ("Davis", "J. H. Davis, four forms of the verb in Sliammon (1971)"),
         ("McCarthy", "J. J. McCarthy; McCarthy and Prince (1993, 1995)"), ("Prince", "A. Prince"),
         ("Riggle", "J. Riggle, infixing reduplication in Pima (2006)"),
         ("Urbanczyk", "S. Urbanczyk, enhancing contrast in reduplication (2005)"),
         ("Yu", "A. C. L. Yu, global optimization in allomorph selection (2016, listed 2017)"),
         ("Harris", "H. R. Harris, a grammatical sketch of Comox (1981)"),
         ("Sapir", "E. Sapir, noun reduplication in Comox (1915)"),
         ("Haynes", "E. F. Haynes, base TETU effects in Kwak’wala (2007)"),
         ("Bell", "S. J. Bell, internal C reduplication in Shuswap (1983)"),
         ("Alderete", "J. Alderete, morphologically governed accent (2001)"),
         ("Andrei Anghelescu", "an editor of the volume"), ("Michael Fry", "an editor of the volume"),
         ("Marianne Huijsmans", "an editor of the volume"), ("Daniel Reisinger", "an editor of the volume")]
LANGUAGES = [(LANGUAGE, "Central Salish, also known as Comox-Sliammon"),
             ("Comox", "ʔayʔaǰuθəm, in the keywords and the titles of Harris and Sapir"),
             ("Sliammon", "ʔayʔaǰuθəm, in the titles of Davis, Blake and Watanabe"),
             ("Mainland Comox", "ʔayʔaǰuθəm, in Watanabe's title"),
             ("Salish", "the family"), ("Shuswap", "Interior Salish, its -C1- diminutive (Bell 1983)"),
             ("Kwak’wala", "Wakashan, a neighbor, single-consonant reduplication (Haynes 2007)"),
             ("Wakashan", "the family of Kwak’wala"), ("Klamath", "distributive reduplication (McCarthy and Prince 1995)"),
             ("Pima", "in Riggle's title"), ("Cupeño", "in Haynes's title")]

document = page_text.paper_document(STEM)[0]
RUNNING = paper.running_numbers_set()
AT_FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
REFERENCES = paper.find(r"^References$")


def printed(number):
    return bool(paper.text(number).strip()) and not paper.lines[number][2] and number not in RUNNING \
        and number not in AT_FOOT


def after(number):
    number += 1
    while number < REFERENCES and not printed(number):
        number += 1
    return number


def caption(start):
    label, text = gen.EXAMPLE.match(paper.text(start)).groups()
    paper.add("(%s)" % label, A, "note", text, "page %d, the caption" % paper.page(start))
    return label


# A lettered line of a word list: the base form and its gloss, the diminutive and its gloss.
PAIR = re.compile(r"^([a-z])\.\s+(\S+)\s+(‘[^’]*’)\s+(\S+)\s+(‘.*)$")
# The reduplicant of the prefixing account, set in bold on the diminutives of (16) and (17).
BOLD = {"16a": "ta", "16b": "tu", "16c": "ʔa", "16d": "me", "17a": "so", "17b": "x̣a", "17c": "ti", "17d": "θɪ"}


def pairs(start, where):
    """A word list: its caption a note, then each lettered line's base form and diminutive a
    transcription row and each gloss a translation row. (1j) closes its last gloss with ‘ as
    printed."""
    label = caption(start)
    line = after(start)
    while line < REFERENCES and PAIR.match(paper.text(line)):
        letter, base, gloss, diminutive, small = PAIR.match(paper.text(line)).groups()
        page, part = paper.page(line), label + letter
        bold = ", the reduplicant %s in bold" % BOLD[part] if part in BOLD else ""
        assert not bold or diminutive.startswith(BOLD[part]), (part, diminutive)
        paper.add("(%s) line 1" % part, L, "transcription", base, "page %d, the base form" % page)
        paper.add("(%s) line 2" % part, A, "translation", gloss, "page %d, the gloss of the base" % page)
        paper.add("(%s) line 3" % part, L, "transcription", diminutive, "page %d, the diminutive%s" % (page, bold))
        paper.add("(%s) line 4" % part, A, "translation", small, "page %d, the gloss of the diminutive%s" % (
            page, ", closed with ‘ as printed" if "’" not in small else ""))
        line = after(line)
    return line


# The paragraph the page prints after each constraint's definition.
RUNS_TO = {"2": r"^In order to derive", "5": r"^The tableau in \(6\)", "9": r"^Though FT-BIN"}


def definition(start, where):
    """A constraint's definition: its caption a note, then a note to each printed line, the
    constraint and its definition in two columns."""
    label = caption(start)
    line, count = after(start), 0
    while not re.match(RUNS_TO[label], paper.text(line)):
        count += 1
        paper.add("(%s) line %d" % (label, count), A, "note", paper.text(line), "page %d" % paper.page(line))
        line = after(line)
    return line


# The ruled grids of the tableaux by their example, read plain and with each bold run in brackets.
PAGES = {"3": 6, "6": 8, "8": 9, "10": 10, "12": 11}
# The cells shaded under each candidate, past the point where FT-BIN decides.
SHADED = {"10c": ("ALIGN-Lred", "DEP"), "12c": ("ALIGN-Lred", "DEP")}
HANDS = {"☞": ", the winner, printed with the pointing hand ☞",
         "☹": ", the attested form the ranking does not choose, printed with ☹"}


def tableau(start, where):
    """A tableau: the input a phonemic row, RED in bold, and the constraint heads a note; each
    candidate a phonemic row naming its reduplicant in bold and the hand set beside it, and each
    violation mark a note under its constraint. The page text sets a grid's row to a line, which
    is checked against its cells."""
    label = gen.EXAMPLE.match(paper.text(start)).group(1)
    page = PAGES[label]
    [(_, _, plain)] = page_text.ruled_tables(document[page - 1])
    [(_, _, marked)] = page_text.ruled_tables(document[page - 1], bold=("[", "]"))
    for offset, row in enumerate(plain):
        assert " ".join(page_text.ruled_line(row).split()) in paper.text(start + offset), \
            (start + offset, paper.text(start + offset))
    count = 0

    def row(who, kind, form, gloss):
        nonlocal count
        count += 1
        paper.add("(%s) line %d" % (label, count), who, kind, form, "page %d, %s" % (page, gloss))

    heads = [page_text.cell_text(one) for one in plain[0][1:]]
    first = page_text.cell_text(marked[0][0])
    assert first.startswith("[RED] + "), first
    row(L, "phonemic", page_text.cell_text(plain[0][0]), "the input, RED in bold")
    row(A, "note", " ".join(heads), "the constraint heads, set on their side, in small capitals")
    for offset in range(1, len(plain)):
        cells = [page_text.cell_text(one) for one in plain[offset]]
        letter, form = re.match(r"^([a-z])\. (\S+)$", cells[0]).groups()
        bold = re.findall(r"\[([^\]]+)\]", page_text.cell_text(marked[offset][0]))
        hand = next((one for one in HANDS if paper.text(start + offset).startswith(one)), "")
        shaded = SHADED.get(label + letter)
        row(L, "phonemic", form, "candidate (%s%s)%s%s%s" % (
            label, letter, ", the reduplicant %s in bold" % bold[0] if bold else ", no reduplicant", HANDS.get(hand, ""),
            ", its cells under %s shaded" % " and ".join(shaded) if shaded else ""))
        for head, cell in zip(heads, cells[1:]):
            if cell:
                row(A, "note", cell, "the violation marks of (%s%s) under %s" % (label, letter, head))
    return start + len(plain)


def columns(start, where):
    """(15): six forms in two columns, each lettered form a transcription row over its segmentation
    and its gloss, a translation row."""
    label = caption(start)
    line = after(start)
    while line < REFERENCES and re.match(r"^[a-z]\.", paper.text(line)):
        page = paper.page(line)
        heads = re.match(r"^([a-z])\.\s+(\S+)\s+([a-z])\.\s+(\S+)$", paper.text(line)).groups()
        split = paper.text(after(line)).split()
        glosses = re.findall(r"‘[^’]*’", paper.text(after(after(line))))
        assert len(split) == 2 and len(glosses) == 2, (split, glosses)
        for column, (letter, form) in enumerate([heads[:2], heads[2:]]):
            part = "(%s%s)" % (label, letter)
            paper.add(part + " line 1", L, "transcription", form, "page %d" % page)
            paper.add(part + " line 2", L, "segmentation", split[column], "page %d, the root and the lexical suffix, "
                      "DIM+ the diminutive" % page if split[column].startswith("DIM") else "page %d, the root and the "
                      "lexical suffix" % page)
            paper.add(part + " line 3", A, "translation", glosses[column], "page %d" % page)
        line = after(after(after(line)))
    return line


# Table 1 has no rules of its own: its column and row edges, measured off page 2.
TABLE_1 = ([40, 124, 207, 290, 370], [422, 405, 335, 278, 240])


def table_1(start, where):
    """Table 1: its caption a note, its column heads and row heads notes, set in bold, and each
    cell a note under its heads, a cell's lines joined."""
    page = paper.page(start)
    paper.add("Table 1", A, "note", paper.text(start), "page %d, the table's caption" % page)
    [(_, _, rows)] = page_text.ruled_tables(document[page - 1], grids=[TABLE_1])
    heads = [page_text.cell_text(one) for one in rows[0]]
    for head in heads[1:]:
        paper.add("Table 1", A, "note", head, "page %d, a column's head, in bold" % page)
    for row in rows[1:]:
        cells = [page_text.cell_text(one) for one in row]
        paper.add("Table 1", A, "note", cells[0], "page %d, a row's head, in bold" % page)
        for head, cell in zip(heads[1:], cells[1:]):
            paper.add("Table 1", A, "note", cell, "page %d, %s under %s" % (page, cells[0], head))
    # The table's lines in the page text end at the paragraph after it.
    return paper.find(r"^Though the assignment", start)


KINDS = {pairs: (1, 4, 7, 11, 13, 14, 16, 17), definition: (2, 5, 9), tableau: (3, 6, 8, 10, 12), columns: (15,)}
blocks = {paper.find(r"^Table 1: "): table_1}
for number in range(1, REFERENCES):
    opened = printed(number) and gen.EXAMPLE.match(paper.text(number))
    if opened:
        blocks[number] = next(kind for kind, labels in KINDS.items() if int(opened.group(1)) in labels)
# Alderete's publisher, Routledge, New York, NY., wraps onto a line of its own that opens like an entry.
paper.reference_lines_run_on = [paper.find(r"^Routledge, New York, NY\.$", REFERENCES)]
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)
# The mark of footnote 10 on x̣ax̣čamɪn in (17b) comes off the form, and its gloss records it.
for row in paper.rows:
    if row[0] == "(17b) line 3" and row[3].endswith("10"):
        row[3] = row[3][:-2]
        row[4] += ", carries footnote 10"
paper.write()
