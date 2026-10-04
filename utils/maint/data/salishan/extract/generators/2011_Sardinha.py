"""The ops of 2011_Sardinha: Katie Sardinha's Prepositional his and the development of morphological
case in Northern Wakashan.

Every example is read here and not by gen.example(). A sentence sets three tiers, the line, its
segmentation and its gloss, over the translation; a tag in parentheses at the right of the line,
(Davidson 2002: 198) or (p. 90), is a citation of its own. (8) and (9) set Rath's reading under the
tiers where a translation would stand, (10) its translation bare in brackets, and (21) two sets of
tiers, the translation split between them. Each example's rows take the language the prose names
for it. Tables 1 to 5, the structures (S.1) and (S.2), the forms of his and the Appendix's glossing
abbreviations are read cell by cell from the renders, each checked against the printed lines letter
for letter. Table 1's rules are underscores in the text layer and are kept, a note to each.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Kwak’wala"
AUTHORS = ["Katie Sardinha"]
paper = gen.Paper("2011_Sardinha", authors=AUTHORS[0], language=LANGUAGE)
NNC, MAKAH, HEILTSUK, HAISLA = "Nuu-Chah-Nulth", "Makah", "Heiltsuk", "Haisla"
NAMES = [("Ruby Dawson Cranmer", "the author's Kwak’wala consultant, thanked in the note on the title"),
         ("Henry Davis", "the author's supervisor, thanked in the note on the title"),
         ("Davis", "Henry Davis (2010), lexical suffixes and the Mosan hypothesis"),
         ("Fortescue", "Michael Fortescue (2006, 2007), the grammaticalization divide and the Wakashan dictionary"),
         ("Rath", "John C. Rath (1981, 1984), the Heiltsuk dictionary and the word classes of Upper North "
                  "Wakashan"),
         ("Howe", "Darin Howe (2000), Oowekyala segmental phonology"),
         ("Jacobsen", "William Jacobsen (2007), the subclassification of Southern Wakashan"),
         ("Nakayama", "Toshihide Nakayama (2001), Nuuchahnulth morphosyntax"),
         ("Sapir", "Edward Sapir (1939, with Morris Swadesh), governing and restrictive suffixes"),
         ("Swadesh", "Morris Swadesh (1939, with Edward Sapir), governing and restrictive suffixes"),
         ("Wojdak", "Rachel Wojdak (2004), the classification of Wakashan lexical suffixes"),
         ("Davidson", "Matthew Davidson (2002), Southern Wakashan grammar, the source of the Nuu-Chah-Nulth "
                      "and Makah examples"),
         ("Klokeid", "Terry Klokeid (1978), encliticization in Nitinaht"),
         ("Stonham", "John Stonham (2004), Nuuchahnulth word formation"),
         ("Woo", "Florence Woo (2004, 2007), Nuu-chah-nulth locatives and the object marker"),
         ("Boas", "Franz Boas (1947), the Kwakiutl grammar"),
         ("Anderson", "Stephen R. Anderson (1984), Kwak’wala syntax"),
         ("Lincoln", "Neville J. Lincoln (1980, 1986, with John C. Rath), the North Wakashan root list and "
                     "the Haisla dictionary"),
         ("Windsor", "Evelyn Windsor (1986, 1989), the Haisla story and the Bella Bella texts"),
         ("Levine", "Robert Levine (1980), the lexical origin of the Kwak’wala passive"),
         ("Bach", "Emmon Bach (1970, 2010, with Dora and Rose Robinson), the Haisla lessons"),
         ("Robinson", "Dora Robinson and Rose Robinson (2010), the Haisla lessons"),
         ("Gordon Robertson", "the Kitlope elder who told the Haisla story")]
LANGUAGES = [(LANGUAGE, "Northern Wakashan"), ("Wakashan", "the family"), ("English", "the translations"),
             (HAISLA, "Upper Northern Wakashan"), (HEILTSUK, "Upper Northern Wakashan"),
             ("Oowekyala", "Upper Northern Wakashan"), (NNC, "Southern Wakashan"), ("Ditidaht", "Southern Wakashan"),
             (MAKAH, "Southern Wakashan"), ("Proto-Northern Wakashan", "the reconstructed ancestor of the branch"),
             ("Proto-Wakashan", "the reconstructed ancestor of the family")]
HEADINGS = {paper.find(pattern): label for label, pattern in (
    ("1", r"^1 Introduction$"), ("2", r"^2 The Wakashan language family$"),
    ("3", r"^3 Prepositions and the development of case-marking in Upper$"),
    ("3.1", r"^3\.1 Prepositions: form and meaning$"), ("3.2", r"^3\.2 Intermediate case-marking"),
    ("3.3", r"^3\.3 The development of case-marking in Kwak’wala$"),
    ("4", r"^4 Northern Wakashan prepositions: verbal and demonstrative$"), ("4.1", r"^4\.1 Prepositional la$"),
    ("4.2", r"^4\.2 Prepositional q-$"), ("4.3", r"^4\.3 Prepositional ga$"), ("4.4", r"^4\.4 Prepositional his$"),
    ("5", r"^5 Conclusion$"), ("Appendix I", r"^Appendix I: Glossing$"))}
assert None not in HEADINGS, HEADINGS
# Rath 1981's place opens its line like an author.
paper.reference_lines_run_on = [paper.find(r"^Ottawa, ON: National Museums of Canada\.$", paper.find(r"^Rath, John C\. 1981"))]
FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
RUNNING = paper.running_numbers_set()

# The language of each example, as the prose names it.
EXAMPLE_LANGUAGES = {"1": NNC, "2": MAKAH, "3": MAKAH, "4": NNC, "8": HEILTSUK, "9": HEILTSUK, "10": HAISLA,
                     "11": HEILTSUK, "12": HEILTSUK, "13": HEILTSUK, "27": NNC, "28": NNC, "29": NNC}
# The caption over (10) and (11), a line each.
CAPTIONS = {"10", "11"}
TAG = r"\((?:(?:Davidson|Rath|Anderson) \d{4}: \d+|p\. \d+|ibid\.|Lincoln, Rath, & Windsor 1986: 21-2, line 89)\)"
TAGGED = re.compile(r"^(.*?\S)\s*(%s)$" % TAG)


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
    and no others, whatever the order the columns set them in."""
    written = sorted("".join(cells).replace(" ", ""))
    source = sorted("".join(paper.text(one) for one in range(first, last + 1) if printed(one)).replace(" ", ""))
    assert written == source, (first, last, "".join(written), "".join(source))


def example_lines():
    """The line each example opens on: its number in sequence, set a gap before what follows it."""
    found, expected = [], 1
    for number in range(1, paper.last + 1):
        opened = gen.EXAMPLE.match(paper.text(number))
        if printed(number) and opened and opened.group(1) == str(expected) and \
                re.match(r"^\(\d+\)\s{2,}", paper.spaced[number]):
            found.append(number)
            expected += 1
    assert expected == 30, expected
    return found


EXAMPLES = example_lines()


def example(first, where):
    """A numbered example: its caption, where it sets one, then three tiers over the translation, or
    over Rath's reading in (8) and (9); (21) sets the tiers twice. Returns the line after it."""
    label, rest = gen.EXAMPLE.match(paper.text(first)).groups()
    language = EXAMPLE_LANGUAGES.get(label, L)
    page = paper.page(first)
    count = [0]

    def row(who, kind, text, gloss=None):
        count[0] += 1
        paper.add("(%s) line %d" % (label, count[0]), who, kind, text, "page %d%s" % (page, ", " + gloss if gloss else ""))

    def tagged(who, kind, text, gloss, where_tag):
        split = TAGGED.match(text)
        row(who, kind, split.group(1) if split else text, gloss)
        if split:
            row(A, "citation", split.group(2), "the tag at the right of %s" % where_tag)

    number = first
    if label in CAPTIONS:
        tagged(A, "note", rest, "over the example", "the caption")
        number = after(first)
        rest = paper.text(number)
    parts = 2 if label == "21" else 1
    for part in range(parts):
        for kind in ("transcription", "segmentation", "gloss"):
            tagged(language, kind, rest, None, "the line")
            number = after(number)
            rest = paper.text(number)
        if rest.startswith("With the following meaning:"):
            row(A, "note", rest, "Rath's reading, under the tiers")
        elif label == "10":
            row(A, "translation", rest, "printed without quotes")
        else:
            assert rest.startswith(("‘", "“", "…")), (label, rest)
            row(A, "translation", rest, None if parts == 1 else
                ("the translation's first part, over the second set of tiers", "the translation's second part")[part])
        number = after(number)
        rest = paper.text(number)
    return number


def heading_wrap(start, where):
    """A heading that wraps onto a second line: the line runs onto the heading's row."""
    assert paper.rows[-1][2] == "heading", paper.rows[-1]
    paper.rows[-1][3] += " " + paper.text(start)
    return after(start)


def table_1(first, where):
    """Table 1: each language or reconstruction beside its prepositions, la-, q- and his- or ga-, the
    rules of the fourth and fifth columns underscores in the text layer. The caption is set under it."""
    page = paper.page(first)
    caption = paper.find(r"^Table 1: ", first)
    rows = (("*Proto-Northern Wakashan", "Proto-Northern Wakashan", ("*la-", "*q-", "*his-", "____")),
            ("Haisla", HAISLA, ("la-", "q-", "his-", "____")), ("Heiltsuk", HEILTSUK, ("la-", "q-", "yis-", "____")),
            ("Oowekyala", "Oowekyala", ("la-", "q-", "yis-", "____")),
            ("Kwak’wala4", LANGUAGE, ("la-", "q-", "____", "ga-")))
    same_letters([paper.text(caption)] + [one for name, _, cells in rows for one in (name,) + cells], first, caption)
    paper.add("Table 1", A, "note", paper.text(caption), "page %d, the table's caption, under it" % page)
    for name, language, cells in rows:
        paper.add("Table 1", A, "note", name, "page %d, the row's head%s" % (
            page, ", carrying footnote 4" if name.endswith("4") else ""))
        for column, cell in enumerate(cells, 2):
            if cell == "____":
                paper.add("Table 1", A, "note", cell, "page %d, %s, a rule in column %d" % (page, name, column))
            else:
                paper.add("Table 1", language, "transcription", cell, "page %d, %s, column %d" % (page, name, column))
    return after(caption)


def table_2(first, where):
    """Table 2: each preposition beside its basic meanings. The caption is set under it."""
    page = paper.page(first)
    caption = paper.find(r"^Table 2: ", first)
    rows = (("la-", "‘at x’, ‘to x’, ‘in x’, ‘on x’, ‘towards x’"), ("q-", "‘for the benefit of x’, ‘concerning x’"),
            ("his-/yis-", "‘with x’, ‘by x’, ‘of x’"), ("ga-", "‘to (the speaker), towards (the speaker)’"))
    same_letters([paper.text(caption)] + [one for pair in rows for one in pair], first, caption)
    paper.add("Table 2", A, "note", paper.text(caption), "page %d, the table's caption, under it" % page)
    for form, meaning in rows:
        paper.add("Table 2", "Northern Wakashan", "transcription", form, "page %d, the row's head" % page)
        paper.add("Table 2", A, "translation", meaning, "page %d, beside %s" % (page, form))
    return after(caption)


def structures(first, where):
    """(S.1) and (S.2): each structure in brackets beside two examples of it, e.g. a Heiltsuk phrase
    and its meaning; (S.2) sets (vis) after each meaning."""
    page = paper.page(first)
    last = paper.find(r"^e\.g\. q=ənn’i", first)
    sets = (("S.1", "[ PREP + NP ]", (("la uxʷƛiasaχi", "‘on the roof’", None), ("qən him’asaχi", "‘for the chief’", None))),
            ("S.2", "[ √PREP + person enclitic ]", (("la=χi", "‘to him/her/it/they’", "(vis)"),
                                                    ("q=ənn’i", "‘for him/her/it/they’", "(vis)"))))
    cells = []
    for label, structure, examples in sets:
        where = "(%s)" % label
        cells += [where, structure]
        paper.add(where, A, "note", structure, "page %d, the structure" % page)
        for form, meaning, after_meaning in examples:
            cells += ["e.g.", form, "–", meaning] + ([after_meaning] if after_meaning else [])
            paper.add(where, A, "note", "e.g.", "page %d, before the example" % page)
            paper.add(where, HEILTSUK, "transcription", form, "page %d, an example of the structure" % page)
            paper.add(where, A, "translation", meaning, "page %d, after a dash" % page)
            if after_meaning:
                paper.add(where, A, "note", after_meaning, "page %d, after the meaning" % page)
    same_letters(cells, first, last)
    return after(last)


def table_3(first, where):
    """Table 3: the Heiltsuk personal deictics, the persons down the side and the five sets of
    enclitics across the top, each cell's forms as the render sets them, the alternations in
    parentheses under the form. The row head 3I carries footnote 9."""
    page = paper.page(first)
    last = paper.find(r"^\(absent\)$", first)
    heads = ("Subject Enclitics", "Object Enclitics", "“his” - Enclitics", "“la” - Enclitics", "“q”- Enclitics")
    rows = (("1 sg.", ("=nugʷ(a)", "=ənƛ(a)", "----", "=ənƛ(a)", "=ənńugʷ(a)")),
            ("1 pl. incl.", ("=ənc", "=ənƛənc", "----", "=ənƛənc", "=a’aənc")),
            ("1 pl. excl.", ("=əntkʷ (=əntxʷ)", "=ənƛəntkʷ (=ənƛəntxʷ)", "----", "=ənƛəntkʷ (=ənƛəntxʷ)",
                             "=a’aəntkʷ (=a’aəntxʷ)")),
            ("2 sg./pl.", ("=su (=cu)", "=uƛ(a)", "=us", "=uƛ(a)", "=əncu")),
            ("3I9 sg./pl.", ("=k(ʷ) (=x(ʷ))", "=qk (=qx)", "=sk (=sx)", "=χk (=χx)", "=a’aənk (=a’aənx)")),
            ("3II sg./pl.", ("=k(ʷ)c (=x(ʷ)c)", "=qkc (=qxc)", "=skc (=sxc)", "=χkc (=χxc)", "=a’aənkc (=a’aənxc)")),
            ("3III sg./pl.", ("=uqʷ (=uχʷ) (=u)", "=qʷ", "=sqʷ (=sχʷ)", "=χʷ", "=ənńuqʷ (=ənńuχʷ) (=ənńu)")),
            ("3IV sg./pl.", ("(=uχʷc)", "=qʷc", "(=sχʷc)", "=χcχʷ", "=ənnuχʷc")),
            ("3V sg./pl.", ("=i", "=qi", "=si", "=χi", "=ənńi")),
            ("3VI sg./pl.", ("=ic", "=qic", "=sic", "=χic", "=ənńic")),
            ("3VII (absent)", ("=k(ʷ)i", "=qki", "=ski", "=χki", "=a’aənki")))
    same_letters([paper.text(first)] + list(heads) + [one for name, cells in rows for one in (name,) + cells],
                 first, last)
    paper.add("Table 3", A, "note", paper.text(first), "page %d, the table's caption" % page)
    for head in heads:
        paper.add("Table 3", A, "note", head, "page %d, a column's head" % page)
    for name, cells in rows:
        paper.add("Table 3", A, "note", name, "page %d, the row's head%s" % (
            page, ", carrying footnote 9 after 3I" if name.startswith("3I9") else ""))
        for head, cell in zip(heads, cells):
            if cell == "----":
                paper.add("Table 3", A, "note", cell, "page %d, %s, under %s, the cell empty" % (page, name, head))
            else:
                paper.add("Table 3", HEILTSUK, "transcription", cell, "page %d, %s, under %s" % (page, name, head))
    return after(last)


def table_4(first, where):
    """Table 4: each third-person enclitic as his with the subject enclitic, with its first segments
    struck out, and as it is used today."""
    page = paper.page(first)
    head_end = paper.find(r"^historically\)$", first)
    last = paper.find(r"^hiski ", first)
    caption = [first, after(first)]
    heads = ("Prep + Subject Enclitics (Unattested but hypothesized to be present historically)",
             "Deletion of initial segments of preposition", "“his” Enclitics in use today")
    rows = (("hisk", "h̶i̶ sk", "=sk (=sx)"), ("hiskc", "h̶i̶ skc", "=skc (=sxc)"), ("hisqʷ", "h̶i̶ sqʷ", "=sqʷ (=sχʷ)"),
            ("hisqʷc", "h̶i̶ sqʷc", "=sqʷc (=sχʷc)"), ("hisi", "h̶i̶ si", "=si"), ("hisic", "h̶i̶ sic", "=sic"),
            ("hiski", "h̶i̶ ski", "=ski"))
    assert head_end < last
    same_letters([paper.joined(caption)] + list(heads) + [one for cells in rows for one in cells], first, last)
    paper.add("Table 4", A, "note", paper.joined(caption), "page %d, the table's caption" % page)
    for head in heads:
        paper.add("Table 4", A, "note", head, "page %d, a column's head" % page)
    for cells in rows:
        for head, cell in zip(heads, cells):
            paper.add("Table 4", HEILTSUK, "transcription", cell, "page %d, under %s%s" % (
                page, head.split(" (")[0], ", hi struck through" if "̶" in cell else ""))
    return after(last)


def table_5(first, where):
    """Table 5: the Haisla oblique enclitics, each person beside its enclitics in braces and, in
    the first person, the his phrase they alternate with. The caption is set under it."""
    page = paper.page(first)
    caption = paper.find(r"^Table 5: ", first)
    rows = (("1 sg.", "{ =ńd.s } / his nugʷa"), ("1 pl. (incl.)", "{ =nis, =ńis } / his nugʷanis"),
            ("1 pl. (excl.)", "{ =nikʷ, -ńukʷ } / his nugʷanukʷ"), ("2 sg./pl.", "{ =us }"), ("3I sg./pl.", "{ =sik }"),
            ("3II sg./pl.", "{ =su }"), ("3III sg./pl.", "{ =si }"), ("3(absent)", "{ = sgi }"))
    same_letters([paper.text(caption)] + [one for pair in rows for one in pair], first, caption)
    paper.add("Table 5", A, "note", paper.text(caption), "page %d, the table's caption, under it" % page)
    for name, forms in rows:
        paper.add("Table 5", A, "note", name, "page %d, the row's head" % page)
        paper.add("Table 5", HAISLA, "transcription", forms, "page %d, beside %s" % (page, name))
    return after(caption)


def forms_of_his(first, where):
    """The form of his in each Upper Northern Wakashan language, the language's name before it."""
    page = paper.page(first)
    last = paper.find(r"^Oowekyala: yis$", first)
    rows = ((HAISLA, "his"), (HEILTSUK, "yis"), ("Oowekyala", "yis"))
    same_letters(["%s: %s" % pair for pair in rows], first, last)
    for language, form in rows:
        paper.add(where, A, "note", language + ":", "page %d, before the form" % page)
        paper.add(where, language, "transcription", form, "page %d, the preposition his" % page)
    return after(last)


ABBREVIATION = re.compile(r"^(\S+) (.+?) ((?:[A-Z][A-Z0-9]*|ø|\d[A-Z])(?:\.[A-Z0-9]+)?) (.+)$")


def glossing(first, where):
    """A table of the Appendix's glossing abbreviations, two to a line, each beside its meaning. It
    runs to the first line after it that holds no two."""
    number, last = first, first
    cells = []
    while printed(number) and ABBREVIATION.match(paper.text(number)) and \
            not paper.text(number).startswith(("Glosses", "The following")):
        page = paper.page(number)
        pairs = ABBREVIATION.match(paper.text(number)).groups()
        for abbreviation, meaning, side in ((pairs[0], pairs[1], "on the left"), (pairs[2], pairs[3], "on the right")):
            paper.add(where, A, "note", abbreviation, "page %d, a glossing abbreviation, %s" % (page, side))
            paper.add(where, A, "note", meaning, "page %d, the meaning of %s" % (page, abbreviation))
            cells += [abbreviation, meaning]
        last = number
        number = after(number)
    same_letters(cells, first, last)
    return number


BLOCKS = {number: example for number in EXAMPLES}
BLOCKS.update({paper.find(r"^Northern Wakashan$", paper.find(r"^3 Prepositions")): heading_wrap,
               paper.find(r"^languages$", paper.find(r"^3\.2 ")): heading_wrap,
               paper.find(r"^origins$", paper.find(r"^4 Northern Wakashan prepositions")): heading_wrap,
               paper.find(r"^\*Proto-Northern Wakashan "): table_1, paper.find(r"^la- ‘at x’"): table_2,
               paper.find(r"^\(S\.1\) "): structures, paper.find(r"^Table 3: "): table_3,
               paper.find(r"^Table 4: "): table_4, paper.find(r"^1 sg\. \{"): table_5,
               paper.find(r"^Haisla: his$"): forms_of_his, paper.find(r"^ACC accusative "): glossing,
               paper.find(r"^ART article "): glossing, paper.find(r"^3V 3rd person"): glossing})
assert None not in BLOCKS, BLOCKS
# Note 1 is on the title, its mark a digit. Note 12's mark is set on lala in (25)'s segmentation,
# lala12=oχ, before the clitic.
found = paper.standard(AUTHORS, NAMES, LANGUAGES, headings=HEADINGS, blocks=BLOCKS, notes_title=("1",),
                       appendix=r"^Katie Sardinha$", glued={"12": "lala"})

# Wojdak's 30th sets its th raised over a row of its own, which the entry reads before the line it
# stands over. The ordinal is set flat, as the text layer sets 19th in Rath's.
wojdak = next(row for row in paper.rows if row[2] == "reference" and row[3].startswith("Wojdak, Rachel. 2004."))
assert " Paper th to be presented at the 30 Meeting " in wojdak[3], wojdak
wojdak[3] = wojdak[3].replace(" Paper th to be presented at the 30 Meeting ", " Paper to be presented at the 30th Meeting ")
wojdak[4] += ", the raised th of 30th set flat"
# Note 11's =χ, where =χ is the accusative case marker, on page 16 where the note runs on, is read
# into the paragraph over the note, whose =χ(a) holds it; it is the note's.
clitic = next(row for row in paper.rows if row[:4] == ["§3.3", L, "cited form", "=χ"])
paper.rows.remove(clitic)
clitic[0] = "footnote 11"
note = max(at for at, row in enumerate(paper.rows) if row[0] == "footnote 11")
paper.rows.insert(note + 1, clitic)
tail =paper.find(r"^Katie Sardinha$", paper.find(r"^References$"))
paper.add("end", A, "note", paper.joined([tail, after(tail)]), "page %d, the author's name and e-mail address" % paper.page(tail))
paper.write()
