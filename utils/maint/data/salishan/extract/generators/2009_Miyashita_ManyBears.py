"""The ops of 2009_Miyashita_ManyBears: Mizuki Miyashita and Rosella Many Bears on their
academic-community collaboration, video-recording spoken Blackfoot for linguistic analysis and for
teaching, transcribed with Rosella and analyzed interlinearly.

(1) is a table of four numbered lines, each a transcription beside its free translation. (2) sets the
same four lines again, each over its morpheme analysis (b) and gloss (c), the free translation beside
line a. Figures 1 to 3 are pictures; their captions and Figure 3's panel labels are notes. Figure 4 is
a flowchart, and each of its boxes is a note.
"""
import re
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Blackfoot"
AUTHORS = ["Mizuki Miyashita", "Rosella Many Bears"]
paper = gen.Paper("2009_Miyashita_ManyBears", authors=" and ".join(AUTHORS), language=LANGUAGE)
NAMES = [("Darrell R. Kipp", "the director of Piegan Institute, thanked"),
         ("Donald Frantz", "thanked; the Blackfoot grammar (1991) and dictionary"),
         ("Delores Many Bears", "thanked"), ("Louise Giebel", "thanked"), ("Leo & Kristen Kipp", "thanked"),
         ("Frantz & Russell", "the Blackfoot dictionary"),
         ("Uhlenbeck & Van de Gulik", "a Blackfoot vocabulary list (1930)"),
         ("Grenoble", "Lenore Grenoble, the limits of linguists (2009)"),
         ("Miyashita and Crow Shoe", "Blackfoot lullabies and language revitalization (2009)"),
         ("Rice", "Karen Rice, the two solitudes (2009)")]
LANGUAGES = [(LANGUAGE, "Algonquian, spoken in Browning, Montana"),
             ("English", "the language of the free translations")]


def cells(number):
    return [one for one in re.split(r"\s{3,}", paper.spaced[number].strip()) if one]


def picture(start, where):
    paper.add(paper.text(start), A, "note", paper.text(start),
              "page %d, the figure's caption; the figure is a picture" % paper.page(start))
    return start + 1


def panels(start, where):
    """Figure 3: its four panel labels, two to a line, then the caption."""
    page = paper.page(start)
    for line in (start, start + 1):
        for label in cells(line):
            paper.add("Figure 3", A, "note", label, "page %d, a panel label; the panels are screenshots" % page)
    return picture(start + 2, where)


def table(start, where):
    """(1): the caption and header notes, then each numbered line's transcription and free
    translation. Line 1 sets no wide space between the two."""
    page = paper.page(start)
    paper.add("(1)", A, "note", gen.EXAMPLE.match(paper.text(start)).group(2), "page %d, the caption" % page)
    paper.add("(1)", A, "note", paper.text(start + 1), "page %d, the table's header" % page)
    for line in range(start + 2, start + 6):
        row = cells(line)
        if len(row) == 2:
            at = row[1].index(" I will")
            row = [row[0], row[1][:at], row[1][at + 1:]]
        here = "(1) line %s" % row[0]
        paper.add(here, L, "transcription", row[1], "page %d" % page)
        paper.add(here, A, "translation", row[2], "page %d" % page)
    return start + 6


def interlinear(start, where):
    """(2): the caption and header notes, then each line's transcription (a), morpheme analysis (b)
    and gloss (c), and the free translation printed beside line a. Line 1b carries a literal
    translation in parentheses beside it."""
    page = paper.page(start)
    paper.add("(2)", A, "note", gen.EXAMPLE.match(paper.text(start)).group(2), "page %d, the caption" % page)
    paper.add("(2)", A, "note", paper.text(start + 1), "page %d, the table's header" % page)
    kinds = {"a": "transcription", "b": "segmentation", "c": "gloss"}
    number, after = None, []
    line = start + 2
    while line < start + 14:
        found = re.match(r"^(\d)?\s*([abc])\.\s+(.*)$", paper.text(line))
        number = found.group(1) or number
        tier, text = found.group(2), found.group(3)
        side = cells(line)[-1]
        beside = tier == "a" or side.startswith("(lit")
        if beside:
            text = text[:-len(side)].strip()
        here = "(2) line %s" % number
        paper.add(here, L, kinds[tier], text, "page %d" % page)
        if beside:
            after.append((side, "page %d" % page if tier == "a" else
                          "page %d, the literal translation, in parentheses beside line %sb" % (page, number)))
        if tier == "c":
            for side, gloss in after:
                paper.add(here, A, "translation", side, gloss)
            after = []
        line += 1
    return line


# Figure 4's boxes as the page draws them, each a printed line to a note. The layer runs the lines of
# boxes side by side into one line, Community Member   Teaching.
BOXES = [(("Community Member", "Language Educators", "Native Speakers"),
          "a rounded box drawn in the figure at the upper left, its arrow pointing to Language Documentation"),
         (("Collaboration Skill Share",), "the label of the double arrow between the two rounded boxes, set "
          "rotated, Collabor broken from ation"),
         (("Academics", "Linguists", "Researchers"),
          "a rounded box drawn in the figure at the lower left, its arrow pointing to Language Documentation"),
         (("Language", "Documentation"), "an oval drawn in the middle of the figure, its arrows pointing to "
          "Teaching Materials, Linguistic Analysis and Language Preservation"),
         (("Teaching", "Materials"), "a box drawn in the figure at the upper right"),
         (("Linguistic", "Analysis"), "a box drawn in the figure at the right, its arrow pointing down to "
          "Dissemination"),
         (("Language", "Preservation"), "a box drawn in the figure under Language Documentation"),
         (("Dissemination", "Conference", "Publication"), "an oval drawn in the figure at the lower right")]


def flowchart(start, where):
    """Figure 4: a note to each printed line of each box of the flowchart, then the caption."""
    page = paper.page(start)
    for count, (lines, place) in enumerate(BOXES, 1):
        for number, label in enumerate(lines, 1):
            paper.add("Figure 4 box %d" % count, A, "note", label,
                      "page %d, %s" % (page, place) if number == 1 else
                      "page %d, line %d of the box" % (page, number))
    caption = paper.find("^Figure 4$")
    paper.add("Figure 4", A, "note", "Figure 4", "page %d, the figure's caption; the figure is a flowchart" % page)
    return caption + 1


def paragraph(start, where):
    """The paragraph that opens on (2) below, prose that names the example."""
    last = paper.find("^analyses of natural speech in Blackfoot", start)
    paper.add(where, A, "note", paper.joined(range(start, last + 1)), "page %d" % paper.page(start))
    return last + 1


# (2) sets its lines 1 to 4 small, and page_footnotes opens a note 2 at its line 2 and runs it to the
# page's end over sections 3 and 4. The paper has one footnote.
footnotes = paper.page_footnotes
paper.page_footnotes = lambda *args, **kwargs: {mark: found for mark, found in footnotes(*args, **kwargs).items()
                                                if mark == "1"}
blocks = {paper.find("^Figure 1$"): picture, paper.find(r"^\(2\) below is"): paragraph, paper.find("^Figure 2$"): picture,
          paper.find(r"^\(a\) Audition All"): panels, paper.find(r"^\(1\) Transcription"): table,
          paper.find(r"^\(2\) Interlinear"): interlinear, paper.find("^Community Member"): flowchart}
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)
rows = paper.rows
# The title carries footnote 1 on its last word, teaching1.
title = next(row for row in rows if row[2] == "title")
title[3], title[4] = title[3][:-1], "page 1, carries footnote 1"
paper.rows = []
paper.footnote("1", list(range(34, 39)), 1, names=NAMES, languages=LANGUAGES)
at = rows.index(title) + 1
rows[at:at] = paper.rows
# The abstract, under the second author's school, is one note.
lines = [index for index, row in enumerate(rows) if row[0] == "front" and row[2] == "note"
         and row[4] == "page 1, under Rosella Many Bears"][1:]
if lines:
    rows[lines[0]][3] = " ".join(rows[index][3] for index in lines)
    rows[lines[0]][4] = "page 1, the abstract"
    rows = [row for index, row in enumerate(rows) if index not in lines[1:]]
# cited() passes over forms defined in plain letters alone, and every italic form here is one. Each
# follows the paragraph that sets it.
CITED = (("tsa kitanikkoo", (("tsa kitanikkoo", "“what is your name?”"), ("anit", "‘say.’"),
                             ("anik", "the surface form of the root anit"), ("anit", "the root, its [t] changed to [k]"))),
         ("For example, the morpheme that shows", (("nit", "the morpheme that shows involvement of a first person "
                                                            "participant"),)))
for inside, forms in CITED:
    at = next(index for index, row in enumerate(rows) if row[2] == "note" and inside in row[3])
    rows[at + 1:at + 1] = [[rows[at][0], L, "cited form", form, "page 5, in italics, %s" % gloss]
                           for form, gloss in forms]
paper.rows = rows
paper.write()
