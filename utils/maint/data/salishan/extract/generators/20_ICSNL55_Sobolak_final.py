"""The ops of 20_ICSNL55_Sobolak_final: Frances Sobolak on the passive suffixes -em/-t of Montana
Salish, whose distribution parallels the objects with an overt suffix on the verb, 1PL, 2SG and 2PL;
syntactic Agree with a participant probe and post-syntactic Impoverishment of 1SG capture the
grouping.

The examples set a transcription over its segmentation, a gloss and a translation, from
S. Thomason's field notes; (1) is Halkomelem, from Wiltschko 2001. Tables 1, 2 and 4 cross the
subject with the object, each cell a row given its subject and object; Table 3 sets the object
markers in the columns of the verb's template. The list (16), the feature classes (17) to (19),
the rule (21), the vocabulary items (22) and the trees (20), (24) and (26) are notes.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Montana Salish"
AUTHORS = ["Frances Sobolak"]
paper = gen.Paper("20_ICSNL55_Sobolak_final", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Sarah Thomason", "field notes and texts of Montana Salish, thanked; Thomason (1992, 2014), "
                            "Everett and Thomason (1993)"),
         ("Wiltschko", "Martina Wiltschko, the passive in Halkomelem and Squamish (2001)"),
         ("Kroeber", "Paul Kroeber, reconstructing Salish syntax (1999)"),
         ("Everett", "Daniel Everett, transitivity in Flathead, with Thomason (1993)"),
         ("Gerdts", "Donna H. Gerdts, object and absolutive in Halkomelem (1988)"),
         ("H. Davis", "Henry Davis, passive resistance in Salish (2018), p.c."),
         ("J. Lyon", "John Lyon, p.c. on Okanagan"),
         ("Brown", "Jason Brown, gaps in transitive paradigms, with Koch and Wiltschko (2005)"),
         ("Koch", "Karsten Koch, with Brown and Wiltschko (2005)"),
         ("Embick", "David Embick, Distributed Morphology, with Noyer (2007)"),
         ("Noyer", "Rolf Noyer, with Embick (2007)"),
         ("Harley", "Heidi Harley, feature geometry of pronouns, with Ritter (2002)"),
         ("Ritter", "Elizabeth Ritter, with Harley (2002)"),
         ("Coon", "Jessica Coon, person and number in Migmaq, with Bale (2014)"),
         ("Bale", "Alan Bale, with Coon (2014)"),
         ("Despić", "Miloje Despić, person and number in Cheyenne, with Hamilton and Murray (2019)"),
         ("Carlson", "Barry F. Carlson, a grammar of Spokane (1972)"),
         ("Mattina", "Anthony Mattina, the Colville-Okanagan transitive system (1982)")]
LANGUAGES = [(LANGUAGE, "Southern Interior Salish; Montana, called Flathead"),
             ("Halkomelem", "Central Salish; (1), from Wiltschko 2001"),
             ("Okanagan", "Southern Interior Salish"),
             ("Lillooet", "Northern Interior Salish"),
             ("Shuswap", "Northern Interior Salish"),
             ("Thompson", "Northern Interior Salish"),
             ("Spokane", "Southern Interior Salish"),
             ("Squamish", "Central Salish")]
# The list, the features, the rules and the trees set as examples: their printed lines, and whether
# each line is a row of its own.
DISPLAYS = {"16": (4, True), "17": 1, "18": 1, "19": 1, "20": (7, True), "21": 1, "22": (3, True),
            "24": (7, True), "26": (7, True)}
OBJECTS = ("1SG", "2SG", "3SG", "1PL", "2PL", "3PL")
# The 1SG row of Tables 1, 2 and 4 sets ∅ in four cells, and the text layer gives the four one
# place: read off the render of page 4, the 1SG on 1PL cell is empty.
FIRST_ROW = ("1SG", "2SG", "3SG", "2PL", "3PL")
RUNNING = paper.running_numbers_set()


def after(number):
    number += 1
    while number <= paper.last and (not paper.text(number).strip() or paper.lines[number][2]
                                    or number in RUNNING):
        number += 1
    return number


def crossed(first, where):
    """Table 1, 2 or 4: the caption, Table 4's line of object suffixes, the line heading the columns,
    the object, then each subject's row, each cell a row given the column it stands under."""
    page = paper.page(first)
    name = paper.text(first).split(":")[0]
    paper.add(name + " line 1", A, "note", paper.text(first), "page %d, the caption" % page)
    line = after(first)
    if paper.text(line).startswith("OBJ SUFF:"):
        paper.add(name + ", the object suffixes", A, "note", "OBJ SUFF:", "page %d, the line over the objects" % page)
        heads = paper.word_positions(after(line))[2:]
        for left, word in paper.word_positions(line)[2:]:
            column = min(heads, key=lambda head: abs(head[0] - left))[1]
            paper.add("%s, the object suffix of %s" % (name, column), L, "cited affix", word,
                      "page %d, the overt object suffix of %s" % (page, column))
        line = after(line)
    heads = paper.word_positions(line)[2:]
    paper.add(name + " line 2", A, "note", paper.text(line), "page %d, the line heading the columns, the object" % page)
    line = after(line)
    paper.add(name + " line 3", A, "note", paper.text(line), "page %d, the line heading the rows, the subject" % page)
    line = after(line)
    for _ in range(len(OBJECTS)):
        found = paper.word_positions(line)
        subject = found[0][1]
        paper.add("%s, %s" % (name, subject), A, "note", subject, "page %d, the subject heading the row" % page)
        if subject == "1SG":
            cells = list(zip(FIRST_ROW, (word for _, word in found[1:])))
            # The four ∅ of the row, a character each, are one row: 1SG is never demoted.
            empty = [column for column, word in cells if word == "∅"]
            paper.add("%s, 1SG on %s" % (name, ", ".join(empty)), A, "note", " ".join("∅" for _ in empty),
                      "page %d, 1SG on %s, no passive: 1SG is not demoted" % (page, " and ".join(empty)))
            cells = [(column, word) for column, word in cells if word != "∅"]
        else:
            cells = [(min(heads, key=lambda head: abs(head[0] - left))[1], word) for left, word in found[1:]]
        for column, word in cells:
            on = "%s on %s" % (subject, column)
            if word == "(RFLX)":
                paper.add("%s, %s" % (name, on), A, "note", word, "page %d, %s, the reflexive" % (page, on))
            elif word == "∅":
                paper.add("%s, %s" % (name, on), A, "note", word, "page %d, %s, no passive: 1SG is not demoted" % (page, on))
            else:
                paper.add("%s, %s" % (name, on), L, "cited affix", word, "page %d, %s, the passive suffix" % (page, on))
        line = after(line)
    return line


def template(first, where):
    """Table 3: the caption, the columns of the verb's template, and the object markers under the
    clitic, the reduplication and the object suffix, each with the object it marks."""
    page = paper.page(first)
    paper.add("Table 3 line 1", A, "note", paper.text(first), "page %d, the caption" % page)
    line = after(first)
    paper.add("Table 3 line 2", A, "note", paper.text(line), "page %d, the columns of the verb's template" % page)
    line = after(line)
    for column, person, form in (("CLITIC", "1SG.OBJ", "kʷu"), ("CLITIC", "1PL", "qe"), ("-OBJ", "1PL", "-l"),
                                 ("-OBJ", "2SG", "-sí"), ("-OBJ", "2PL", "-m")):
        paper.add("Table 3, %s %s" % (column, person), A, "note", person, "page %d, the object, in the %s column" % (page, column))
        paper.add("Table 3, %s %s" % (column, person), L, "cited affix" if form.startswith("-") else "cited form",
                  form, "page %d, %s, its marker in the %s column" % (page, person, column))
    paper.add("Table 3, (REDUP-)", A, "note", "(3PL.OBJ CVC-)",
              "page %d, the (REDUP-) column: a plural third person object, reduplicated optionally" % page)
    # The five lines of markers, to the prose after the table.
    for _ in range(5):
        line = after(line)
    return line


paper.opening = "auto"
blocks = {}
for number in range(1, paper.last + 1):
    text = paper.text(number)
    if text.startswith("Table 3:"):
        blocks[number] = template
    elif text.startswith(("Table 1:", "Table 2:", "Table 4:")):
        blocks[number] = crossed
paper.standard(AUTHORS, NAMES, LANGUAGES, displays=DISPLAYS, blocks=blocks)
for row in paper.rows:
    # (1) is Halkomelem, cited from Wiltschko 2001.
    if row[0].startswith("(1) line") and row[1] == LANGUAGE:
        row[1] = "Halkomelem"
    # The translation of (7) opens on a closing quote, U+2019(S)he/they/we hunted you (SG).’
    if row[0] == "(7) line 4":
        row[1], row[2] = A, "translation"
paper.write()
