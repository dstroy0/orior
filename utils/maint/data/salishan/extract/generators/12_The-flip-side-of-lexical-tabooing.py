"""The ops of 12_The-flip-side-of-lexical-tabooing: David Douglas Robertson on Coast Salish puns,
names and intangible cultural heritage: humorous word play in the Salish languages of Washington
State as the other face of the areal custom of tabooing words that sound like the name of the dead.

Table 1 runs over pages 3 to 5 in five columns, LANGUAGE, +WORD 1, -WORD 2, WORD 3 and CONTEXT, each
word given to its column by where it stands. Its caption heads page 3 over the notes set small at
the page's foot, and gen reads no note on that page; the notes are read with the table's lines
left out. The table's language cells and forms carry the marks of notes 2 to 10, and each note is
written after the row that carries its mark.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Upper Chehalis"
AUTHORS = ["David Douglas Robertson"]
paper = gen.Paper("12_The-flip-side-of-lexical-tabooing", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("William Elmendorf", "Twana word tabooing (1951, 1992)"),
         ("Thelma Adamson", "Coast Salish folk-tales (2009), the play on words"),
         ("Lucy Heck", "storyteller, the 1926 Humptulips myth"), ("Silas Heck", "her husband, its translator"),
         ("Kinkade", "M. Dale Kinkade, the Upper Chehalis dictionary (1991)"),
         ("Bierwert", "Crisca Bierwert, Lushootseed texts (1996)"),
         ("Amrine Goertz", "Jolynn Amrine Goertz, Chehalis stories (2018)"),
         ("Boas", "Franz Boas, the Lower Chehalis myth (1890) and Chinook (1910)"),
         ("Jay Miller", "puns in Salish visual art (2006)"), ("lessLIE", "Coast Salish artist, Figure 1"),
         ("Michael Noonan", "inverted roots in Salish (1997)"), ("Emmett Chase", "nicknamed emét")]
LANGUAGES = [(LANGUAGE, "Tsamosan, Coast Salish"), ("Lower Chehalis", "Tsamosan, Coast Salish"),
             ("Lushootseed", "Central Coast Salish"), ("Twana", "Central Coast Salish"),
             ("English", "the translations"), ("Chinuk Wawa", "the loan láys ‘rice’"),
             ("Klallam", "personal names"), ("Kiksht", "Wishram Chinookan"), ("Quinault", "jəl̓qín"),
             ("Spokane", "snʔóʔcqeʔtn ‘outhouse’"), ("Tillamook", "*p > h"), ("Quileute", "name avoidance"),
             ("Stó:lō", "emét ‘sit down’"), ("Lillooet", "punning, van Eijk 1984")]

# The italic runs come off the glyphs with a space at each raised or combining mark, q̓íləč ̓šəc čn and
# t ̓ íq-m̓ ɬ, and the page text holds each form whole; a table cell's run sets the -- of an empty
# column before the next cell's form.
ITALICS = {}
for page, runs in paper.italics().items():
    for run in runs:
        run = re.sub(r"\s+(?=[̀-ͯ])|(?<=[̀-ͯ])\s+", "", run)
        ITALICS.setdefault(page, []).extend(one for one in re.split(r"^-- |, ", run) if one)
paper.italics = lambda: ITALICS
# The language of each italic form the prose and notes cite that is not Upper Chehalis.
FORM_LANGUAGE = {"stə́bəqəb": "Twana", "láys": "Chinuk Wawa", "q̓íləč̓šəc čn": "Lower Chehalis",
                 "ʔíbəš": "Lushootseed", "tuʔáɬəd": "Lushootseed", "syəl̓qín": "Lower Chehalis",
                 "jəl̓qín": "Quinault", "wəlíʔ": "Lushootseed", "s-": "Lower Chehalis", "-i": "Lower Chehalis",
                 "snʔóʔcqeʔtn": "Spokane", "ʔócqeʔ": "Spokane", "h~p̓": "Lushootseed", "*p": "Tillamook",
                 "h": "Tillamook", "emét": "Stó:lō", "<KwaL>": L}
NOT_FORMS = {"•", "--"}


def form_language(run):
    if run in NOT_FORMS:
        return None
    return FORM_LANGUAGE.get(run, L if gen.orthographic(run) else None)


paper.form_language = form_language

CAPTION = paper.find(r"^Table 1: Some Coast Salish puns$")
# The table's printed lines: page 3 from the caption to the first note, and the head of pages 4 and 5
# down to the notes or the next section.
TABLE = [one for one in range(CAPTION, paper.find(r"^4\s+Structure$"))
         if paper.page(one) in (3, 4, 5) and paper.text(one).strip()]
FOUND = paper.page_footnotes(stops=range(CAPTION, paper.find(r"^2 Lushootseed words are as found", CAPTION)))
paper.page_footnotes = lambda *args, **kwargs: FOUND
AT_FOOT = {one for parts, _ in FOUND.values() for one in parts}
RUNNING = paper.running_numbers_set()
ROWS = [one for one in TABLE if one not in AT_FOOT and one not in RUNNING]
# The notes whose marks stand in the table, written by table() after the row that carries each.
TABLE_MARKS = [str(one) for one in range(2, 11)]
place = paper.place_footnotes
paper.place_footnotes = lambda marks, write, placed=(), **rest: place(marks, write, placed=tuple(placed) + tuple(TABLE_MARKS),
                                                                      **rest)

# The left edges of the columns past the first: +WORD 1, -WORD 2, WORD 3 and CONTEXT.
EDGES = (118, 172, 235, 280)
COLUMNS = ("LANGUAGE", "WORD 1", "WORD 2", "WORD 3", "CONTEXT")
TABLE_LANGUAGE = {"Lushootseed": "Lushootseed", "Upper Chehalis": L, "Lower Chehalis": "Lower Chehalis"}
# Note 9: the form for ‘sink into water’ in a Lower Chehalis row is cited from Upper Chehalis.
CELL_LANGUAGE = {"nə́č": L}


def column(left):
    return sum(left >= edge for edge in EDGES)


def note(mark):
    parts, at = FOUND[mark]
    paper.footnote(mark, parts, at, NAMES, LANGUAGES, gloss="page %d, footnote %s" % (at, mark))


def cells(lines):
    """{column: [(line, text)]} for an entry's printed lines, the words of each line in a column
    joined."""
    found = {}
    for line in lines:
        for left, word in paper.word_positions(line):
            held = found.setdefault(column(left), [])
            if held and held[-1][0] == line:
                held[-1] = (line, held[-1][1] + " " + word)
            else:
                held.append((line, word))
    return found


def glossed(pieces):
    """A word's gloss from its lines, joined as the page sets them: the - of - ‘impotent, tired’
    stands on a line of its own over the quote, and ‘bare- / backside’ is broken at its hyphen."""
    return " ".join(pieces)


def table(start, where):
    """Table 1: its caption, the line heading its columns, and each entry a row of the table: the
    language cell a language row, each word's forms a cited form under the entry's language with its
    +/- gloss a translation, and the context a note, its italic forms after it."""
    page = paper.page(start)
    paper.add("Table 1", A, "note", paper.text(start), "page %d, the caption" % page)
    paper.add("Table 1", A, "note", paper.text(start + 1), "page %d, heads the columns" % page)
    entries, current = [], []
    for line in ROWS[2:]:
        current.append(line)
        # An entry's context closes on its source, (Bierwert 1996:98)., a note's mark after it.
        if re.search(r"\)\.\d*$", paper.text(line)):
            entries.append(current)
            current = []
    language = None
    for count, lines in enumerate(entries, 1):
        at = "Table 1 row %d" % count
        page = paper.page(lines[0])
        found = cells(lines)
        if found.get(0):
            printed = " ".join(text for _, text in found[0])
            mark = re.search(r"\d+$", printed).group(0)
            language = printed[:-len(mark)]
            paper.add(at, A, "language", language, "page %d, Table 1, LANGUAGE, carries footnote %s" % (page, mark))
            paper.mentioned.add(("language", language))
            note(mark)
        who = TABLE_LANGUAGE[language]
        for index, name in ((1, "+WORD 1"), (2, "-WORD 2"), (3, "WORD 3")):
            pieces = [text for _, text in found.get(index, ())]
            if pieces == ["--"]:
                paper.add(at, A, "note", "--", "page %d, Table 1, %s, empty" % (page, name))
                continue
            split = next(number for number, text in enumerate(pieces) if re.match(r"^[+-]?‘|^[+-]$", text))
            marks = []
            for form in ", ".join(pieces[:split]).split(", "):
                form = form.rstrip(",")
                mark = re.search(r"(?<=\D)\d+$", form)
                if mark:
                    form = form[:mark.start()]
                    marks.append(mark.group(0))
                paper.add(at, CELL_LANGUAGE.get(form, who), "cited form", form, "page %d, Table 1, %s%s" % (
                    page, name, ", carries footnote " + mark.group(0) if mark else ""))
                paper.cited_done.add(form)
            text = glossed(pieces[split:])
            # ʔúl=ps sets its reading in brackets under its gloss, [seemingly ‘bare-backside’].
            bracket = text.find(" [")
            paper.add(at, A, "translation", text if bracket < 0 else text[:bracket], "page %d, Table 1, %s" % (page, name))
            if bracket >= 0:
                paper.add(at, A, "note", text[bracket + 1:], "page %d, Table 1, %s, under the gloss" % (page, name))
            for mark in marks:
                note(mark)
        context = " ".join(text for _, text in found[4])
        paper.add(at, A, "note", context, "page %d, Table 1, CONTEXT" % page)
        paper.cited(at, context, sorted({paper.page(line) for line in lines}))
        mark = re.search(r"\)\.(\d+)$", context)
        if mark:
            note(mark.group(1))
    return ROWS[-1] + 1


def quotation(start, where):
    """Adamson's words set as a block quotation, one note, and the source under it a citation."""
    source = paper.find(r"^\(Adamson 2009:287 fn\. 2\)$", start)
    body = paper.joined(range(start, source))
    page = paper.page(start)
    paper.add(where, A, "note", body, "page %d, a block quotation" % page)
    paper.add(where, A, "citation", paper.text(source), "page %d, under the quotation" % page)
    paper.mentions(where, body, LANGUAGES, "language")
    return source + 1


def caption(start, where):
    """A figure's caption, a note of its own."""
    number = re.match(r"^Figure (\d+):", paper.text(start)).group(1)
    paper.add(where, A, "note", paper.text(start), "page %d, the caption of Figure %s" % (paper.page(start), number))
    paper.mentions(where, paper.text(start), NAMES, "name")
    return start + 1


def bullets(start, where):
    """The list of §6, a note to each item from its bullet to the next, the page's number passed over."""
    end = paper.find(r"^Among other explanations for the rampant", start)
    items = []
    for number in range(start, end):
        if number in RUNNING or not paper.text(number).strip():
            continue
        if paper.text(number).startswith("•"):
            items.append([])
        items[-1].append(number)
    for lines in items:
        body = paper.joined(lines)
        pages = sorted({paper.page(one) for one in lines})
        paper.add(where, A, "note", body, "page %d, a bulleted item" % pages[0])
        paper.cited(where, body, pages)
        paper.mentions(where, body, NAMES, "name")
        paper.mentions(where, body, LANGUAGES, "language")
    return end


blocks = {CAPTION: table, paper.find(r"^\.\.\.play on words, a feature"): quotation,
          paper.find(r"^•\s+Cultural values:"): bullets}
for number in range(1, paper.last + 1):
    if re.match(r"^Figure \d+: ", paper.text(number)):
        blocks[number] = caption
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)

# Three entries open on no surname and a comma that references() looks for: Amrine Goertz's
# two-word surname, Montler's with a stop after it, and lessLIE's in lower case. Each runs onto the
# entry above it.
for index in range(len(paper.rows) - 1, -1, -1):
    row = paper.rows[index]
    if row[2] != "reference":
        continue
    pieces = re.split(r" (?=Amrine Goertz, Jolynn\. 2018\.|Montler\. Timothy, Adeline Smith|lessLIE\. 2007\.)", row[3])
    if len(pieces) > 1:
        paper.rows[index:index + 1] = [[row[0], row[1], row[2], piece, row[4]] for piece in pieces]
paper.write()
