"""The ops of 15_ICSNL55_Mellesmoen_Urbanczyk_final: Gloria Mellesmoen and Suzanne Urbanczyk on the
allomorphs of the Hul’q’umi’num’ imperfective, reduplication, ablaut, metathesis, a glottal stop
infix, sonorant aspiration, schwa deletion and insertion, analyzed as a prefixed empty mora.

Most examples are lists of pairs, a perfective and its gloss, then the imperfective and its gloss,
and in (1) the allomorph's name after them. Each form is a transcription row and each gloss a
translation row, in the order printed, with a name or a parenthesized form after the last gloss a
note or a phonemic row. A lettered line with no gloss (a. Resultative), a root heading (√root =
‘dry’) and the column header (Perfective Imperfective) are notes. The constraints, the tableaux and
the mora diagrams are a note to each printed line; the underlining the page sets on the part of
each imperfective that shows the allomorph (1) is not in the text layer.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Hul’q’umi’num’"
AUTHORS = ["Gloria Mellesmoen", "Suzanne Urbanczyk"]
paper = gen.Paper("15_ICSNL55_Mellesmoen_Urbanczyk_final", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = []
LANGUAGES = [(LANGUAGE, "Island Halkomelem, Central Salish")]
FOOTNOTES = paper.page_footnotes()
AT_FOOT = {one for parts, _ in FOOTNOTES.values() for one in parts}
REFERENCES = paper.find(r"^References$")

# The perfective and imperfective pairs.
PAIRS = {1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15, 32, 35, 36}
# The constraints, tableaux and mora diagrams, a note to each printed line.
LINES = {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 33, 34, 37}
PART = re.compile(r"^([a-z])\.\s+(.*)$")
ROMAN = re.compile(r"^(i{1,3})\.\s+(.*)$")
ENGLISH = {"the", "of", "and", "to", "in", "is", "that", "for", "as", "with", "are", "be", "by", "this",
           "which", "we", "on", "it", "not", "or", "from", "can", "an", "these", "has", "have"}
# A gloss closes on ’ or ” before a space, a stop or the line's end, never on the ’ of one’s.
CLOSE = re.compile(r"[’”](?=\s|$|[.,;)])")


def prose(text):
    """Whether a line is running prose: nine words or more, three of them English function words.
    A line of pairs, t̓ə́m̓ət ‘pound on it, beat drum’ ..., opens on a lowercase form or a letter."""
    words = text.split()
    if "‘" in text and not text[:1].isupper():
        return False
    return len(words) >= 9 and sum(1 for one in words if one.lower().strip(",.;:()") in ENGLISH) >= 3


def printed(number):
    return paper.text(number).strip() and not paper.lines[number][2] and number not in AT_FOOT \
        and number not in paper.running_numbers_set()


def pairs_of(spaced):
    """[(kind, text)] for a line of pairs: each form, each gloss in its quotes, and what follows
    the last gloss. A gloss the page leaves open, ‘wrap it up in (32c) and ‘swallow in (7c), ends
    before the form ahead of the next gloss, or at the line's end. A stop after a gloss closes it,
    ‘gathering’. in (9a); a word in parentheses after one, (perfective) in footnote 5, is a note."""
    found, rest = [], spaced.strip()
    while rest:
        opened = rest.find("‘")
        if opened < 0:
            found.append(("tail", " ".join(rest.split())))
            break
        form = " ".join(rest[:opened].split())
        said = re.match(r"^(\(\w+\))\s+(.*)$", form)
        if said:
            found.append(("tail", said.group(1)))
            form = said.group(2)
        if form:
            found.append(("form", form))
        rest = rest[opened:]
        closed = CLOSE.search(rest)
        again = rest.find("‘", 1)
        if closed and (again < 0 or closed.start() < again):
            end = closed.end() + len(re.match(r"[.,;]*", rest[closed.end():]).group(0))
        elif again > 0:
            end = rest[:again].rstrip().rfind(" ")
            end = end if end > 0 else again
        else:
            end = len(rest)
        found.append(("gloss", " ".join(rest[:end].split())))
        rest = rest[end:].strip()
    return found


def pairs(start, where):
    """A list of pairs from line start, a lettered part at a time, to the prose after it."""
    number_label = gen.EXAMPLE.match(paper.text(start)).group(1)
    counts = {}
    label = "(%s)" % number_label

    def row(person, kind, form, line, gloss=None):
        counts[label] = counts.get(label, 0) + 1
        paper.add("%s line %d" % (label, counts[label]), person, kind, form,
                  "page %d%s" % (paper.page(line), ", " + gloss if gloss else ""))

    line, letter = start, None
    while line < REFERENCES:
        if not printed(line):
            line += 1
            continue
        text, spaced = paper.text(line), paper.spaced[line]
        if line > start and (gen.EXAMPLE.match(text) or prose(text) or line in HEADINGS):
            break
        if line == start:
            text = gen.EXAMPLE.match(text).group(2) or ""
            spaced = re.sub(r"^\s*\(\d+\)\s*", "", spaced)
        # (11)'s pairs i. and ii. under each lettered root, (11ai) and (11aii).
        roman = ROMAN.match(text) if letter else None
        part = None if roman else PART.match(text)
        if roman:
            label = "(%s%s%s)" % (number_label, letter, roman.group(1))
            text = roman.group(2)
            spaced = re.sub(r"^\s*[ivx]+\.\s+", "", spaced)
        elif part:
            letter = part.group(1)
            label = "(%s%s)" % (number_label, letter)
            text = part.group(2)
            spaced = re.sub(r"^\s*[a-z]\.\s+", "", spaced)
        if not text:
            pass
        elif "‘" not in text or re.match(r"^(?:√root|Root) =", text):
            row(A, "note", text, line, "set over the pairs" if line == start or part else "the columns' header")
        else:
            for kind, piece in pairs_of(spaced):
                if kind == "form":
                    row(L, "phonemic" if piece.startswith("/") else "transcription", piece, line)
                elif kind == "gloss":
                    row(A, "translation", piece, line)
                elif re.fullmatch(r"\(/[^()]+/\)", piece):
                    row(L, "phonemic", piece[1:-1], line, "in parentheses after the gloss")
                else:
                    row(A, "note", piece, line, "the allomorph" if number_label == "1" else "after the gloss")
        line += 1
    return line


def lines(start, where):
    """A constraint, tableau or diagram: a note to each printed line, to the prose after it."""
    number_label = gen.EXAMPLE.match(paper.text(start)).group(1)
    line, count = start, 0
    while line < REFERENCES:
        if not printed(line):
            line += 1
            continue
        text = paper.text(line)
        if line > start and (gen.EXAMPLE.match(text) or prose(text) or line in HEADINGS):
            break
        if line == start:
            text = gen.EXAMPLE.match(text).group(2) or ""
        if text:
            count += 1
            paper.add("(%s) line %d" % (number_label, count), A, "note", text,
                      "page %d, set as an example" % paper.page(line))
        line += 1
    return line


def table(start, where):
    """A table: its caption a note and each printed line under it a note, to the prose after it."""
    name = re.match(r"^(Table \d+)", paper.text(start)).group(1)
    paper.add(name, A, "note", paper.text(start), "page %d, the table's caption" % paper.page(start))
    line, count = start + 1, 0
    while line < REFERENCES:
        if not printed(line):
            line += 1
            continue
        text = paper.text(line)
        if prose(text) or line in HEADINGS:
            break
        count += 1
        paper.add("%s line %d" % (name, count), A, "note", text, "page %d, %s" % (paper.page(line), name))
        line += 1
    return line


HEADINGS = paper.headings(1, REFERENCES - 1, skip=AT_FOOT)
blocks = {paper.find(r"^Table 1: "): table, paper.find(r"^Table 2: "): table}
at = 1
for label in sorted(PAIRS | LINES):
    at = next(one for one in range(at, REFERENCES) if one not in AT_FOOT
              and re.match(r"^\(%d\)(?:\s|$)" % label, paper.text(one)))
    blocks[at] = pairs if label in PAIRS else lines
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, headings=HEADINGS)
# Footnote 5's (i) sets one pair, read by the footnote's example reader as a single form.
for index, (where, who, kind, form, gloss) in enumerate(paper.rows):
    if where.startswith("footnote 5 (i)") and kind == "transcription":
        rows = []
        for count, (piece_kind, piece) in enumerate(pairs_of(form), 2):
            rows.append(["footnote 5 (i) line %d" % count, L if piece_kind == "form" else A,
                         {"form": "transcription", "gloss": "translation"}.get(piece_kind, "note"), piece,
                         gloss + (", after the gloss" if piece_kind == "tail" else "")])
        paper.rows[index:index + 1] = rows
        break
paper.write()
