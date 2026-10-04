"""The ops of Bischoff-etal-final: A bibliography of Coeur d'Alene with commentary, by Shannon
Bischoff, Amy Fountain, Audra Vincent and John Ivens: a century of recording the language, from
Cyprian Nicodemus's tales with Teit through Reichard, Lawrence Nicodemus and Doak to the Coeur
d'Alene Online Resource Center, with where each work can be found.

The four authors stand in two columns over their universities, a university wrapping onto a line
of its own, and are read by hand. The tales of (1) and (2) are numbered lists set in two columns,
each title wrapping inside its column; the columns are cut where the right one's numbers stand, and
each title, a lettered part of one too, is a note. (3) is one column and runs past a page's
footnotes onto the next page.
"""
import os
import collections
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
LANGUAGE = "Coeur d’Alene"
AUTHORS = ["Shannon Bischoff", "Amy Fountain", "Audra Vincent", "John Ivens"]
paper = gen.Paper("Bischoff-etal-final", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Cyprian Nicodemus", "community scholar, the tales recorded with Teit"),
         ("James Teit", "recorded Cyprian Nicodemus's tales"), ("Teit", "Coeur d’Alene Tales (1917)"),
         ("Franz Boas", "edited Folk-Tales of Salishan and Sahaptin Tribes"),
         ("Gladys Reichard", "recorded the narratives of 1927 and 1929"), ("Reichard", "Gladys Reichard"),
         ("Dorthy Nicodemus", "community scholar, the narratives with Reichard"),
         ("Tom Miyal", "community scholar, the narratives with Reichard"),
         ("Julia Antelope Nicodemus", "community scholar, two narratives of her own"),
         ("Lawrence Nicodemus", "community scholar, worked with Reichard; the dictionary"),
         ("Brinkman", "Raymond Brinkman (2003)"), ("Raymond Brinkman", "p.c. on the names"),
         ("Ivy Doak", "Coeur d’Alene Grammatical Relations (1997)"), ("Doak", "Ivy Doak"),
         ("Adele Froelich", "compared the narratives with those of other communities"),
         ("Sloat", "Clarence Sloat, the phonology"), ("Lyon", "the root dictionary (2007)"),
         ("Greene-Wood", "the root dictionary (2007)"), ("Wanda Matt", "the language books (2000)"),
         ("Reva Hess", "the language books (2000)"), ("Gary Sobbing", "the language books (2000)"),
         ("Jill Wagner", "the language books (2000)"), ("Dianne Allen", "the language books (2000)")]
LANGUAGES = [(LANGUAGE, "Southern Interior Salish"), ("Snchitsu’umshtsn", "Coeur d’Alene, in its own name"),
             ("Navajo", "Reichard's later work")]
# Each author, the university under them and the page's line it stands on, left column then right.
AUTHOR_LINES = [("Shannon Bischoff", "Indiana University-Purdue University"),
                ("Amy Fountain", "University of Arizona"),
                ("Audra Vincent", "University of British Columbia/Coeur d’Alene Tribe"),
                ("John Ivens", "University of Arizona")]
RUNNING = paper.running_numbers_set()
FOUND = paper.page_footnotes()
SKIP = {one for parts, _ in FOUND.values() for one in parts}
ITEM = re.compile(r"^(?:\d{1,2}|[a-h])\.\s")


def printed(number):
    return paper.text(number).strip() and not paper.lines[number][2] and number not in RUNNING \
        and number not in SKIP


def listed(start, where):
    """A numbered list of titles under its caption, in one column or two, to the first line that
    holds no item or wrap of one: each title a note."""
    number = gen.EXAMPLE.match(paper.text(start)).group(1)
    line = start + 1
    while not ITEM.match(paper.text(line)):
        line += 1
    caption = paper.joined(range(start, line))
    paper.add("(%s)" % number, A, "note", gen.EXAMPLE.match(caption).group(2), "page %d, the caption" % paper.page(start))
    # An item, a line holding both columns, or a title's short wrap is the list's; the first line
    # of prose across the page ends it.
    lines = []
    while line <= paper.last:
        if printed(line):
            if not (ITEM.match(paper.text(line)) or "   " in paper.spaced[line] or len(paper.text(line)) < 45):
                break
            lines.append((line, paper.word_positions(line)))
        line += 1
    # The right column stands where its numbers do, left of the page's middle by a little.
    rights = collections.Counter(round(left) for _, words in lines for left, word in words
                                 if left > 150 and re.fullmatch(r"\d{1,2}\.", word))
    cut = rights.most_common(1)[0][0] - 2 if rights else None
    columns = [[], []]
    for number_line, words in lines:
        left = " ".join(word for at, word in words if cut is None or at < cut)
        right = " ".join(word for at, word in words if cut is not None and at >= cut)
        for side, text in ((0, left), (1, right)):
            if text:
                columns[side].append((number_line, text))
    items = []
    for column in columns:
        for number_line, text in column:
            if ITEM.match(text) or not items:
                items.append([paper.page(number_line), text])
            else:
                # A compound broken at its own hyphen, daughter-in- law, closes up.
                items[-1][1] += ("" if items[-1][1].endswith("-") else " ") + text
    for page, text in items:
        paper.add("(%s)" % number, A, "note", text, "page %d, a title of the list" % page)
    return lines[-1][0] + 1


def caption(start, where):
    """A figure's caption, a line of its own under the scan it names: a note."""
    paper.add(where, A, "note", paper.text(start), "page %d, the caption" % paper.page(start))
    return start + 1


blocks = {one: listed for one in (paper.find(r"^\(1\) Coeur"), paper.find(r"^\(2\) Coeur"), paper.find(r"^\(3\) Recorded"))}
blocks.update({one: caption for one in range(1, paper.last + 1) if re.match(r"^Figure \d ", paper.text(one))})
# The three sections; the numbered titles of the lists, 3. Coyote Overpowers Sun, would read as
# headings and take the conclusion's number.
HEADINGS = {paper.find(r"^1\s+Introduction$"): "1", paper.find(r"^2\s+Discussion of works$"): "2",
            paper.find(r"^3\s+Conclusion$"): "3"}
# Marks the page sets a space after their word, each named by the text before the space.
SPACED = {"4": "Nicodemus", "5": "form,", "10": "volume,", "16": "grammar"}
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, spaced=SPACED, headings=HEADINGS)
# The front's reading of the author block, which splits Purdue's University onto a line of its own
# and sets Ivens beside Vincent's university, gives way to the four authors read by hand.
TITLE = next(index for index, row in enumerate(paper.rows) if row[2] == "title")
ABSTRACT = next(index for index, row in enumerate(paper.rows) if row[3].startswith("Abstract:"))
by_hand = []
for name, university in AUTHOR_LINES:
    by_hand.append(["front", A, "name", name, "author"])
    by_hand.append(["front", A, "note", university, "page 1, under %s" % name])
paper.rows[TITLE + 1:ABSTRACT] = by_hand
paper.write()
