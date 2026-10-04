"""The ops of 7_Mellesmoen_SV_ComoxSliammon: Gloria Mellesmoen on the voiced obstruents /g/ and /ǰ/ of
Comox-Sliammon, evidence that the language has a Type II voicing system with Sonorant Voice, [SV], and
no [voice], and a development of /ǰ/ and /g/ from Proto-Salish *y and *w that keeps [SV] throughout.

(1), (2), (6) and (12) set a realization of /ǰ/ or /g/ in brackets, the underlying form between
slashes, the surface form in brackets and the gloss: a phonetic row to each bracket, a phonemic row
and a translation, the gloss of (12) printed without quotes. (3), (4) and (7) are forms each with
its gloss, (5) a root between slashes before them. (13) and (14) are an underlying form, a surface
form, a gloss and the source at the right; (13e) to (13k) and (14) print the underlying form with no
slashes. (8) and (10) set two forms side by side, each a column of tiers: in (8) a transcription, its
segmentation, the gloss and a translation to each lettered part, in (10) a segmented form, its gloss
and a translation to each side of a lettered part. (9) and (11) are interlinear and read by
gen.Paper.example: the surface form, the underlying segmentation, the gloss and the translation. The
source under an example is a citation. Tables 1 and 3 are a note to each printed line; Table 2 is a
note to each column head and, to each row, Gibbs's definition a transcription, the modern form a
phonetic row and its gloss a translation. Each figure caption is a note; the spectrograms are not in
the text layer.
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
paper = gen.Paper("7_Mellesmoen_SV_ComoxSliammon", authors=", ".join(AUTHORS), language=LANGUAGE)

SPEAKER = "Comox-Sliammon speaker, thanked in the note on the title"
NAMES = [(name, SPEAKER) for name in ("Phyllis Dominic", "Joanne Francis", "Jerry Francis", "Karen Galligos",
                                      "Marion Harry", "Freddie Louie", "Elsie Paul", "Margaret Vivier",
                                      "Betty Wilson", "Maggie Wilson")]
LANGUAGES = [(LANGUAGE, "Central Salish, British Columbia"), ("ʔayʔaǰuθəm", "the language's own name"),
             ("Proto-Salish", "the source of /ǰ/ and /g/ in *y and *w"),
             ("Twana", "Central Salish, nasals to voiced obstruents"),
             ("Lushootseed", "Central Salish, nasals to voiced obstruents; *y and *w both shifted"),
             ("Squamish", "Central Salish, *w and *y remain"), ("Bella Coola", "*w and *y remain"),
             ("Lillooet", "Interior Salish, only *y shifted; has /ɣ/"),
             ("Thompson", "Interior Salish, only *y shifted; has /ɣ/"),
             ("Straits", "Central Salish, *y to voiceless /č/"), ("Quinault", "[y] or [ǰ] in diminutives"),
             ("Lummi", "a Straits language, adverbs that do not shift"),
             ("English", "loanwords voiceless for voiced, footnote 11")]
REFERENCES = paper.find(r"^References$")
RUNNING = paper.running_numbers_set()
ENGLISH = {"the", "of", "and", "to", "in", "is", "that", "for", "as", "with", "are", "be", "by", "this",
           "which", "we", "on", "it", "not", "or", "from", "can", "an", "these", "has", "have"}
# The section headings. 2.3.1 and 2.3.2 open on a form in brackets.
HEADINGS = {paper.find(pattern): label for label, pattern in (
    ("1", r"^1 Introduction$"), ("2", r"^2 Voiced obstruents in"), ("2.1", r"^2\.1 "), ("2.1.1", r"^2\.1\.1 "),
    ("2.1.2", r"^2\.1\.2 "), ("2.1.3", r"^2\.1\.3 "), ("2.2", r"^2\.2 "), ("2.3", r"^2\.3 "),
    ("2.3.1", r"^2\.3\.1 "), ("2.3.2", r"^2\.3\.2 "), ("2.4", r"^2\.4 "), ("3", r"^3 The diachronic"),
    ("3.1", r"^3\.1 "), ("4", r"^4 Future questions$"), ("5", r"^5 Conclusion$"))}


def printed(number):
    """Whether line number holds printed text: page breaks, page numbers and blank lines hold none."""
    return not paper.lines[number][2] and paper.text(number) and number not in RUNNING


def after(number):
    """The next printed line after number."""
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def row(label, count, who, kind, form, number, gloss=None):
    paper.add("(%s) line %d" % (label, count), who, kind, form,
              "page %d%s" % (paper.page(number), ", " + gloss if gloss else ""))


def prose(text):
    """Whether a line is running prose: nine words or more, three of them English function words."""
    words = text.split()
    return len(words) >= 9 and sum(1 for one in words if one.lower().strip(",.;:()") in ENGLISH) >= 3


def parts(start):
    """([letter, text, line] for each lettered part of the example on line start, the line after
    them). A part is one printed line."""
    found, line, text = [], start, gen.EXAMPLE.match(paper.text(start)).group(2)
    while True:
        sub = gen.SUB.match(text)
        if not sub:
            return found, line
        found.append([sub.group(1), sub.group(2), line])
        line = after(line)
        text = paper.text(line)


def sources(label, count, line):
    """The source lines under an example, (Blake 2000:47) or Watanabe (2003:373,375), each a
    citation; returns the line after them."""
    while re.fullmatch(r"(?:[A-Z][a-z]+ )?\([^()]*\d[^()]*\)", paper.text(line)):
        row(label, count, A, "citation", paper.text(line), line, "under the example")
        line = after(line)
    return line


# (1), (2), (6) and (12): [ǰ] = /huǰ-it/ [hoǰit] ‘ready’.
REALIZED = re.compile(r"^(\[[^\]]*\])\s*=\s*(/[^/]*/)\s+(\[[^\]]*\])\s+(.*)$")


def realized(start, where):
    """(1), (2), (6) or (12): to each part the realization shown a phonetic row, the underlying form a
    phonemic row, the surface form a phonetic row and the gloss a translation; the source under the
    parts a citation."""
    label = gen.EXAMPLE.match(paper.text(start)).group(1)
    found, line = parts(start)
    for letter, text, at in found:
        shown, underlying, surface, said = REALIZED.match(text).groups()
        row(label + letter, 1, L, "phonetic", shown, at, "the realization the part shows")
        row(label + letter, 2, L, "phonemic", underlying, at)
        row(label + letter, 3, L, "phonetic", surface, at)
        row(label + letter, 4, A, "translation", said, at, "" if said.startswith("‘") else "printed without quotes")
    return sources(label + found[-1][0], 5, line)


def glossed(start, where):
    """(3), (4), (5) or (7): to each part each form a transcription and its gloss a translation, in the
    order printed; the root between slashes that opens a part of (5) a phonemic row. The source under
    the parts a citation."""
    label = gen.EXAMPLE.match(paper.text(start)).group(1)
    found, line = parts(start)
    for letter, text, at in found:
        count = 0
        root = re.match(r"^(/[^/]*/)\s+(.*)$", text)
        if root:
            count += 1
            row(label + letter, count, L, "phonemic", root.group(1), at, "the root")
            text = root.group(2)
        for form, said in re.findall(r"(\S+)\s+(‘[^‘]*’)", text):
            count += 1
            row(label + letter, count, L, "transcription", form, at)
            count += 1
            row(label + letter, count, A, "translation", said, at)
    return sources(label + found[-1][0], count + 1, line)


# (13) and (14): /č̌̓ag=tn/ [č̌̓ɛwtən] ‘helper’ (Blake 2000:337).
SOURCED = re.compile(r"^(.*?)\s+(\[[^\]]*\])\s+(‘[^’]*’)\s+(\(.*\))$")


def sourced(start, where):
    """(13) or (14): to each part the underlying form a phonemic row, the surface form a phonetic row,
    the gloss a translation and the source at the right a citation."""
    label = gen.EXAMPLE.match(paper.text(start)).group(1)
    found, line = parts(start)
    for letter, text, at in found:
        underlying, surface, said, source = SOURCED.match(text).groups()
        row(label + letter, 1, L, "phonemic", underlying, at,
            "" if underlying.startswith("/") else "printed with no slashes")
        row(label + letter, 2, L, "phonetic", surface, at)
        row(label + letter, 3, A, "translation", said, at)
        row(label + letter, 4, A, "citation", source, at, "the source at the right")
    return line


def halves(text):
    """The two sides of a line set in two columns: two words, or two glosses in quotes."""
    quoted = re.findall(r"‘[^‘]*’", text)
    return quoted if text.startswith("‘") else text.split()


def paired(start, where):
    """(8): the lettered parts side by side, a. ǩ̓ʷət b. ʔaq̌̓nampič, and under them each line a tier of
    both, to each part a transcription, its segmentation, the gloss and a translation."""
    label, text = gen.EXAMPLE.match(paper.text(start)).groups()
    heads = re.match(r"^a\.\s+(\S+)\s+b\.\s+(\S+)$", text).groups()
    for letter, form in zip("ab", heads):
        row(label + letter, 1, L, "transcription", form, start)
    line = after(start)
    for count, kind in enumerate(("segmentation", "gloss", "translation"), 2):
        for letter, form in zip("ab", halves(paper.text(line))):
            row(label + letter, count, A if kind == "translation" else L, kind, form, line)
        line = after(line)
    return line


def compared(start, where):
    """(10): to each lettered part two forms side by side, the form with /g/ as [w] on the left and the
    one where /g/ stays [g] on the right, each a segmented form over its gloss and a translation. A
    part is three printed lines, its forms, their glosses and their translations."""
    label, text = gen.EXAMPLE.match(paper.text(start)).groups()
    line = start
    while gen.SUB.match(text):
        letter, forms = gen.SUB.match(text).groups()
        glosses = after(line)
        said = after(glosses)
        tiers = [halves(forms), halves(paper.text(glosses)), halves(paper.text(said))]
        for side, what in enumerate(("the form on the left", "the form on the right")):
            base = 3 * side
            row(label + letter, base + 1, L, "segmentation", tiers[0][side], line, what)
            row(label + letter, base + 2, L, "gloss", tiers[1][side], glosses, what)
            row(label + letter, base + 3, A, "translation", tiers[2][side], said, what)
        line = after(said)
        text = paper.text(line)
    return line


def interlinear(start, where):
    """(9) or (11): read by gen.Paper.example."""
    return paper.example(start)


def lines_table(start, where):
    """Table 1 or 3: its caption and each printed line under it a note, to the prose after it."""
    name = re.match(r"^(Table \d+)", paper.text(start)).group(1)
    paper.add(name, A, "note", paper.text(start), "page %d, the table's caption" % paper.page(start))
    line, count = after(start), 0
    while line < REFERENCES and not prose(paper.text(line)):
        count += 1
        paper.add("%s line %d" % (name, count), A, "note", paper.text(line), "page %d, %s" % (paper.page(line), name))
        line = after(line)
    return line


# Table 2: bo-osh’ [moʔos] ‘head’.
GIBBS = re.compile(r"^(\S+)\s+(\[[^\]]*\])\s+(‘.*’)$")


def gibbs(start, where):
    """Table 2: the caption a note, each column head a note, and to each row Gibbs's definition a
    transcription, the modern form a phonetic row and its gloss a translation."""
    paper.add("Table 2", A, "note", paper.text(start), "page %d, the table's caption" % paper.page(start))
    line = after(start)
    heads = re.match(r"^(Gibbs \(1877\)) (\(Modern\) Mainland Comox-Sliammon) (Translation)$", paper.text(line)).groups()
    page = paper.page(line)
    for head in heads:
        paper.add("Table 2 line 1", A, "note", head, "page %d, Table 2, a column head" % page)
    line, count = after(line), 1
    while GIBBS.match(paper.text(line)):
        count += 1
        cells = GIBBS.match(paper.text(line)).groups()
        for (who, kind), cell, head in zip(((L, "transcription"), (L, "phonetic"), (A, "translation")), cells, heads):
            paper.add("Table 2 line %d" % count, who, kind, cell, "page %d, Table 2, %s" % (page, head))
        line = after(line)
    return line


def caption(start, where):
    """A figure's caption, a note."""
    name = re.match(r"^(Figure \d+)", paper.text(start)).group(1)
    paper.add(name, A, "note", paper.text(start), "page %d, the figure's caption" % paper.page(start))
    return after(start)


def at(label):
    return paper.find(r"^\(%s\) " % label)


blocks = {paper.find(r"^Table 1: "): lines_table, paper.find(r"^Table 2: "): gibbs,
          paper.find(r"^Table 3: "): lines_table}
blocks.update({paper.find(r"^Figure %d: " % number): caption for number in range(1, 5)})
for labels, reader in ((("1", "2", "6", "12"), realized), (("3", "4", "5", "7"), glossed),
                       (("13", "14"), sourced), (("8",), paired), (("10",), compared), (("9", "11"), interlinear)):
    blocks.update({at(label): reader for label in labels})
# Note 1 is marked on the title, Comox-Sliammon1.
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, headings=HEADINGS,
               notes_title=tuple(gen.TITLE_MARKS) + ("1",))

# Three entries open on no surname and initials that references() takes for an entry's opening: the
# First Peoples' Cultural Council's, after Davis (2005); Gibbs's, after the council's web address,
# which runs over a line; and Van Eijk's, a surname of two words, after Tolmie and Dawson's.
for opening in (" First Peoples’ Cultural Council. (2014).", " Gibbs, G. (1877).", " Van Eijk, J. (2011)."):
    holder = next(one for one in paper.rows if one[2] == "reference" and opening in one[3])
    split = holder[3].index(opening)
    paper.rows.insert(paper.rows.index(holder) + 1, [holder[0], holder[1], "reference", holder[3][split + 1:], holder[4]])
    holder[3] = holder[3][:split]
paper.write()
