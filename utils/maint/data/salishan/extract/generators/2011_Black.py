"""The ops of 2011_Black: Alexis Black's Ostension and definiteness in the Kwak'wala noun phrase.

Every example is read here and not by gen.example(). A sentence sets its IPA in brackets, a phonetic
row, over the line in the orthography and its gloss, the pair set again where the sentence wraps,
over the translation; the data from Boas and from Littell set no IPA. A tag in parentheses at the
right of a tier, (VF), (VG) or a source, is a citation of its own, and the consultant's comments
under an example are speaker comments. Each caption and context over an example is given its
printed line count. The English examples, (12), (13), (19) to (21), (23) and (25), are notes, a
sentence each, and the test contexts under (26) a note to each heading and passage. The schema of
(7), the charts of (8) and (32), the structure in (44) and the Appendix's table are read cell by
cell from the renders, each checked against the printed lines letter for letter. Their
subscripts, the DP of (7) and the Appendix and the KP and DP of (44), are set on the line as the
text layer has them, and a note says so.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Kwak’wala"
AUTHORS = ["Alexis Black"]
paper = gen.Paper("2011_Black", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("RCD", "the author's consultant, thanked in the note on the title"),
         ("Henry Davis", "thanked for feedback, and cited by personal communication"),
         ("Lisa Matthewson", "thanked for feedback"), ("Molly Babel", "thanked for feedback"),
         ("Boas", "Franz Boas (1900, 1903, 1911, 1947), the grammars and texts the paper compares with"),
         ("George Hunt", "Boas's collaborator, whose dialect the grammars record"),
         ("Nicolson", "Marianne Nicolson (2009, with Adam Werle), the modern determiner systems"),
         ("Werle", "Adam Werle (2009, with Marianne Nicolson), the modern determiner systems"),
         ("Anderson", "Stephen R. Anderson (1984, 2003, 2005), Kwakwala syntax and clitics"),
         ("Bach", "Emmon Bach (2006), deixis in Northern Wakashan"),
         ("Chung", "Yunhee Chung (2007), the Kwak’wala nominal domain"),
         ("Littell", "Patrick Littell (2010), the equative predicate structure"),
         ("Anonby", "Stan J. Anonby (1999), the dialects and language shift"),
         ("Berman", "Judith Berman (1982, 1983, 1994), Hunt and the Kwak’wala texts"),
         ("Irene Heim", "Heim (1982, 1991), definiteness as familiarity"),
         ("Matthewson", "Lisa Matthewson (1998), assertion of existence in Salish determiners"),
         ("Gillon", "Carrie Gillon (2006, 2009), domain restriction and maximality"),
         ("Gerner", "Matthias Gerner (2009), the deictic features of demonstratives"),
         ("Ludlow and Neale", "Peter Ludlow and Stephen Neale (1991), specificity"),
         ("Russell", "Bertrand Russell (1905), uniqueness asserted"),
         ("Enç", "Murvet Enç (1991), specificity"),
         ("Kadmon", "Nirit Kadmon (1992, 2001), unique reference"),
         ("Hawkins", "John A. Hawkins (1978, 1991), definiteness and indefiniteness"),
         ("Abbott", "Barbara Abbott (1999), a unique theory of definiteness"),
         ("Lyons", "Christopher Lyons (1999), definiteness"),
         ("Frege", "Gottlob Frege (1892), uniqueness presupposed"),
         ("Diessel", "Holger Diessel, demonstratives, the pointing gesture"),
         ("Stalnaker", "Robert C. Stalnaker, the common ground"),
         ("Lewis", "David Lewis (1979), accommodation"),
         ("Link", "Godehard Link (1983), the supremum"),
         ("Brisson", "Christine Brisson (1998), maximality"),
         ("Donnellan", "Keith Donnellan (1966), definite descriptions"),
         ("Partee", "Barbara Partee (1972), specific indefinites"),
         ("Prince", "Ellen F. Prince (1992), hearer-old and hearer-new"),
         ("Mount", "Mount (2008), mutually-recognized salience"),
         ("Longobardi", "Longobardi (2001), the diagnostics for determinerhood"),
         ("Fillmore", "Charles J. Fillmore (1966), deixis"),
         ("Jamieson-McLarnon", "Susan Jamieson-McLarnon (2005), revitalization")]
LANGUAGES = [(LANGUAGE, "Northern Wakashan"), ("Wakashan", "the family"), ("English", "the translations"),
             ("Kwakiutl", "the dialect of Boas's grammars"),
             ("Gwaỷi", "the Kingcome Inlet dialect of Kwak’wala, the author's data"),
             ("Haisla", "Northern Wakashan"), ("Sḵwx̱wú7mesh", "Central Salish, Gillon's data"),
             ("Lillooet", "Interior Salish"), ("Salishan", "the determiners Matthewson describes"),
             ("Turkish", "a language that marks specificity on its determiners"),
             ("Lisu", "a Tibeto-Burman language with ostensive demonstratives"),
             ("Miao", "a language with compound anchors")]
HEADINGS = {paper.find(pattern): label for label, pattern in (
    ("1", r"^1 Introduction$"), ("2", r"^2 Language Background$"), ("3", r"^3 The semantics of Definiteness$"),
    ("3.1", r"^3\.1 Familiarity$"), ("3.2", r"^3\.2 Uniqueness$"), ("3.3", r"^3\.3 Specificity$"),
    ("3.4", r"^3\.4 Assertion of existence$"), ("3.5", r"^3\.5 Domain Restriction$"),
    ("4", r"^4 The proposal: ostension$"), ("5", r"^5 The syntax of -da$"), ("5.1", r"^5\.1 Argumenthood$"),
    ("5.2", r"^5\.2 Distributional restrictions: DPs vs NPs$"),
    ("5.3", r"^5\.3 Distributional restrictions: agreement$"), ("6", r"^6 Conclusions$"),
    ("Appendix", r"^Appendix$"))}
# Note 4 is set at the body size, under the rule at the foot of page 4.
paper.body_size_notes = [paper.find(r"^4 Typically; some exceptions")]
# The place of Boas 1911 opens its line like an author.
paper.reference_lines_run_on = [paper.find(r"^Washington, Government Printing Office")]
FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
RUNNING = paper.running_numbers_set()

# The printed lines of each caption over an example, and of each context: (35) and (36) run on.
CAPTIONS = {"2": 1, "3": 1, "6": 1, "9": 1, "10": 1, "11": 1, "17": 1, "18": 1, "28": 1, "29": 1, "37": 1,
            "38": 1, "39": 1}
CONTEXTS = {"34": 1, "35": 2, "36": 3}
# The printed lines of each English example, its number's line among them.
ENGLISH_LINES = {"12": 3, "13": 5, "19": 2, "20": 4}
# A caption or context over a lettered part, by its printed lines.
PART_CAPTIONS = {"6a": 1, "6b": 1}
PART_CONTEXTS = {"33a": 1, "33b": 2}
TAG = r"\((?:VF|VG|VF, VG)\)|\(Littell, pg\. \d+\)|\(Boas \d{4}:\d+\)\.?"
TAGGED = re.compile(r"^(.*?\S)\s*(%s)$" % TAG)
TRANSLATED = re.compile(r"’(?:\s*(?:%s))?$" % TAG)
UNREAD = []


def printed(number):
    return bool(paper.text(number).strip()) and not paper.lines[number][2] and number not in RUNNING \
        and number not in FOOT


def after(number):
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def columns(number):
    """The columns of a printed line, split at the gaps the page sets between them."""
    return re.split(r"\s{2,}", paper.spaced[number].strip())


def same_letters(cells, first, last):
    """Check that the cells written from lines first to last hold the printed letters, all of them
    and no others, whatever the order the columns set them in."""
    written = sorted("".join(cells).replace(" ", ""))
    source = sorted("".join(paper.text(one) for one in range(first, last + 1) if printed(one)).replace(" ", ""))
    assert written == source, (first, last, "".join(written), "".join(source))


def example_lines():
    """The line each example opens on: its number in sequence, and not a line of prose that opens
    on a reference, (40) is the fact."""
    found, expected = [], 1
    for number in range(1, paper.last + 1):
        opened = gen.EXAMPLE.match(paper.text(number))
        if printed(number) and opened and opened.group(1) == str(expected):
            found.append(number)
            expected += 1
    assert expected == 45, expected
    return found


EXAMPLES = example_lines()


def is_prose(number):
    """A line of the paragraph after an example: six words or more and no gap between columns."""
    text = paper.text(number)
    return len(text.split()) >= 6 and not re.search(r"\S\s{2,}\S", paper.spaced[number].strip()) and \
        not text.startswith(("‘", "[", "*", "#", "Consultant’s comment:"))


def example(first, where, resumed=None):
    """A numbered example, from the line it opens on to the first line after it that is none of
    its own; returns that line. A part set after the prose that follows its example, (14c), is
    read as resumed, the example's label and the letter before its own."""
    label, rest = gen.EXAMPLE.match(paper.text(first)).groups() if not resumed else (resumed[0], None)
    counts = {}
    state = {"label": label, "letter": resumed[1] if resumed else "`"}
    items, number = [(first, paper.text(first) if resumed else rest)], after(first)
    while number <= paper.last and number not in EXAMPLES:
        items.append((number, paper.text(number)))
        number = after(number)
    ends = number

    def row(who, kind, text, at, gloss=None):
        counts[state["label"]] = counts.get(state["label"], 0) + 1
        paper.add("(%s) line %d" % (state["label"], counts[state["label"]]), who, kind, text,
                  "page %d%s" % (paper.page(at), ", " + gloss if gloss else ""))

    def tagged(who, kind, text, at, gloss, where_tag):
        """A row with the tag at its right a citation of its own."""
        split = TAGGED.match(text)
        row(who, kind, split.group(1) if split else text, at, gloss)
        if split:
            row(A, "citation", split.group(2), at, "the tag at the right of %s" % where_tag)

    def letter(text):
        """The lettered part a line opens, the letter the next in turn."""
        expected = chr(ord(state["letter"]) + 1)
        return re.match(r"^(%s)\.\s+(.*)$" % expected, text)

    def take(start, count, text, gloss):
        """A caption or context of count printed lines, the first holding text."""
        parts = [text] + [one for _, one in items[start + 1:start + count]]
        row(A, "note", " ".join(parts), items[start][0], gloss)
        return start + count

    index = 0
    if label in CAPTIONS:
        index = take(0, CAPTIONS[label], rest, "over the example")
    elif label in CONTEXTS:
        index = take(0, CONTEXTS[label], rest, "the context over the example")
    mode = "start"
    while index < len(items):
        at, text = items[index]
        part = letter(text)
        if part:
            state["letter"] = part.group(1)
            state["label"] = label + part.group(1)
            text = part.group(2)
            mode = "start"
            if state["label"] in PART_CAPTIONS:
                index = take(index, PART_CAPTIONS[state["label"]], text, "over the part")
                continue
            if state["label"] in PART_CONTEXTS:
                index = take(index, PART_CONTEXTS[state["label"]], text, "the context over the part")
                continue
        if mode == "start":
            if mode == "start" and is_prose(at) and not part:
                return at
            # The IPA in brackets, run on to its closing bracket.
            if text.startswith("["):
                parts = [text]
                while "".join(parts).count("[") > "".join(parts).count("]"):
                    index += 1
                    parts.append(items[index][1])
                tagged(L, "phonetic", " ".join(parts), at, None, "the IPA")
                index += 1
                if index >= len(items):
                    return ends
                at, text = items[index]
            # The line and its gloss in turn, to a translation, a comment, the next part or prose.
            count = 0
            while True:
                tagged(L, ("transcription", "gloss")[count % 2], text, at, None, "the line")
                count += 1
                index += 1
                if index >= len(items):
                    return ends
                at, text = items[index]
                if text.startswith(("‘", "Consultant’s comment:")) or letter(text) or is_prose(at):
                    break
            assert count % 2 == 0 and count <= 10, (label, count)
            # Each translation, run on to its closing quote; (28) gives two.
            while text.startswith("‘"):
                parts = [text]
                while not TRANSLATED.search(parts[-1]):
                    index += 1
                    parts.append(items[index][1])
                tagged(A, "translation", " ".join(parts), at, None, "the translation")
                index += 1
                if index >= len(items):
                    return ends
                at, text = items[index]
            mode = "after"
            continue
        # After the tiers: the consultant's comments, each run on to its close.
        if text.startswith("Consultant’s comment:"):
            parts = [text]
            while not re.search(r"[.?!”’)]$", parts[-1]):
                index += 1
                parts.append(items[index][1])
            tagged(A, "speaker comment", " ".join(parts), at, "the consultant's comment", "the comment")
            index += 1
            continue
        UNREAD.append((label, at, text))
        return at
    return ends


def english(first, where):
    """(12), (13), (19), (20) and (23): English sentences, a note each, under a context where one is
    set; (13) sets a speaker's label over the parts, and (19) each sentence's reading right of an
    arrow."""
    label, rest = gen.EXAMPLE.match(paper.text(first)).groups()
    page = paper.page(first)
    lines = [(first, rest)]
    number = after(first)
    while len(lines) < ENGLISH_LINES[label]:
        lines.append((number, paper.text(number)))
        number = after(number)
    heard = {}

    def row(name, kind, text, gloss):
        heard[name] = heard.get(name, 0) + 1
        paper.add("(%s) line %d" % (name, heard[name]), A, kind, text, "page %d, %s" % (page, gloss))

    index = 0
    while index < len(lines):
        at, text = lines[index]
        part = re.match(r"^([a-z])\.\s+(.*)$", text)
        name = label + part.group(1) if part else label
        text = part.group(2) if part else text
        if text.startswith("Context:"):
            # A context runs on to its stop, (20) lie / on the table, or to a speaker's line, (13).
            while not text.endswith(".") and index + 1 < len(lines) and \
                    not re.match(r"^(?:[a-z]\. |\w+:)", lines[index + 1][1]):
                index += 1
                text += " " + lines[index][1]
            row(name, "note", text, "the context over the example")
        elif text.endswith(":"):
            row(name, "note", text, "the speaker's label over the parts under it")
        elif " → " in text:
            sentence, reading = text.split(" → ")
            row(name, "note", sentence, "an English sentence" + (", * marking it ill-formed"
                                                                  if sentence.startswith("*") else ""))
            row(name, "note", reading, "the sentence's reading, right of an arrow")
        else:
            while index + 1 < len(lines) and not re.match(r"^[a-z]\. ", lines[index + 1][1]) and \
                    not text.endswith((".", "?")):
                index += 1
                text += " " + lines[index][1]
            row(name, "note", text, "an English sentence" + (", * marking it ill-formed" if "*" in text[:20] else ""))
        index += 1
    return number


def twenty_one(first, where):
    """(21): Gillon's denotation of the, then a. the sentence beside its domain and under them the
    denotation it yields. The subscripts, the pencil of C and i and ii of pencil, are set on the
    line."""
    page = paper.page(first)
    formula, sentence, yields = first, after(first), after(after(first))
    subscripts = "its subscripts set on the line as the text layer has them"
    paper.add("(21) line 1", A, "note", gen.EXAMPLE.match(paper.text(formula)).group(2),
              "page %d, the denotation of the, a formula" % page)
    said, domain = columns(sentence)
    said = re.sub(r"^a\.\s+", "", said)
    paper.add("(21a) line 1", A, "note", said, "page %d, an English sentence, * marking it ill-formed" % page)
    paper.add("(21a) line 2", A, "note", domain, "page %d, the domain right of the sentence, %s" % (page, subscripts))
    paper.add("(21a) line 3", A, "note", paper.text(yields), "page %d, the denotation the sentence yields, a formula" % page)
    same_letters(["(21)", gen.EXAMPLE.match(paper.text(formula)).group(2), "a.", said, domain, paper.text(yields)],
                 first, yields)
    return after(yields)


def twenty_three(first, where):
    """(23): an English passage over two lines."""
    last = after(first)
    paper.add("(23)", A, "note", gen.EXAMPLE.match(paper.text(first)).group(2) + " " + paper.text(last),
              "page %d, an English passage, the test context" % paper.page(first))
    return after(last)


def twenty_five(first, where):
    """(25): Ludlow and Neale's three terms, each beside its definition, and the source under them."""
    page = paper.page(first)
    paper.add("(25)", A, "note", gen.EXAMPLE.match(paper.text(first)).group(2), "page %d, over the example" % page)
    number = after(first)
    while not paper.text(number).startswith("("):
        term, meaning = columns(number)
        part, term = re.match(r"^([a-c])\.\s+(.*)$", term).groups()
        number = after(number)
        while not re.match(r"^(?:[a-c]\.\s|\()", paper.text(number)):
            meaning += " " + paper.text(number)
            number = after(number)
        paper.add("(25%s) line 1" % part, A, "note", term, "page %d, the term defined" % page)
        paper.add("(25%s) line 2" % part, A, "note", meaning, "page %d, the term's definition" % page)
    paper.add("(25)", A, "citation", paper.text(number), "page %d, the source under the definitions" % page)
    return after(number)


def test_contexts(first, where):
    """The test contexts under (26): each lettered heading, run on to its close, over its passage."""
    page = paper.page(first)
    paper.add("(26)", A, "note", paper.text(first), "page %d, over the test contexts" % page)
    ends = paper.find(r"^These examples demonstrate the target NP", first)
    number = after(first)
    while number < ends:
        part, heading = re.match(r"^([a-d])\.\s+(.*)$", paper.text(number)).groups()
        number = after(number)
        while not re.search(r"\)\d*$", heading):
            heading += " " + paper.text(number)
            number = after(number)
        passage = []
        while number < ends and not re.match(r"^[a-d]\.\s", paper.text(number)):
            passage.append(number)
            number = after(number)
        paper.add("(26%s) line 1" % part, A, "note", heading, "page %d, the test context's heading" % page)
        paper.add("(26%s) line 2" % part, A, "note", paper.joined(passage),
                  "page %d, the test context, an English passage" % page)
    return ends


def seven(first, where):
    """(7): the schema of the noun phrase, its DP a subscript set on the line, over the braces'
    labels, then an example of it: the line, its gloss and the translation."""
    page = paper.page(first)
    lines = [first]
    while len(lines) < 5:
        lines.append(after(lines[-1]))
    schema, braces, said, gloss, translation = lines
    subscript = "the DP a subscript, set on the line as the text layer has it"
    paper.add("(7) line 1", A, "note", gen.EXAMPLE.match(paper.text(schema)).group(2),
              "page %d, the schema of the noun phrase, %s" % (page, subscript))
    under = columns(braces)
    assert under == ["Prenominal", "Postnominal"], under
    paper.add("(7) line 2", A, "note", under[0], "page %d, the label under the brace over Case = LOC = DEF" % page)
    paper.add("(7) line 3", A, "note", under[1], "page %d, the label under the brace over =Temp=VIS" % page)
    paper.add("(7) line 4", L, "transcription", paper.text(said), "page %d, %s" % (page, subscript))
    paper.add("(7) line 5", L, "gloss", paper.text(gloss), "page %d, %s" % (page, subscript))
    paper.add("(7) line 6", A, "translation", paper.text(translation), "page %d" % page)
    return after(translation)


def forms(where, cell, page, gloss):
    """A chart cell: each form a transcription, forms set together a row each, and a label in
    parentheses beside a form a note."""
    split = re.match(r"^(\S+) (\(.*\))$", cell)
    form, label = split.groups() if split else (cell, None)
    for one in form.split(", "):
        paper.add(where, L, "transcription", one, "page %d, %s%s" % (
            page, gloss, ", one of the forms set together, %s" % form if ", " in form else ""))
    if label:
        paper.add(where, A, "note", label, "page %d, %s, beside %s" % (page, gloss, form))


def chart(where, first, last, letter, title, spans, heads, rows, merged):
    """One of (8)'s charts, read from the render: its title, the heads spanning its columns, the
    column heads, each row's head and the cells it holds alone, and each cell rows share."""
    page = paper.page(first)
    paper.add(where, A, "note", title, "page %d, the chart's title" % page)
    for label, over in spans:
        paper.add(where, A, "note", label, "page %d, a head spanning %s" % (page, over))
    for head in heads:
        paper.add(where, A, "note", head, "page %d, a column's head" % page)
    for name, cells in rows:
        paper.add(where, A, "note", name, "page %d, the row's head" % page)
        for head, cell in zip(heads[1:], cells):
            if cell:
                forms(where, cell, page, "%s, under %s" % (name, head))
    for head, cells, shared in merged:
        for cell in cells:
            forms(where, cell, page, "under %s, the cell %s share" % (head, shared))
    same_letters([letter, title] + [label for label, _ in spans] + list(heads) +
                 [one for name, cells in rows for one in (name,) + cells] +
                 [one for _, cells, _ in merged for one in cells], first, last)


def eight(first, where):
    """(8): the determiners as Boas gives them, a., and as the Gwaỷi dialect has them, b., two
    charts. A cell spanning rows is written once, with the rows it spans; an empty cell is not."""
    page = paper.page(first)
    paper.add("(8)", A, "note", gen.EXAMPLE.match(paper.text(first)).group(2), "page %d, over the charts" % page)
    opens = after(first)
    assert paper.text(opens) == "a.", paper.text(opens)
    last = paper.find(r"^3-inv ", opens)
    rows = (("1-vis", ("", "", "-k")), ("1-inv", ("", "", "-gaʔ")), ("2-vis", ("", "", "-iχ")),
            ("2-inv", ("", "", "-aq’/aχ")), ("3-vis", ("", "", "-∅/-i")), ("3-inv", ("", "", "-a/-i")))
    merged = (("LOC", ("-gʲa",), "1-vis and 1-inv"), ("LOC", ("-oχ",), "2-vis and 2-inv"),
              ("LOC", ("-i (+Subj)", "-∅ (-Subj)"), "3-vis and 3-inv"),
              ("DEF", ("-(d)a (Def)", "-∅ (Indef)"), "every row"))
    chart("(8a)", opens, last, "a.", "Kwakiutl dialect (Boas 1947)",
          (("Prenominal", "Anchor, LOC and DEF"), ("Postnominal", "VIS")), ("Anchor", "LOC", "DEF", "VIS"),
          rows, merged)
    second = paper.find(r"^b\. Gwaỷi dialect \(2010\)$", last)
    last = paper.find(r"^4- ", second)
    rows = (("1-vis", ("", "", "-x")), ("1-inv", ("", "", "")), ("2-vis", ("", "", "-iχ, -ɛχ, -χ")),
            ("2-inv", ("", "", "")), ("3-vis", ("", "", "")), ("3-inv", ("", "", "-a/-ɛʔ")),
            ("4-", ("-a (‘-Subj’)", "", "")))
    merged = (("LOC", ("-gʲa",), "1-vis and 1-inv"), ("LOC", ("-oχ",), "2-vis and 2-inv"),
              ("LOC", ("-i",), "3-vis and 3-inv"), ("DEM", ("-da (+Dem)",), "every row"))
    chart("(8b)", second, last, "b.", "Gwaỷi dialect (2010)",
          (("Prenominal", "Distance, LOC and DEM"), ("Postnominal", "VIS")), ("Distance", "LOC", "DEM", "VIS"),
          rows, merged)
    return after(last)


def thirty_two(first, where):
    """(32): the enclitic pronouns as Boas gives them, a., and as the Gway’i dialect has them, b.,
    two tables side by side. Each sets the pronominal's cases over their forms, a brace from them
    down to the rows, and under it the forms by anchor or distance. (32b) sets its 1 on one row and
    leaves the row under it empty. The ordinal 3rd is set flat, as the text layer has it."""
    page = paper.page(first)
    paper.add("(32)", A, "note", gen.EXAMPLE.match(paper.text(first)).group(2), "page %d, over the tables" % page)
    last = paper.find(r"^3-inv ", first)
    sides = (("a", "Kwakiutl (1947)", "Demonstrative 3rd person Pronominal", "Anchor",
              (("1-vis", "-kʲ"), ("1-inv", "-gʲaʔ"), ("2-vis", "-oχ"), ("2-inv", "-oʔ"), ("3-vis", "-iq"),
               ("3-inv", "-iʔ"))),
             ("b", "Gway’i (2010)", "Demonstrative 3rd person Pronominal", "Distance",
              (("1", "-gʲa"), ("2-vis", "-oχ"), ("2-inv", "-oʔ"), ("3-vis", "-i"), ("3-inv", "-ɛʔ"))))
    cells = []
    for part, title, head, first_head, rows in sides:
        where = "(32%s)" % part
        side = "on the left" if part == "a" else "on the right"
        paper.add(where, A, "note", title, "page %d, %s, the table's title" % (page, side))
        paper.add(where, A, "note", head, "page %d, %s, the head over the cases" % (page, side))
        for case, form in zip(("NOM", "ACC", "OBL"), ("-∅+", "-χ+", "-s+")):
            paper.add(where, A, "note", case, "page %d, %s, a column's head" % (page, side))
            paper.add(where, L, "transcription", form, "page %d, %s, under %s, a brace from it down to the forms "
                      "under LOC+VIS" % (page, side, case))
        for column in (first_head, "LOC+VIS"):
            paper.add(where, A, "note", column, "page %d, %s, a column's head" % (page, side))
        for name, form in rows:
            paper.add(where, A, "note", name, "page %d, %s, the row's head" % (page, side))
            paper.add(where, L, "transcription", form, "page %d, %s, %s, under LOC+VIS" % (page, side, name))
        cells += ["%s." % part, title, head, "NOM", "ACC", "OBL", "-∅+", "-χ+", "-s+", first_head, "LOC+VIS"] + \
            [one for pair in rows for one in pair]
    same_letters(cells, after(first), last)
    return after(last)


def forty_four(first, where):
    """(44): the structure of the noun phrase, KP and DP subscripts set on the line."""
    paper.add("(44)", A, "note", gen.EXAMPLE.match(paper.text(first)).group(2),
              "page %d, the structure of the noun phrase, KP and DP subscripts set on the line as the text layer "
              "has them" % paper.page(first))
    return after(first)


def appendix_table(first, where):
    """The Appendix's table: the LOC categories down the side, the cases across the top, and in each
    cell the determiner string, DP a subscript set on the line. The ordinals are set flat."""
    page = paper.page(first)
    last = paper.find(r"^3rd/\(4th\) ", first)
    heads = ("LOC Category", "Nominative", "Accusative", "Oblique")
    rows = []
    for number in range(after(after(first)), last + 1):
        if printed(number):
            cells = columns(number)
            if len(cells) == 3:
                # The page sets no gap between the Accusative and Oblique cells of the first two rows.
                cells = cells[:2] + list(re.match(r"^(.*\. \. \.) (Word.*)$", cells[2]).groups())
            rows.append(cells)
    same_letters(list(heads) + [one for cells in rows for one in cells], first, last)
    for head in heads:
        paper.add(where, A, "note", head, "page %d, a column's head of the table" % page)
    for name, *cells in rows:
        paper.add(where, A, "note", name, "page %d, the row's head" % page)
        for head, cell in zip(heads[1:], cells):
            paper.add(where, A, "note", cell, "page %d, %s, under %s, DP a subscript set on the line as the text "
                      "layer has it" % (page, name, head))
    return after(last)


BLOCKS = {number: example for number in EXAMPLES}
BLOCKS.update({EXAMPLES[6]: seven, EXAMPLES[7]: eight, EXAMPLES[11]: english, EXAMPLES[12]: english,
               EXAMPLES[18]: english, EXAMPLES[19]: english, EXAMPLES[20]: twenty_one,
               EXAMPLES[22]: twenty_three, EXAMPLES[24]: twenty_five, EXAMPLES[31]: thirty_two,
               EXAMPLES[43]: forty_four, paper.find(r"^Test contexts:$"): test_contexts,
               paper.find(r"^c\. \[dúχʷaλɛn"): lambda first, where: example(first, where, ("14", "b")),
               paper.find(r"^LOC Nominative Accusative Oblique$"): appendix_table})
# Note 1 is on the title, its mark a digit.
found = paper.standard(AUTHORS, NAMES, LANGUAGES, headings=HEADINGS, blocks=BLOCKS, notes_title=("1",))
# An example ends on the first line that is none of its own. Every such line here opens prose.
assert all(re.match(r"^[A-Z][a-z]*,? ", text) for _, _, text in UNREAD), UNREAD
paper.write()
