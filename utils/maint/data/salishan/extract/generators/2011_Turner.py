"""The ops of 2011_Turner: Claire K. Turner's A SENĆOŦEN web database for linguists and community,
a Filemaker database of the sentences, verbs and roots of her fieldwork with two Saanich elders,
linked so that a verb leads to the sentences it is used in and to its root, to be read on the web.

The fields of the 'Sentence' and 'Verb' tables are set as a list, a label and its colon opening each
entry, and each entry is a note. The six figures are screen views, images the layer does not carry;
their captions are notes, and Figure 4's names the verb it shows, a cited form. The glossing
abbreviations of the appendix are a note each.
"""
import re
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "SENĆOŦEN"
AUTHORS = ["Claire K. Turner"]
paper = gen.Paper("2011_Turner", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Montler", "Timothy Montler, the morphology and phonology of Saanich (1986) and its word list (1991)"),
         ("Kiyota", "Masaru Kiyota, situation aspect and viewpoint aspect (2008)"),
         ("Davis", "John Davis, pronominal paradigms in Sliammon (1978)"),
         ("Watanabe", "Honoré Watanabe, a morphological description of Sliammon (2003)"),
         ("Thompson", "Laurence C. Thompson, control in Salish grammar (1985)"),
         ("Czaykowska-Higgins & Kinkade", "Ewa Czaykowska-Higgins and M. Dale Kinkade, Salish languages "
          "and linguistics (1998)"),
         ("Vendler", "Zeno Vendler, verbs and times (1957)"),
         ("Janet Leonard", "who worked with one of the elders and gave some of the examples"),
         ("Dunstan Brown", "who planned the database's structure with the author"),
         ("Greville Corbett", "who advised on the database's morphological analysis and glossing"),
         ("Nick Jenkins", "of the University of Surrey, who set up the web implementation"),
         ("Dave Elliott", "whose SENĆOŦEN alphabet many learners in the Saanich community use")]
LANGUAGES = [(LANGUAGE, "a dialect of Northern Straits Salish spoken on the Saanich Peninsula, "
                        "Vancouver Island, the language of the database"),
             ("Saanich", "the English name of the community and its language"),
             ("Northern Straits Salish", "the language SENĆOŦEN is a dialect of"),
             ("English", "its progressive, which shares a property with the imperfective"),
             ("Archi", "a Nakh-Daghestanian language, with an online dictionary at Surrey")]


def fields(first, where):
    """A list of the fields of a table, a label and its colon opening each entry."""
    number, entry, at = first, [], first
    while paper.text(number).strip():
        if re.match(r"^[A-Z][\w ()]+: ", paper.text(number).strip()) and entry:
            paper.add(where, A, "note", " ".join(entry), "page %d, a field of the table" % paper.page(at))
            entry, at = [], number
        entry.append(paper.text(number).strip())
        number += 1
    paper.add(where, A, "note", " ".join(entry), "page %d, a field of the table" % paper.page(at))
    # The labels of the fields are set in italics as well, and are no forms.
    held = len(paper.rows)
    paper.cited(where, paper.joined(range(first, number)), sorted({paper.page(first), paper.page(number - 1)}))
    paper.rows[held:] = [row for row in paper.rows[held:] if not row[3].endswith("alphabet)")]
    # The persistent suffix -i sets its hyphen upright and bold, and the italic run is the i alone.
    if paper.find(r"persistent -i,", first, number):
        paper.add(where, L, "cited form", "-i", "page %d, in italics" % paper.page(number - 1))
    return number


def caption(first, where):
    text = paper.text(first).strip()
    label = re.match(r"^Figure \d+", text).group(0)
    paper.add(label, A, "note", text, "page %d, the figure's caption; the figure is an image" % paper.page(first))
    # Figure 4's caption is set in italics whole; the verb it names is a cited form with its gloss.
    verb = re.search(r"containing (\S+ \S+) (‘[^’]+’)$", text)
    if verb:
        paper.add(label, L, "cited form", verb.group(1),
                  "page %d, in the figure's caption, %s" % (paper.page(first), verb.group(2)))
    return first + 1


def abbreviations(first, where):
    """The glossing abbreviations of the appendix, run on over three lines, a note each."""
    text = paper.joined([first, first + 1, first + 2])
    for one in text.rstrip(".").split("; "):
        paper.add("appendix", A, "note", one.strip(), "page %d, a glossing abbreviation" % paper.page(first))
    return first + 3


BLOCKS = {paper.find(r"^Sentence \(SENĆOŦEN alphabet\):"): fields,
          paper.find(r"^Verb \(SENĆOŦEN alphabet\):"): fields,
          paper.find(r"^1=1st person;"): abbreviations}
for number in range(1, paper.last + 1):
    if re.match(r"^Figure \d+ ", paper.text(number).strip()):
        BLOCKS[number] = caption
# The footnote marks open the next line, 2 In addition, and the heading finder takes them for
# section numbers; the headings are named here.
HEADINGS = {paper.find(r"^%s$" % re.escape(title)): label for label, title in (
    ("1", "1 Introduction"), ("2", "2 Purpose and benefits"), ("3", "3 Structure and contents"),
    ("4", "4 Application"), ("5", "5 Access"), ("6", "6 Relationship to other web materials"),
    ("7", "7 Possible expansions of the database"), ("8", "8 Conclusion"),
    ("appendix", "Appendix: Glossing abbreviations"))}
# Footnote 4's mark stands alone on the line after ‘Root’., where it reads as a page number.
paper._running = paper.running_numbers() - {paper.find(r"^4$", paper.find(r"consists of three tables"))}
# Footnote 8's mark stands a space after its word, raised off the line.
found = paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS, headings=HEADINGS, spaced={"8": "event"},
                       appendix=r"^Claire K\. Turner")
for row in paper.rows:
    if row[0] == "§appendix":
        row[0] = "appendix"
# The abstract, set with no heading under the university, is one note.
lines = [index for index, row in enumerate(paper.rows) if row[0] == "front" and row[2] == "note"
         and row[4] == "page 1, under Claire K. Turner"][1:]
if lines:
    paper.rows[lines[0]][3] = " ".join(paper.rows[index][3] for index in lines)
    paper.rows[lines[0]][4] = "page 1, the abstract"
    paper.rows = [row for index, row in enumerate(paper.rows) if index not in lines[1:]]
# The references open on the author's name or, for the next work of the same author, on U+2014.
paper.rows = [row for row in paper.rows if row[0] != "references"]
end, tail = paper.find(r"^References$"), paper.find(r"^Claire K\. Turner$", paper.find(r"^References$"))
paper.add("references", A, "heading", paper.text(end), "page %d" % paper.page(end))
for text, at in paper.references(end + 1, tail - 1, opens=r"^[A-Z][\w-]+, [A-Z]|^—\."):
    paper.add("references", A, "reference", text, "page %d" % at)
paper.add("end", A, "note", paper.joined([tail, tail + 1, tail + 2]),
          "page %d, the author's name and e-mail addresses" % paper.page(tail))
paper.write()
