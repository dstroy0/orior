"""The ops of 22_ICSNL55_Webb_final: Clark Webb on Accelerated Second Language Acquisition (ASLA)
as the primary teaching method of Gumbaynggirr, from his first sight of it in 2010 to the community
lessons, the Kulai preschool, his own household and a planned immersion school.

The paper is prose. Its Gumbaynggirr stands in one glossed example, (1), a phrase a child made up,
and a bulleted list of the phrases the child was taught, each in italics with its gloss in
parentheses. Two Arapaho story titles on page 2 are italic too. Gumbaynggirr is defined in plain
Latin letters, and a form is told from English by its italics alone.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
LANGUAGE = "Gumbaynggirr"
AUTHORS = ["Clark Webb"]
paper = gen.Paper("22_ICSNL55_Webb_final", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Clark Webb", "the author, Bularri Muurlay Nyanggan Aboriginal Corporation"),
         ("Greymorning", "Neyooxet Greymorning, the maker of ASLA"),
         ("Amber", "Greymorning's daughter, who told Arapaho stories in 2010"),
         ("Cecil ‘Bing’ Laurie", "the last first language speaker of Gumbaynggirr"),
         ("Nathan", "a learner of the author's"), ("Kaleesha", "a learner of the author's"),
         ("Kamla", "the author's wife"), ("Jayalaani", "the author's daughter"),
         ("Morelli", "Steve Morelli, the Gumbaynggirr Dictionary (2015) and Stories Book (2016)"),
         ("Williams", "Gary Williams, with Morelli and Walker (2016)"),
         ("Walker", "Dallas Walker, with Morelli and Williams (2016)"),
         ("Campbell", "Lyle Campbell and others, The Catalogue of Endangered Languages (2017)"),
         ("Knightley", "Philip Knightley, on the Australian policy of taking children (2001)"),
         ("Australians Together", "on the lack of a treaty (2020)")]
LANGUAGES = [(LANGUAGE, "Pama-Nyungan, of the New South Wales north coast, taught through ASLA"),
             ("Arapaho", "Algonquian, the language Greymorning teaches ASLA in"),
             ("Bundjalung", "a neighboring language"), ("Dungghutti", "a neighboring language"),
             ("English", "the learners' first language")]
# The Arapaho titles of two stories Amber told, each with its English title after a dash.
ARAPAHO = {"Coo’ouu3ih’oohut", "Notkonii’hii"}


def form_language(run):
    """The language of an italic run: the two Arapaho story titles, and the Gumbaynggirr of page 6.
    The volume header, a web address and the titles in the references are no forms."""
    if run in ARAPAHO:
        return "Arapaho"
    if paper.find(r"^• %s \(" % run.replace("(", r"\(").replace(")", r"\)")) or run.startswith("Nyamiganambu"):
        return LANGUAGE
    return None


def items(start, where):
    """A list: each item, opened on its number or a bullet, a note with its wrapped lines, to the
    blank line under the last. Item 7 of the list on page 4 runs over the page break. A bullet's
    Gumbaynggirr, in italics, is a cited form with the gloss in parentheses after it."""
    running = paper.running_numbers_set()
    rows, broken, number = [], False, start
    while number <= paper.last:
        text = paper.text(number)
        if paper.lines[number][2] or number in running:
            broken = True
        elif not text.strip():
            if not broken:
                break
        else:
            broken = False
            if re.match(r"^(?:\d+\.|•)\s", text) or not rows:
                rows.append([number])
            else:
                rows[-1].append(number)
        number += 1
    for lines in rows:
        body = paper.joined(lines)
        paper.add(where, A, "note", body, "page %d" % paper.page(lines[0]))
        bullet = re.match(r"^•\s+(.+?) \((‘[^’]+’)\)$", body)
        if bullet:
            paper.add(where, LANGUAGE, "cited form", bullet.group(1),
                      "page %d, in italics, %s" % (paper.page(lines[0]), bullet.group(2)))
            paper.cited_done.add(bullet.group(1))
        paper.mentions(where, body, NAMES, "name")
        paper.mentions(where, body, LANGUAGES, "language")
    return number


paper.form_language = form_language
REFERENCES = paper.find(r"^References$")
SKIP = {one for parts, _ in paper.page_footnotes().values() for one in parts}
# 4.3 is set at the end of the paragraph over it, on the same line (see below), and the headings
# in sequence would leave out 4.4 after 4.2.
HEADINGS = paper.headings(1, REFERENCES - 1, skip=SKIP)
HEADINGS[paper.find(r"^4\.4 Learners")] = "4.4"
blocks = {number: items for number in range(1, REFERENCES)
          if re.match(r"^(?:1\.|•)\s", paper.text(number)) and not re.match(r"^(?:\d+\.|•)\s", paper.text(number - 1))}
paper.standard(AUTHORS, NAMES, LANGUAGES, front_languages=1, blocks=blocks, headings=HEADINGS)

# The page prints 4.3 Kulai Aboriginal Preschool in bold at the end of the paragraph's last line,
# images in skillset five. 4.3 Kulai Aboriginal Preschool: the heading is cut off that note and set
# after the paragraph's rows, and the rows up to 4.4 are under it.
INLINE = "4.3 Kulai Aboriginal Preschool"
at = next(index for index, row in enumerate(paper.rows) if row[2] == "note" and row[3].endswith(" " + INLINE))
paper.rows[at][3] = paper.rows[at][3][:-len(INLINE)].rstrip()
after = next(index for index in range(at + 1, len(paper.rows)) if paper.rows[index][2] in ("note", "heading"))
paper.rows.insert(after, ["§4.3", A, "heading", INLINE, "page %d, at the end of the paragraph's last line"
                          % paper.page(paper.find(r"4\.3 Kulai"))])
for row in paper.rows[after + 1:]:
    if row[2] == "heading":
        break
    if row[0] == "§4.2":
        row[0] = "§4.3"
# (1) glosses its words in plain English, Girl (ergative). walking water drinking, with no label in
# capitals for gen.is_gloss to find.
for row in paper.rows:
    if row[0] == "(1) line 2":
        row[2] = "gloss"
paper.write()
