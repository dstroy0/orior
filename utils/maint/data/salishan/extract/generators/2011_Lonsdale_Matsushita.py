"""The ops of 2011_Lonsdale_Matsushita: Deryle Lonsdale and Hitokazu Matsushita's Annotating and
exploring Lushootseed morphosyntax, a PC-Kimmo morphological parser and a Link Grammar parser run
over 500 Lushootseed sentences, their links loaded into a relational database and queried.

The page text is the default reading: the figures are parser output in Courier, a typewriter face,
and page_text keeps the text layer's own spaces between its glyphs. The Lushootseed of the figures is
the parsers' Romanized ASCII, gWEdsutudZildubut, ?u+ da?a. Each figure is read by a block: Figure
1's two words a transcription each, the parse under it a segmentation and a gloss; Figure 2's rules
a rule each; Figure 3's and Figure 5's sentences a transcription each, the link diagram over it a
note and the word row under the diagram a parse; Figure 4's links and the statistics a note each;
Figure 6's lexical suffixes a cited affix each, with its count, and its link types a note each.
"""
import re
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
AUTHORS = ["Deryle Lonsdale", "Hitokazu Matsushita"]
paper = gen.Paper("2011_Lonsdale_Matsushita", authors="; ".join(AUTHORS), language="Lushootseed")
NAMES = [("Hess", "Thom Hess, the Dictionary of Puget Salish (1976) and the Lushootseed readers"),
         ("Bates", "Dawn Bates, the Lushootseed Dictionary (1994) and its TEI encoding (2010)"),
         ("Koskenniemi", "Kimmo Koskenniemi, the two-level model of morphology (1983)"),
         ("Antworth", "Evan Antworth, PC-KIMMO (1990)"),
         ("Sleator and Temperley", "Daniel Sleator and Davy Temperley, the Link Grammar parser (1993)"),
         ("Grinberg", "Dennis Grinberg, Lafferty and Sleator, robust Link Grammar parsing (1995)"),
         ("Hilbert and Hess", "Vi Hilbert and Thom Hess, the stories of Ruth Sehome Shelton (1995)"),
         ("Ruth Sehome Shelton", "the teller of the published stories in the corpus"),
         ("Father Chirouse", "a 19th century Oblate missionary, translator of the liturgical materials"),
         ("Porter", "the Porter stemming algorithm for English web searches")]
LANGUAGES = [("Lushootseed", "Central Salish, the language parsed"),
             ("Finnish", "a morphologically complex language the two-level approach has been applied to"),
             ("Turkish", "a morphologically complex language the two-level approach has been applied to"),
             ("Arabic", "a morphologically complex language the two-level approach has been applied to"),
             ("Aymara", "a native American language the two-level approach has been applied to"),
             ("English", "the language of the Porter stemming algorithm's web searches")]


def caption(first, last, where):
    paper.add(where, A, "note", paper.joined(range(first, last + 1)),
              "page %d, the caption" % paper.page(first))


def figure_1(first, where):
    """Two words given to PC-KIMMO's recognize, each over its parse: the morphemes and their glosses."""
    for command in (first, first + 2):
        word = paper.text(command).split()[-1]
        label = "Figure 1, word %d" % (1 if command == first else 2)
        paper.add(label, L, "transcription", word,
                  "page %d, the word given as %s" % (paper.page(command), paper.text(command)))
        segmentation, gloss = paper.text(command + 1).rsplit(" ", 1)
        paper.add(label, L, "segmentation", segmentation, "page %d, the parse" % paper.page(command))
        paper.add(label, L, "gloss", gloss, "page %d, the parse's glosses" % paper.page(command))
    caption(first + 4, first + 4, "Figure 1")
    return first + 5


def figure_2(first, where):
    """Five Link Grammar rules, a rule that runs past its line continuing on the next."""
    rules = []
    number = first
    while not paper.text(number).startswith("Figure 2:"):
        if paper.text(number).startswith("<") or not rules:
            rules.append([number])
        else:
            rules[-1].append(number)
        number += 1
    for at, lines in enumerate(rules, 1):
        paper.add("Figure 2, rule %d" % at, A, "rule", paper.joined(lines),
                  "page %d, a Link Grammar rule" % paper.page(lines[0]))
    caption(number, number, "Figure 2")
    return number + 1


def linkage(first, label):
    """A sentence given to linkparser>, the link diagram over it and the row of its words; returns the
    line after the word row."""
    prompt = "linkparser> "
    paper.add(label, L, "transcription", paper.text(first)[len(prompt):],
              "page %d, the sentence given to linkparser>" % paper.page(first))
    wall = first + 1
    while not paper.text(wall).startswith("LEFT-WALL"):
        wall += 1
    paper.add(label, A, "note", paper.joined(range(first + 1, wall)),
              "page %d, the link diagram" % paper.page(first))
    paper.add(label, L, "parse", paper.text(wall),
              "page %d, the sentence's words under the diagram" % paper.page(first))
    return wall + 1


def figure_3(first, where):
    number = first
    for at in (1, 2, 3):
        number = linkage(number, "Figure 3, sentence %d" % at)
    caption(number, number, "Figure 3")
    return number + 1


def figure_4(first, where):
    number = first
    while not paper.text(number).startswith("Figure 4:"):
        paper.add("Figure 4", A, "note", paper.text(number),
                  "page %d, a link in linearized format" % paper.page(number))
        number += 1
    caption(number, number, "Figure 4")
    return number + 1


def figure_5(first, where):
    paper.add("Figure 5", A, "note", paper.text(first), "page %d, over the first query's result" % paper.page(first))
    number = linkage(first + 1, "Figure 5, the longest link")
    paper.add("Figure 5", A, "note", paper.text(number), "page %d, over the second query's result" % paper.page(number))
    paper.add("Figure 5, the most complex predicate", L, "segmentation", paper.text(number + 1),
              "page %d, the most morphologically complex predicate of the 500 sentences" % paper.page(number))
    caption(number + 2, number + 2, "Figure 5")
    return number + 3


def statistics(first, where):
    """The sample statistics under their heading, and Figure 6 on the next page: the lexical suffixes
    with their counts, then the link types with theirs."""
    paper.add(where, A, "note", paper.text(first), "page %d, over the statistics" % paper.page(first))
    number = first + 1
    while paper.text(number).startswith("*"):
        paper.add(where, A, "note", paper.text(number), "page %d, a statistic of the parsed corpus" % paper.page(number))
        number += 1
    # The page's number and the next page's head stand between the statistics and Figure 6.
    while not paper.text(number) or paper.text(number).isdigit():
        number += 1
    while paper.text(number).split()[-1].startswith("="):
        count, suffix = paper.text(number).split()
        paper.add("Figure 6", L, "cited affix", suffix,
                  "page %d, a lexical suffix of the 500-sentence corpus, counted %s" % (paper.page(number), count))
        number += 1
    while not paper.text(number).startswith("Figure 6:"):
        paper.add("Figure 6", A, "note", paper.text(number),
                  "page %d, a link type of the 500-sentence corpus and its count" % paper.page(number))
        number += 1
    caption(number, number + 1, "Figure 6")
    return number + 2


def bullets(first, where, end):
    """A bulleted list, each item a note, its wrapped lines with it, up to the line end matches: an
    item's wrapped line can open on a capital, Oblate missionary, as the paragraph after it does."""
    items = []
    number = first
    while not re.match(end, paper.text(number)):
        if paper.text(number).startswith("•"):
            items.append([number])
        else:
            items[-1].append(number)
        number += 1
    for lines in items:
        body = paper.joined(lines)
        paper.add(where, A, "note", body, "page %d, a bulleted item" % paper.page(lines[0]))
        paper.mentions(where, body, NAMES, "name")
        paper.mentions(where, body, LANGUAGES, "language")
    return number


BLOCKS = {paper.find(r"^• published stories"): lambda first, where: bullets(first, where, r"^Five hundred"),
          paper.find(r"^• sentences that have a negative"): lambda first, where: bullets(first, where, r"^Figure 5 gives"),
          paper.find(r"^PC-KIMMO>recognize"): figure_1, paper.find(r"^<pref-asp1>"): figure_2,
          paper.find(r"^linkparser> \?u\+"): figure_3, paper.find(r"^1 tu\+"): figure_4,
          paper.find(r"^Longest link:$"): figure_5, paper.find(r"^Sample statistics"): statistics}
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS)
# The abstract, set with no heading under the university, is one note.
lines = [index for index, row in enumerate(paper.rows) if row[0] == "front" and row[2] == "note"
         and row[4] == "page 1, under Hitokazu Matsushita"][1:]
if lines:
    paper.rows[lines[0]][3] = " ".join(paper.rows[index][3] for index in lines)
    paper.rows[lines[0]][4] = "page 1, the abstract"
    paper.rows = [row for index, row in enumerate(paper.rows) if index not in lines[1:]]
# The paragraph that lists the parser's three merits runs onto page 6 at its (ii), which flow takes
# for a list item of its own. It is one note.
first = next(index for index, row in enumerate(paper.rows) if row[3].startswith("The Link Grammar parser is well suited"))
assert paper.rows[first + 1][3].startswith("(ii)") and paper.rows[first + 2][3].startswith("without any")
paper.rows[first][3] = " ".join(row[3] for row in paper.rows[first:first + 3])
del paper.rows[first + 1:first + 3]
paper.write()
