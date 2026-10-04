"""The ops of MellesmoenAndreotti_NTrStative_final: Gloria Mellesmoen and Bruno Andreotti on the
non-control stative in ʔayʔaǰuθəm, marked on the non-control transitivizer by contrastive pitch and not
by /i/-infixation, with phonetic and semantic evidence that it is productive and denotes a result state.

Every example opens on its number and a caption, and each lettered part is its own tiers. (1) sets a
phonetic form and its pitch pattern in brackets on one line, then the underlying form between
slashes, the gloss and the translation; (2) and the first (3) the phonetic form and pitch pattern
over a segmentation, the gloss and the translation. The paper numbers two examples (3), the second on
page 5; it and (4), (5), (6) and (8) are a segmentation over its gloss and a translation, the
judgement mark, # or ??, kept on the form and the translation of a rejected sentence printed in
parentheses. The paper prints no (7), and (6b) is printed (b). The floating stress of the stative is a
bracket pair with an acute accent in it, [́]. The title carries two marks, 1 and ∗, and the paper
prints no footnote 1. The five figures are pitch tracks and a timeline, their captions notes.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "ʔayʔaǰuθəm"
AUTHORS = ["Gloria Mellesmoen", "Bruno Andreotti"]
paper = gen.Paper("MellesmoenAndreotti_NTrStative_final", authors=" and ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Joanne Francis", "the authors' consultant, thanked in the note on the title"),
         ("Marianne Huijsmans", "thanked in the note on the title"), ("Henry Davis", "thanked in the note on the title"),
         ("Watanabe", "H. Watanabe, A Morphological Description of Sliammon (2003)"),
         ("Jacobs", "P. Jacobs, Control in Skwxwu7mesh (2011)"), ("Blake", "S. J. Blake (2000)"),
         ("Thompson", "L. C. Thompson (1985)"), ("Matthewson", "L. Matthewson"), ("Bar-el", "L. A. Bar-el (2005)"),
         ("Brown", "Brown and Thompson (2005)"),
         ("Andrei Anghelescu", "an editor of the volume"), ("Michael Fry", "an editor of the volume"),
         ("Daniel Reisinger", "an editor of the volume")]
LANGUAGES = [(LANGUAGE, "Central Salish, British Columbia; also known as Comox-Sliammon"),
             ("ʔay̓aǰuθəm", "the language's name with the glottalized y, in the abstract and §1"),
             ("Comox-Sliammon", "ʔayʔaǰuθəm"), ("Comox", "ʔayʔaǰuθəm, in the keywords"),
             ("Sliammon", "ʔayʔaǰuθəm, in Watanabe's and Blake's titles"),
             ("St’at’imcets", "Northern Interior Salish, its unaccusative roots"),
             ("Skwxwú7mesh", "Central Salish, its unaccusative roots"),
             ("Upriver Halkomelem", "Central Salish, a pitch accent (Brown and Thompson 2005)"),
             ("English", "the translations")]

# The footnotes are numbered from 2; the title's mark 1 has no note.
paper.first_footnote = 2
RUNNING = paper.running_numbers_set()
AT_FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
# A part's first line with its phonetic form and pitch pattern, [qʷoˑmotʰ] [HL].
PHONETIC = re.compile(r"^(\[[^\]]*\]) (\[[HLM/]+\])$")


def printed(number):
    return bool(paper.text(number).strip()) and not paper.lines[number][2] and number not in RUNNING \
        and number not in AT_FOOT


def after(number):
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def example(start, where):
    """An example: its caption a note, then each lettered part's tiers, a part opening on a., b. or
    (b). The second (3) of the paper is named so in its caption's gloss."""
    opened = gen.EXAMPLE.match(paper.text(start))
    label = opened.group(1)
    second = label == "3" and paper.page(start) == 5
    paper.add("(%s)" % label, A, "note", opened.group(2),
              "page %d, the caption%s" % (paper.page(start), ", of the second example the paper numbers (3)"
                                          if second else ""))
    line = after(start)
    letter, count = "", 0
    while line <= paper.last and not gen.HEADING.match(paper.text(line)) and \
            not gen.EXAMPLE.match(paper.text(line)) and not prose(line):
        text = paper.text(line)
        part = re.match(r"^(?:([a-z])\.|\(([a-z])\))\s*(.*)$", text)
        if part:
            letter, count, text = part.group(1) or part.group(2), 0, part.group(3)

        def row(who, kind, form, gloss=""):
            nonlocal count
            count += 1
            paper.add("(%s%s) line %d" % (label, letter, count), who, kind, form,
                      "page %d%s" % (paper.page(line), gloss))

        phonetic = PHONETIC.match(text)
        if phonetic:
            row(L, "phonetic", phonetic.group(1))
            row(L, "phonetic", phonetic.group(2), ", the pitch pattern")
        elif text.startswith("/"):
            row(L, "phonemic", text, ", the underlying form")
        elif text.startswith(("‘", "(‘")):
            row(A, "translation", text, ", in parentheses, the sentence rejected" if text.startswith("(") else "")
        elif count and paper.rows[-1][2] in ("segmentation", "phonemic"):
            row(L, "gloss", text)
        else:
            row(L, "segmentation", text)
        line = after(line)
    return line


def prose(number):
    """Whether a line is the running prose after an example: nine words or more."""
    return len(paper.text(number).split()) >= 9 and not paper.text(number).startswith(("‘", "(‘"))


# Figures 1 to 4: [yɛ́ɬʌt] from hahays yɛɬʌt piš, ‘I slowly called Pish (cat)’ [HL].
PITCH_TRACK = re.compile(r"^Figure (\d)[:.] (\[[^\]]*\]) from .* (\[[HL]+\])$")


def caption(start, where):
    """A figure's caption a note, the figure a picture over it. A pitch track's caption gives the form
    it tracks in brackets and its pitch pattern, each a phonetic row, and the sentence it comes from
    in italics, a cited form."""
    text = paper.text(start)
    name = "Figure " + re.match(r"^Figure (\d)", text).group(1)
    track = PITCH_TRACK.match(text)
    paper.add(name, A, "note", text, "page %d, the figure's caption; the figure is %s" %
              (paper.page(start), "a pitch track" if track else "a drawing of two timelines"))
    if track:
        paper.add(name, L, "phonetic", track.group(2), "page %d, in the caption" % paper.page(start))
        paper.add(name, L, "phonetic", track.group(3), "page %d, in the caption, the pitch pattern" % paper.page(start))
    paper.cited(name, text, [paper.page(start)])
    paper.mentions(name, text, LANGUAGES, "language")
    return after(start)


blocks = {number: example for number in range(1, paper.last + 1)
          if printed(number) and re.match(r"^\(\d+\) [A-Zč]", paper.text(number))}
blocks.update({number: caption for number in range(1, paper.last + 1)
               if printed(number) and re.match(r"^Figure \d[:.] ", paper.text(number))})
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)
# The title's mark 1 comes off it with the ∗ after it.
title = next(row for row in paper.rows if row[2] == "title")
title[3] = re.sub(r"1$", "", title[3])
title[4] = "page 1, carries footnotes 1 and *; the paper prints no footnote 1"
# The reference reader ends Davis and Matthewson (2009) at its title's stop and runs the journal line
# on into the two entries of page 14, First Peoples' Cultural Council (2014) and Jacobs (2011).
at = next(number for number, row in enumerate(paper.rows) if row[2] == "reference" and
          row[3].startswith("Language and Linguistics Compass"))
journal, rest = paper.rows[at][3].split(" First Peoples’ ")
council, jacobs = ("First Peoples’ " + rest).split(" Jacobs, P. ")
paper.rows[at - 1][3] += " " + journal
paper.rows[at][3], paper.rows[at][4] = council, "page 14"
paper.rows[at + 1:at + 1] = [paper.rows[at][:3] + ["Jacobs, P. " + jacobs, "page 14"]]
paper.write()
