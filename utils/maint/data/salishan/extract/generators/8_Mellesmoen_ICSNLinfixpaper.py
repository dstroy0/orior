"""The ops of 8_Mellesmoen_ICSNLinfixpaper: Gloria Mellesmoen on paʔapyaʔ ‘one by one’ in
Comox-Sliammon, composed of diminutive CV reduplication, a pluractional -Vʔ- infix and the numeral,
the infix a temporal pluractional marker requiring subevents that do not overlap in time.

The interlinear examples set a form, its gloss and a translation, and under them any number of
contexts, each wrapping over lines until it ends on a stop or on the initials of the speaker who gave
it; the initials are a citation. (39) sets the consultant's correction as a form, gloss and
translation of its own. (7) and (22) are English sentences with their contexts. (13), (15) and (40)
set their parts side by side and are read by gen.Paper.columns; (14) sets each part a line of forms,
each beside its gloss in quotes. (16) and (25) to (27) are lexical entries, a formula over three
printed lines; (21), (23) and (24) set a formula to a line. Table 1 is a note to each printed line.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Comox-Sliammon"
AUTHORS = ["Gloria Mellesmoen"]
paper = gen.Paper("8_Mellesmoen_ICSNLinfixpaper", authors=", ".join(AUTHORS), language=LANGUAGE)

SPEAKER = "Comox-Sliammon speaker, thanked in the note on the title"
# The speakers by the initials each example ends on.
INITIALS = {"JF": "Joanne Francis", "PD": "Phyllis Dominic", "EP": "Elsie Paul", "FL": "Freddie Louie",
            "MH": "Marion Harry"}
NAMES = [(name, SPEAKER) for name in INITIALS.values()] + [
    ("Henry Davis", "thanked"), ("Lisa Matthewson", "thanked; the analysis of pəlpálaʔ/pipálaʔ (2000)"),
    ("Hotze Rullmann", "thanked"), ("Marianne Huijsmans", "helped with elicitation; an editor of the volume"),
    ("Kaining Xu", "helped with elicitation"), ("Shannon Arsenault", "English grammaticality judgments"),
    ("Darvell Long", "English grammaticality judgments"), ("Roger Lo", "an editor of the volume"),
    ("Daniel Reisinger", "an editor of the volume"), ("Oksana Tkachman", "an editor of the volume"),
    ("Lasersohn", "P. N. Lasersohn, pluractionality (1995)"), ("Watanabe", "H. Watanabe, Sliammon grammar (2003)"),
    ("Beaumont", "R. C. Beaumont, Sechelt dictionary (2011)"), ("Blake", "S. J. Blake, schwa in Sliammon (2000)"),
    ("Kratzer", "A. Kratzer, event semantics (2003)"), ("Krifka", "M. Krifka, numbers with alternatives (1999)"),
    ("Jelinek", "E. Jelinek, quantification in Straits Salish (1995)"),
    ("Koontz-Garboden", "A. Koontz-Garboden, the Monotonicity Hypothesis (2007)"),
    ("Anderson", "G. D. Anderson, reduplicated numerals in Salish (1999)"),
    ("Drachman", "G. Drachman, Twana phonology (1969)"), ("Kuipers", "A. H. Kuipers, Salish etymological dictionary (2002)"),
    ("Urbanczyk", "S. Urbanczyk, enhancing contrast in reduplication (2005)")]
LANGUAGES = [(LANGUAGE, "Central Salish, British Columbia"), ("ʔayʔaǰuθəm", "the language's own name"),
             ("Lillooet", "Interior Salish, pəlpálaʔ/pipálaʔ"), ("Sechelt", "Central Salish, pápəla"),
             ("Twana", "Central Salish, Table 1"), ("Lushootseed", "Central Salish, Table 1"),
             ("Klallam", "Central Salish, Table 1"), ("Saanich", "Central Salish, Table 1"),
             ("Musqueam", "Central Salish, Table 1"), ("Thompson", "Interior Salish, paʔa cognates"),
             ("Proto-Salish", "*nak̓/*nk̓-uʔ ‘one’"), ("English", "each, one")]
RUNNING = paper.running_numbers_set()
FOUND = paper.page_footnotes()
SKIP = {one for parts, _ in FOUND.values() for one in parts}
# A line that ends a translation or a context: a stop, a closing quote or bracket.
ENDS = re.compile(r"[.!?’)]$")
# The initials of the speaker at the right of the line that ends an example's part.
SPOKEN = re.compile(r"^(.*?)\s+(%s)$" % "|".join(INITIALS))


def printed(number):
    """Whether line number holds printed text of the body: page breaks, page numbers, blank lines and
    footnotes hold none."""
    return not paper.lines[number][2] and paper.text(number) and number not in RUNNING and number not in SKIP


def after(number):
    """The next printed line after number."""
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def row(name, count, who, kind, form, number, gloss=None):
    paper.add("(%s) line %d" % (name, count), who, kind, " ".join(form.split()),
              "page %d%s" % (paper.page(number), ", " + gloss if gloss else ""))


def gathered(line):
    """(text, the line after it) for the translation or context opening on line, carried over the
    lines under it until one ends on a stop or on a speaker's initials."""
    text = paper.text(line)
    while not SPOKEN.match(text) and not ENDS.search(text):
        line = after(line)
        text += " " + paper.text(line)
    return text, after(line)


def said(name, count, who, kind, line, gloss=None):
    """Write the translation or context opening on line, with the speaker's initials at its end a
    citation; returns (the next count, the line after it)."""
    text, following = gathered(line)
    spoken = SPOKEN.match(text)
    row(name, count, who, kind, spoken.group(1) if spoken else text, line, gloss)
    if spoken:
        count += 1
        row(name, count, A, "citation", spoken.group(2), line, "the speaker's initials, %s" % INITIALS[spoken.group(2)])
    return count + 1, following


def contexts(name, count, line):
    """The contexts under a part, each a note; returns (the next count, the line after them)."""
    while re.match(r"^#?\s*Context:", paper.text(line)):
        count, line = said(name, count, A, "note", line, "a context")
    return count, line


def parts(first):
    """(label, letter or '', text) of the example opening on line first."""
    label, text = gen.EXAMPLE.match(paper.text(first)).groups()
    sub = gen.SUB.match(text)
    return (label,) + (sub.groups() if sub else ("", text))


def interlinear(first, where):
    """An example of form, gloss, translation and contexts, to each lettered part; (39) sets the
    consultant's correction under the contexts, a form, gloss and translation of its own."""
    label, letter, text = parts(first)
    line = first
    while True:
        name = label + letter
        row(name, 1, L, "transcription", text, line)
        line = after(line)
        row(name, 2, L, "gloss", paper.text(line), line)
        count, line = said(name, 3, A, "translation", after(line))
        count, line = contexts(name, count, line)
        consultant = re.match(r"^Consultant:\s+(.*)$", paper.text(line))
        if consultant:
            row(name, count, A, "note", "Consultant:", line, "the consultant's correction follows")
            row(name, count + 1, L, "transcription", consultant.group(1), line, "the consultant's correction")
            line = after(line)
            row(name, count + 2, L, "gloss", paper.text(line), line)
            count, line = said(name, count + 3, A, "translation", after(line))
        sub = gen.SUB.match(paper.text(line))
        if not sub:
            return line
        letter, text = sub.groups()


def english(first, where):
    """(7) or (22): to each part an English sentence, a note, and the contexts under it."""
    label, letter, text = parts(first)
    line = first
    while True:
        row(label + letter, 1, A, "note", text, line,
            "an English sentence" + (", # marking it infelicitous" if text.startswith("#") else ""))
        count, line = contexts(label + letter, 2, after(line))
        sub = gen.SUB.match(paper.text(line))
        if not sub:
            return line
        letter, text = sub.groups()


def entry(first, where):
    """(16) and (25) to (27): a lexical entry, a formula over three printed lines, the subscript m of
    its last line set on a line of its own under it. (16) ends on its source, a citation."""
    label, text = gen.EXAMPLE.match(paper.text(first)).groups()
    line = after(first)
    while paper.text(line) != "𝑚":
        text += " " + paper.text(line)
        line = after(line)
    row(label, 1, A, "note", text, first, "the lexical entry, a formula; its subscripts set inline")
    line = after(line)
    source = re.fullmatch(r"\([A-Z][a-z]+ \d{4}:\d+\)", paper.text(line))
    if source:
        row(label, 2, A, "citation", paper.text(line), line, "the formula's source")
        line = after(line)
    return line


def formulas(first, where):
    """(21), (23) and (24): to each lettered part a formula a line, each a note; (23) sets under each
    part's standard interpretation its alternatives, a formula of its own."""
    label, letter, text = parts(first)
    line, count = first, 0
    while True:
        count += 1
        row(label + letter, count, A, "note", text, line, "a denotation, a formula")
        line = after(line)
        text = paper.text(line)
        sub = gen.SUB.match(text)
        if sub:
            letter, text = sub.groups()
            count = 0
        elif not text.startswith("⟦"):
            return line


def glossed(first, where):
    """(14): to each part a line of forms, each form a transcription and its gloss in quotes a
    translation."""
    label, letter, text = parts(first)
    line = first
    while True:
        count = 0
        for form, meaning in re.findall(r"(\S+)\s+(‘[^’]*’)", text):
            row(label + letter, count + 1, L, "transcription", form, line)
            row(label + letter, count + 2, A, "translation", meaning, line)
            count += 2
        line = after(line)
        sub = gen.SUB.match(paper.text(line))
        if not sub:
            return line
        letter, text = sub.groups()


def table(first, where):
    """Table 1: its caption, the column heads and each language's row, each printed line a note."""
    paper.add("Table 1", A, "note", paper.text(first), "page %d, the table's caption" % paper.page(first))
    line = after(first)
    for count in range(1, 9):
        paper.add("Table 1 line %d" % count, A, "note", paper.text(line), "page %d, %s" % (
            paper.page(line), "the column heads" if count == 1 else "a language's forms"))
        line = after(line)
    return line


READERS = {"7": english, "22": english, "13": None, "15": None, "40": None, "14": glossed,
           "16": entry, "25": entry, "26": entry, "27": entry, "21": formulas, "23": formulas, "24": formulas}
blocks = {paper.find(r"^Table 1: "): table}
line = 1
for number in range(1, 41):
    # (17) opens a line of prose too, (17) and (18) have CV reduplication.
    line = paper.find(r"^\(%d\)\s+(?!and )" % number, line)
    reader = READERS.get(str(number), interlinear)
    blocks[line] = reader or (lambda first, where: paper.columns(first, skip=SKIP))
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)
# The source under (40), at the right below both columns, is a citation of the whole example.
source = next(one for one in paper.rows if one[0].startswith("(40") and one[3] == "(Watanabe 2003:401–402)")
source[0], source[2], source[4] = "(40) line 4", "citation", "page 16, the source of both parts"
# Four entries open where references() finds no entry's opening: the First Peoples' Cultural
# Council's, no surname and initials, and Jelinek's after it, which follows a web address that runs
# over a line; Matthewson's after Lasersohn's, which ends on no stop; and Van Eijk's, a surname of two
# words.
for opening in (" First Peoples’ Cultural Council. (2014).", " Jelinek, E. (1995).", " Matthewson, L. (2000).",
                " Van Eijk, J. (2011)."):
    holder = next(one for one in paper.rows if one[2] == "reference" and opening in one[3])
    split = holder[3].index(opening)
    paper.rows.insert(paper.rows.index(holder) + 1, [holder[0], holder[1], "reference", holder[3][split + 1:], holder[4]])
    holder[3] = holder[3][:split]
paper.write()
