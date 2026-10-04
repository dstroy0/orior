"""The ops of 09-Crowgey_ICSNL50_final-6: Joshua Crowgey on the dissertation question of semantic
composition in Lushootseed roots and stems: whether a verb's characteristic variable is underspecified
for semantic type in the lexicon, and a metagrammar testing environment in which competing analysis
becomes an empirical question.

Example (1) is an MRS representation of English 'Kim walks', a note to each printed predication. (2)
is the research question, a note, and its three bullets a note each. Figure 1 is drawn on the page, its
labels in the text layer: a note to each box, the gloss naming where it stands and what its arrows join,
then the caption.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
LANGUAGE = "Lushootseed"
AUTHORS = ["Joshua Crowgey"]
paper = gen.Paper("09-Crowgey_ICSNL50_final-6", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Davis and Matthewson", "Henry Davis and Lisa Matthewson, the primacy of the root (2009)"),
         ("Matthewson", "Lisa Matthewson, with Davis (2009)"),
         ("Hess", "Thom Hess, Lushootseed verb stems (1993); Beck and Hess (2014, 2015)"),
         ("Mattina", "Nancy Mattina, Okanagan word formation (1996)"),
         ("Willet", "Marie-Louise Willet, Nxa’amxcin (2003)"),
         ("Copestake", "Ann Copestake, Minimal Recursion Semantics (2005)"),
         ("Davidson", "Donald Davidson, action sentences (1967)"), ("Parsons", "Terence Parsons, events (1990)"),
         ("Szabó", "Zoltán Gendler Szabó, compositionality (2013)"),
         ("Pollard", "Carl Pollard, HPSG (1994)"), ("Sag", "Ivan A. Sag, HPSG (1994)"),
         ("Bender and Good", "Emily M. Bender and Jeff Good, a bipartite lexicon (2005)"),
         ("Beck and Hess", "David Beck and Thom Hess, Tellings from Our Elders (2014, 2015)"),
         ("Fokkens", "Antske Fokkens, metagrammar engineering (2011, 2014)"),
         ("Crowgey", "the author, the morphophonological analyzer (2014)"),
         ("Miellet", "Antoine Meillet, spelled Miellet (1903)"), ("Emily Bender", "translated Miellet (2008)")]
LANGUAGES = [(LANGUAGE, "Central Salish"), ("Salishan", "the family"), ("English", "'Kim walks'")]

FIGURE = paper.find(r"^Text Processing System$")
CAPTION = paper.find(r"^Figure 1: ", FIGURE)
# Figure 1's boxes as the page draws them, each with where it stands and what its arrows join.
BOXES = [("Text Processing System", "above, the label over a dashed box that holds FST and Grammar"),
         ("FST Gen System", "above left, a box whose arrow points into FST"),
         ("FST", "above, in the dashed box"),
         ("Grammar", "above, in the dashed box"),
         ("Grammar Gen System", "above right, a box whose arrow points into Grammar"),
         ("Lexical DB", "above, under the rest, a box whose curved arrows point up to FST Gen System and "
          "Grammar Gen System"),
         ("Lushootseed orthographic sentences", "below, the first box of the pipeline, a double arrow to FST"),
         ("FST", "below, the second box, double arrows to its neighbours"),
         ("Canonical forms word-lattice", "below, the third box, double arrows to its neighbours"),
         ("Grammar", "below, the fourth box, double arrows to its neighbours"),
         ("HPSG signs", "below, the last box, signs in italics")]


def figure(start, where):
    """Figure 1: a note to each box, then the caption, one note."""
    page = paper.page(start)
    for count, (label, place) in enumerate(BOXES, 1):
        paper.add("Figure 1 box %d" % count, A, "note", label, "page %d, a box drawn in the figure, %s" % (page, place))
    line, text = CAPTION, []
    while not text or not text[-1].endswith("."):
        text.append(paper.text(line))
        line += 1
    paper.add("Figure 1", A, "note", " ".join(text), "page %d, the figure's caption" % page)
    return line


def predications(start, where):
    """(1): a note to each printed predication."""
    line = start
    for count in range(1, 4):
        text = re.sub(r"^\(1\)\s+", "", paper.text(line))
        paper.add("(1) line %d" % count, A, "note", text,
                  "page %d, an MRS elementary predication for English 'Kim walks'" % paper.page(line))
        line += 1
    return line


def question(start, where):
    """(2): the question a note, then a note to each bullet."""
    end = paper.find(r"^5 Conclusion$", start)
    parts, line = [[re.sub(r"^\(2\)\s+", "", paper.text(start))]], start + 1
    while line < end:
        text = paper.text(line)
        if text.startswith("•"):
            parts.append([text])
        else:
            parts[-1].append(text)
        line += 1
    paper.add("(2)", A, "note", " ".join(parts[0]), "page %d, the research question" % paper.page(start))
    for count, part in enumerate(parts[1:], 1):
        paper.add("(2) bullet %d" % count, A, "note", " ".join(part), "page %d, a bulleted item" % paper.page(start))
    return end


blocks = {FIGURE: figure, paper.find(r"^\(1\)\s"): predications, paper.find(r"^\(2\) What are"): question}
found = paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)

# The predications of (1) end h4) and h5:, which place_footnotes takes for the marks of footnotes 4
# and 5; each goes after the note that carries its mark on the page.
for mark, carrier in (("4", "environment.4 Point"), ("5", "hang-together”5 I")):
    moved = [row for row in paper.rows if row[0] == "footnote " + mark]
    paper.rows = [row for row in paper.rows if row[0] != "footnote " + mark]
    at = next(index for index, row in enumerate(paper.rows) if row[2] == "note" and carrier in row[3])
    paper.rows[at + 1:at + 1] = moved
paper.write()
