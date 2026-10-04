"""The ops of 2010_Lyon: John Lyon's Nominal modification in Upper Nicola Okanagan: A working paper.
The determiner iʔ and the oblique marker t introduce the head and the modifier of a nominal in six
patterns; complex DPs and complex nominal predicates (pattern 2) take only individual-level
unaccusative modifiers, or stage-level ones stative ac- can coerce, and are attributive
modification; the sequence iʔ t (patterns 3 and 6) stands only before transitive modifiers and marks
relative clauses; patterns 1 and 4 show both.

An example's part is its form line, the speaker's initials at its right, and where it is glossed a
gloss in capitals under it and an unquoted translation under that; a part given as a form line
alone has no gloss or translation. (15), (55), (57) and (95) set their form over two lines, each
with its gloss. Tables 1 to 6 are read cell by cell from the renders, each checked against the
printed lines letter for letter.

Page text read by glyph rows, the Word export's unmapped /gN glyphs named by outline
(PAPER_TOUNICODE in page_text.py).
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Okanagan"
AUTHORS = ["John Lyon"]
paper = gen.Paper("2010_Lyon", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Hank Charters", "a language consultant, footnote 1; HC after an example"),
         ("Lottie Lindley", "a language consultant, footnote 1; LL after an example"),
         ("Sarah McLeod", "a language consultant, footnote 1; SM after an example"),
         ("Nancy Saddleman", "a language consultant, footnote 1; NS after an example"),
         ("Rita Stewart", "a language consultant, footnote 1; RS after an example"),
         ("Theresa Tom", "a language consultant, footnote 1"), ("Wilford Tom", "a language consultant, footnote 1"),
         ("Kroeber", "Paul Kroeber, relativization in Thompson (1997) and The Salish Language Family (1999)"),
         ("Koch", "Karsten Koch, predicate modification (2004) and nominal modification (2006) in Thompson"),
         ("Matthewson & Davis", "Lisa Matthewson and Henry Davis, the structure of DP in St'át'imcets (1995)"),
         ("Davis, Lai & Matthewson", "Henry Davis, Irene Lai and Lisa Matthewson (1997), attributive "
                                     "modification in Lillooet and Shuswap"),
         ("Davis", "Henry Davis, restrictions on modification (2002) and locative relative clauses (2004) "
                   "in St'at'imcets"),
         ("Matthewson", "Lisa Matthewson, determiner systems and quantificational strategies (1999)"),
         ("Montler", "Timothy Montler, attributive constructions in Saanich (1993)"),
         ("A. Mattina", "Anthony Mattina, Colville grammar (1973), The Golden Woman (1985), Okanagan aspect "
                        "(1993), to-be and intentional forms (1996), sandhi (2000), the transitive sentence (2004)"),
         ("N. Mattina", "Nancy Mattina, aspect and category in Okanagan word formation (1996) and "
                        "Moses-Columbian DPs (2006)"),
         ("Gardiner", "Dwight Gardiner, preverbal positions in Shuswap (1993)"),
         ("Heim & Kratzer", "Irene Heim and Angelika Kratzer, Semantics in Generative Grammar (1998)"),
         ("Carlson", "Gregory Carlson, Reference to Kinds in English (1977)"),
         ("Kayne", "Richard Kayne, The Antisymmetry of Syntax (1994)")]
LANGUAGES = [(LANGUAGE, "Southern Interior Salish, the language of the paper, its Upper Nicola dialect"),
             ("Thompson", "Northern Interior Salish"), ("Lillooet", "Northern Interior Salish"),
             ("Shuswap", "Northern Interior Salish"), ("Saanich", "Central Salish, Montler's attributive constructions"),
             ("Coeur d'Alene", "Southern Interior Salish"), ("Moses-Columbian", "Southern Interior Salish"),
             ("Spokane-Kalispel-Flathead", "Southern Interior Salish"),
             ("Nsyilxcn", "the language's own name, footnote 1"), ("English", "the translations")]
FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
RUNNING = paper.running_numbers_set()
PART = re.compile(r"^([a-h])\.\s+(.*)$")
# The initials of the speaker who gave or judged a form, at its right, and any note after them.
TAGGED = re.compile(r"^(.*?\S)\s*(\((?:[A-Z]{2})(?:,[A-Z]{2})*\))((?:\s+\([^()]*\))*)$")
# A gloss sets its labels in capitals, DET, 1SG.ERG, -(DIR)-; (44a) and (45a) gloss in words alone.
GLOSS = re.compile(r"(?<![\w’'])(?:DET|DEM|NEG|EPIS|AFF|AFFIRM|COMP|LOC|SB|det)(?![\w])|\d(?:SG|PL)|"
                   r"-\(?[A-Z]{2,}|^[A-Z]{3,}-")
PLAIN_GLOSSES = {"true/straight t chief", "frightened t man"}
# A note set after a translation, (volunteered gloss) in (1a), (cf 30a,b) in (42a).
TRAILING = re.compile(r"^(.*\S)\s+(\((?:cf |volunteered|captikʷɬ)[^()]*\)\.?)$")
COMMENT = re.compile(r"^(Consultant(?:'s|’s)? Comment|Consultant):\s")
READING = re.compile(r"^\*?(?:Intended|Actual):\s")
CONTEXT = re.compile(r"^\(Context:")


def printed(number):
    return bool(paper.text(number).strip()) and not paper.lines[number][2] and number not in RUNNING \
        and number not in FOOT


def after(number):
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def same_letters(cells, first, last):
    """Check that the cells written from lines first to last hold the printed letters, all of them
    and no others, whatever the order the columns set them in. A pattern's number standing alone on
    its line in Tables 5 and 6 is read as a page number; inside a table it counts."""
    written = sorted("".join(cells).replace(" ", ""))
    source = sorted("".join(paper.text(one) for one in range(first, last + 1)
                            if printed(one) or one in RUNNING).replace(" ", ""))
    assert written == source, (first, last, "".join(written), "".join(source))


def opens(text):
    return bool(PART.match(text) or gen.EXAMPLE.match(text) or gen.HEADING.match(text))


def is_gloss(text):
    return not opens(text) and bool(GLOSS.search(text) or text in PLAIN_GLOSSES)


def example(first, where):
    """Each lettered part of the example in turn, or the example itself where it has no letters."""
    label, text = gen.EXAMPLE.match(paper.text(first)).groups()
    line, letter = first, ""
    while True:
        item = PART.match(text)
        if item:
            letter, text = item.groups()
        line = part("%s%s" % (label, letter), text, line)
        if line > paper.last or not PART.match(paper.text(line)):
            return line
        text = paper.text(line)


def part(label, text, line):
    """One part from its form line: the form, its initials and notes, then while a gloss stands
    under it the gloss, the next form of a two-line example and its gloss, and the translation,
    run on to the line under it where it wraps; last the readings of (16b), the consultant's comments
    and the context under the part. Returns the line after the part."""
    count = [0]

    def row(who, kind, form, number, what):
        count[0] += 1
        paper.add("(%s) line %d" % (label, count[0]), who, kind, form, "page %d, %s" % (paper.page(number), what))

    number = line
    while True:
        below = after(number)
        glossed = below <= paper.last and is_gloss(paper.text(below))
        tagged = TAGGED.match(text)
        form = tagged.group(1) if tagged else text
        row(L, "segmentation" if glossed else "transcription", form, number,
            "the form over its gloss" if glossed else "the form, given with no gloss")
        if tagged:
            row(A, "citation", tagged.group(2), number, "the speaker's initials at the right")
            for note in re.findall(r"\([^()]*\)", tagged.group(3)):
                row(A, "note", note, number, "set after the initials")
        if not glossed:
            number = below
            break
        row(L, "gloss", paper.text(below), below, "the gloss")
        number = after(below)
        text = paper.text(number)
        if not opens(text) and is_gloss(paper.text(after(number))):
            continue
        start = number
        while text[-1:].isalpha() and not opens(paper.text(after(number))) and paper.text(after(number))[:1].islower():
            number = after(number)
            text += " " + paper.text(number)
        trailing = TRAILING.match(text)
        row(A, "translation", trailing.group(1) if trailing else text, start, "the translation")
        if trailing:
            row(A, "note", trailing.group(2), number, "set after the translation")
        number = after(number)
        break
    while number <= paper.last:
        text = paper.text(number)
        if READING.match(text):
            row(A, "translation", text, number, "the %s reading" % ("intended" if "Intended" in text else "actual"))
        elif CONTEXT.match(text):
            row(A, "note", text, number, "the context")
        elif COMMENT.match(text):
            tagged = TAGGED.match(text)
            row(A, "speaker comment", tagged.group(1) if tagged else text, number, "under the part")
            if tagged:
                row(A, "citation", tagged.group(2), number, "the speaker's initials at the right of the comment")
        else:
            break
        number = after(number)
    return number


def paragraph(first, where):
    """A paragraph of prose opening on an example's number, (20) is clearly different, to the next
    paragraph's indent."""
    starts = paper.paragraph_starts()
    lines, number = [first], after(first)
    while number <= paper.last and number not in starts and not opens(paper.text(number)):
        lines.append(number)
        number = after(number)
    body = paper.joined(lines)
    pages = sorted({paper.page(one) for one in lines})
    paper.add(where, A, "note", body, "page %d" % pages[0])
    paper.cited(where, body, pages)
    paper.mentions(where, body, NAMES, "name")
    paper.mentions(where, body, LANGUAGES, "language")
    return number


def generalization(first, where):
    """One of section 5's numbered generalizations, a note to its lines, its lettered points
    a note each."""
    label = gen.EXAMPLE.match(paper.text(first)).group(1)
    lines, number = [first], after(first)
    groups = [lines]
    while number <= paper.last and not gen.EXAMPLE.match(paper.text(number)) and \
            not gen.HEADING.match(paper.text(number)):
        if PART.match(paper.text(number)):
            groups.append([])
        groups[-1].append(number)
        number = after(number)
    for count, group in enumerate(groups):
        body = paper.joined(group)
        paper.add("§5 (%s)" % label, A, "note", body, "page %d, %s" % (
            paper.page(group[0]), "the generalization" if count == 0 else "a lettered point of it"))
        paper.cited("§5 (%s)" % label, body, [paper.page(group[0])])
        paper.mentions("§5 (%s)" % label, body, NAMES, "name")
        paper.mentions("§5 (%s)" % label, body, LANGUAGES, "language")
    return number


def grid(label, first, last, caption, heads, rows, cells=None, forms=False):
    """A table: its caption, each column's head, then each row's head and each cell under the head
    it stands under. rows is [(row head, [(column, cell)])]; forms writes a cell of particles as a
    cited form. cells, where given, are the printed cells for the letter check in place of these."""
    page = paper.page(first)
    written = list(caption) + list(heads) + [one for head, row in rows for one in [head] + [cell for _, cell in row]]
    same_letters(cells if cells is not None else written, first, last)
    paper.add(label, A, "note", " ".join(caption), "page %d, the table's caption, under it" % page)
    for head in heads:
        paper.add(label, A, "note", head, "page %d, a column's head" % page)
    for head, row in rows:
        paper.add(label, A, "note", head, "page %d, the row's head" % page)
        for column, cell in row:
            particles = forms and re.fullmatch(r"(?:iʔ|t)(?: t)?", cell)
            paper.add(label, L if particles else A, "cited form" if particles else "note", cell,
                      "page %d, %s, %s" % (page, head, column))
    return after(last)


def table_1(first, where):
    heads = ("Complex DPs", "CNPs", "Complex Obliques", "Det-Det structures")
    rows = [("head-final", list(zip(heads, "√√√√"))), ("head-initial", list(zip(heads, "**√√")))]
    return grid("Table 1", first, paper.find(r"^a subset of Okanagan Modificational Structures$", first),
                ("Table 1. Permissible Head-modifier orderings for", "a subset of Okanagan Modificational Structures"),
                heads, rows)


def table_2(first, where):
    heads = ("clause type", "configuration", "Lillooet", "Thompson", "Okanagan")
    printed_rows = (("pre-nominal", "[D1 [clause NP]]", "√", "*", "*"),
                    ("post-posed", "[D1 [NP clause]]", "√", "*", "*"),
                    ("post-nominal", "[D1[NP [D2 clause]]", "√", "√", "√"),
                    ("pre-posed", "[D1 clause D2 NP]", "*", "√", "√"))
    rows = [(one[0], list(zip(heads[1:], one[1:]))) for one in printed_rows]
    return grid("Table 2", first, paper.find(r"^Table 2\. ", first),
                ("Table 2. Possible Relative Clause Configurations in Lillooet, Thompson, and Okanagan12",),
                heads, rows)


def table_3(first, where):
    columns = ("HEAD-INITIAL before nominal", "HEAD-INITIAL before modifier", "HEAD-FINAL before modifier",
               "HEAD-FINAL before nominal")
    heads = ("Pattern", "HEAD-INITIAL", "HEAD-FINAL", "before nominal", "before modifier", "before modifier",
             "before nominal")
    printed_rows = (("1", "iʔ", "iʔ", "iʔ", "iʔ"), ("2", "iʔ", "t", "iʔ", "t"), ("3", "iʔ", "iʔ t", "iʔ t15", "iʔ"),
                    ("4", "t", "t", "t", "t"), ("5", "t", "iʔ", "t", "iʔ"), ("6", "t", "iʔ t", "iʔ t14", "t"))
    rows = [("Pattern " + one[0], list(zip(columns, one[1:]))) for one in printed_rows]
    cells = list(heads) + [cell for one in printed_rows for cell in one] + \
        ["Table 3. Surface Patterns Displayed by Head/Modifier Introductory Particles in",
         "Okanagan Nominal Modification Structures"]
    last = paper.find(r"^Okanagan Nominal Modification Structures$", first)
    page = paper.page(first)
    same_letters(cells, first, last)
    paper.add("Table 3", A, "note", "Table 3. Surface Patterns Displayed by Head/Modifier Introductory Particles in "
              "Okanagan Nominal Modification Structures", "page %d, the table's caption, under it" % page)
    for head, what in zip(heads, ("", "", "", " under HEAD-INITIAL", " under HEAD-INITIAL", " under HEAD-FINAL",
                                  " under HEAD-FINAL")):
        paper.add("Table 3", A, "note", head, "page %d, a column's head%s" % (page, what))
    for head, row in rows:
        paper.add("Table 3", A, "note", head.split()[-1], "page %d, the row's head, the pattern's number" % page)
        for column, cell in row:
            if cell[-1].isdigit():
                # The cell's particles carry a footnote's mark, raised after the t.
                paper.add("Table 3", A, "note", cell, "page %d, %s, %s, the particles iʔ t and the mark of "
                          "footnote %s" % (page, head, column, cell[-2:]))
            else:
                paper.add("Table 3", L, "cited form", cell, "page %d, %s, %s" % (page, head, column))
    return after(last)


def table_4(first, where):
    heads = ("Unergative", "Unaccusative", "Stage Level 1: non-coercible", "Stage Level 2: coercible", "w/o ac-",
             "with ac-", "Individual Level")
    rows = [("CNP", [("Unergative", "*"), ("Unaccusative, Stage Level 1", "*"),
                     ("Unaccusative, Stage Level 2, w/o ac-", "*"), ("Unaccusative, Stage Level 2, with ac-", "√"),
                     ("Unaccusative, Individual Level", "√")]),
            ("Complex DP", [("Unergative", "*"), ("Unaccusative, Stage Level 1", "*"),
                            ("Unaccusative, Stage Level 2, the cell across w/o ac- and with ac-", "√"),
                            ("Unaccusative, Individual Level", "√")])]
    return grid("Table 4", first, paper.find(r"^Table 4\. ", first),
                ("Table 4. Restrictions on Unergative/Unaccusative Modifiers in CNPs and Complex DPs",), heads, rows)


SUMMARY_HEADS = ("non-eventive unaccusative modifiers", "unergative modifiers", "genitive/transitive modifiers")
SUMMARY = (("1", (("iʔ modifier - iʔ head", "√", "√", "√"), ("iʔ head - iʔ modifier", "√ (+human only)", "√", "√"))),
           ("2", (("iʔ modifier - t head", "√", "*", "*"), ("iʔ head - t modifier", "*", "*", "*"))),
           ("3", (("iʔ modifier - iʔ t head", "*", "*", "*"), ("iʔ head - iʔ t modifier", "*", "*", "√"))),
           ("4", (("t modifier - t head", "√", "√", "√ (future only)"),
                  ("t head - t modifier", "√", "√", "√ (future only)"))),
           ("5", (("t modifier - iʔ head", "*", "*", "*"), ("t head - iʔ modifier", "*", "*", "*"))),
           ("6", (("iʔ t modifier - t head", "*", "*", "*"), ("t head - iʔ t modifier", "*", "*", "√"))))


def summary(label, first, caption, patterns):
    """Tables 5 and 6: under each pattern's number its two orderings, each a row of three cells."""
    last = paper.find(r"^%s$" % re.escape(caption[-1]), first)
    page = paper.page(first)
    cells = list(caption) + list(SUMMARY_HEADS) + ["Pattern %s" % number for number, _ in patterns] + \
        [cell for _, rows in patterns for row in rows for cell in row]
    same_letters(cells, first, last)
    paper.add(label, A, "note", " ".join(caption), "page %d, the table's caption, under it" % page)
    for head in SUMMARY_HEADS:
        paper.add(label, A, "note", head, "page %d, a column's head" % page)
    for number, rows in patterns:
        paper.add(label, A, "note", "Pattern %s" % number, "page %d, the head of the two rows beside it" % page)
        for row in rows:
            paper.add(label, A, "note", row[0], "page %d, an ordering of pattern %s" % (page, number))
            for head, cell in zip(SUMMARY_HEADS, row[1:]):
                paper.add(label, A, "note", cell, "page %d, pattern %s, %s, %s" % (page, number, row[0], head))
    return after(last)


def table_5(first, where):
    return summary("Table 5", first, ("Table 5. Interim Summary: Modifier/Head introductory Particles, Head-modifier",
                                      "orderings, and Semantic Types of Modifiers."), SUMMARY[:3])


def table_6(first, where):
    # Table 6 sets pattern 3's first ordering as iʔ t modifier - t head, where Table 5 has iʔ modifier -
    # iʔ t head (renders of pages 23 and 29).
    patterns = list(SUMMARY)
    patterns[2] = ("3", (("iʔ t modifier - t head", "*", "*", "*"), SUMMARY[2][1][1]))
    return summary("Table 6", first, ("Table 6. Summary: Modifier/Head introductory Particles, Head-modifier orderings,",
                                      "and Semantic Types of Modifiers."), patterns)


def abbreviations(first, where):
    """The list of glossing abbreviations, two to a line, each beside its meaning."""
    paper.add("abbreviations", A, "heading", paper.text(first), "page %d" % paper.page(first))
    number = after(first)
    while not paper.text(number).startswith("References"):
        page = paper.page(number)
        cells = re.split(r"\s{3,}", paper.spaced[number])
        assert len(cells) == 4, paper.spaced[number]
        for abbreviation, meaning, side in ((cells[0], cells[1], "on the left"), (cells[2], cells[3], "on the right")):
            paper.add("abbreviations", A, "note", abbreviation, "page %d, a glossing abbreviation, %s" % (page, side))
            paper.add("abbreviations", A, "note", meaning, "page %d, the meaning of %s" % (page, abbreviation))
        number = after(number)
    return number


BODY = paper.find(r"^1\s+Introduction$")
END = paper.find(r"^References$")
HEADINGS = paper.headings(BODY, END - 1, skip=FOOT)
# The heading reader wants a capital after the number; the three headings that open on the particle t,
# 4.4 t - t (Pattern 4), are added here.
for pattern in (r"^4\.4\s+t - t \(Pattern 4\)$", r"^4\.5\s+t - Determiner \(Pattern 5\)$",
                r"^4\.6\s+t - Determiner t \(Pattern 6\)$"):
    at = paper.find(pattern, BODY)
    HEADINGS[at] = paper.text(at).split()[0]
HEADINGS = dict(sorted(HEADINGS.items()))
BLOCKS = {}
summary_at = paper.find(r"^5\s+Summary and Discussion$", BODY)
for number in range(BODY, END):
    opened = gen.EXAMPLE.match(paper.text(number))
    if not opened or number in FOOT or not printed(number):
        continue
    if number > summary_at:
        BLOCKS[number] = generalization if number < paper.find(r"^5\.1\s", summary_at) else example
    elif (opened.group(2) or "").startswith("is clearly"):
        BLOCKS[number] = paragraph
    else:
        BLOCKS[number] = example
table_5_at = paper.find(r"^non-eventive\s", BODY)
BLOCKS.update({paper.find(r"^Complex DPs\s+CNPs", BODY): table_1, paper.find(r"^clause type\s", BODY): table_2,
               paper.find(r"^Pattern\s+HEAD-INITIAL", BODY): table_3, paper.find(r"^Unergative\s+Unaccusative", BODY): table_4,
               table_5_at: table_5, paper.find(r"^non-eventive\s", table_5_at + 1): table_6,
               paper.find(r"^Abbreviations$", BODY): abbreviations})
# Note 1 is marked on the title, A working paper1.
paper.standard(AUTHORS, NAMES, LANGUAGES, headings=HEADINGS, blocks=BLOCKS, appendix=r"^John Lyon$",
               notes_title=tuple(gen.TITLE_MARKS) + ("1",))
# (95) sets its indices as subscripts, [səntumisten]₂ [iʔ tl t₂]₁ and t₁, and (17) the DP that
# closes its bracket (renders of pages 8 and 31); the text layer sets them on the line.
for row in paper.rows:
    if row[0] == "(95) line 1" and row[2] == "segmentation":
        row[3] = row[3].replace("]2", "]₂").replace("t2]1", "t₂]₁")
        row[4] += ", its indices subscripts"
    elif row[0] == "(95) line 3" and row[2] == "segmentation":
        row[3] = row[3].replace("t1", "t₁")
        row[4] += ", its index a subscript"
    elif row[0] in ("(17a) line 1", "(17b) line 1") and "]DP" in row[3]:
        row[4] += ", the DP after the bracket a subscript, set on the line as the text layer has it"
# The reference reader takes the second line of Heim and Kratzer for a reference of its own; it joins
# the one before it.
at = next(at for at, row in enumerate(paper.rows) if row[2] == "reference" and row[3] == "Malden, Mass.: Blackwell.")
paper.rows[at - 1][3] += " " + paper.rows[at][3]
del paper.rows[at]
tail = paper.find(r"^John Lyon$", END)
for number, what in zip((tail, after(tail), after(after(tail))),
                        ("the author's name", "an e-mail address", "a second e-mail address")):
    paper.add("end", A, "note", paper.text(number), "page %d, %s" % (paper.page(number), what))
paper.write()
