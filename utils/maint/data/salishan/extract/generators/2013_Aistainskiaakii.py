"""The ops of 2013_Aistainskiaakii: Áístainskiaakii, Issapóíkoan, Áínnootaa and David Osgarby's
Aakíípisskani ‘the women’s buffalo jump’, a traditional Blackfoot story of the first marriage between
man and woman, arranged by the creator-trickster Náápi, told in the Káínaa (Blood) dialect. The story
is given whole in the Blackfoot syllabary, whole in the standard Roman orthography and whole in
English, each in 86 numbered sentences, and then sentence by sentence as interlinear examples (0) to
(86), each timed to its recording.

Page text read by glyph rows, with the word-space share page_text.py PAPER_SHARE sets for the paper's
loose small-caps glosses.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Blackfoot"
AUTHORS = ["Áístainskiaakii", "Issapóíkoan", "Áínnootaa", "David Osgarby"]
paper = gen.Paper("2013_Aistainskiaakii", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Sandra Many Feathers", "Áístainskiaakii, a storyteller"),
         ("Brent Prairie Chicken", "Issapóíkoan, a storyteller"),
         ("Wes Crazy Bull", "Áínnootaa, a storyteller"),
         ("Sáómittsinski", "Stella Crazy Bull, a parent of Áístainskiaakii"),
         ("Iistohkói’poyo’p", "Mike Steel, a parent of Áístainskiaakii"),
         ("Karsten Koch", "commented on the transcription and interlinear gloss"),
         ("Uhlenbeck", "C. C. Uhlenbeck, A new series of Blackfoot texts from the Southern Peigans Blackfoot Reservation (1912)"),
         ("Joseph Tatsey", "Southern Peigan, Uhlenbeck's primary consultant"),
         ("Dean Many Guns", "p.c., March 12, 2013, the syllabary of Figure 1"),
         ("John William Tims", "missionary, formulated the syllabary"),
         ("Harry W. Gibbon Stocken", "missionary, formulated the syllabary"),
         ("Ermineskin", "Rachel Ermineskin and Darin Howe, On Blackfoot Syllabics and the Law of Finals (2005)"),
         ("Frantz", "Donald G. Frantz, Blackfoot grammar (1997); Frantz and Russell, Blackfoot dictionary (1995)"),
         ("Náápi", "the creator-trickster of the story")]
LANGUAGES = [(LANGUAGE, "Algonquian, the language of the story"),
             ("Káínaa", "the Blood dialect of Blackfoot, the storytellers'"),
             ("English", "the translation")]
# The abbreviations of §7 on page 21 are set at the notes' size and one line opens on a 4; the
# paper's notes end at 3, on page 6.
ABBREVIATIONS = paper.find(r"^>\s+subject-object relation")
page_footnotes = paper.page_footnotes
paper.page_footnotes = lambda *args, **kwargs: page_footnotes(
    stops=range(ABBREVIATIONS, paper.last + 1), **{key: one for key, one in kwargs.items() if key != "stops"})
FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
RUNNING = paper.running_numbers_set()
CITATION = re.compile(r"^(.*?)\s*(\(\d\d:\d\d, BLA_[\d-]+_\d+\))$")


def printed(number):
    return bool(paper.text(number)) and not paper.lines[number][2] and number not in RUNNING \
        and number not in FOOT


def heading(number):
    """A section heading sets a column gap after its number; sentence 7 of a telling, 7 Nííksi,
    sets a word space."""
    return bool(re.match(r"^\d\s{3,}\S", paper.spaced[number]))


def after(number):
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def sentences(first, stop):
    """The numbered sentences of a telling from line first to line stop: (number, text, page). The
    number is set against its sentence's first letter, and an opening quote of the telling's speech
    stands before it, "48Óónahkayi, and goes with the sentence it opens."""
    lines, line = [], first
    while line < stop:
        lines.append((paper.text(line), paper.page(line)))
        line = after(line)
    out = []
    for text, page in lines:
        for piece in re.split(r"(?:(?<=\s)|^|(?<=“))(?=\d+\D)", text):
            number = re.match(r"^(\d+)(.*)$", piece)
            if number and (not out or int(number.group(1)) == out[-1][0] + 1):
                opening = out[-1][1].endswith("“") if out else False
                if opening:
                    out[-1][1] = out[-1][1][:-1]
                out.append([int(number.group(1)), ("“" if opening else "") + number.group(2), page])
            elif piece:
                out[-1][1] += " " + piece
    return [(number, " ".join(text.split()), page) for number, text, page in out]


def telling(kind, who, what):
    def block(first, where):
        stop = next(one for one in range(first + 1, paper.last + 1) if heading(one))
        title = paper.text(first)
        paper.add(where, who, kind, title, "page %d, the title of the story %s" % (paper.page(first), what))
        told = sentences(after(first), stop)
        assert [one[0] for one in told] == list(range(1, 87)), [one[0] for one in told]
        for number, text, page in told:
            paper.add("%s sentence %d" % (where, number), who, kind, text, "page %d, sentence %d %s" % (page, number, what))
        return stop
    return block


# Figure 1, the syllabary: each row's head and the columns its cells stand in, read off the render.
# The last row, _S, prints its low lines, which residue.py CORRECTIONS puts back on the row.
COLUMNS = {"base": ("a", "i", "o"), "final": ("i", "o"), "W": ("a", "i", "o"), "Y": ("a", "i", "o"),
           "H": ("indep",), "_S": ("indep",)}


def figure(first, where):
    caption = paper.find(r"^Figure 1: ", first)
    page = paper.page(first)
    paper.add("Figure 1", A, "note", paper.text(first), "page %d, the figure's column heads" % page)
    line = after(first)
    while line < caption:
        head, *cells = re.split(r"\s{3,}", paper.spaced[line])
        columns = COLUMNS.get(head, ("indep", "a", "i", "o"))
        assert len(cells) == len(columns), (head, cells)
        paper.add("Figure 1", A, "note", head, "page %d, the row's head" % page)
        for column, cell in zip(columns, cells):
            paper.add("Figure 1", LANGUAGE, "transcription", cell, "page %d, %s row, %s column" % (page, head, column))
        line = after(line)
    paper.add("Figure 1", A, "note", paper.text(caption), "page %d, the figure's caption" % page)
    return caption + 1


def example(first, where):
    """An interlinear example: its transcription, segmentation and gloss, wrapping to further sets of
    the three, then the translation on one line or two, then its recording, the minute and file,
    right of the translation or under it. The speech of the story's women and of Náápi opens its
    transcription and translation on a double quote, (18), (48) to (53) and (70) to (72); the
    translation is the line or two after the last whole set of three."""
    label = gen.EXAMPLE.match(paper.text(first)).group(1)
    lines, line = [], first
    while True:
        text = paper.text(line)
        if line != first and gen.EXAMPLE.match(text) or heading(line):
            break
        lines.append((re.sub(r"^\(\d+\)\s+", "", text) if line == first else text, paper.page(line)))
        line = after(line)
        if CITATION.match(text):
            break
    cited = CITATION.match(lines[-1][0])
    source = None
    if cited:
        source = (cited.group(2), lines[-1][1], "under the translation" if not cited.group(1) else
                  "right of the translation")
        lines[-1] = (cited.group(1), lines[-1][1])
        if not lines[-1][0]:
            lines.pop()
    sets = max((count for count in range(0, len(lines) + 1, 3)
                if len(lines) - count in (1, 2) and lines[count][0][:1] in "‘“"), default=None)
    assert sets is not None, (label, lines)
    count = 0
    for index, (text, page) in enumerate(lines[:sets]):
        count += 1
        who, kind = ((L, "transcription"), (L, "segmentation"), (A, "gloss"))[index % 3]
        paper.add("(%s) line %d" % (label, count), who, kind, text, "page %d" % page)
    count += 1
    paper.add("(%s) line %d" % (label, count), A, "translation",
              " ".join(one[0] for one in lines[sets:]), "page %d" % lines[sets][1])
    if source:
        count += 1
        paper.add("(%s) line %d" % (label, count), A, "citation", source[0],
                  "page %d, the recording, %s" % (source[1], source[2]))
    return line


def abbreviations(first, where):
    """§7, two entries to a printed line, each a note."""
    stop = paper.find(r"^References$", first)
    line = first
    while line < stop:
        cells = re.split(r"\s{3,}", paper.spaced[line])
        for at in range(0, len(cells), 2):
            paper.add("§7", A, "note", "%s   %s" % tuple(cells[at:at + 2]),
                      "page %d, an abbreviation of the glosses" % paper.page(line))
        line = after(line)
    return stop


BLOCKS = {paper.find(r"^indep\s+a\s+i\s+o$"): figure,
          paper.find(r"^ᖳᖽᑯᑦᖿᖹ$"): telling("transcription", L, "in the Blackfoot syllabary"),
          paper.find(r"^Aakíípisskani$"): telling("transcription", L, "in the standard Roman orthography"),
          paper.find(r"^The women’s buffalo jump$"): telling("translation", A, "in English"),
          ABBREVIATIONS: abbreviations}
at = 1
for label in range(0, 87):
    at = paper.find(r"^\(%d\)\s+\S" % label, at)
    BLOCKS[at] = example
    at += 1
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS)
# The three affiliations stand under the whole author line, each opening on the mark its authors
# carry.
AFFILIATIONS = {"a": "Áístainskiaakii, Issapóíkoan and Áínnootaa", "b": "Áístainskiaakii and David Osgarby",
                "c": "David Osgarby"}
for row in paper.rows:
    if row[0] == "front" and row[2] == "note" and row[4] == "page 1, under David Osgarby":
        row[4] = "page 1, affiliation %s, of %s" % (row[3][0], AFFILIATIONS[row[3][0]])
paper.write()
