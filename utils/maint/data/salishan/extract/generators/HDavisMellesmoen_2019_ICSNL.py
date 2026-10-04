"""The ops of HDavisMellesmoen_2019_ICSNL: Henry Davis and Gloria Mellesmoen on degree constructions
in St'át'imcets and ʔayʔaǰuθəm. Against Lo and Reisinger (2018), both languages test positively for
the three degree parameters of Beck et al. (2009), the Degree Semantics Parameter, the Degree
Abstraction Parameter and the Degree Phrase Parameter; a sketch of their degree semantics follows,
and one speaker's conjoined comparatives in ʔayʔaǰuθəm are shown to test as [+DSP].

Page text read by glyph rows, the Symbol font's λ and ¬ of the formulas mapped by page_text. The
examples are segmented on their first line (opening = "segmentation"), glossed under it and
translated. The parameters (5) to (7), the tables (8) and (74), the formulas (76) to (78), (82) and
(85), the trees (83) and (86) and the English readings under (79) and (80) are read by blocks.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
LANGUAGE = "ʔayʔaǰuθəm"
L = gen.L
STATIMCETS = "St’át’imcets"
AUTHORS = ["Henry Davis", "Gloria Mellesmoen"]
paper = gen.Paper("HDavisMellesmoen_2019_ICSNL", authors=" and ".join(AUTHORS), language=LANGUAGE)
paper.opening = "segmentation"
NAMES = [("Carl Alexander", "a St’át’imcets speaker the authors thank"),
         ("Laura Thevarge", "a St’át’imcets speaker"),
         ("Joanne Francis", "a ʔayʔaǰuθəm speaker"), ("Karen Galligos", "a ʔayʔaǰuθəm speaker"),
         ("Marion Harry", "a ʔayʔaǰuθəm speaker"), ("Freddie Louie", "a ʔayʔaǰuθəm speaker"),
         ("Elsie Paul", "a ʔayʔaǰuθəm speaker"), ("Randy Timothy", "a ʔayʔaǰuθəm speaker"),
         ("Margaret Vivier", "a ʔayʔaǰuθəm speaker"), ("Betty Wilson", "a ʔayʔaǰuθəm speaker"),
         ("Lisa Travis", "thanked"), ("Vera Hohaus", "thanked; Hohaus and Deal (2019), Hohaus (2018)"),
         ("Lisa Matthewson", "thanked; Matthewson (1998, 2008)"), ("Marcin Morzycki", "thanked"),
         ("Daniel Reisinger", "D. K. E. Reisinger; Lo and Reisinger (2018), L&R"),
         ("Beck", "Sigrid Beck et al., comparison constructions (2009) and phrasal comparatives (2012)"),
         ("Heim", "Irene Heim, comparatives (1985) and degree operators (2000)"),
         ("Kennedy", "Christopher Kennedy (1997, 2007); Alrenga and Kennedy (2014)"),
         ("Bhatt", "Rajesh Bhatt and Shoichi Takahashi, phrasal comparatives (2011)"),
         ("Stassen", "Leon Stassen, comparison (1985, 2013)"),
         ("Beaumont", "Ronald Beaumont, Sechelt dictionary (2011)"),
         ("Bochnak", "M. Ryan Bochnak (2013, 2015), Washo"),
         ("JF", "a ʔayʔaǰuθəm consultant, the speaker of the conjoined comparatives")]
LANGUAGES = [(LANGUAGE, "Central Salish, also known as Comox-Sliammon, ISO 639-3 coo"),
             (STATIMCETS, "Northern Interior Salish, also known as Lillooet, ISO 639-3 lil"),
             ("Lillooet", "St’át’imcets"), ("Comox-Sliammon", "ʔayʔaǰuθəm"),
             ("Northern Interior Salish", "St’át’imcets"), ("Central Salish", "ʔayʔaǰuθəm"),
             ("Salish", "the family"), ("English", "degree constructions compared"),
             ("Nez Perce", "a [-DSP] language with a comparative morpheme"),
             ("Japanese", "[+DSP] but [-DAP]"), ("Mandarin", "[+DSP] but [-DAP]"),
             ("Motu", "conjoined comparatives"), ("Fijian", "conjoined comparatives"),
             ("Washo", "conjoined comparatives"), ("Warlpiri", "conjoined comparatives"),
             ("Samoan", "conjoined comparatives, old Samoan"), ("Greek", "genitive comparatives"),
             ("Russian", "genitive comparatives"), ("Sechelt", "hičim, c(ə)xʷin (Beaumont 2011)"),
             ("Squamish", "superlatives"), ("Cowlitz", "superlatives"), ("Tillamook", "superlatives"),
             ("Halkomelem", "superlatives"), ("Northern Straits", "superlatives"),
             ("Kwakw’ala", "Northern Wakashan, a neighbor of ʔayʔaǰuθəm"),
             ("Northern Wakashan", "Kwakw’ala")]

# Footnote 3 runs on at the foot of page 8 with no mark, Examples are given below: and its (i) and
# (ii), over footnote 4; the reader opens page 8's notes on footnote 4's number.
reader = paper.page_footnotes


def page_footnotes(*args, **options):
    found = reader(*args, **options)
    parts, at = found["3"]
    found["3"] = (parts + list(range(paper.find(r"^Examples are given below:"), paper.find(r"^4 There are reports"))), at)
    return found


paper.page_footnotes = page_footnotes
FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
SKIP = FOOT | set(paper.volume_header()) | paper.running_numbers()


def lines_from(number):
    """The body's lines from number on, passing over footnotes, page numbers and page marks."""
    while number <= paper.last:
        if number not in SKIP and not paper.lines[number][2] and paper.text(number).strip():
            yield number
        number += 1


def until(start, ends):
    """[(line, text)] of the body from start to the line before the first matching ends, and that
    line's number."""
    taken = []
    for number in lines_from(start):
        if re.match(ends, paper.text(number)):
            return taken, number
        taken.append((number, paper.text(number)))
    raise ValueError(ends)


def put(where, who, kind, form, gloss):
    paper.add(where, who, kind, form, gloss)


def parameter(start, where):
    """(5) to (7): a parameter's name, its statement wrapped over two lines, and its source."""
    label, name = gen.EXAMPLE.match(paper.text(start)).groups()
    at = paper.page(start)
    taken, end = until(start + 1, r"^(\(\d+\)|Of these three)")
    text = " ".join(line for _, line in taken)
    statement, source = re.match(r"^(.*?)\s+(\(Beck et al\. [\d:]+\))$", text).groups()
    put("(%s) line 1" % label, A, "note", name, "page %d, the parameter's name" % at)
    put("(%s) line 2" % label, A, "note", statement, "page %d, the parameter" % at)
    put("(%s) line 3" % label, A, "citation", source, "page %d, the tag or source at the right" % at)
    return end


def table(start, where):
    """(8) and (74): a caption, the two languages' column heads, and a row a construction."""
    label, caption = gen.EXAMPLE.match(paper.text(start)).groups()
    at = paper.page(start)
    name = "(%s)" % label
    put(name, A, "note", caption, "page %d, the table's caption" % at)
    put(name, A, "note", paper.text(start + 1), "page %d, the column heads" % at)
    taken, end = until(start + 2, r"^(They conclude|In terms of)")
    for _, text in taken:
        row, first, second = re.match(r"^(.*?)\s+(Yes|No|\?)\s+(Yes|No|\?)$", text).groups()
        put(name, A, "note", row, "page %d, the row's head" % at)
        put(name, A, "note", first, "page %d, %s, ʔayʔaǰuθəm" % (at, row))
        put(name, A, "note", second, "page %d, %s, St’át’imcets" % (at, row))
    return end


def formula(start, where):
    """A formula on its number's line, its subscripts set inline."""
    label, rest = gen.EXAMPLE.match(paper.text(start)).groups()
    put("(%s) line 1" % label, A, "formula", rest, "page %d, a formula, its subscripts set inline" % paper.page(start))
    return start + 1


def formula_85(start, where):
    """(85): the layer sets the comma of D's subscript ⟨d,t⟩ on a line of its own (300 dpi render
    of page 23)."""
    put("(85) line 1", A, "formula", "[[p̓aʔxʷ]] = λdd. λD⟨d,t⟩. MAX(D) > d",
        "page %d, a formula, its subscripts set inline" % paper.page(start))
    return start + 2


def tree(ends):
    def block(start, where):
        label = gen.EXAMPLE.match(paper.text(start)).group(1)
        at = paper.page(start)
        taken, end = until(start, ends)
        first = gen.EXAMPLE.match(taken[0][1]).group(2)
        for count, text in enumerate([first] + [text for _, text in taken[1:]], 1):
            put("(%s) tree line %d" % (label, count), A, "note", text,
                "page %d, a line of the tree as the text layer reads it, its subscripts set inline" % at)
        return end
    return block


def english(start, where):
    """An English example, a note a sentence: (55) and (75) set a. and b. a line each."""
    label, rest = gen.EXAMPLE.match(paper.text(start)).groups()
    at = paper.page(start)
    lettered = re.match(r"^a\.\s+(.*)$", rest)
    if not lettered:
        put("(%s) line 1" % label, A, "note", rest, "page %d, an English example" % at)
        return start + 1
    put("(%sa) line 1" % label, A, "note", lettered.group(1), "page %d, an English example" % at)
    second = re.match(r"^b\.\s+(.*)$", paper.text(start + 1)).group(1)
    put("(%sb) line 1" % label, A, "note", second, "page %d, an English example" % at)
    return start + 2


def contexts(start, where):
    """(94): the context over the example, then a. and b., each with its tiers, its translation with
    the tag (JF) at the right and the consultant's comment under it, b.'s wrapped onto a second line."""
    at = paper.page(start)
    put("(94) line 1", A, "note", gen.EXAMPLE.match(paper.text(start)).group(2), "page %d, over the example" % at)
    taken, end = until(start + 1, r"^Note that without")
    lines = [text for _, text in taken]
    for letter, first in (("a", 0), ("b", 4)):
        name = "(94%s)" % letter
        put(name + " line 1", L, "segmentation", re.match(r"^[ab]\.\s+(.*)$", lines[first]).group(1), "page %d" % at)
        put(name + " line 2", L, "gloss", lines[first + 1], "page %d" % at)
        translation, tag = re.match(r"^(‘.*’)\s+(\(JF\))$", lines[first + 2]).groups()
        put(name + " line 3", A, "translation", translation, "page %d" % at)
        put(name + " line 4", A, "citation", tag, "page %d, the tag or source at the right of the translation" % at)
        comment = " ".join(lines[first + 3:first + 4] if letter == "a" else lines[first + 3:])
        put(name + " line 5", A, "speaker comment", comment,
            "page %d, under the example, the consultant the tag names" % at)
    return end


blocks = {}
for number in (1, 2, 3, 4, 35, 43, 48, 55, 64, 69, 75):
    blocks[paper.find(r"^\(%d\)\s+[A-Za-z]" % number)] = english
blocks[paper.find(r"^\(94\)\s+Context")] = contexts
for number in (5, 6, 7):
    blocks[paper.find(r"^\(%d\)\s+The Degree" % number)] = parameter
blocks[paper.find(r"^\(8\)\s+Degree semantics")] = table
blocks[paper.find(r"^\(74\)\s+Degree semantics")] = table
for number in (76, 77, 78, 82):
    blocks[paper.find(r"^\(%d\) " % number)] = formula
blocks[paper.find(r"^\(85\) ")] = formula_85
blocks[paper.find(r"^\(83\)")] = tree(r"^This will allow")
blocks[paper.find(r"^\(86\)")] = tree(r"^The resulting meaning")
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, spaced={"1": "kʷ(u) =."})

rows = paper.rows
# The examples not in ʔayʔaǰuθəm, by number, and the notes' examples by label.
STATIMCETS_EXAMPLES = set(range(9, 26)) | {36, 37, 38, 44, 45, 49, 50, 51, 56, 57, 58, 59, 65, 66, 70, 71, 79, 81, 84}
BY_LABEL = {"footnote 2 (i)": STATIMCETS, "footnote 3 (i)": STATIMCETS, "footnote 3 (ii)": STATIMCETS,
            "footnote 6 (i)": "Sechelt", "footnote 6 (ii)": "Sechelt"}
# The cited forms not in ʔayʔaǰuθəm: every one of §3.1.1 and §5.1 is St'át'imcets but Squamish -tan.
CITED = {"-er": "English", "-est": "English", "-tan": "Squamish", "hičim": "Sechelt", "t̓im": "Cowlitz",
         "-wins": "Tillamook", "c(ə)xʷin": "Sechelt"}
CITED_STATIMCETS = {"Nqwal’uttenlhkálha", "huʔ", "-ʔúlaʔɬ", "-aʔɬ", "sq̓ʷax̌t", "skənkán", "c̓íla", "=ƛ̓uʔ",
                    "c̓íla nkaʔ", "kʷ="}
for row in rows:
    where, who, kind, form, gloss = row
    example = re.match(r"^(\((\d+)[a-z]?\)|footnote \d+ \([iv]+\))", where)
    if example and who == L:
        if example.group(1) in BY_LABEL:
            row[1] = BY_LABEL[example.group(1)]
        elif example.group(2) and int(example.group(2)) in STATIMCETS_EXAMPLES:
            row[1] = STATIMCETS
    if kind == "cited form" and who == L:
        if form in CITED:
            row[1] = CITED[form]
        elif form in CITED_STATIMCETS or where in ("§3.1.1", "§5.1"):
            row[1] = STATIMCETS
    if form == "n-...-tən only":
        row[3], row[4] = "n-...-tən", gloss + ", the italics running on over only"
    if where == "(34) line 4" or where == "(72) line 3":
        row[4] += ", carries footnote %s" % ("8" if where.startswith("(34)") else "15")


def index_of(test):
    return next(index for index, row in enumerate(rows) if test(row))


# §3.1.1's italic l= ‘at’ and the n-... -ten the page sets with a gap in it, which the italic runs miss.
at = index_of(lambda row: row[3] == "ʔə=")
rows.insert(at, ["§3.1.1", STATIMCETS, "cited form", "l=", "page 6, in italics, ‘at’"])
at = index_of(lambda row: row[3] == "n-lám-xal-tən")
rows.insert(at, ["§3.1.1", STATIMCETS, "cited form", "n-... -ten", "page 8, in italics"])

# (42)'s speaker's translation opens the paragraph after it, footnote 9's mark on it.
at = index_of(lambda row: row[0] == "§4.1" and row[3].startswith("(‘This is how the length"))
comment, rest = re.match(r"^(\(‘This is how.*?\)9) (The construction in \(42\).*)$", rows[at][3]).groups()
rows[at][3] = rest
rows.insert(at, ["(42) line 4", A, "note", comment,
                 "page 13, under the translation, the speaker's translation, carries footnote 9"])
note = rows.pop(index_of(lambda row: row[0] == "footnote 9"))
rows.insert(at + 1, note)

# The English readings (i) and (ii) under (79) and (80).
for label in ("79", "80"):
    last = max(index for index, row in enumerate(rows) if row[0].startswith("(%s) line" % label))
    for step in (1, 2):
        row = rows[last + step]
        row[0], row[4] = "(%s) line %d" % (label, 5 + step if label == "79" else 3 + step), \
            row[4] + ", a reading under the example, an English example"

# §6's list (i) to (iii) wraps each possibility onto a second line, and the paragraph after it
# opens on As far as (i).
first = index_of(lambda row: row[0] == "§6" and row[3].startswith("(i) She has"))
tail, after = re.match(r"^(conjoined comparatives\.) (As far as \(i\).*)$", rows[first + 5][3]).groups()
items = [rows[first][3] + " " + rows[first + 1][3], rows[first + 2][3] + " " + rows[first + 3][3],
         rows[first + 4][3] + " " + tail]
rows[first:first + 6] = [["§6", A, "note", item, "page 24"] for item in items] + [["§6", A, "note", after, "page 24"]]

# Kennedy (2007) wraps its editors onto a line the reference reader opens as an entry.
at = index_of(lambda row: row[2] == "reference" and row[3].startswith("Elliott, M."))
rows[at - 1][3] += " " + rows[at][3]
del rows[at]
paper.write()
