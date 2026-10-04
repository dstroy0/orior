"""The ops of 2011_Stewart: Catherine Stewart on Kwak'wala demonstrative predicates and clefts, the
stems yu-, he- and gʸe- read by a visibility distinction after Nicholson and Werle (2009) in place of
Boas's person-based one, and the clefts of focus constructions set against Koch's (2008) account of
Nɬeʔkepmxcin.

The page text is the closeup reading: the dots below of x̣ sit out of order in the glyph rows, and the
default reading leaves a space inside the word. The examples keep no common shape. Most set a word
tier over a segmentation and a gloss and a translation in quotes, some with a remark in parentheses
at the right of the translation and a source under it; a Context: or Answers: line opens many; (2)
and (36) set two tiers only, and (9) and (39) wrap their last words onto three more lines. Each
example is written from a table of its lines and their tiers. Table 1 sets the personal pronoun stems, each row the person, the
stem and an example, (24) to (28).
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Kwak'wala"
AUTHORS = ["Catherine Stewart"]
paper = gen.Paper("2011_Stewart", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Boas", "Franz Boas, the Kwakiutl Grammar (1947) and its person-based analysis of the stems"),
         ("Nicholson", "Marianne Nicolson and Adam Werle, modern Kwak'wala determiner systems (2009)"),
         ("Koch", "Karsten Koch, intonation and focus in Nɬeʔkepmxcin (2008) and its clefts (2009)"),
         ("Kroeber", "the cleft-focus observation (1997, 1999)"),
         ("Chung", "Yunhee Chung, the Kwak'wala nominal domain (2007), cited as 2006"),
         ("Levine", "cited on the discourse enclitics as referring to old information"),
         ("Lincoln & Rath", "Neville Lincoln and John Rath, the North Wakashan comparative root list (1980)"),
         ("Henry Davis", "lectures for Ling 447A (2010), thanked"),
         ("Patrick Littell", "notes on Kwak'wala focus constructions (2010), the source of (17), thanked"),
         ("Michael Rochement", "lectures for Ling 447B (2011), thanked"),
         ("Ruby Dawson Cranmer", "the author's consultant, thanked")]
LANGUAGES = [(LANGUAGE, "Northern Wakashan, the language studied"),
             ("Nɬeʔkepmxcin", "Thompson River Salish, Koch's language of focus and clefts"),
             ("English", "its demonstratives this and that and its clefts"),
             ("Wakashan", "the family of Kwak'wala"), ("Oowekyala", "Northern Wakashan"),
             ("Heiltsuk", "Northern Wakashan"), ("Haisla", "Northern Wakashan"),
             ("Nitinat", "Southern Wakashan"), ("Nootka", "Southern Wakashan"), ("Makah", "Southern Wakashan")]

KINDS = {"T": "transcription", "S": "segmentation", "G": "gloss", "R": "translation", "C": "citation"}
# Each example: its label and the lines of each tier, in order. X is a Context: or Answers: line,
# N a note under the example, R the translation with any remark in parentheses at its right; a
# tuple is one tier the page wraps over two or three lines.
EXAMPLES = {
    183: ("1", [("T", 183), ("S", 184), ("G", 186), ("R", 187), ("C", 188)]),
    206: ("2", [("S", 206), ("G", 207), ("R", 208), ("C", 209)]),
    224: ("3", [("T", 224), ("S", 225), ("G", 226), ("R", 227)]),
    229: ("4", [("T", 229), ("S", 230), ("G", 231), ("R", 232)]),
    234: ("5", [("T", 234), ("S", 235), ("G", 236), ("R", 237)]),
    239: ("6", [("X", 239)]),
    241: ("6a", [("T", 241), ("S", 242), ("G", 244), ("R", 245)]),
    312: ("6b", [("T", 312), ("S", 313), ("G", 314), ("R", 315), ("N", (316, 317))]),
    341: ("7", [("T", 341), ("S", 342), ("G", 343), ("R", 344)]),
    346: ("8", [("T", 346), ("S", 347), ("G", 348), ("R", 349)]),
    351: ("9", [("T", 351), ("S", 352), ("G", 353), ("T", 354), ("S", 355), ("G", 356), ("R", 357)]),
    370: ("10", [("T", 370), ("S", 371), ("G", 372), ("R", 373)]),
    375: ("11", [("T", 375), ("S", 376), ("G", 377), ("R", 378)]),
    380: ("12", [("T", 380), ("S", 381), ("G", 382), ("R", 383)]),
    385: ("13", [("T", 385), ("S", 386), ("G", 387), ("R", 388)]),
    390: ("14", [("T", 390), ("S", 391), ("G", 392), ("R", 393)]),
    408: ("15", [("T", 408), ("S", 409), ("G", 410), ("R", 411)]),
    413: ("16", [("T", 413), ("S", 414), ("G", 415), ("R", 416)]),
    418: ("17", [("T", 418), ("S", 419), ("G", 420), ("R", (421, 422)), ("C", 423)]),
    434: ("Chung (2006) (5a)", [("T", 434), ("S", 435), ("G", 437), ("R", 438), ("C", 439)]),
    446: ("18", [("X", (446, 447, 448))]),
    450: ("18a", [("T", 450), ("S", 451), ("G", 453), ("R", 454)]),
    458: ("18b", [("T", 458), ("S", 459), ("G", 460), ("R", 461), ("N", 462)]),
    464: ("19", [("X", (464, 465)), ("T", 467), ("S", 468), ("G", 469), ("R", 470), ("N", 471)]),
    488: ("20", [("X", (488, 489, 490)), ("T", 492), ("S", 493), ("G", 494), ("R", 495)]),
    508: ("21", [("X", 508), ("T", 509), ("S", 510), ("G", 511), ("R", 512)]),
    517: ("22", [("X", 517), ("T", 518), ("S", 519), ("G", 520), ("R", 521)]),
    525: ("23", [("X", 525), ("T", 526), ("S", 527), ("G", 528), ("R", 529)]),
    591: ("29", [("T", 591), ("S", 592), ("G", 593), ("N", 594), ("R", 595)]),
    609: ("30a", [("T", 609), ("S", 610), ("G", 611), ("R", 612)]),
    614: ("30b", [("T", 614), ("S", 615), ("G", 616), ("R", 617)]),
    707: ("35", [("X", 707), ("T", 708), ("S", 709), ("G", 710), ("R", 711)]),
    713: ("36", [("X", 713), ("S", 714), ("G", 715), ("R", 716)]),
    725: ("37", [("X", 725), ("T", 726), ("S", 727), ("G", 728), ("R", 729)]),
    731: ("38", [("X", 731), ("T", 732), ("S", 733), ("G", 734), ("R", 735)]),
    737: ("39", [("X", 737), ("T", 738), ("S", 739), ("G", 740), ("T", 741), ("S", 742), ("G", 743),
                 ("R", 744)]),
    756: ("40", [("X", 756), ("T", 757), ("S", 758), ("G", 759), ("R", 760)]),
    762: ("41", [("X", 762), ("T", 763), ("S", 764), ("G", 765), ("R", 766)]),
    768: ("42", [("X", 768), ("T", 769), ("S", 770), ("G", 771), ("R", 772)]),
    782: ("43", [("X", 782), ("T", 783), ("S", 784), ("G", 786), ("R", 787)]),
    789: ("44", [("X", 789), ("T", 790), ("S", 791), ("G", 792), ("R", 793)]),
    795: ("45", [("X", 795), ("T", 796), ("S", 797), ("G", 798), ("R", 799)]),
    808: ("46", [("X", 808), ("T", 809), ("S", 810), ("G", 811), ("R", 812)]),
    814: ("47", [("X", 814), ("T", 815), ("S", 816), ("G", 817), ("R", 818)]),
    824: ("48", [("T", 824), ("S", 825), ("G", 826), ("R", 827)]),
    850: ("49", [("X", 850), ("T", 851), ("S", 852), ("G", 853), ("R", 854)]),
    863: ("50", [("X", (863, 864)), ("T", 865), ("S", 866), ("G", 867), ("R", 868)]),
}
# Table 1's rows: the row's first line, the first line of its example, the person, the stem form
# and the example's number. The person 3rd Person Singular (m/f) and the stem form nu-…- / ənox̣
# wrap in their cells; the stem form keeps its break as printed, a space after the hyphen.
TABLE = {539: (540, "1st Person Singular", "nu-", "24"), 544: (545, "2nd Person Singular", "k'u-", "25"),
         549: (551, "3rd Person Singular (m/f)", "k'ɑ-", "26"),
         558: (560, "1st Person Plural", "nu-…- ənox̣", "27"), 564: (565, "2nd Person Plural", "k'u-", "28")}
OPENS = set(EXAMPLES) | {row[0] for row in TABLE.values()}
LABEL = re.compile(r"^(?:\(\w+\)\s*)?(?:[ab]\.\s*)?")
PAREN_RIGHT = re.compile(r"^(.*[’'])\s+(\(.*\))$")


def tier_text(lines):
    return paper.text(lines) if isinstance(lines, int) else paper.joined(lines)


def write_example(label, tiers):
    """Write one example's tiers in order, each a row labeled by its line in the example."""
    for count, (kind, lines) in enumerate(tiers, 1):
        first = lines if isinstance(lines, int) else lines[0]
        page = paper.page(first)
        text = tier_text(lines)
        if first in OPENS:
            text = LABEL.sub("", text)
        where = ("%s line %d" if label.startswith("Chung") else "(%s) line %d") % (label, count)
        if kind == "X":
            paper.add(where, A, "note", text, "page %d, the context the example answers" % page)
        elif kind == "N":
            paper.add(where, A, "note", text, "page %d, under the example" % page)
        elif kind == "R":
            said = PAREN_RIGHT.match(text)
            paper.add(where, A, "translation", said.group(1) if said else text, "page %d" % page)
            if said:
                paper.add(where, A, "note", said.group(2), "page %d, at the right of the translation" % page)
        else:
            paper.add(where, L if kind in "TSG" else A, KINDS[kind], text, "page %d" % page)
        if kind in ("X", "N", "R"):
            paper.mentions(where, text, NAMES, "name")


def example(first, where):
    label, tiers = EXAMPLES[first]
    write_example(label, tiers)
    return max(lines if isinstance(lines, int) else max(lines) for _, lines in tiers) + 1


def table_row(first, where):
    start, person, stem, label = TABLE[first]
    page = paper.page(first)
    paper.add("Table 1", A, "note", person, "page %d, a row's person" % page)
    paper.add("Table 1", L, "cited form", stem, "page %d, the stem form of the %s%s" % (
        page, person, ", wrapped in its cell after nu-…-" if " " in stem else ""))
    write_example(label, [("T", start), ("S", start + 1), ("G", start + 2), ("R", start + 3)])
    return start + 4


def table_head(first, where):
    """The caption over Table 1, and its header, Stem Form set over two lines in its cell."""
    page = paper.page(first)
    paper.add("Table 1", A, "note", paper.text(first), "page %d, the table's caption" % page)
    paper.add("Table 1", A, "note", "Person Stem Form Example", "page %d, the table's header" % page)
    return first + 3


def display(first, where):
    paper.add(where, A, "note", paper.text(first),
              "page %d, the order of the clitics, set as a display" % paper.page(first))
    return first + 1


def statement(first, where):
    """Koch's constraints (31) and (32) and his predictions (33) and (34), a note each."""
    label = gen.EXAMPLE.match(paper.text(first)).group(1)
    what = "a constraint Koch postulates" if label in ("31", "32") else "a prediction of Koch's for focus projection"
    paper.add("(%s) line 1" % label, A, "note", LABEL.sub("", paper.joined([first, first + 1])),
              "page %d, %s" % (paper.page(first), what))
    return first + 2


def abbreviations(first, where):
    paper.add("appendix A", A, "heading", paper.text(first), "page %d" % paper.page(first))
    number = first + 1
    while True:
        text = paper.text(number)
        if text:
            paper.add("appendix A", A, "note", text, "page %d, an abbreviation" % paper.page(number))
        if text.startswith("P – "):
            return number + 1
        number += 1


BLOCKS = {first: example for first in EXAMPLES}
BLOCKS.update({first: table_row for first in TABLE})
BLOCKS.update({536: table_head, 162: display, 924: abbreviations,
               666: statement, 669: statement, 676: statement, 679: statement})
for first, opening in ((162, "CASE-LOC-DET"), (536, "Table 1:"), (666, "(31)"), (679, "(34)"),
                       (924, "A. Abbreviations")):
    assert paper.text(first).startswith(opening), first
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS, notes_title=("1",),
                       references=r"^References:$", appendix=r"^Catherine Stewart$")
# The abstract, set with no heading under the university, is one note.
lines = [index for index, row in enumerate(paper.rows) if row[0] == "front" and row[2] == "note"
         and row[4] == "page 1, under Catherine Stewart"][1:]
if lines:
    paper.rows[lines[0]][3] = " ".join(paper.rows[index][3] for index in lines)
    paper.rows[lines[0]][4] = "page 1, the abstract"
    paper.rows = [row for index, row in enumerate(paper.rows) if index not in lines[1:]]
# The heading of §3.6 holds a stop, vs., and the heading reader passes it by; the section runs to §4.
first = next(index for index, row in enumerate(paper.rows) if row[3].startswith("3.6 Demonstrative Predicates"))
paper.rows[first][2] = "heading"
for row in paper.rows[first:]:
    if row[0] == "§4":
        break
    if row[0] == "§3.5":
        row[0] = "§3.6"
# Two references the reader parts wrongly: Boas and Yampolsky's dictionary of 1948 runs onto a line
# that opens Philadelphia, and Davis's lectures, one line, stand over Grubb's entry.
rows = paper.rows
at = next(index for index, row in enumerate(rows) if row[3].startswith("Philadelphia, PA:"))
rows[at - 1][3] += " " + rows[at][3]
del rows[at]
at = next(index for index, row in enumerate(rows) if row[3].startswith("Davis, Henry."))
davis, grubb = rows[at][3].split(" Grubb, David", 1)
rows.insert(at + 1, rows[at][:3] + ["Grubb, David" + grubb] + rows[at][4:])
rows[at][3] = davis
tail = paper.find(r"^Catherine Stewart$", 954)
paper.add("end", A, "note", paper.joined([tail, tail + 1]),
          "page %d, the author's name and e-mail" % paper.page(tail))
paper.write()
