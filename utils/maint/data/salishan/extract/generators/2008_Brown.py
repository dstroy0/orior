"""The ops of 2008_Brown: Jason Brown's An unexpected gap in Gitksan consonant cluster phonotactics,
Rigsby's (1986) word-initial clusters summed up, and the stop + sonorant sequence the language never
has, though it has fricative + sonorant.

The examples are word lists set in columns, a form and its gloss to a cell: (1) a form beside its
possessed form, (2) a singleton beside a cluster under two column heads, (3) and (4) the inventories
of bi- and tri-consonantal clusters, a row to each form under its sequence type, (5) the apparent
exceptions in brackets, (6) the singular beside its plural and (7) the underlying forms between
slashes. Each form is a transcription row, a bracketed one phonetic and a slashed one phonemic, each
gloss a translation row, and each head or sequence type a note. The gloss of tχalpχ, ‘four (things,
animals), is printed with no closing quote.

The fonts are CID TrueType subsets with no ToUnicode; page_text.outlined_fonts names their glyphs by
outline, and the paper is read by glyph rows (page_text.py rows), the stream setting its word spaces.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Gitksan"
AUTHORS = ["Jason Brown"]
paper = gen.Paper("2008_Brown", authors=AUTHORS[0], language=LANGUAGE)

NAMES = [("Barbara Sennott", "the author's Gitksan teacher"),
         ("Doreen Jensen", "the author's Gitksan teacher"),
         ("Henry Davis", "discussed the ideas presented here"),
         ("Gunnar Hansson", "discussed the ideas presented here"),
         ("Douglas Pulleyblank", "discussed the ideas presented here"),
         ("Bruce Rigsby", "Rigsby (1986), Gitksan grammar; discussed the paper with the author")]
LANGUAGES = [(LANGUAGE, "Interior Tsimshianic"), ("Tsimshianic", "the family of Gitksan"),
             ("Coast Tsimshian", "Tsimshianic; its [dloq’] and its reduplication"),
             ("French", "the source of the loanword [libleːt] ‘priest’"),
             ("English", "its schwa deletion in fast speech")]

REFERENCES = paper.find(r"^References$")
AT_FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
RUNNING = paper.running_numbers_set()
# The sequence type heading a row of (3) and (4), Stop+Stop or Fricative+Fricative+Stop, set before
# the row's form by a column's gap or a single space.
SEQUENCE = re.compile(r"^((?:Stop|Fricative)(?:\s*\+\s*(?:Stop|Fricative|Sonorant|ʔ|h))+)(?:\s+(.*))?$")


def printed(number):
    return paper.text(number).strip() and not paper.lines[number][2] and number not in AT_FOOT \
        and number not in RUNNING


def cells(text):
    """A row's cells: the column gaps part them, and a gloss set a single space after its form,
    muwin ‘your (sg.) ear’, parts from it."""
    found = []
    for cell in re.split(r"\s{3,}", text.strip()):
        form, quote, gloss = cell.partition(" ‘")
        found.append(form)
        if quote:
            found.append("‘" + gloss)
    return [one for one in found if one]


def word_list(start, where):
    """A numbered word list in columns from line start to the first printed line with no column gap."""
    label = gen.EXAMPLE.match(paper.text(start)).group(1)
    count = [0]

    def row(who, kind, form, line, gloss=None):
        count[0] += 1
        paper.add("(%s) line %d" % (label, count[0]), who, kind, form,
                  "page %d%s" % (paper.page(line), ", " + gloss if gloss else ""))

    text = gen.EXAMPLE.match(paper.text(start)).group(2) or ""
    line = start + 1
    if text.startswith("Inventory"):
        # The inventories' titles wrap: (summation of / Rigsby 1986).
        row(A, "note", text + " " + paper.text(line), start, "the title of the table")
        line += 1
        heads = True
    else:
        heads = not re.search(r"‘|^[\[/]", text)
        line = start
    while line < REFERENCES:
        if not printed(line):
            line += 1
            continue
        # The line as the page text sets it, its column gaps kept.
        text = paper.spaced[line]
        if line == start:
            text = re.sub(r"^\(\d\)\s*", "", text)
        # A heading sets a column gap after its number, 2.3   Initial tri-consonantal clusters.
        elif not re.search(r"\s{3}", text) or re.match(r"\d+(?:\.\d+)*\.?\s", text):
            break
        found = cells(text)
        if heads:
            for cell in found:
                row(A, "note", cell, line, "a column's head")
            heads = False
            line += 1
            continue
        sequence = SEQUENCE.match(found[0])
        if sequence:
            row(A, "note", sequence.group(1), line, "the sequence type")
            found = ([sequence.group(2)] if sequence.group(2) else []) + found[1:]
        for cell in found:
            if cell.startswith("‘"):
                row(A, "translation", cell, line)
            elif cell == "* * *":
                row(A, "note", cell, line, "no example, the gap")
            elif cell.startswith("["):
                row(L, "phonetic", cell, line)
            elif cell.startswith("/"):
                row(L, "phonemic", cell, line)
            else:
                row(L, "transcription", cell, line)
        line += 1
    return line


blocks = {}
at = 1
for label in range(1, 8):
    at = next(one for one in range(at, REFERENCES) if one not in AT_FOOT
              and re.match(r"^\(%d\)(?:\s|$)" % label, paper.text(one)))
    blocks[at] = word_list
# The paper numbers its conclusion 5, after 3 Discussion, and headings() takes a number only where it
# follows the one before.
start = paper.find(r"^1 Introduction$")
headings = paper.headings(start, REFERENCES - 1)
headings[paper.find(r"^5 Conclusion$", start)] = "5"
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, appendix=r"^Jason Brown$", headings=headings)
# Clements's and Zwicky's entries wrap after an editor's initial, In J. / Kingston & M.E. Beckman,
# onto a line that opens like an entry.
paper.merge_references()
tail = paper.find(r"^Jason Brown$", REFERENCES)
paper.add("end", A, "note", paper.joined([tail, tail + 1]),
          "page %d, the author's name and e-mail address" % paper.page(tail))
paper.write()
