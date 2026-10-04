"""The ops of 2010_Mayer: Connor Mayer's Voice onset time and the realization of voiced stops in
Kwak'wala, an acoustic study of one speaker's 669 tokens of the ejective, aspirated and voiced stops
and affricates, word-initially and between vowels, before /u/, /i/ and /a/.

Table 1 sets the inventory in three rows, a series each, a cited form each. Tables 2 and 3 and the
eight figures are images the layer does not carry: the captions of the tables are notes, and so are
the figure labels, Fig. 1 to Fig. 8, which the two-column pages set among the prose. The heading of
§4.2 wraps onto the line of the label Fig. 3.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Kwak'wala"
AUTHORS = ["Connor Mayer"]
paper = gen.Paper("2010_Mayer", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Ladefoged", "Peter Ladefoged, phonetic data analysis (2003), and with Cho, VOT in 18 languages (1999)"),
         ("Lisker & Abramson", "Leigh Lisker and Arthur Abramson, a cross-language study of voicing (1964)"),
         ("Boas", "Franz Boas, Kwakiutl in the Handbook of American Indian Languages (1911)"),
         ("Grubb", "David McC. Grubb, a Kwakiutl phonology (1969)"),
         ("Lincoln & Rath", "Neville Lincoln and John Rath, the North Wakashan comparative root list"),
         ("Cho & Jun", "Taehong Cho and Sun-Ah Jun, domain-initial strengthening (2000)"),
         ("Westbury", "John Westbury, supraglottal cavity enlargement (1983)"),
         ("Bell-Berti", "Fredericka Bell-Berti, pharyngeal cavity size (1975)"),
         ("Ohala", "John Ohala, passive vocal tract enlargement with Riordan (1979) and the origin of sound patterns"),
         ("Riordan", "Carol Riordan, with Ohala (1979)"),
         ("Keating", "Patricia Keating, the representation of stop voicing (1984)"),
         ("Flege", "James Flege, laryngeal timing in English stops (1982)"),
         ("Babel", "Molly Babel, the effects of moribundity (2008)")]
LANGUAGES = [(LANGUAGE, "Northern Wakashan, the language measured"),
             ("English", "its voicing, which the speaker's resembles"),
             ("French", "its [+voice], which requires modal voicing"),
             ("Pauite", "Babel's study of moribundity")]
SERIES = ("the ejective series", "the voiceless aspirated series", "the voiced series")


def inventory(first, where):
    """Table 1's three rows of stops and affricates, a cited form each, then its caption."""
    for index, name in enumerate(SERIES):
        paper.add("Table 1", L, "cited form", paper.text(first + index),
                  "page %d, %s of stops and affricates" % (paper.page(first), name))
    paper.add("Table 1", A, "note", paper.text(first + 3), "page %d, the table's caption" % paper.page(first))
    return first + 4


def captions(first, where):
    """The captions of Tables 2 and 3, set side by side over the tables, which are images."""
    for name, lines in (("Table 2", [first, first + 1, first + 2]), ("Table 3", [first + 3, first + 4, first + 5])):
        paper.add(name, A, "note", paper.joined(lines),
                  "page %d, the table's caption; the table is an image" % paper.page(first))
    return first + 6


def label(first, where):
    text = paper.text(first)
    paper.add(text.rstrip("."), A, "note", text,
              "page %d, the figure's label; the figure is an image" % paper.page(first))
    return first + 1


BLOCKS = {paper.find(r"^p' t' ƛ'"): inventory, paper.find(r"^Table 2: Mean VOT"): captions}
for number in range(1, paper.last + 1):
    if paper.text(number) in ("Fig. %d" % one for one in range(1, 9)):
        BLOCKS[number] = label
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS)
# The abstract, set with no heading under the university, is one note.
lines = [index for index, row in enumerate(paper.rows) if row[0] == "front" and row[2] == "note"
         and row[4] == "page 1, under Connor Mayer"][1:]
if lines:
    paper.rows[lines[0]][3] = " ".join(paper.rows[index][3] for index in lines)
    paper.rows[lines[0]][4] = "page 1, the abstract"
    paper.rows = [row for index, row in enumerate(paper.rows) if index not in lines[1:]]
# §4.2's heading wraps onto the line of the label Fig. 3: the heading takes the words, the label is a note.
at = next(index for index, row in enumerate(paper.rows) if row[3] == "4.2 Word-initial vs")
rest, figure = paper.rows[at + 1][3].rsplit(" Fig.", 1)
paper.rows[at][3] += " " + rest
paper.rows[at + 1] = ["Fig. 3", A, "note", "Fig." + figure,
                      paper.rows[at][4] + ", the figure's label; the figure is an image"]
# Three paragraphs run on past the tables and figures set inside them; each continuation joins its start.
for start in ("voiced affricates (p < 0.001).", "well established in the literature,", "highest prosodic boundary"):
    at = next(index for index, row in enumerate(paper.rows) if row[3].startswith(start))
    before = max(index for index in range(at) if paper.rows[index][0] == paper.rows[at][0]
                 and paper.rows[index][2] == "note")
    paper.rows[before][3] += " " + paper.rows[at][3]
    del paper.rows[at]
# The reference to Babel runs on into the one to Cho and Ladefoged.
at = next(index for index, row in enumerate(paper.rows) if row[3].startswith("Babel, M. (2008)"))
babel, cho = paper.rows[at][3].split(" Cho T., &", 1)
paper.rows[at][3] = babel
paper.rows.insert(at + 1, paper.rows[at][:3] + ["Cho T., &" + cho, paper.rows[at][4]])
paper.write()
