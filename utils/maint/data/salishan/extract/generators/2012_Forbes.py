"""The ops of 2012_Forbes: Clarissa Forbes's Gitxsan adjectives: evidence from nominal modification.
Attributive -m and -a join any predicate to any other and tell no word class apart, while a
relativized intransitive predicate stands before the noun where it is an adjective and after it
where it is a verb; the prenominal kind takes no WH-word and is no CP relative.

Each example sets the Gitxsan orthography over its APA segmentation and gloss, and the translation
in straight single quotes, which the stock example reader does not take; this generator reads the
examples itself, and footnote() reads the footnotes' (i) and (ii) through it. A starred or
questioned part can stand alone on its line with no tiers. Intended: under a part is its intended
reading; BS: and VG: are the consultants' comments, Barbara Sennott's and Vincent Gogag's. The page
12 table of positions by predicate sets its check marks in Wingdings, mapped by page_text's
PRIVATE_USE. Appendix 1 is the orthography against APA, five pairs a printed row, and Appendix 2
the abbreviations and the three series of pronouns.
"""
import re
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Gitxsan"
AUTHORS = ["Clarissa Forbes"]
paper = gen.Paper("2012_Forbes", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Barbara Sennott", "Gitxsan consultant (BS), of an eastern dialect"),
         ("Vincent Gogag", "Gitxsan consultant (VG)"), ("Hector Hill", "Gitxsan consultant"),
         ("Henry Davis", "thanked; Davis (2002, 2011), Davis and Brown (2011)"),
         ("Bruce Rigsby", "devised the orthography with Lonnie Hindle; Rigsby (1975, 1986)"),
         ("Lonnie Hindle", "devised the orthography with Bruce Rigsby; Hindle and Rigsby (1973)"),
         ("Rigsby", "Bruce Rigsby, Nass-Gitxsan syntax (1975), Gitxsan Grammar (1986)"),
         ("Hunt", "Katharine D. Hunt, clause structure, agreement and case in Gitksan (1993)"),
         ("Davis", "Henry Davis, relative clauses in St'át'imcets (2002) and WH-relatives in Interior Tsimshianic (2011)"),
         ("Brown", "Jason Brown, with Davis (2011)"),
         ("Tarpent", "Marie-Lucie Tarpent, a grammar of the Nisgha language (1987)"),
         ("Swadesh", "Morris Swadesh, Nootka internal syntax (1939)"),
         ("Kinkade", "M. Dale Kinkade, Salish evidence against noun and verb (1983)"),
         ("Jelinek", "Eloise Jelinek, with Demers (1994)"), ("Demers", "Richard A. Demers, with Jelinek (1994)"),
         ("Renker", "Anne M. Renker, noun and verb in Southern Wakashan (1987)"),
         ("Van Eijk", "Jan van Eijk, with Hess (1986)"), ("Hess", "Thom Hess, with Van Eijk (1986)"),
         ("Matthewson", "Lisa Matthewson, with Demirdache (1995)"),
         ("Demirdache", "Hamida Demirdache, with Matthewson (1995)"),
         ("Wojdak", "Rachel Wojdak, Nuuchahnulth modification (2000)"),
         ("Stebbins", "Tonya Stebbins, adjectives in Coast Tsimshian (1996, 2003)"),
         ("Montler", "Timothy Montler, auxiliaries in Straits Salishan (2003)"),
         ("Koch", "Karsten Koch, nominal modification in Nɬeʔkepmxcin (2006)"),
         ("Dixon", "R. M. W. Dixon, with Aikhenvald (2004)"),
         ("Aikhenvald", "Alexandra Aikhenvald, with Dixon (2004)"),
         ("Sauerland", "Uli Sauerland, unpronounced heads in relative clauses (2003)"),
         ("Kayne", "Richard Kayne, The Antisymmetry of Syntax (1994)"),
         ("Thompson", "James Thompson, syntactic nominalization in Halkomelem (2012)"),
         ("Alice", "in (13) and (17)")]
LANGUAGES = [(LANGUAGE, "Tsimshianic, of the British Columbia northern interior"),
             ("Tsimshianic", "the family of Gitxsan"), ("Interior Tsimshianic", "Gitxsan and Nisgha"),
             ("Coast Tsimshian", "a closed class of adjectival particles, Stebbins (1996)"),
             ("Nisgha", "Tarpent (1987)"), ("Salish", "the word class literature"),
             ("Wakashan", "the word class literature"), ("English", "adjectives as English speakers take them")]
FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
SKIP = FOOT | set(paper.volume_header()) | paper.running_numbers()
RUNNING = paper.running_numbers_set()
CONSULTANTS = {"BS": "Barbara Sennott", "VG": "Vincent Gogag"}
OPEN = re.compile(r"^\((\d+|[ivx]+)\)\s*(.*)$")
LETTER = re.compile(r"^([a-d])\.\s*(.*)$")
# A translation in straight quotes, the footnote mark on its closing quote and a parenthesis at its
# right, 'He's the only man at the party.'6 and (Lit: 'The fast rabbit didn't win.').
TRANSLATED = re.compile(r"^('.*?')(\d{1,2})?(?:\s+(\(.*\)))?$")
LABELED = re.compile(r"^(?:Intended|Note|BS|VG):")


def usable(number, skip):
    return not (number in skip or paper.lines[number][2] or not paper.text(number) or number in RUNNING)


def example(start, last=None, skip=(), resume=None, prefix=""):
    """Read the example opening on line start, its lettered parts each a transcription over its
    segmentation and gloss (two such triples in (45) and (46)) and a translation, or a starred or
    questioned transcription alone; what the page sets under a part after it; and return the line
    after the example."""
    last = last or paper.last
    label, text = OPEN.match(paper.text(start)).groups()
    state = {"label": label, "count": 0}

    def row(who, kind, form, at, gloss=""):
        state["count"] += 1
        paper.add("%s(%s) line %d" % (prefix, state["label"], state["count"]), who, kind, form,
                  "page %d%s" % (paper.page(at), gloss))

    def after(number):
        number += 1
        while number <= last and not usable(number, skip):
            number += 1
        return number

    number, stage = start, "start"
    while True:
        letter = LETTER.match(text)
        if letter:
            state.update(label=label + letter.group(1), count=0)
            text, stage = letter.group(2), "start"
        # A transcription can open on the glottalization mark, 'Miin batsdi'yhl in (27a); a quote
        # opens a translation only after a gloss.
        if stage == "start" or stage == "gloss" and not text.startswith("'"):
            row(L, "transcription", text, number)
            stage = "transcription"
        elif stage == "transcription" and not LABELED.match(text):
            row(L, "segmentation", text, number)
            stage = "segmentation"
        elif stage == "segmentation":
            row(L, "gloss", text, number)
            stage = "gloss"
        elif text.startswith("'"):
            said, mark, piece = TRANSLATED.match(text).groups()
            row(A, "translation", said + (mark or ""), number)
            if piece:
                row(A, "note", piece, number, ", at the right of the translation")
            stage = "after"
        elif text.startswith("Intended:"):
            row(A, "translation", text, number, ", the intended reading of the starred part")
            stage = "after"
        elif text.startswith("Note:"):
            row(A, "note", text, number, ", under the example")
            stage = "after"
        else:
            initials = text[:2]
            said, piece = re.match(r"^(.*?[.!?])(?:\s+(\(.*\)))?$", text).groups()
            row(A, "speaker comment", said, number, ", under the example, %s, %s" % (initials, CONSULTANTS[initials]))
            if piece:
                row(A, "note", piece, number, ", at the right of the comment")
            stage = "after"
        number = after(number)
        if number > last:
            return number
        text = paper.text(number)
        if OPEN.match(text):
            return number
        if LETTER.match(text) or LABELED.match(text):
            continue
        # A tier under a transcription opens in lower case or on a letter of the APA; prose after
        # a part set alone opens on a capital, and a translation always follows a gloss.
        if stage == "transcription" and text[:1].isupper() or stage == "after":
            return number


paper.example = example
# The italic runs that are Gitxsan, set in plain letters; the rest (Individual-level, the list of
# Dixon and Aikhenvald's classes, the comments under (19) to (21), the Series captions) are English.
FORMS = {"hlgu", "sim", "'wii", "sii", "naa", "t'aat", "t'uuts'xwit", "=hl", "-m", "-a", "-it", "-at"}
paper.form_language = lambda run: L if run in FORMS else None
# The second quality of Tarpent's list wraps onto a line of its own, and Individual-level: over
# (31) opens a paragraph after a page break.
paper._starts = paper.paragraph_starts() - {paper.find(r"^predicatively\) as either")} | \
    {paper.find(r"^Individual-level:$")}
EXAMPLES = {}
expected = 1
for number in range(1, paper.last + 1):
    opened = OPEN.match(paper.text(number)) if usable(number, SKIP) else None
    if opened and opened.group(1) == str(expected):
        EXAMPLES[number] = lambda first, where: example(first, skip=SKIP)
        expected += 1
TABLE = paper.find(r"^Transitive Intransitive Intransitive$")
ORTHOGRAPHY = paper.find(r"^Appendix 1:")
ABBREVIATIONS = paper.find(r"^Appendix 2:")
REFERENCES = paper.find(r"^References$")


def body_lines(first, last):
    return [one for one in range(first, last + 1) if usable(one, SKIP)]


def cells(number):
    return re.split(r"\s{3,}", paper.spaced[number].strip())


def table(first, where):
    """Page 12: the positions a relative clause takes by the kind of its predicate, the heads over
    two printed lines, then a row head and a mark under each head."""
    page = paper.page(first)
    heads = ["Transitive Predicates", "Intransitive Eventive", "Intransitive Stative"]
    for number in (first, first + 1):
        paper.add(where, A, "note", paper.text(number), "page %d, the table's heads, a printed line" % page)
    # A row is one note: its marks are single signs, X, ? and ✓, which the checks read in no row
    # of their own.
    for number in (first + 2, first + 3):
        head, *marks = cells(number)
        placed = ", ".join("%s under %s" % (mark, column) for column, mark in zip(heads, marks))
        paper.add(where, A, "note", paper.text(number), "page %d, a row of the table, %s" % (page, placed))
    return first + 4


def orthography(first, where):
    """Appendix 1: the orthography against APA, five pairs a printed row."""
    paper.add("appendix 1", A, "heading", paper.text(first), "page %d" % paper.page(first))
    for number in body_lines(first + 1, ABBREVIATIONS - 1):
        gloss = "the table's heads, Orth. and APA five times" if number == first + 1 else \
            "a printed row, orthography and APA, five pairs side by side"
        paper.add("appendix 1", A, "note", paper.text(number), "page %d, %s" % (paper.page(number), gloss))
    return ABBREVIATIONS


def abbreviations(first, where):
    """Appendix 2: the glossing abbreviations, two columns a printed row, then the pronouns, each
    series a caption, its heads, and a row head and forms a person."""
    paper.add("appendix 2", A, "heading", paper.text(first), "page %d" % paper.page(first))
    lines = body_lines(first + 1, REFERENCES - 1)
    pronouns = paper.find(r"^Pronouns:$", first)
    series = None
    for number in lines:
        text, page = paper.text(number), paper.page(number)
        if number < pronouns:
            # AX's and SX's glosses wrap, extraction marker under each set on the next printed row.
            wrap = {"extraction marker OBL oblique": ", extraction marker the rest of AX's gloss",
                    "DM determinate marker extraction marker": ", extraction marker the rest of SX's gloss"}
            paper.add("appendix 2", A, "note", text, "page %d, a printed row of the abbreviations, two columns side by side%s"
                      % (page, wrap.get(text, "")))
        elif number == pronouns:
            paper.add("appendix 2", A, "note", text, "page %d, over the tables of pronouns" % page)
        elif text.startswith("Series"):
            series = text.split(":")[0]
            paper.add("appendix 2", A, "note", text, "page %d, a table's caption" % page)
        elif text == "Singular Plural":
            paper.add("appendix 2", A, "note", text, "page %d, %s, the table's heads" % (page, series))
        else:
            head, *forms = cells(number)
            paper.add("appendix 2", A, "note", head, "page %d, %s, a row of the table" % (page, series))
            for column, form in zip(("singular", "plural"), forms):
                paper.add("appendix 2", L, "cited form", form, "page %d, %s, %s %s" % (page, series, head, column))
    return REFERENCES


paper.standard(AUTHORS, NAMES, LANGUAGES, appendix=r"^Clarissa Forbes$",
               blocks={**EXAMPLES, TABLE: table, ORTHOGRAPHY: orthography, ABBREVIATIONS: abbreviations})
END = paper.find(r"^Clarissa Forbes$", REFERENCES)
paper.add("end", A, "name", paper.text(END), "page %d, the author, after the references" % paper.page(END))
paper.add("end", A, "note", paper.text(END + 1), "page %d, the author's e-mail" % paper.page(END + 1))
rows = paper.rows

# The abstract, set with no heading under the author's university, is one note.
lines = [index for index, row in enumerate(rows) if row[0] == "front" and row[2] == "note"
         and row[4] == "page 1, under Clarissa Forbes"][1:]
if lines:
    rows[lines[0]][3] = " ".join(rows[index][3] for index in lines)
    rows[lines[0]][4] = "page 1, the abstract"
paper.rows = rows = [row for index, row in enumerate(rows) if index not in lines[1:]]
# A cited form takes the gloss in straight quotes the paragraph gives right after it, hlgu 'small'.
body = ""
for row in rows:
    if row[2] == "note":
        body = row[3]
    elif row[2] == "cited form" and "in italics" in row[4]:
        found = re.search(r"(?<![\w'])%s(?![\w'])\s+('[^']*')" % re.escape(row[3]), body)
        if found:
            row[4] += ", " + found.group(1)
paper.write()
