"""The ops of 02-Brown-heavy-syllables-12: Jason Brown on heavy syllables in Gitksan, the weight that
stress gives a long vowel and not a coda consonant, and the weight that reduplication and the minimal
word give a coda consonant too.

The examples are word lists, a form or two and a gloss to a printed line: (4) and (5) set a root
beside its reduplicated form, (9) a form beside its variant after a comma, and (11) a form beside its
pronunciation in brackets. Each form is a transcription row (the bracketed one phonetic) and each
gloss a translation row, the footnote mark after a gloss kept on it; the name over a list, CV-
reduplication, is a note. The ‘ of ‘naː and laχ‘ní is a letter of the orthography, never a gloss's
opening quote: a gloss opens on a ‘ after a space and runs to the ’ that ends the line.

The page text is read closed up (page_text.py closeup): the glyph rows read t’eː ‘lt apart where
the page prints t’eː‘lt whole.
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
paper = gen.Paper("02-Brown-heavy-syllables-12", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Barbara Sennott", "the author's Gitxsanimx teacher"),
         ("Doreen Jensen", "the author's Gitxsanimx teacher, late"),
         ("Henry Davis", "read and commented on earlier drafts"),
         ("Clarissa Forbes", "read and commented on earlier drafts; Forbes 2015 on Gitksan stress"),
         ("Forrest Panther", "read and commented on earlier drafts"),
         ("Tyler Peterson", "read and commented on earlier drafts"),
         ("Michael Schwan", "read and commented on earlier drafts, pointed out the offglide")]
LANGUAGES = [(LANGUAGE, "Interior Tsimshianic"), ("Gitxsanimx", "the Gitksan name of Gitksan"),
             ("Coast Tsimshian", "Maritime Tsimshianic, writes the offglide"),
             ("Maori", "Austronesian"), ("Kashmiri", "Indo-European"), ("Mam", "Mayan"),
             ("Latin", "a language with weight inconsistencies"), ("Kiowa", "a language with weight inconsistencies"),
             ("Lhasa Tibetan", "a language with weight inconsistencies"),
             ("Nisgha", "Interior Tsimshianic, related to Gitksan"), ("English", "its function words")]

# The volume's header opens on In Papers for the International Conference, which
# gen.Paper.volume_header does not take for its opening.
HEADER_FIRST = paper.find(r"^In Papers for the International Conference", 1, 20)
HEADER = list(range(HEADER_FIRST, paper.find(r"\b(?:19|20)\d\d\.$", HEADER_FIRST, HEADER_FIRST + 2) + 1))
paper.volume_header = lambda: HEADER

FOOTNOTES = paper.page_footnotes()
AT_FOOT = {one for parts, _ in FOOTNOTES.values() for one in parts} | set(HEADER)
REFERENCES = paper.find(r"^References$")
RUNNING = paper.running_numbers_set()
# A printed line of a list: at most three forms, a comma between variants, then a gloss that opens
# after a space and closes the line, a footnote's number after it or none.
LISTED = re.compile(r"^(?P<forms>.*?)\s+(?P<gloss>‘[^‘]+’)(?P<mark>\d{1,2})?$")


def printed(number):
    return paper.text(number).strip() and not paper.lines[number][2] and number not in AT_FOOT \
        and number not in RUNNING


def listed(text):
    line = LISTED.match(text)
    if not line or len(line.group("forms").replace(", ", ",").split()) > 3:
        return None
    return line


def word_list(start, where):
    """A numbered word list from line start to the first printed line that is not a form and gloss."""
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
            text = gen.EXAMPLE.match(text).group(2) or ""
        found = listed(text)
        if line == start and not found:
            row(A, "note", text, line, "the name of the list")
            line += 1
            continue
        if not found:
            break
        for form in re.findall(r"\[[^\]]+\]|[^\s,\[]+", found.group("forms")):
            row(L, "phonetic" if form.startswith("[") else "transcription", form, line)
        row(A, "translation", found.group("gloss") + (found.group("mark") or ""), line)
        line += 1
    return line


def figure(start, where):
    """Figure 1, the two syllable trees: a note to each printed line, then its caption."""
    caption = paper.find(r"^Figure 1 ", start)
    count = 0
    for line in range(start, caption):
        if printed(line):
            count += 1
            paper.add("Figure 1 line %d" % count, A, "note", paper.text(line), "page %d, the figure" % paper.page(line))
    paper.add("Figure 1", A, "note", paper.text(caption), "page %d, the figure's caption" % paper.page(caption))
    return caption + 1


blocks = {}
at = 1
for label in range(1, 12):
    at = next(one for one in range(at, REFERENCES) if one not in AT_FOOT
              and re.match(r"^\(%d\)(?:\s|$)" % label, paper.text(one)))
    blocks[at] = word_list
blocks[paper.find(r"^σ$", paper.find(r"^2\.1 Implications$"))] = figure
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)

# An entry that wraps after an editor's initial, In T. Honma, M. / Okazaki, or onto a line opening on
# a surname, Hume and K. Rice, runs on the entry above it: every entry opens Surname, X.
paper.merge_references(r"^[A-Z][\w’'\-]+, [A-Z]\.")
paper.write()
