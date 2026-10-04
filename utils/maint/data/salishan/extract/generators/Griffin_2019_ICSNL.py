"""The ops of Griffin_2019_ICSNL: Laura Sehyun Griffin on qəǰi ‘again’/‘still’ in ʔayʔaǰuθəm
(Comox-Sliammon), a possible presupposition trigger that does not require its content in the
common ground: offered and accepted for events happening for the first time with no "Hey, wait a
minute!" response, removed from most provided sentences, in support of Gauker (1998) and Koch's
(2011) Presupposition Constraint.

Each example sets the ʔayʔaǰuθəm segmented over its gloss and the translation in quotes, some in
lettered parts, (1) with Watanabe's page at its right. The four tables chart, by storyboard, where
qəǰi was included or removed and which sentences were volunteered or accepted: a caption note and a
note to each printed line.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "ʔayʔaǰuθəm"
AUTHORS = ["Laura Sehyun Griffin"]
paper = gen.Paper("Griffin_2019_ICSNL", authors=AUTHORS[0], language=LANGUAGE)
paper.opening = "segmentation"
NAMES = [("Joanne Francis", "the author's consultant, a ʔayʔaǰuθəm speaker"),
         ("Henry Davis", "thanked"), ("Marianne Huijsmans", "thanked"), ("Gloria Mellesmoen", "thanked"),
         ("Lisa Matthewson", "thanked"), ("Daniel Reisinger", "thanked"), ("Kaining Xu", "thanked"),
         ("Matthewson", "Lisa Matthewson, presuppositions and cross-linguistic variation (2006)"),
         ("Gauker", "Christopher Gauker, what is a context of utterance? (1998)"),
         ("Koch", "Karsten Koch, focus marking in a language lacking pragmatic presuppositions (2011)"),
         ("Watanabe", "Honoré Watanabe, a morphological description of Sliammon (2003)"),
         ("Birner", "Betty J. Birner, introduction to pragmatics (2012)"),
         ("Beaver & Geurts", "David Beaver and Bart Geurts, presupposition (2014)"),
         ("von Fintel", "Kai von Fintel, presupposition accommodation (2000, 2008)"),
         ("Stalnaker", "Robert Stalnaker (1974), the common ground"),
         ("Cable", "Seth Cable, presuppositions in the Salish language family (2008)"),
         ("Harris", "Herbert Raymond Harris, recordings of Island Comox (1981)"),
         ("Marie Clifton", "a fluent speaker of Island Comox, recorded by Harris (1981)"),
         ("Mrs. Clifton", "Marie Clifton"),
         ("Laurie", "a name in the storyboards"), ("Henry", "a name in the storyboards"),
         ("Art", "a name in the storyboards"), ("Laura", "a name in the storyboards"),
         ("Gloria", "a name in the storyboards"), ("Marianne", "a name in the storyboards"),
         ("Daniel", "a name in the storyboards")]
LANGUAGES = [(LANGUAGE, "Comox-Sliammon, Central Salish, critically endangered, about 47 fluent speakers"),
             ("Comox-Sliammon", "ʔayʔaǰuθəm"), ("Central Salish", "the branch"),
             ("St’át’imcets", "Interior Salish, Matthewson's presupposition triggers"),
             ("Interior Salish", "the branch of St’át’imcets"), ("English", "its again and still"),
             ("Nɬeʔkepmxcin", "Koch's focus marking"), ("Island Comox", "a related dialect, Marie Clifton's"),
             ("Tla’amin", "a ʔayʔaǰuθəm community"), ("K’ómoks", "a ʔayʔaǰuθəm community"),
             ("Klahoose", "a ʔayʔaǰuθəm community"), ("Homalco", "a ʔayʔaǰuθəm community")]

# The thanks at the foot of page 1 carry no mark.
THANKS = list(range(paper.find(r"^I am very grateful to"), paper.find(r"insight on this project\. Contact info:") + 1))
found = dict(paper.page_footnotes(), **{gen.UNMARKED: (THANKS, 1)})
paper.page_footnotes = lambda stops=(), symbols_on=(1,): found
SKIP = {one for parts, _ in found.values() for one in parts}
APPENDIX = paper.find(r"^Appendix A Details of storyboards")
# 5 Analysis and 6 Conclusion are set in a bold the heading reader does not take.
HEADINGS = paper.headings(paper.find(r"^Keywords:") + 1, paper.find(r"^References$") - 1, skip=SKIP)
HEADINGS.update({paper.find(r"^5 Analysis$"): "5", paper.find(r"^6 Conclusion$"): "6"})


# Tables 1 and 2 leave one column blank on most rows, and a flat line loses the column a mark stands
# under. These are the columns' left edges in points, from word_positions, checked against the
# rendered pages 6 and 7.
COLUMNS = {"Table 1": ((220, "qəǰi not included"), (350, "qəǰi included")),
           "Table 2": ((238, "qəǰi removed"), (346, "qəǰi included"))}


def placed(label, line):
    """Each cell of a Table 1 or 2 row with the column its words start in."""
    cells = {}
    for left, word in paper.word_positions(line):
        heading = [name for edge, name in COLUMNS[label] if left >= edge]
        if heading:
            cells.setdefault(heading[-1], []).append(word)
    return "; ".join("%s under %s" % (" ".join(words), name) for name, words in cells.items())


def table(start, where):
    """A table: its caption a note, and a note to each printed line to the blank line after it."""
    label = paper.text(start).split(":")[0]
    paper.add(label, A, "note", paper.text(start), "page %d, the caption" % paper.page(start))
    line, count = start + 1, 0
    while paper.text(line).strip() and not paper.lines[line][2]:
        count += 1
        gloss = "page %d, a line of the table as the text layer reads it" % paper.page(line)
        if label in COLUMNS and "N/A" in paper.text(line):
            gloss += "; the N/A note runs across both columns"
        elif label in COLUMNS and re.match(r"^\d+ ", paper.text(line)):
            gloss += "; " + placed(label, line)
        paper.add("%s line %d" % (label, count), A, "note", paper.text(line), gloss)
        line += 1
    return line


blocks = {paper.find(r"^Table %d: " % one): table for one in (1, 2, 3, 4)}
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, headings=HEADINGS, appendix=r"^Appendix A Details")
# Dunlop's entry and von Fintel's are read as one.
rows = paper.rows
for index, row in enumerate(rows):
    if row[2] == "reference" and " von Fintel, Kai. 2000." in row[3]:
        first, second = row[3].split(" von Fintel, Kai. 2000.")
        row[3] = first
        rows.insert(index + 1, ["references", A, "reference", "von Fintel, Kai. 2000." + second, row[4]])
        break
# Appendix A describes each storyboard and its context, a paragraph each, set flush and opening on
# its name, Storyboard 1a:.
paper.add("§Appendix A", A, "heading", paper.text(APPENDIX), "page %d" % paper.page(APPENDIX))
running = paper.running_numbers_set()
body = [one for one in range(APPENDIX + 1, paper.last + 1)
        if one not in running and not paper.lines[one][2] and paper.text(one).strip()]
opens = [index for index, one in enumerate(body) if re.match(r"^Storyboard \d+[ab]?:", paper.text(one))]
for start, end in zip(opens, opens[1:] + [len(body)]):
    lines = body[start:end]
    text = paper.joined(lines)
    pages = sorted({paper.page(one) for one in lines})
    paper.add("§Appendix A", A, "note", text, "page %d" % pages[0])
    paper.cited("§Appendix A", text, pages)
    paper.mentions("§Appendix A", text, NAMES, "name")
    paper.mentions("§Appendix A", text, LANGUAGES, "language")
paper.write()
