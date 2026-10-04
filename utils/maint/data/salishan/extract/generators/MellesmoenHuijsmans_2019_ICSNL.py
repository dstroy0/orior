"""The ops of MellesmoenHuijsmans_2019_ICSNL: Gloria Mellesmoen and Marianne Huijsmans on stative
allomorph selection and raised pitch in ʔayʔaǰuθəm. The stative is -it, -i- or raised pitch alone; every
allomorph carries an underlying H tone, and listed allomorphs ranked by Priority, with Final-CStem,
Align-RStative and the tone constraints, select among them. ʔayʔaǰuθəm is then a pitch accent language
in Hyman's (2006) sense.

The page text is read by glyph rows (page_text.py rows), which keeps the columns of the examples.
The paper's AboriginalSerif sets its glottalized letters in the private use area, mapped in
page_text.PRIVATE_USE. The examples are lists: (1) to (3), (27), (28) and (41) a form and its gloss
twice a line, root and stative; (4) to (10) and (29) to (37) a form, its phonetic form in brackets,
its pitch pattern when one is printed and its gloss; (39), (40), (42) and (43) a lettered form with
its stem brackets and a gloss. Each form is a transcription row, each bracketed form and pitch pattern
a phonetic row, each gloss a translation row. The constraints (11), (12), (14), (18), (19), (23) and
(38) and the rankings (16), (21) and (26) are a note to each printed line. Each tableau's input and
candidates are phonemic rows, the violation marks of a candidate a note in the order printed (the
text layer keeps no column), and the tone lines over them notes. The glyph rows cannot read the
constraint heads (20), (22), (24) and (25) set turned on their side, and those heads are given by
hand, read off 250 dpi renders of pages 12 and 16.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "ʔayʔaǰuθəm"
AUTHORS = ["Gloria Mellesmoen", "Marianne Huijsmans"]
paper = gen.Paper("MellesmoenHuijsmans_2019_ICSNL", authors=" and ".join(AUTHORS), language=LANGUAGE)
NAMES = [("Joanne Francis", "a ʔayʔaǰuθəm speaker the authors work with"),
         ("Elsie Paul", "a ʔayʔaǰuθəm speaker"), ("Freddie Louie", "a ʔayʔaǰuθəm speaker"),
         ("Karen Galligos", "a ʔayʔaǰuθəm speaker"), ("Betty Wilson", "a ʔayʔaǰuθəm speaker, of the dictionary team"),
         ("Marion Harry", "a ʔayʔaǰuθəm speaker"), ("Margaret Vivier", "a ʔayʔaǰuθəm speaker"),
         ("Jerry Francis", "a ʔayʔaǰuθəm speaker"), ("Phyllis Dominic", "a ʔayʔaǰuθəm speaker"),
         ("Mary Harry", "a ʔayʔaǰuθəm speaker"), ("Herman Francis", "a ʔayʔaǰuθəm speaker"),
         ("Maggie Wilson", "a ʔayʔaǰuθəm speaker, late"), ("Dave Dominic", "a ʔayʔaǰuθəm speaker, late"),
         ("Eva Francis", "a ʔayʔaǰuθəm speaker, late"), ("Randolph Timothy", "of the dictionary team"),
         ("Koosen Pielle", "of the dictionary team"), ("Suzanne Urbanczyk", "thanked; Urbanczyk (2004)"),
         ("Henry Davis", "thanked"), ("Watanabe", "Honoré Watanabe, a morphological description of Sliammon (2003)"),
         ("Blake", "Susan Blake (1992, 2000)"), ("Kinkade", "M. Dale Kinkade (1996)"),
         ("Mascaró", "Joan Mascaró, external allomorphy (2007)"), ("Andreotti", "Bruno Andreotti (2017, 2018)"),
         ("Hyman", "Larry M. Hyman, word-prosodic typology (2006)"), ("Davis", "John Davis, Mainland Comox (1970)"),
         ("Dyck", "Ruth Anne Dyck, Squamish stress (2004)"), ("Czaykowska-Higgins", "Ewa Czaykowska-Higgins (1998)"),
         ("Keating", "Patricia A. Keating (1988)"), ("Meyers", "Scott Meyers (1997)"), ("Rubach", "Jerzy Rubach (2000)"),
         ("Suttles", "Wayne Suttles, Musqueam (2004)"), ("Galloway", "Brent Galloway, Upriver Halkomelem (1993)"),
         ("McCarthy", "John J. McCarthy; McCarthy and Prince (1994, 1995)"), ("Prince", "Alan Prince"),
         ("Smolensky", "Paul Smolensky; Smolensky and Prince (1993)"), ("Urbancyzk", "Urbanczyk (2001), so spelled")]
LANGUAGES = [(LANGUAGE, "Central Salish, also known as Comox-Sliammon"), ("Comox-Sliammon", "ʔayʔaǰuθəm"),
             ("Sliammon", "ʔayʔaǰuθəm, in the titles of Blake and Watanabe"),
             ("Mainland Comox", "ʔayʔaǰuθəm, in the titles of Davis and Watanabe"),
             ("Proto-Salish", "*ʔac- (Kinkade 1996)"), ("Kwak’wala", "Northern Wakashan, whose influence lost the prefixes"),
             ("Northern Wakashan", "the branch of Kwak’wala"), ("Musqueam", "durative and resultative (Suttles 2004)"),
             ("Upriver Halkomelem", "durative and resultative (Galloway 1993)"),
             ("Lushootseed", "a C-Final-Root constraint (Urbanczyk 2001)"), ("Squamish", "root shape (Dyck 2004)"),
             ("Nxaʔamxcín", "the phonological stem (Czaykowska-Higgins 1998)"),
             ("Moses-Columbia Salish", "Nxaʔamxcín, in Czaykowska-Higgins's title"),
             ("Skwxwú7mesh", "Squamish, in Dyck's title"), ("Slavic languages", "in Rubach's title")]

FOOTNOTES = paper.page_footnotes()
AT_FOOT = {one for parts, _ in FOOTNOTES.values() for one in parts}
RUNNING = paper.running_numbers_set()
REFERENCES = paper.find(r"^References$")
HEADINGS = paper.headings(1, REFERENCES - 1, skip=AT_FOOT)
# A pitch pattern, H H, H L M or HL.
PITCH = re.compile(r"^[HLM](?: ?[HLM])*$")


def printed(number):
    return bool(paper.text(number).strip()) and not paper.lines[number][2] and number not in RUNNING \
        and number not in AT_FOOT


def after(number):
    number += 1
    while number < REFERENCES and not printed(number):
        number += 1
    return number


def cells(number):
    return [one for one in re.split(r"\s{3,}", paper.spaced[number].strip()) if one]


def prose(number):
    """Whether a line is running prose or a heading: a single column of nine words or more."""
    text = paper.text(number)
    return number in HEADINGS or (len(cells(number)) == 1 and len(text.split()) >= 9)


def caption(start):
    """The number and caption of the example at start, the caption a note with its footnote mark,
    (37)'s 26, for the footnote to follow."""
    label, text = gen.EXAMPLE.match(paper.text(start)).groups()
    if text:
        paper.add("(%s)" % label, A, "note", text, "page %d, the caption" % paper.page(start))
    return label


def gloss_row(where, text, number):
    """A translation row. A footnote mark after its closing quote, (32)'s 25, stays in the text,
    where the footnotes are placed by their marks, and the gloss names it."""
    mark = re.search(r"’(\d+)$", text)
    paper.add(where, A, "translation", text, "page %d%s" % (
        paper.page(number), ", carries footnote " + mark.group(1) if mark else ""))


def pairs(start, where):
    """A list of pairs, a form and its gloss twice a line: the column heads a note, then each form a
    transcription row and each gloss a translation row. A (cf. ...) line under a pair is a note, its
    form a cited form."""
    label = caption(start)
    line, count = after(start), 0
    while line < REFERENCES and not prose(line) and not gen.EXAMPLE.match(paper.text(line)):
        count += 1
        where = "(%s) line %d" % (label, count)
        text = paper.text(line)
        if text.startswith("(cf."):
            paper.add(where, A, "note", text, "page %d, under the pair above" % paper.page(line))
            paper.cited(where, text, [paper.page(line)])
        else:
            for one in cells(line):
                if one.startswith("‘"):
                    gloss_row(where, one, line)
                else:
                    paper.add(where, L, "transcription", one, "page %d" % paper.page(line))
        line = after(line)
    return line


def listed(start, where):
    """A list of forms: each line a form, its phonetic form in brackets, its pitch pattern and its
    gloss. (8) sets the stative's phonetic form under its eventive with no form of its own; (35) sets
    Watanabe's page where two forms have no phonetic form; (36) and (37) open each line on the root."""
    label = caption(start)
    line, count = after(start), 0
    while line < REFERENCES and not prose(line) and not gen.EXAMPLE.match(paper.text(line)):
        count += 1
        where = "(%s) line %d" % (label, count)
        row = cells(line)
        forms = [one for one in row if not one.startswith(("[", "‘", "`", "(")) and not PITCH.match(one)]
        for one in row:
            merged = re.match(r"^(\[[^\]]*\]) ([HLM ]+)$", one)
            if merged:
                paper.add(where, L, "phonetic", merged.group(1), "page %d" % paper.page(line))
                paper.add(where, L, "phonetic", merged.group(2), "page %d, the pitch pattern" % paper.page(line))
            elif one.startswith("["):
                paper.add(where, L, "phonetic", one, "page %d%s" % (
                    paper.page(line), ", the stative of the form above" if row[0] == one else ""))
            elif PITCH.match(one):
                paper.add(where, L, "phonetic", one, "page %d, the pitch pattern" % paper.page(line))
            elif one.startswith(("‘", "`")):
                gloss_row(where, one, line)
            elif one.startswith("("):
                paper.add(where, A, "citation", one, "page %d, the source in place of a phonetic form" % paper.page(line))
            else:
                paper.add(where, L, "transcription", one, "page %d%s" % (
                    paper.page(line), ", the root" if len(forms) == 2 and one == forms[0] else ""))
        line = after(line)
    return line


def lettered(start, where):
    """Stem boundaries: a. and a form with its brackets and subscript stem, and its gloss."""
    label = caption(start)
    line = after(start)
    while line < REFERENCES and re.match(r"^[a-z]\. ", paper.text(line)):
        letter, form, gloss = re.match(r"^([a-z])\.\s+(.*?)\s+([‘`].*)$", paper.text(line)).groups()
        paper.add("(%s%s) line 1" % (label, letter), L, "transcription", form,
                  "page %d, the stem boundaries marked ]stem" % paper.page(line))
        gloss_row("(%s%s) line 2" % (label, letter), gloss, line)
        line = after(line)
    return line


# The definitions whose second column runs on in lines as long as prose, each ended at the paragraph
# the page prints after it.
RUNS_TO = {"11": r"^The Priority constraint accounts", "12": r"^This is a modification",
           "14": r"^The tableau in \(15\)", "18": r"^Together with \(19\)",
           "19": r"^Returning to the question", "23": r"^All these constraints",
           "38": r"^The forms in \(40\)"}


def definition(start, where):
    """A constraint's definition or a ranking: a note to each printed line, a ranking's line as long
    as prose kept by its »."""
    label = caption(start)
    line, count = after(start), 0
    ends = RUNS_TO.get(label)
    while line < REFERENCES and not gen.EXAMPLE.match(paper.text(line)) and \
            (re.match(ends, paper.text(line)) is None if ends else "»" in paper.text(line) or not prose(line)):
        count += 1
        paper.add("(%s) line %d" % (label, count), A, "note", paper.text(line), "page %d" % paper.page(line))
        line = after(line)
    return line


# The heads the glyph rows cannot read, set on their side over (20), (22), (24) and (25).
TURNED = {"20": "*Float, * Mult-Link, MaxH, Final-CStem, Dep, Priority, Max, Align-LH",
          "22": "*Float, * Mult-Link, MaxH, Final-CStem, Dep, Priority, Max, Align-LH",
          "24": "Final-CStem, Align-RStative, Dep, Priority, Max, Align-LH",
          "25": "*Float, * Mult-Link, MaxH, Final-CStem, Align-RStative, Dep, Priority, Max, Align-LH"}
CANDIDATE = re.compile(r"^([a-z])\.\s*([☞☹])?\s*(\S+)\s*(.*)$")


def tableau(start, where):
    """A tableau: its input a phonemic row and its constraint heads a note, each candidate a phonemic
    row and its violation marks a note, and a line of tones over the input or a candidate a note. The
    subscript 1 and 2 of each allomorph are read on the line as digits."""
    label = caption(start)
    line, count, pending = after(start), 0, None

    def row(who, kind, form, gloss):
        nonlocal count
        count += 1
        paper.add("(%s) line %d" % (label, count), who, kind, form, "page %d, %s" % (paper.page(line), gloss))

    first = True
    while line < REFERENCES and not prose(line) and not gen.EXAMPLE.match(paper.text(line)):
        text, row_cells = paper.text(line), cells(line)
        candidate = CANDIDATE.match(text)
        if re.match(r"^%?H(?: +H)*(?:\s|$)", text):
            pending = row_cells[0]
            row(A, "note", pending, "the tones over the input" if first else "the tones linked over the candidate under")
        elif candidate:
            letter, hand, form, marks = candidate.groups()
            row(L, "phonemic", form, "candidate (%s%s)%s" % (label, letter, {
                "☞": ", the winner, printed with the pointing hand ☞", "☹": ", the attested form the ranking "
                "does not choose, printed with ☹"}.get(hand, "")))
            if marks:
                row(A, "note", marks, "the violation marks of (%s%s) in the order printed" % (label, letter))
        elif first and re.search(r"[+(]", row_cells[0]):
            row(L, "phonemic", row_cells[0], "the input")
            heads = TURNED.get(label) or " ".join(row_cells[1:])
            row(A, "note", heads, "the constraint heads%s" % (
                ", set on their side and read off the render" if label in TURNED else ""))
            first = False
        line = after(line)
    return line


def table_1(start, where):
    """Table 1: its caption and column heads notes, then each allomorph a cited form, its type a
    note, its example a cited form glossed, and Watanabe's page a citation."""
    page = paper.page(start)
    paper.add("Table 1", A, "note", paper.text(start), "page %d, the table's caption" % page)
    paper.add("Table 1", A, "note", paper.text(start + 1), "page %d, the column heads" % page)
    for number in range(start + 2, start + 6):
        allomorph, kind, example, gloss, cited = re.match(
            r"^(-it|-i-|Secondary stress)\s+(\S+ \([^)]*\))\s+(\S+)\s+(‘[^’]*’)\s+(p\. \d+)$", paper.text(number)).groups()
        paper.add("Table 1", L if allomorph.startswith("-") else A, "cited form" if allomorph.startswith("-") else "note",
                  allomorph, "page %d, Table 1, the allomorph" % page)
        paper.add("Table 1", A, "note", kind, "page %d, Table 1, the type" % page)
        paper.add("Table 1", L, "cited form", example, "page %d, Table 1, %s" % (page, gloss))
        paper.add("Table 1", A, "translation", gloss, "page %d, Table 1, the gloss of %s" % (page, example))
        paper.add("Table 1", A, "citation", cited, "page %d, Table 1, the page of Watanabe (2003)" % page)
    return start + 6


KINDS = {pairs: (1, 2, 3, 27, 28, 41), listed: (4, 5, 6, 7, 8, 9, 10, 29, 30, 31, 32, 33, 34, 35, 36, 37),
         lettered: (39, 40, 42, 43), definition: (11, 12, 14, 16, 18, 19, 21, 23, 26, 38),
         tableau: (13, 15, 17, 20, 22, 24, 25)}
blocks = {paper.find(r"^Table 1: "): table_1}
for number in range(1, REFERENCES):
    opened = printed(number) and gen.EXAMPLE.match(paper.text(number))
    if opened:
        blocks[number] = next(kind for kind, labels in KINDS.items() if int(opened.group(1)) in labels)
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, headings=HEADINGS, appendix=r"^Appendix A$")
# Footnote 25 sets y in italics beside ǰ, a single letter the orthographic test passes over.
paper.add("footnote 25", L, "cited form", "y", "page %d, in italics, the coda alternant of ǰ"
          % paper.page(paper.find(r"^25 ǰ in onset")))
# Appendix A: its heading, its paragraph, and the two pitch tracks, each figure's axes a note and its
# caption a note with its form a cited form.
start = paper.find(r"^Appendix A$")
paper.add("Appendix A", A, "heading", "Appendix A", "page %d" % paper.page(start))
body = " ".join(paper.text(number) for number in range(start + 1, start + 6))
paper.add("Appendix A", A, "note", body, "page %d" % paper.page(start))
paper.cited("Appendix A", body, [paper.page(start)])
for figure in (1, 2):
    at = paper.find(r"^Figure %d: " % figure)
    ticks = paper.text(at - 2)
    paper.add("Figure %d" % figure, A, "note", "0 to 5000 (Hz); Time (s) %s" % ticks.replace("   ", " to "),
              "page %d, the axes of the pitch track, the frequency axis, Frequency (Hz), set on its side"
              % paper.page(at))
    paper.add("Figure %d" % figure, A, "note", paper.text(at), "page %d, the figure's caption" % paper.page(at))
    paper.cited("Figure %d" % figure, paper.text(at), [paper.page(at)])
paper.write()
