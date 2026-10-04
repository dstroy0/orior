"""The ops of 03-Dunn-Tsimshian-14: John A. Dunn on Tsimshian syllable devolution, the coda
elements {ʔ, h, r, l, j, w} spreading their features to onset and peak and losing them in the
coda, in stages, and the variants of one root that the stages leave across speakers and dialects.

The examples (1) to (37) are lists: each printed line a form or two and a gloss with its source,
B262 for Boas (1912:262), D151 for Dunn (1978), L for the Language Authority, S for the Southern
Tsimshian field notes, G for Gitxsen and N or T for NisGa'a, and most close on the staging the
author reads off them. Each form is a transcription row in the language its source's letter names,
each gloss a translation, each source a citation, and a staging a note with its forms cited after
it. Figures 1 to 6 are syllable trees set in text, a note to each printed line and its caption.

The page text is read closed up (page_text.py closeup): the glyph rows read Gap- Gāᵒp!-El and
Sm 'algyax apart where the page prints each whole.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Coast Tsimshian"
AUTHORS = ["John A. Dunn"]
paper = gen.Paper("03-Dunn-Tsimshian-14", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Dale Kinkade", "confirmed the variability after a field methods course with a Gitxsen speaker"),
         ("Boas", "Franz Boas, Tsimshian (1911) and Tsimshian Texts (1912), the B sources"),
         ("Matthews", "Art Matthews, the Gitxsen lexical database (2001), the G sources"),
         ("Nislaus", "Violet Nislaus, the Southern Tsimshian field notes (1976–1981), the S sources"),
         ("Tarpent", "Marie-Lucie Tarpent, the Nishga Phrase Dictionary (1986), the T sources"),
         ("Williams and Rai", "Verna Williams and Dianna Rai, the NisGa'a Dictionary (2001), the N sources"),
         ("Anderson", "Margaret Anderson et al., Sm'algyax living legacy (2013)"),
         ("Farlex", "The free dictionary by Farlex, the definition of devolution")]
LANGUAGES = [(LANGUAGE, "Maritime Tsimshianic"), ("Tsimshian", "the family, and Coast Tsimshian"),
             ("Sm'algyax", "the Coast Tsimshian name of Coast Tsimshian"),
             ("Southern Tsimshian", "Maritime Tsimshianic, the S sources"),
             ("Sgüüχs", "the Southern Tsimshian name of Southern Tsimshian"),
             ("Gitxsen", "Interior Tsimshianic, the G sources"), ("NisGa'a", "Interior Tsimshianic, the N and T sources"),
             ("English", "its [s] against the Tsimshian [s]")]

# The language of a source by its letter, B262, D151, L146, S9/76, G7, N18 and T413 (section 4.1).
SOURCE_LANGUAGE = {"B": L, "D": L, "L": L, "S": "Southern Tsimshian", "G": "Gitxsen", "N": "NisGa'a", "T": "NisGa'a"}

# The italic runs come off the text layer with a space inside the form at each raised letter and
# with the raised letter set on the line, p ʔ ē oG-al where the page prints pˀēᵒG-al. Each run is
# looked for on its page's lines letter by letter, a raised letter taking its place, and the page
# text's letters are the run.
RAISED = {"ʔ": "ʔˀ", "o": "oᵒ", "y": "yʸ", "w": "wʷ", "\ufffe": "-"}


PAGE_LINES = {}
for number in range(1, paper.last + 1):
    PAGE_LINES.setdefault(paper.page(number), []).append(number)


def repaired(page, run):
    pattern = r"\s*".join("[%s]" % re.escape(RAISED.get(one, one)) for one in run if not one.isspace())
    for number in PAGE_LINES.get(page, ()):
        found = re.search(r"(?<![^\W\d_])%s(?![^\W\d_])" % pattern, paper.text(number))
        if found:
            return found.group(0)
    return None


ITALICS = {}
for page, runs in paper.italics().items():
    for run in runs:
        # A run can hold forms a comma apart, a root and the word that holds it, diHɬ in
        # Ga-diHɬ-g-m-was, or a derivation's steps, daxw> dō.
        for one in re.split(r", | in |\s*>\s*|‘", run):
            found = repaired(page, one) if one.strip() else None
            if found:
                ITALICS.setdefault(page, []).append(found)
paper.italics = lambda: ITALICS

FOOTNOTES = paper.page_footnotes()
AT_FOOT = {one for parts, _ in FOOTNOTES.values() for one in parts} | set(paper.volume_header())
REFERENCES = paper.find(r"^References$")
RUNNING = paper.running_numbers_set()
# A printed line of a list: forms, a gloss in quotes (hūᵒs root’ in (10) prints no opening quote),
# a language after a comma where the source is not Coast Tsimshian, then the source in parentheses
# and a footnote's mark after it. (25) and (31) leave a source's parenthesis unclosed, (B281.
LISTED = re.compile(r"^(?P<forms>.+?) (?P<gloss>‘.*’|\S+’)(?:, (?P<language>[A-Z][\w' ]+?))? "
                    r"(?P<source>\(.*?\)|\([A-Z][\d.]+)(?P<mark>\d)?$")
# A staging, staged haˀq > haaˀq > ..., and staging for the same from (13) on.
STAGED = re.compile(r"^(?:staged|staging)\b")


def printed(number):
    return paper.text(number).strip() and not paper.lines[number][2] and number not in AT_FOOT \
        and number not in RUNNING


def complete(text):
    """Whether a list's printed line has closed its source: a line that wraps inside its gloss or
    its source runs on the next."""
    return bool(re.search(r"\)\d?$|\([A-Z][\d.]+$", text))


def runs_on(number):
    """Whether line number carries on the list line above it: no example opens on it, no heading,
    and no paragraph's indent, the prose after (21) to (24) that closes on no staging."""
    text = paper.text(number)
    return printed(number) and not gen.EXAMPLE.match(text) and not text.startswith(" ") \
        and not re.match(r"^\d+(?:\.\d+)* ", text)


# Page 12, (36): the page prints Ga-qˀaw-tk whole, the raised ˀ close on its q; the text layer sets a
# space before the ˀ. Read at 400 dpi. Page 7's Ga-dīɬ -g-m-wəs prints its space.
CLOSED = {"Ga-q ˀaw-tk": "Ga-qˀaw-tk"}


def word_list(start, where):
    """A numbered list from line start to its staging, or to the first printed line that is no form,
    gloss and source."""
    label = gen.EXAMPLE.match(paper.text(start)).group(1)
    count = [0]

    def row(who, kind, form, line, gloss=None):
        count[0] += 1
        paper.add("(%s) line %d" % (label, count[0]), who, kind, form,
                  "page %d%s" % (paper.page(line), ", " + gloss if gloss else ""))

    line = start
    while line < REFERENCES:
        if not printed(line):
            line += 1
            continue
        text = paper.text(line)
        if line == start:
            text = gen.EXAMPLE.match(text).group(2)
        elif STAGED.match(text):
            lines = [line]
            # (6)'s staging runs onto a second line opening on its >.
            while printed(lines[-1] + 1) and paper.text(lines[-1] + 1).startswith(">"):
                lines.append(lines[-1] + 1)
            body = paper.joined(lines)
            row(A, "note", body, line, "the staging")
            paper.cited("(%s) staging" % label, body, [paper.page(line)])
            return lines[-1] + 1
        lines = [line]
        while not complete(text) and runs_on(lines[-1] + 1):
            lines.append(lines[-1] + 1)
            text += " " + paper.text(lines[-1])
        found = LISTED.match(" ".join(text.split()))
        if not found:
            return line
        source = found.group("source")
        who = SOURCE_LANGUAGE[re.search(r"(?<![\w'])([BDLSGNT])\d", source).group(1)]
        for form in found.group("forms").split(", "):
            # A root and the word that holds it, pˀaˀla in ni-pˀaˀla.
            root, _, word = form.partition(" in ")
            word = CLOSED.get(word, word)
            row(who, "transcription", root, line)
            if word:
                row(who, "transcription", word, line, "the word that holds " + root)
        row(A, "translation", found.group("gloss"), line)
        if found.group("language"):
            row(A, "language", found.group("language"), line, "the language of the line's form")
        row(A, "citation", source + (found.group("mark") or ""), lines[-1],
            "the source" + (", carries footnote " + found.group("mark") if found.group("mark") else ""))
        line = lines[-1] + 1
    return line


def figure(start, where):
    """A syllable tree set in text: a note to each printed line, then its caption."""
    caption = paper.find(r"^Figure \d+ ", start)
    number = re.match(r"^Figure (\d+) ", paper.text(caption)).group(1)
    count = 0
    for line in range(start, caption):
        if printed(line):
            count += 1
            paper.add("Figure %s line %d" % (number, count), A, "note", paper.text(line),
                      "page %d, the tree" % paper.page(line))
    paper.add("Figure %s" % number, A, "note", paper.text(caption),
              "page %d, the caption of Figure %s" % (paper.page(caption), number))
    return caption + 1


blocks = {}
for number in range(1, REFERENCES):
    if number in AT_FOOT:
        continue
    if gen.EXAMPLE.match(paper.text(number)):
        blocks[number] = word_list
    elif paper.text(number).strip() == "σ":
        blocks[number] = figure
# 6.5 and 6.6 open on a bracket, [j]-glide and [w]-glide, which the common heading pattern leaves out.
headings = paper.headings(1, REFERENCES - 1, skip=AT_FOOT)
for number in range(1, REFERENCES):
    bracketed = re.match(r"^(6\.[56]) \[[jw]\]-glide$", paper.text(number))
    if bracketed:
        headings[number] = bracketed.group(1)
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, notes_title=tuple(gen.TITLE_MARKS) + ("1",),
               headings=headings)

# Two entries open on no surname and comma that references() looks for, Farlex with its years in
# parentheses and the Language Authority's name of four words. Each runs onto the entry above it.
for index in range(len(paper.rows) - 1, -1, -1):
    row = paper.rows[index]
    if row[2] != "reference":
        continue
    pieces = re.split(r" (?=Farlex \(2003–2015\)\.|Ts'msyeen Sm'algyax Language Authority\. \(2001\)\.)", row[3])
    if len(pieces) > 1:
        paper.rows[index:index + 1] = [[row[0], row[1], row[2], piece, row[4]] for piece in pieces]
paper.write()
