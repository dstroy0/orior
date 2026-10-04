"""The ops of 14-Davis_Forbes_ICSNL50_final-32: Henry Davis and Clarissa Forbes on the Gitksan
connectives. Determinate t and =s are one connective, =s the softened t after a coindexed Series II
-t, and dip is an associative marker outside the connective system; the analysis extends to Coast
Tsimshian.

The examples set the sentence over its gloss, the two aligned word by word (opening =
"segmentation"). The caption on the label of (3) to (14), singular determinate S in independent
clause, is a note. (25) gives two readings, each a note to its label and a translation. (34), (35)
and (37) are underlying forms, the two tiers with no translation. (55) and (56) set a Coast Tsimshian
part over a Gitksan one, each under its caption. The rules (15), (21), (30) to (32) and (46), the
condition (18) and the derivation (49) are displays, a note to each printed line. Each of the ten
tables is a note to its caption and to each printed row, and each lettered list a note to each item.
The researcher's questions and the consultant's answers under (39) to (42) are notes and speaker
comments. (50) to (54) and (55a), (56a) are Coast Tsimshian, in the Sm’algyax community
orthography. The italic forms in the prose are cited by their runs, each with its language.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Gitksan"
COAST = "Coast Tsimshian"
AUTHORS = ["Henry Davis", "Clarissa Forbes"]
paper = gen.Paper("14-Davis_Forbes_ICSNL50_final-32", authors=", ".join(AUTHORS), language=LANGUAGE)
paper.opening = "segmentation"
# The exchanges under (39) to (42) name the researcher and BS, the consultant, as well.
gen.DIALOGUE = re.compile(r"^(Interviewer|Consultant|Researcher|BS):\s")

NAMES = [("Barbara Sennott", "Gitksan speaker, nee Harris, thanked; BS in the examples"),
         ("Vince Gogag", "Gitksan speaker, thanked"), ("Hector Hill", "Gitksan speaker, thanked"),
         ("Lisa Matthewson", "thanked for contributions and comments"),
         ("Margaret Anderson", "thanked, the Coast Tsimshian data; Visible Grammar with Ignace (2008)"),
         ("Boas", "Franz Boas, Tsimshian (1911)"), ("Rigsby", "Bruce Rigsby, Gitxsan grammar (1986)"),
         ("Tarpent", "Marie-Lucie Tarpent, a grammar of Nisgha (1987b)"),
         ("Hunt", "Katharine Hunt, clause structure, agreement and case in Gitksan (1993)"),
         ("Dunn", "John Asher Dunn, Coast Tsimshian (1979a, b, c)"),
         ("Mulder", "Jean Gail Mulder, ergativity in Coast Tsimshian (1994)"),
         ("Bach", "Emmon Bach, argument marking in Riverine Tsimshian (2004)"),
         ("Ignace", "Marianne Ignace, Visible Grammar with Anderson (2008)"),
         ("Brown", "Jason Brown, A'-dependencies in Gitksan with Davis (2011)"),
         ("Hindle", "Lonnie Hindle, the practical dictionary with Rigsby (1973)"),
         ("Stebbins", "Tonya Stebbins, adjectives in Coast Tsimshian (2003)"),
         ("Sellers", "Holly Sellers, clitics in Sm’algyax with Mulder (2010)"),
         ("Peterson", "Tyler Peterson, Tsimshian connectives (2004), evidentiality in Gitksan (2010)"),
         ("McCarthy", "John J. McCarthy, faithfulness with Prince (1995)"),
         ("Prince", "Alan Prince, faithfulness with McCarthy (1995)"),
         ("Corbett", "Greville G. Corbett, Number (2001)"),
         ("McClay", "Elise McClay, a field methods poster (2015)"),
         ("Sasama", "Fumiko Sasama, Coast Tsimshian plurals (1995)")]
LANGUAGES = [(LANGUAGE, "Tsimshianic, Interior branch"),
             ("Tsimshianic", "the family"), (COAST, "Tsimshianic; CT; (50) to (56a)"),
             ("Interior Tsimshianic", "Gitksan and Nisga'a; IT"), ("Nisga’a", "Tsimshianic, Interior branch"),
             ("Sm’algyax", "Coast Tsimshian"), ("Southern Tsimshian", "Sgüüxs, Tarpent (1998)"),
             ("Proto-Tsimshianic", "the reconstructed ancestor, *=ahl"),
             ("English", "the translations"), ("Wakashan", "compared for complexity"),
             ("Salish", "compared for complexity")]
# The examples of Coast Tsimshian, by label.
COAST_EXAMPLES = re.compile(r"^\((?:5[0-4]|55a|56a)\) line")
# The displays, by number: their printed lines, each a note.
DISPLAYS = {"15": (3, True), "18": (4, True), "21": (3, True), "30": (3, True), "31": (3, True),
            "32": (3, True), "46": (3, True), "49": (2, True)}
# The caption on the label of (3) to (14), the argument each example shows.
paper.captions = {"%s determinate %s in %s clause" % (number, function, clause)
                  for number in ("singular", "plural") for function in "SAO" for clause in ("independent", "dependent")}
# The captions over the parts of (55) and (56).
PART_CAPTIONS = ("Coast Tsimshian", "Interior Tsimshian (Gitksan)")
# The italic runs that are forms, by language. The Gitksan orthography defines most in plain letters,
# t, dip and =hl, where orthographic() takes them for English. The Coast Tsimshian ones are the
# Sm’algyax connectives and aspect markers, and Tarpent's postclitics are Southern Tsimshian. The
# derivation (49) in the prose is a display's, and a, b, i and C label parts and items.
FORM_LANGUAGE = {"t": L, "Dip": L, "dip": L, "=s": L, "=hl": L, "hl": L, "s": L, "d": L, "gat": L, "=gat": L,
                 "=ima(')a": L, "go(')o": L, "go(')o=": L, "a=": L, "loo": L, "loo-t": L, "loo-n": L,
                 "lit-si'm": L, "*lisi'm": L, "si'm": L, "yukw": L, "'nit": L, "'nidiit": L, "as": L,
                 "as 'niin": L, "as 'nidiit": L, "t Tom": L, "t dip": L, "=s t": L, "t s": L, "gan": L,
                 "gya": L, "ga": L, "ga-": L, "t-": L, "-t": L, "-diit": L, "-(t)xw": L, "=ahl": L,
                 "=a": COAST, "yagwa": COAST, "yakw": COAST, "=da": COAST, "=dVt": COAST, "=(V)t": COAST,
                 "ɬ": COAST, "ɬa": COAST, "nah": COAST, "dm": COAST, "-d=it": COAST, "=da'a": "Southern Tsimshian", "=ga'a": "Southern Tsimshian",
                 "*=ahl": "Proto-Tsimshianic", "a-t t Mary → a-t=s Mary → a=s Mary": None}
paper.form_language = lambda run: FORM_LANGUAGE[run] if run in FORM_LANGUAGE else L if gen.orthographic(run) else None

RUNNING = paper.running_numbers_set()
AT_FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts} | set(paper.volume_header())
# A list's item opens on its letter or numeral, A. The distribution, iv. For determinates.
ITEM = re.compile(r"^(?:[A-E]|I{1,2}|i{1,3}|iv)\.\s")
# A reading of (25), (i) ‘S/he didn’t see Michael.’
READING = re.compile(r"^\((i{1,3})\)\s+(‘.*)$")
# The lettered lists: the first item's opening, and the printed lines the list runs over.
LISTS = [(r"^i\. As specified in the left-hand column", 10), (r"^A\. The distribution of the common noun", 7),
         (r"^I\. t and =s are allomorphs", 2), (r"^A\. Case is a form of dependent marking", 6),
         (r"^A\. =s is derived from determinate t", 8), (r"^A\. The determinate connective t and its plural", 13)]


def printed(number):
    return bool(paper.text(number).strip()) and not paper.lines[number][2] and number not in RUNNING \
        and number not in AT_FOOT


def after(number):
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def row(state, label, who, kind, form, at, gloss=""):
    """A row of an example read here, its lines numbered on through the lettered parts as example()
    numbers them."""
    state["count"] += 1
    paper.add("(%s) line %d" % (label, state["count"]), who, kind, form, "page %d%s" % (paper.page(at), gloss))


def translation(state, label, text, at):
    said, pieces = gen.split_translation(text) or (text, [])
    row(state, label, A, "translation", said, at)
    for piece in pieces:
        row(state, label, A, "citation" if gen.is_source(piece) else "note", piece, at,
            ", at the right of the translation")


def table(start, where):
    """A table: a note to its caption and to each printed row, to the blank line under it."""
    here = re.match(r"^(Table \d+)", paper.text(start)).group(1)
    paper.add(here, A, "note", paper.text(start), "page %d, the table's caption" % paper.page(start))
    line, count = start + 1, 0
    while paper.text(line).strip():
        if printed(line):
            count += 1
            paper.add("%s line %d" % (here, count), A, "note", paper.text(line),
                      "page %d, a row of the table" % paper.page(line))
        line += 1
    return line


def listing(count):
    """A lettered list over count printed lines: a note to each item, its wrapped lines run on."""
    def block(start, where):
        line, done, items = start, 0, []
        while done < count:
            if printed(line):
                if ITEM.match(paper.text(line)) or not items:
                    items.append([paper.text(line), line])
                else:
                    items[-1][0] += " " + paper.text(line)
                done += 1
            line += 1
        for text, at in items:
            paper.add(where, A, "note", text, "page %d, an item of the list" % paper.page(at))
            paper.cited(where, text, [paper.page(at)])
            paper.mentions(where, text, NAMES, "name")
            paper.mentions(where, text, LANGUAGES, "language")
        return line
    return block


def underlying(start, where):
    """(34), (35) or (37): an underlying form over its gloss, with no translation."""
    opened = gen.EXAMPLE.match(paper.text(start))
    state = {"count": 0}
    row(state, opened.group(1), L, "segmentation", opened.group(2), start, ", the underlying form")
    below = after(start)
    row(state, opened.group(1), L, "gloss", paper.text(below), below)
    return after(below)


def readings(start, where):
    """(25): the sentence over its gloss, then each reading, a note to its label and a translation."""
    opened = gen.EXAMPLE.match(paper.text(start))
    number, state = opened.group(1), {"count": 0}
    row(state, number, L, "segmentation", opened.group(2), start)
    line = after(start)
    row(state, number, L, "gloss", paper.text(line), line)
    line = after(line)
    while READING.match(paper.text(line)):
        reading = READING.match(paper.text(line))
        row(state, number, A, "note", "(%s)" % reading.group(1), line, ", the reading's label")
        translation(state, number, reading.group(2), line)
        line = after(line)
    return line


def compared(start, where):
    """(55) or (56): a caption and the Coast Tsimshian part a., a caption and the Gitksan part b.,
    each part a sentence over its gloss and its translation."""
    opened = gen.EXAMPLE.match(paper.text(start))
    number, state = opened.group(1), {"count": 0}
    line, text, label, tiers = start, opened.group(2), number, 0
    while True:
        sub = gen.SUB.match(text)
        if sub:
            label, tiers, text = number + sub.group(1), 0, sub.group(2)
        if text in PART_CAPTIONS:
            row(state, number, A, "note", text, line, ", over the part under it")
        elif text.startswith("‘"):
            translation(state, label, text, line)
            below = paper.text(after(line))
            if not (gen.SUB.match(below) or below in PART_CAPTIONS):
                return after(line)
        else:
            row(state, label, COAST if label.endswith("a") else L, ("segmentation", "gloss")[tiers], text, line)
            tiers += 1
        line = after(line)
        text = paper.text(line)


blocks = {number: table for number in range(1, paper.last + 1) if re.match(r"^Table \d+: ", paper.text(number))}
for opening, count in LISTS:
    blocks[paper.find(opening)] = listing(count)
READERS = {"25": readings, "34": underlying, "35": underlying, "37": underlying, "55": compared, "56": compared}
for number in range(1, paper.last + 1):
    opened = gen.EXAMPLE.match(paper.text(number))
    if opened and printed(number) and opened.group(1) in READERS:
        blocks[number] = READERS[opened.group(1)]
paper.standard(AUTHORS, NAMES, LANGUAGES, displays=DISPLAYS, blocks=blocks)

# An example's own label, with the footnote it stands in: footnote 15 (iii), (45b).
EXAMPLE_ROW = re.compile(r"^((?:footnote \S+ )?\((?:\d+|[ivx]+))[a-z]?\) line \d+$")
rows, relabeled = [], set()
for where, who, kind, form, gloss in paper.rows:
    if COAST_EXAMPLES.match(where) and who == L:
        who = COAST
    # The researcher's question is a note; BS's remark under (39) is the consultant's.
    if form.startswith("Researcher:"):
        kind, gloss = "note", re.sub(r", [^,]*$", ", the researcher's question", gloss)
    if form.startswith("BS:"):
        kind, gloss = "speaker comment", re.sub(r", [^,]*$", ", the consultant's remark", gloss)
    # A translation under its label read as a tier: (a) ‘They hit him/her.’ in note 15, intended:
    # ‘The chief and his family arrived at the feast.’ under (45b).
    labeled = re.match(r"^(\([a-z]\)|intended:)\s+(‘.*’)$", form)
    if labeled and kind in ("segmentation", "gloss"):
        page = re.match(r"^page \d+", gloss).group(0)
        rows.append([where, A, "note", labeled.group(1), page + ", the translation's label"])
        rows.append([where, A, "translation", labeled.group(2), page])
        relabeled.add(EXAMPLE_ROW.match(where).group(1))
        continue
    rows.append([where, who, kind, form, gloss])
# The lines of an example the labels split, numbered again in order.
counts = {}
for one in rows:
    example = EXAMPLE_ROW.match(one[0])
    if example and example.group(1) in relabeled:
        counts[example.group(1)] = counts.get(example.group(1), 0) + 1
        one[0] = re.sub(r"line \d+$", "line %d" % counts[example.group(1)], one[0])
# An entry that wraps onto a line opening on a capital, Dissertation, University or Berez, Jean
# Mulder, runs on the entry above it: every entry opens on a surname and gives its year.
merged = []
for one in rows:
    if one[2] == "reference" and merged and merged[-1][2] == "reference" and \
            not re.match(r"^[A-Z][\w’'\-]+, .{0,70}?\b(?:19|20)\d\d[a-z]?\.", one[3]):
        merged[-1][3] += " " + one[3]
        continue
    merged.append(one)
paper.rows = merged
paper.write()
