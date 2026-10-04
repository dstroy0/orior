"""The ops of 2012_Lee: JeongEun Lee's Body part noun incorporation in Blackfoot: allomorphy analysis,
that a body part medial is an allomorph of its independent noun, taken by an intransitive verb stem
with a deriving intransitive final while a transitive stem takes the noun with an agreeing final.

The examples set the Blackfoot line over its segmentation and gloss, the translation in curly
quotes with its source after it, the consultant's session code in brackets or a citation in
parentheses. Beside Blackfoot the paper cites Mohawk, Halkomelem Salish, Kusaiean and Mapudungun
from the accounts it compares.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Blackfoot"
AUTHORS = ["JeongEun Lee"]
paper = gen.Paper("2012_Lee", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Sandra Crazy Bull", "Chief Sandra Crazy Bull of the Kainaa (Blood) Tribe, the language consultant"),
         ("Armoskaite", "Solveiga Armoskaite, the destiny of roots in Blackfoot and Lithuanian (2011)"),
         ("Frantz", "Donald G. Frantz, a generative grammar of Blackfoot (1971) and Blackfoot grammar (2009)"),
         ("Russell", "Norma Jean Russell, with Frantz the Blackfoot dictionary (1995)"),
         ("Dunham", "Joel Dunham, noun incorporation in Blackfoot (2009)"),
         ("Baker", "Mark Baker, incorporation (1988), the polysynthesis parameter (1996) and head movement (2009)"),
         ("Wiltschko", "Martina Wiltschko, root incorporation in Halkomelem Salish (2009)"),
         ("Rosen", "Sara Thomas Rosen, two types of noun incorporation (1989)"),
         ("Mithun", "Marianne Mithun, the evolution of noun incorporation (1984)"),
         ("Sapir", "Edward Sapir, the problem of noun incorporation (1911)"),
         ("Mühlbauer", "Jeff Mühlbauer, Blackfoot morphology key (2005) and intentionality in Plains Cree (2008)"),
         ("Katamba", "Francis Katamba, English words (2005)"),
         ("Chomsky", "Noam Chomsky, the Minimalist Program (1995)"),
         ("Ross", "John Robert Ross, Infinite Syntax (1986)"),
         ("Hornstein", "Norbert Hornstein, Jairo Nunes and Kleanthes K. Grohmann, Understanding Minimalism (2005)"),
         ("Ritter", "Elizabeth Ritter, with Rosen possessors as external arguments (2010)"),
         ("Salas", "Adalberto Salas, El mapuche o araucano (1992)"),
         ("Lee", "Kee-Dong Lee, Kusaiean reference grammar (1975)")]
LANGUAGES = [(LANGUAGE, "Algonquian, spoken in Alberta and Montana, the language of the paper"),
             ("Mohawk", "Iroquoian, a Classifier NI language in Baker's and Rosen's accounts"),
             ("Halkomelem Salish", "Central Salish, whose lexical suffixes Wiltschko analyzes as roots"),
             ("Kusaiean", "Micronesian, a Compound NI language in Rosen's account"),
             ("Mapudungun", "Araucanian, with Mohawk in Baker's account"),
             ("Niuean", "Polynesian, whose incorporated nouns take modifiers in Baker's account"),
             ("Latin", "which allows Left Branch Condition violations"),
             ("Russian", "which allows Left Branch Condition violations"),
             ("Plains Cree", "Algonquian, the subject of Mühlbauer (2008)"),
             ("Lithuanian", "compared with Blackfoot in Armoskaite (2011)"),
             ("English", "whose sentences the translation tasks use")]

SKIP = {one for parts, _ in paper.page_footnotes().values() for one in parts} | paper.running_numbers_set()


def columns(number):
    """The columns of a line the page sets side by side, a. and b. taken off."""
    found = [re.sub(r"^[ab]\.\s*", "", re.sub(r"^\(\d+\)\s*", "", one.strip()))
             for one in re.split(r"\s{3,}", paper.spaced[number])]
    return [one for one in found if one]


def title(label, number):
    text = re.sub(r"^\(\d+\)\s+", "", paper.text(number)).strip()
    paper.add(label, A, "note", text, "page %d, the example's heading" % paper.page(number))


def lines(label, first, count, gloss):
    """Each of count lines after the example's heading a note: a template, a tree's row."""
    for number in range(first, first + count):
        paper.add(label, A, "note", re.sub(r"^\(\d+\)\s+", "", paper.text(number)).strip(),
                  "page %d, %s" % (paper.page(number), gloss))
    return first + count


def items(label, first, end):
    """A bulleted list: each item from its bullet to the next, over its wrapped lines, a note."""
    found = []
    for number in range(first, end):
        if number in SKIP or not paper.text(number).strip():
            continue
        if paper.text(number).startswith("●") or not found:
            found.append([number])
        else:
            found[-1].append(number)
    for one in found:
        paper.add(label, A, "note", paper.joined(one), "page %d, an item of the list" % paper.page(one[0]))
    return end


def template(start, where):
    """(5), the order of a verb's morphemes over two lines, one note."""
    paper.add("(5)", A, "note", re.sub(r"^\(5\)\s+", "", paper.joined([start, start + 1])),
              "page %d, set as an example" % paper.page(start))
    return start + 2


def six(start, where):
    """(6), two examples side by side: a. over four lines, b. with its gloss and translation."""
    left, right = columns(start), columns(start + 1)
    third, fourth = columns(start + 2), columns(start + 3)
    page = "page %d" % paper.page(start)
    for kind, text in (("transcription", left[0]), ("segmentation", right[0]), ("gloss", third[0]),
                       ("translation", fourth[0])):
        paper.add("(6a)", L if kind != "translation" else A, kind, text, page)
    for kind, text in (("transcription", left[1]), ("gloss", right[1]), ("translation", third[1])):
        paper.add("(6b)", L if kind != "translation" else A, kind, text, page + ", in the right column")
    paper.add("(6b)", A, "citation", fourth[1], page + ", the source at the right of (6a)'s translation")
    return start + 4


def eight(start, where):
    """(8), a heading over two examples side by side with their translations under them."""
    title("(8)", start)
    forms, said = columns(start + 1), columns(start + 2)
    page = "page %d" % paper.page(start)
    for letter, form, translation in zip("ab", forms, said):
        paper.add("(8%s)" % letter, L, "transcription", form, page)
        paper.add("(8%s)" % letter, A, "translation", translation, page)
    paper.add("(8b)", A, "citation", paper.text(start + 3).strip(), page + ", under both translations")
    return start + 4


def eleven(start, where):
    """(11), a heading over one form's three tiers, its translation and source on one line."""
    title("(11)", start)
    page = "page %d" % paper.page(start)
    for kind, number in (("transcription", start + 1), ("segmentation", start + 2), ("gloss", start + 3)):
        paper.add("(11)", L, kind, paper.text(number).strip(), page)
    said, source = re.match(r"(‘.*?’)\s*(.*)", paper.text(start + 4).strip()).groups()
    paper.add("(11)", A, "translation", said, page)
    paper.add("(11)", A, "citation", source, page + ", at the right of the translation")
    return start + 5


def twelve(start, where):
    """(12), a heading over two forms side by side, one translation and source under both."""
    title("(12)", start)
    page = "page %d" % paper.page(start)
    for kind, number in (("transcription", start + 1), ("segmentation", start + 2), ("gloss", start + 3)):
        for letter, text in zip("ab", columns(number)):
            paper.add("(12%s)" % letter, L, kind, text, page)
    said, source = re.match(r"(‘.*?’)\s*(.*)", paper.text(start + 4).strip()).groups()
    paper.add("(12)", A, "translation", said, page + ", under both forms, the translation of each")
    paper.add("(12)", A, "citation", source, page + ", at the right of the translation")
    return start + 5


def table(start, where):
    """(13), the table of body part medials and their nouns: its heads and each row a note, and the
    medial and the noun of each row cited forms."""
    title("(13)", start)
    page = paper.page(start)
    paper.add("(13)", A, "note", paper.text(start + 1).strip(), "page %d, the column heads" % page)
    number = start + 2
    while len(columns(number)) == 4:
        part, animacy, medial, noun = columns(number)
        paper.add("(13)", A, "note", paper.text(number).strip(), "page %d, a row of the table" % page)
        english = re.sub(r"\d+$", "", part).lower()
        if medial != "N/A":
            paper.add("(13)", L, "cited form", medial, "page %d, the medial, ‘%s’, %s" % (page, english, animacy.lower()))
        paper.add("(13)", L, "cited form", noun, "page %d, the independent noun, ‘%s’, %s" % (page, english,
                                                                                               animacy.lower()))
        number += 1
    return number


def listed(end):
    """A heading over a bulleted list that runs to the line end matches."""
    def block(start, where):
        label = re.match(r"\(\d+\)", paper.text(start)).group(0)
        title(label, start)
        return items(label, start + 1, paper.find(end, start))
    return block


def tree(count):
    def block(start, where):
        label = re.match(r"\(\d+\)", paper.text(start)).group(0)
        title(label, start)
        return lines(label, start + 1, count, "a line of the tree as the text layer reads it")
    return block


def classification(start, where):
    title("(10)", start)
    return lines("(10)", start + 1, 3, "set as an example")


def complex_verbs(start, where):
    """(33), the lexicalized AI verbs, each item a note and its verb a cited form."""
    end = items("(33)", start + 1, start + 5)
    title_row = len(paper.rows) - 4
    paper.rows.insert(title_row, ["(33)", A, "note", re.sub(r"^\(33\)\s+", "", paper.text(start)).strip(),
                                  "page %d, the example's heading" % paper.page(start)])
    for number in range(start + 1, start + 5):
        meaning, verb = re.match(r"●\s*(.*?):\s*(\S+)$", paper.text(number).strip()).groups()
        paper.add("(33)", L, "cited form", verb, "page %d, ‘%s’" % (paper.page(number), meaning.lower()))
    return end


def formulas(start, where):
    """(49) and (50), the steps of a compound's formation, each over two lines a note."""
    number = re.match(r"\((\d+)\)", paper.text(start)).group(1)
    if number == "49":
        for letter, first in (("a", start), ("b", start + 2)):
            text = re.sub(r"^\(49\)\s+|^[ab]\.\s*", "", paper.joined([first, first + 1]))
            text = re.sub(r"^[ab]\.\s*", "", text)
            paper.add("(49%s)" % letter, A, "note", text, "page %d, set as an example" % paper.page(first))
        return start + 4
    paper.add("(50)", A, "note", re.sub(r"^\(50\)\s+", "", paper.joined([start, start + 1])),
              "page %d, set as an example" % paper.page(start))
    return start + 2


BLOCKS = {paper.find(r"^\(5\) "): template, paper.find(r"^\(6\) a\."): six, paper.find(r"^\(8\) "): eight,
          paper.find(r"^\(9\) "): listed(r"^However, I adopt"), paper.find(r"^\(10\) "): classification,
          paper.find(r"^\(11\) "): eleven, paper.find(r"^\(12\) "): twelve, paper.find(r"^\(13\) "): table,
          paper.find(r"^\(17\) "): tree(4), paper.find(r"^\(20\) "): tree(6), paper.find(r"^\(21\) "): tree(4),
          paper.find(r"^\(33\) "): complex_verbs, paper.find(r"^\(44\) "): listed(r"^If we posit"),
          paper.find(r"^\(49\) "): formulas, paper.find(r"^\(50\) "): formulas}

# Footnote 6's (i) sets four forms with their glosses on two lines, and its source.
reader = paper.example


def example(start, last=None, skip=(), resume=None, prefix=""):
    if not prefix.startswith("footnote 6"):
        return reader(start, last, skip=skip, resume=resume, prefix=prefix)
    text = paper.joined([start, start + 1])
    page = "page %d" % paper.page(start)
    for letter, form, said in re.findall(r"([a-d])\. (\S+) (‘[^’]*’)", text):
        paper.add("%s(i%s)" % (prefix, letter), L, "transcription", form, page)
        paper.add("%s(i%s)" % (prefix, letter), A, "translation", said, page)
    paper.add("%s(id)" % prefix, A, "citation", re.search(r"\([^)]*\)$", text).group(0),
              page + ", at the right of the translation")
    return start + 2


paper.example = example
# Footnote 4's mark stands a space after (2b); footnote 8's closes the parenthesis of (7a)'s
# optional object, (sináákia'tsis)8.
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS, spaced={"4": "(2b)"}, glued={"8": "(sináákia’tsis)"})
# The examples the reader takes whole open on a heading, set as their first line; a line set in
# another language's words, Mohawk's and Kusaiean's, is read by the tiers it holds.
HEADED = {"(7)", "(14)", "(15)", "(16)", "(18)", "(19)", "(28)", "(29)", "(30)", "(40)"}
KINDS = {"(14) line 2": "transcription", "(16b) line 2": "gloss", "(18) line 2": "transcription",
         "(18) line 4": "transcription", "(29) line 2": "transcription", "(29) line 3": "segmentation",
         "(29) line 4": "gloss", "(30) line 2": "transcription", "(30) line 3": "segmentation",
         "(30) line 4": "gloss", "(32) line 2": "segmentation"}
for row in paper.rows:
    label = row[0].split(" line ")[0]
    if label in HEADED and row[0].endswith(" line 1") and row[2] == "transcription":
        row[1], row[2], row[4] = A, "note", row[4] + ", the example's heading"
    row[2] = KINDS.get(row[0], row[2])
# (14) to (19) set the other languages' incorporation beside Blackfoot's, each named in its heading.
OTHER = {"14": "Mohawk", "15": "Halkomelem Salish", "16": "Kusaiean", "18": "Mohawk", "19": "Halkomelem Salish"}
# The words the reader opens or accents where the page sets them closed up or bare, as residue.py
# reads them from the renders.
READ = (("nit s’ áapino’tok", "nit’sáapino’tok"), ("wı ´l", "wı´l"), ("so.it.ýear", "so.it.year"),
        ("rabahb́́ót", "rabahbót"))
for row in paper.rows:
    number = re.match(r"\((\d+)", row[0])
    if number and row[1] == L and number.group(1) in OTHER:
        row[1] = OTHER[number.group(1)]
    for before, after in READ:
        if before in row[3]:
            row[3] = row[3].replace(before, after)
            if after == "rabahbót":
                row[4] += ", a stray stroke of accents over its b rising into the gloss line above"
paper.write()
