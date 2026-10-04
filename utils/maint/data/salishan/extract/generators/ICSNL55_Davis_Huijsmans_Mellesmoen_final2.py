"""The ops of ICSNL55_Davis_Huijsmans_Mellesmoen_final2: Henry Davis, Marianne Huijsmans and Gloria
Mellesmoen on statives in ʔayʔaǰuθəm and St'át'imcets, target states against result states, and the
maintaining state reading of transitive statives.

Examples are set with a caption naming the language, ʔayʔaǰuθəm: or St’át’imcets:, a context, the
orthographic line, a phonemic segmentation in the NAPA, its gloss and the translation (footnote 2);
each example takes its language from its caption (languages_by_caption). The paradigms (1) to (6)
set two columns, eventive and stative, each cell a form, its phonemic form and a gloss: a
transcription, a phonemic and a translation row to each cell. (7) and (8) are German over an English
gloss; (9) and (10) Kratzer's formulas. Each figure caption is a note, two to a line where the page
sets two figures side by side.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
AUTHORS = ["Henry Davis", "Marianne Huijsmans", "Gloria Mellesmoen"]
LANGUAGE = "ʔayʔaǰuθəm"
paper = gen.Paper("ICSNL55_Davis_Huijsmans_Mellesmoen_final2", authors=", ".join(AUTHORS), language=LANGUAGE)
paper.opening = "auto"

NAMES = [("Elsie Paul", "ʔayʔaǰuθəm consultant"), ("Betty Wilson", "ʔayʔaǰuθəm consultant"),
         ("Freddie Louie", "ʔayʔaǰuθəm consultant"), ("Joanne Francis", "ʔayʔaǰuθəm consultant"),
         ("Phyllis Dominic", "ʔayʔaǰuθəm consultant"), ("Karen Galligos", "the late ʔayʔaǰuθəm consultant"),
         ("Marion Harry", "the late ʔayʔaǰuθəm consultant"), ("Carl Alexander", "St’át’imcets consultant"),
         ("Kratzer", "Angelika Kratzer, target and result states (2000)"),
         ("Parsons", "Terence Parsons, target and result states (1990)"),
         ("Watanabe", "Honoré Watanabe, the grammar of ʔayʔaǰuθəm (2003)")]
LANGUAGES = [(LANGUAGE, "Comox-Sliammon, Central Salish, ISO 639-3 coo"),
             ("St’át’imcets", "Lillooet, Northern Interior Salish, ISO 639-3 lil"),
             ("Comox-Sliammon", "the paper's other name for ʔayʔaǰuθəm"), ("Lillooet", "St’át’imcets"),
             ("Bella Coola", "Nuxalk, which also lacks the reflex of *ʔac-"), ("Nuxalk", "Bella Coola"),
             ("German", "Kratzer's adjectival passives, (7) and (8)"), ("English", "the adjectival passive compared"),
             ("Sḵwx̱wú7mesh", "Squamish, Bar-el 2005 on progressive CV- reduplication")]
CAPTIONS = {LANGUAGE: LANGUAGE, "St’át’imcets": "St’át’imcets", "German": "German"}
# A paradigm cell: a form, its phonemic form between slashes, and its gloss in quotes.
CELL = re.compile(r"^(.+?)\s+(/[^/]+/)\s+(‘.*’)$")


def cells(line):
    """The two cells of a paradigm row, eventive and stative: (form, phonemic, gloss) each."""
    slashes = [found.span() for found in re.finditer(r"/[^/]+/", line)]
    (first, first_end), (second, second_end) = slashes[0], slashes[1]
    between = line[first_end:second]
    # The gloss closes on the first quote a space follows; a form holds one inside it, (e)s7ats’xs.
    close = re.search(r"’(?=\s)", between).start()
    return [(line[:first].strip(), line[first:first_end], between[:close + 1].strip()),
            (between[close + 1:].strip(), line[second:second_end], line[second_end:].strip())]


def paradigm(start, where):
    """(1) to (6): the caption, the column heads, then a row to each printed row of cells, a wrapped
    gloss run on."""
    label = gen.EXAMPLE.match(paper.text(start)).group(1)
    caption = gen.EXAMPLE.match(paper.text(start)).group(2)
    language = CAPTIONS[caption[:-1]]
    paper.add("(%s) line 1" % label, A, "note", caption, "page %d, over the example" % paper.page(start))
    paper.add("(%s) line 2" % label, A, "note", paper.text(start + 1), "page %d, the column heads" % paper.page(start))
    rows, number = [], start + 2
    while paper.text(number) and not gen.EXAMPLE.match(paper.text(number)):
        if "/" in paper.text(number):
            rows.append([paper.text(number), number])
        else:
            rows[-1][0] += " " + paper.text(number)
        number += 1
    for index, (text, at) in enumerate(rows, 1):
        for letter, (form, phonemic, gloss) in zip("ab", cells(text)):
            here = "(%s%s) line %d" % (label, letter, index)
            column = "the eventive column" if letter == "a" else "the stative column"
            paper.add(here, language, "transcription", form, "page %d, %s" % (paper.page(at), column))
            paper.add(here, language, "phonemic", phonemic, "page %d, %s" % (paper.page(at), column))
            paper.add(here, A, "translation", gloss, "page %d, %s" % (paper.page(at), column))
    return number


def german(start, where):
    """(7), (8): German over its English gloss."""
    label = gen.EXAMPLE.match(paper.text(start)).group(1)
    paper.add("(%s) line 1" % label, A, "note", gen.EXAMPLE.match(paper.text(start)).group(2),
              "page %d, over the example" % paper.page(start))
    paper.add("(%s) line 2" % label, "German", "transcription", paper.text(start + 1), "page %d" % paper.page(start))
    paper.add("(%s) line 3" % label, A, "translation", paper.text(start + 2),
              "page %d, a gloss word for word that stands as the translation" % paper.page(start))
    return start + 3


def figures(start, where):
    """A line of figure captions, a note to each."""
    text = paper.text(start)
    for found in re.finditer(r"Figure (\d+)[:.].*?(?=\s+Figure \d+:|$)", text):
        paper.add("Figure %s" % found.group(1), A, "note", found.group(0), "page %d, the figure's caption" % paper.page(start))
    return start + 1


blocks = {}
for number in range(1, paper.last + 1):
    opened = gen.EXAMPLE.match(paper.text(number))
    if opened and opened.group(1) in ("1", "2", "3", "4", "5", "6"):
        blocks[number] = paradigm
    elif opened and opened.group(1) in ("7", "8"):
        blocks[number] = german
    elif re.match(r"^Figure \d+: ", paper.text(number)):
        blocks[number] = figures
paper.standard(AUTHORS, NAMES, LANGUAGES, front_languages=2, blocks=blocks, displays={"9": 1, "10": 1})
print("# uncaptioned:", paper.languages_by_caption(CAPTIONS), file=sys.stderr)
paper.write()
