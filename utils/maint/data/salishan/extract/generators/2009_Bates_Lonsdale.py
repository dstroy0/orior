"""The ops of 2009_Bates_Lonsdale: Dawn Bates and Deryle Lonsdale's Recovering and updating legacy
dictionary data, the Lushootseed Dictionary carried from its LEXWARE files to HTML, TEI P4 and P5 XML,
and the Kirrkirr dictionary browser.

The paper is prose. Its Lushootseed words are the language's own name, dxʷləšucid, Violet Hilbert's
name taqʷšəblu, and the headword cícuʔ of the TEI snippet and its Kirrkirr form on page 6, each a
cited form. The three figures are screen images, and each caption is a note of its own.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
AUTHORS = ["Dawn Bates", "Deryle Lonsdale"]
paper = gen.Paper("2009_Bates_Lonsdale", authors=" and ".join(AUTHORS), language="Lushootseed")
NAMES = [("Violet taqʷšəblu Hilbert", "an Upper Skagit elder, first-language speaker, teacher and researcher"),
         ("Thom Hess", "author of the Dictionary of Puget Salish (1976)"),
         ("Louise George", "a speaker Thom Hess worked with"),
         ("Mr. and Mrs. Lamont", "speakers Thom Hess worked with")]
LANGUAGES = [("Lushootseed", "Central Salish, Puget Salish"), ("Puget Salish", "Lushootseed"),
             ("dxʷləšucid", "Lushootseed, in its own name"),
             ("Northern Lushootseed", "Lushootseed, its northern division"),
             ("Southern Lushootseed", "Lushootseed, its southern division"),
             ("Warlpiri", "an Australian language, Pama-Nyungan"),
             ("Slovene", "the Slovene dialect of Resia")]


def caption(start, where):
    """A figure's caption, set under its screen image, a note."""
    paper.add(where, A, "note", paper.text(start), "page %d, the caption of %s" % (
        paper.page(start), paper.text(start).split(":")[0]))
    return start + 1


CAPTIONS = {paper.find(r"^Figure %d: " % number): caption for number in (1, 2, 3)}
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=CAPTIONS)
rows = paper.rows
# The universities and the abstract under them are one note from front(); the abstract opens on
# Ongoing, the line after the universities.
at = next(index for index, row in enumerate(rows) if row[3].startswith("Arizona State University"))
universities = paper.text(paper.find(r"^Arizona State University"))
abstract = rows[at][3][len(universities):].strip()
rows[at][3], rows[at][4] = universities, "page 1, the authors' universities"
rows[at + 1:at + 1] = [rows[at][:3] + [abstract, "page 1, the abstract"]]
# The TEI snippet and the Kirrkirr item it became each hold the headword cícuʔ, set in the roman of
# the running text.
at = next(index for index, row in enumerate(rows) if row[2] == "note" and "cícuʔ" in row[3])
rows[at + 1:at + 1] = [[rows[at][0], A, "cited form", "cícuʔ", "page 6, the headword of the TEI snippet "
                        "<orth>cícuʔ </orth>, a space before its closing tag as printed"],
                       [rows[at][0], A, "cited form", "cícuʔ", "page 6, the headword of the Kirrkirr item "
                        "<HW>cícuʔ</HW>"]]
# The authors' names and addresses close the paper under Suttles's entry, a note.
closing = paper.joined([paper.find(r"^Deryle Lonsdale / Dawn Bates$"), paper.find(r"^lonz@byu\.edu")])
last = next(row for row in rows if row[2] == "reference" and row[3].startswith("Suttles, Wayne"))
last[3] = last[3][:-len(closing)].strip()
rows.append(["references", A, "note", closing, "page 12, the authors' names and addresses"])
paper.rows = rows
paper.write()
