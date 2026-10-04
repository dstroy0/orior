"""The ops of 2013_Thompson_Sloat: Nile R. Thompson and C. Dale Sloat's Twana and the difficult
language belief, the belief of Southern Puget Sound Salish speakers that Twana was hard to learn,
the observations that supported it, and the evidence of a century of language use against it.

Tables 1, 2, 3 and 6 set an English gloss beside its Twana form and the Puget Sound Salish cognate,
a cell to a column: each form is a cited form of its language glossed by the English, and a dialect
in brackets after a Puget Sound Salish form, (So.) or (Skagit), goes to its gloss. Table 4's
provenance of wives and Table 5's distances are a note to each printed line.
"""
import os
import re
import sys
import unicodedata

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402
import page_text  # noqa: E402

A, L = gen.A, gen.L
STEM = "2013_Thompson_Sloat"
AUTHORS = ["Nile R. Thompson", "C. Dale Sloat"]
TWANA, PUGET = "Twana", "Puget Sound Salish"
paper = gen.Paper(STEM, authors=", ".join(AUTHORS), language=TWANA)
paper.italic_letters = page_text.GENSAL
# The paper is set ragged right: a line ends on a hyphen only where the hyphen is printed, upper- / class.
paper.hyphen_ends_join = True
# A reference's web address breaks where the line ends, muckleshoot08m. / html.
paper.address_ends_join = True
DIALECTS = {"So.": "Southern", "Skagit": "Skagit", "Suquamish": "Suquamish"}
NAMES = [("Elmendorf", "William W. Elmendorf (1951, 1960, 1993), the Twana ethnographer"),
         ("Drachman", "Gaberell Drachman (1969), the Twana phonology"),
         ("Henry Allen", "a Twana speaker, Elmendorf's consultant"), ("Frank Allen", "a Twana speaker, his brother"),
         ("Eells", "Myron Eells (1985), the missionary"), ("Curtis", "Edward S. Curtis (1913), the source of the cognates"),
         ("Kuipers", "Aert H. Kuipers (2002), the Salish etymological dictionary"),
         ("Miller", "Jay Miller, who explained Twana as word taboo"), ("Lane", "Barbara Lane (1973)"),
         ("Suttles", "Wayne Suttles (1977)"), ("Mary Adams", "a Twana singer of Squaxin and Samish descent"),
         ("Weidman", "Shirley Allen Weidman, a Skokomish speaker")]
LANGUAGES = [(TWANA, "the language of the paper"), (PUGET, "the neighboring language, Lushootseed"),
             ("Lushootseed", "Puget Sound Salish"), ("Klallam", "a Straits Salish language"),
             ("Chimakum", "the unrelated language the belief first concerned"), ("Upper Chehalis", "Tsamosan"),
             ("Nooksack", "a Coast Salish language"), ("Coast Salish", "the branch Twana belongs to")]


def cells(number):
    """A table row's cells, parted at the runs of spaces the glyph rows keep between columns."""
    return [one.strip() for one in re.split(r"\s{3,}", paper.spaced[number]) if one.strip()]


def cognates(start, where):
    """A table of cognates: the head, each row's English, Twana and Puget Sound Salish, and the caption."""
    caption = paper.find(r"^Table \d[:.]", start)
    table = re.match(r"^Table (\d)", paper.text(caption)).group(1)
    page = paper.page(start)
    paper.add(where, A, "note", paper.text(start), "page %d, the heads of Table %s" % (page, table))
    for number in range(start + 1, caption):
        if not paper.text(number) or paper.lines[number][2]:
            continue
        row = cells(number)
        paper.add(where, A, "note", paper.text(number), "page %d, a row of Table %s" % (page, table))
        english, twana, puget = row[0], row[1], " ".join(row[2:])
        paper.add("Table %s, %s" % (table, english), TWANA, "cited form", twana,
                  "page %d, Table %s, ‘%s’" % (page, table, english))
        for piece in puget.split(";"):
            said = re.search(r"\s*\(([^)]*)\)\s*$", piece)
            form = piece[:said.start()].strip() if said else piece.strip()
            dialect = DIALECTS.get(said.group(1), said.group(1)) if said else None
            paper.add("Table %s, %s" % (table, english), PUGET, "cited form", form,
                      "page %d, Table %s, ‘%s’%s" % (page, table, english,
                                                     ", the %s dialect" % dialect if dialect else ""))
    paper.add(where, A, "note", paper.text(caption), "page %d, the caption" % paper.page(caption))
    return caption + 1


def printed_lines(start, where):
    """A table a note to each printed line, to its caption."""
    caption = paper.find(r"^Table \d[:.]", start)
    for number in range(start, caption + 1):
        if paper.text(number) and not paper.lines[number][2]:
            paper.add(where, A, "note", paper.text(number), "page %d, %s" % (
                paper.page(number), "the caption" if number == caption else "a printed line of " +
                re.match(r"^Table \d", paper.text(caption)).group(0)))
    return caption + 1


def comparison(start, where):
    """The side by side comparison of page 42: the heads, then each column's paragraphs, the left
    column's before the right's. A column's line that ends a sentence closes its paragraph, the
    next opening indented under it."""
    page = paper.page(start)
    paper.add(where, A, "note", paper.text(start), "page %d, the heads of the comparison" % page)
    heads = cells(start)
    columns = [[] for _ in heads]
    number = start + 1
    while paper.text(number) and not re.match(r"^This disturbing", paper.text(number)):
        for at, cell in enumerate(re.split(r"\s{3,}", paper.spaced[number].strip())):
            columns[at].append(cell)
        number += 1
    for head, column in zip(heads, columns):
        paragraph = []
        for cell in column:
            paragraph.append(cell)
            if re.search(r"[.…]\"?$", cell) or cell is column[-1]:
                paper.add(where, A, "note", " ".join(paragraph), "page %d, the %s column" % (page, head))
                paragraph = []
    return number


def block_starts(step=10.0, spread=1.4):
    """The line numbers that open a paragraph: a glyph row set a wider gap below the row over it
    than the page's common line spacing, the space around a block quote, or a row set further in
    than the row over it, a paragraph's first line in the prose or in a quote. The lines of a quote
    stand at one indent, and a quote's paragraph is one paragraph. A line opening on a bullet opens
    one too. The first line
    of a page opens a paragraph where it stands indented under the line below it, or where the
    page before ends on a quote and it does not open one or ends on prose and it opens a quote; it
    carries on the paragraph where both are prose or both are quote."""
    document = page_text.paper_document(STEM)[0]
    found, placed, margins = {}, {}, {}
    for number in range(len(document)):
        textpage = document[number].get_textpage()
        glyphs = []
        for index in range(textpage.count_chars()):
            symbol = textpage.get_text_range(index, 1)
            if symbol and not symbol.isspace():
                left, bottom, right, top = textpage.get_charbox(index, loose=True)
                glyphs.append((bottom, left, symbol))
        # A row is the glyphs within two points of one baseline: the small capitals of ELMENDORF
        # stand 0.4 points off the E before them.
        rows = []
        for glyph in sorted(glyphs, reverse=True):
            if rows and rows[-1][0][0] - glyph[0] <= 2.0:
                rows[-1].append(glyph)
            else:
                rows.append([glyph])
        # A raised mark is a row of its own and no line, the 8 of Table 1,8.
        lines = [(max(one[0] for one in row), min(one[1] for one in row),
                  "".join(one[2] for one in sorted(row, key=lambda one: one[1]))) for row in rows if len(row) >= 3]
        if len(lines) < 2:
            continue
        gaps = sorted(lines[at][0] - lines[at + 1][0] for at in range(len(lines) - 1))
        spacing = gaps[len(gaps) // 2]
        # The margin is the least left edge three rows share; a page given mostly to quotes sets
        # most of its rows indented.
        lefts = sorted(one[1] for one in lines)
        margin = next((one for one in lefts if sum(abs(other - one) <= 1.5 for other in lefts) >= 3), lefts[0])
        margins[number + 1] = margin
        placed[number + 1] = [(text, left) for bottom, left, text in lines]
        previous = None
        for bottom, left, text in lines:
            if previous is None or previous[0] - bottom > spread * spacing or left >= previous[1] + step:
                found.setdefault(number + 1, []).append(text)
            previous = (bottom, left)
    # A line and its row are matched without digits, the raised footnote mark of EELLS:5 being a row
    # of its own, with the glyph row's GenSal codes read as their letters, Now those two families are
    # °Å™°élaË, and without marks or ʷ, a raised w being a row of its own too.
    def key(text):
        text = unicodedata.normalize("NFD", "".join(page_text.GENSAL.get(one, one) for one in gen.letters(text)))
        return "".join(one for one in text if not unicodedata.combining(one) and one != "ʷ" and not one.isdigit())[:24]
    def matches(number, table):
        line = key(paper.text(number))
        return len(line) >= 6 and any(key(row).startswith(line) or line.startswith(key(row))
                                      for row in table.get(paper.page(number), ()) if len(key(row)) >= 6)

    def left_of(number):
        line = key(paper.text(number))
        return next((left for row, left in placed.get(paper.page(number), ())
                     if len(line) >= 6 and len(key(row)) >= 6 and (key(row).startswith(line) or line.startswith(key(row)))),
                    None)

    def quoted(number):
        left = left_of(number)
        return left is not None and left >= margins[paper.page(number)] + step
    starts = {number for number in range(1, paper.last + 1) if matches(number, found) or paper.text(number).startswith("●")}
    notes = {one for parts, _ in paper.page_footnotes().values() for one in parts}
    running = paper.running_numbers_set()
    body = [one for one in range(1, paper.last + 1)
            if paper.text(one) and not paper.lines[one][2] and one not in notes and one not in running]
    for at in range(1, len(body)):
        before, after = body[at - 1], body[at]
        if paper.page(before) == paper.page(after) or paper.text(after).startswith("●") or left_of(after) is None:
            continue
        under = body[at + 1] if at + 1 < len(body) and paper.page(body[at + 1]) == paper.page(after) else None
        if under is not None and left_of(under) is not None and left_of(after) >= left_of(under) + step \
                or quoted(after) != quoted(before):
            starts.add(after)
        else:
            starts.discard(after)
    return starts


paper._starts = block_starts()
BLOCKS = {number: cognates for number in range(1, paper.last + 1) if paper.text(number) == "English Twana Puget Sound Salish"}
BLOCKS[paper.find(r"^Wife’s People")] = printed_lines
BLOCKS[paper.find(r"^Coast Salish\s+Indo-European")] = printed_lines
BLOCKS[paper.find(r"^Original\s+Altered$")] = comparison
# A heading's number runs to two digits, 10 and 11, and two print a space in it or a point after
# it, 7. 1 and 8.2., read as 7.1 and 8.2.
HEADINGS = {number: re.sub(r"\s", "", re.match(r"^(\d{1,2}(?:\.\s?\d){0,2})", paper.text(number)).group(1))
            for number in range(1, paper.last + 1)
            if re.match(r"^\d{1,2}(?:\.\s?\d){0,2}\.?\s{3}\S", paper.spaced[number])}
# The mark of footnote 3 stands on the year of the treaty, 18553 for 1855, and 8 on the comma after
# the number of Table 1, as those in Table 1,8 that.
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS, headings=HEADINGS, glued={"3": "1855", "8": "1,"})
# The languages a form of the prose can be given to, by the name the prose sets before it.
NAMED = {"Twana": TWANA, "Puget Sound Salish": PUGET, "Lushootseed": PUGET, "Puyallup": PUGET,
         "Klallam": "Klallam", "Nooksack": "Nooksack", "Upper Chehalis": "Upper Chehalis", "CS": "Proto-Coast Salish",
         "PS": "Proto-Salish", "Cowichan": "Halkomelem"}
NAME_ALTERNATION = "|".join(re.escape(one) for one in sorted(NAMED, key=len, reverse=True))
# The name right before a form, Puget Sound Salish form, čugʷaš, CS *kʷutx̌, the PS form for *s-pəlq,
# and Lushootseed (xʷəlšucid).
NAME_BEFORE = re.compile(r"\b(%s)(?:\s+(?:form|word)s?)?(?:\s+for)?(?:\s+is)?,?\s*\(?$" % NAME_ALTERNATION)
# Or the name in brackets right after it, xʷəlšucid (Lushootseed).
NAME_AFTER = re.compile(r"^\s*\((%s)\)" % NAME_ALTERNATION)
# The forms whose language the sentence gives some other way: the Cowichan pənálxαċ call a painted
# bluff xá•los, they being the subject, and the Upper Chehalis word for 'penis', namely spəlq, their
# word, after the Upper Chehalis. Kuipers's suffix of the PS *s-pəlq is -q or –aq.
SAID_BY = {"xá•los": "Halkomelem", "spəlq": "Upper Chehalis", "-q": "Proto-Salish", "–aq": "Proto-Salish"}
# A name the census lists is no form of a language.
NOT_FORMS = {"Na’-mĭt-hu"}
TOKEN = re.compile(r"[^\s,;“”()\[\]]+")
ENGLISH = {"½", "…", "•", "–", "—"}


def prose_forms(note, pages):
    """[(form, language, gloss)] for the forms of a paragraph in the order printed: each word with a
    letter outside plain English, each starred form, and each affix the pages set in italics."""
    italic = {piece.strip(",;:.()[]“”") for page in pages for run in paper.italics().get(page, [])
              for piece in run.split()}
    found = []
    for token in TOKEN.finditer(note):
        form = token.group(0).rstrip(".,:;!?")
        before, after = note[:token.start()], note[token.end():]
        # A quote's marks at the edges are no part of the form; a closing ’ is the quote's only
        # where a quote is open before the word, and otherwise the form's own glottalization, č’ič’ic’.
        if form.startswith("‘"):
            form = form[1:]
            form = form[:-1] if form.endswith("’") else form
        elif form.endswith("’") and before.count("‘") > before.count("’"):
            form = form[:-1]
        form = form.rstrip(".,:;!?")
        # A footnote's mark set on a word, Chimakum2, or a possessive is no part of a form.
        if not form or form in ENGLISH or form in NOT_FORMS or not re.search(r"[^\W\d_]", form) or \
                form in NAMED or re.search(r"’s$|\d$", form) or re.fullmatch(r"[A-Z][a-z]+(?:[-’][A-Za-z]+)*", form):
            continue
        # A word in plain letters is English, multi- of bi- and multi-lingual, unless it is an affix the
        # page sets in italics with its dash upright, Kuipers's -q and –aq; a letter alone, the ǰ of j,
        # ǰ, g and gʷ, is no form.
        plain = re.fullmatch(r"[A-Za-z\-–]+", form)
        affix = re.match(r"^[-–]", form) and form.lstrip("-–") in italic
        if plain and not affix or not (form.startswith("*") or affix or gen.orthographic(form)) or not affix and \
                len(re.sub(r"[^\w]|[ʷ_]", "", unicodedata.normalize("NFD", form))) < 2:
            continue
        named = NAME_BEFORE.search(before) or NAME_AFTER.search(after)
        if form in SAID_BY:
            language = SAID_BY[form]
        elif named:
            language = NAMED[named.group(1)]
        elif form.startswith("*"):
            language = "Proto-Salish"
        else:
            language = TWANA
        said = re.match(r"\s*(?:\(|\[)?\s*(‘[^’]*’)", after)
        # The English in brackets after a form, kʷ̓ayɛ́q [Upper Chehalis], is its gloss; a remark there,
        # [through her father], is not.
        bracket = re.match(r"\s*\[([A-Z][^\]‘]*)\]", after)
        gloss = said.group(1) if said else "[%s]" % bracket.group(1) if bracket else ""
        found.append((form, language, re.sub(r"[,.]’$", "’", gloss), token.start()))
    return found


def line_pages(note, page):
    """[(offset, page)] for each printed line of a paragraph opening on page: where in the note the
    line's text stands and the page it is printed on. A footnote's lines and a page number set
    between two of the paragraph's lines stand nowhere in it."""
    start = next((one for one in range(1, paper.last + 1)
                  if paper.page(one) == page and paper.text(one) and note.startswith(paper.text(one))), None)
    if start is None:
        return [(0, page)]
    found, at = [], 0
    for number in range(start, paper.last + 1):
        if paper.page(number) > page + 1 or at >= len(note):
            break
        text = paper.text(number)
        spot = note.find(text, at, at + len(text) + 1) if text else -1
        if spot in (at, at + 1):
            found.append((spot, paper.page(number)))
            at = spot + len(text)
    return found or [(0, page)]


rows = []
for row in paper.rows:
    if row[2] == "cited form" and "in italics" in row[4]:
        continue
    rows.append(row)
    # A table's printed lines are its block's; its forms are read there.
    if row[2] == "note" and (row[0].startswith("§") or row[0].startswith("footnote")) and "Table" not in row[4]:
        page = int(re.match(r"page (\d+)", row[4]).group(1))
        lines = line_pages(row[3], page)
        for form, language, gloss, at in prose_forms(row[3], (page, page + 1)):
            kind = "cited affix" if re.match(r"^\*?[-–]", form) or form.endswith("-") else "cited form"
            printed = [one for spot, one in lines if spot <= at][-1:] or [page]
            rows.append([row[0], language, kind, form, "page %d, in the prose%s" % (printed[0], ", " + gloss if gloss else "")])
paper.rows = rows
# Heading 6.3 wraps onto a second line, women at a high rate?
for at, row in enumerate(paper.rows):
    if row[2] == "heading" and row[3].startswith("6.3 ") and paper.rows[at + 1][3] == "women at a high rate?":
        row[3] += " " + paper.rows.pop(at + 1)[3]
        break
# The front sets the two affiliations under the two names, parted by the slash: Dushuyay Research is
# Thompson's and North Seattle Community College Sloat's.
for row in paper.rows:
    if row[0] == "front" and row[2] == "note" and row[3] == "Dushuyay Research/":
        row[4] = "page 1, under Nile R. Thompson"
# The references, an entry to each line that opens one: an entry under the author above opens on a
# rule, ______. or ______,, an entry with no year opens on n.d., Seattle Art Museum. n.d., an author
# the paper supplies in brackets opens on the bracket, [Tarpent,] Marie-Lucie, a body's name can
# hold and, Skokomish Culture and Art Committee. 2002., and the census's opens U.S. Census. Bergland's place, Seattle, WA:, carries on his entry. The th of Allen's (May 12th)
# stands on a line of its own over the entry and is no part of the list. Each author's name and
# address close the last page.
first = paper.find(r"^References$")
addresses = [paper.find(r"^Nile Thompson$", first), paper.find(r"^C\. Dale Sloat$", first)]
paper.reference_lines_run_on = {paper.find(r"^Seattle, WA: Pacific Northwest Region", first)}
entries = paper.references(first + 1, addresses[0] - 1, skip={paper.find(r"^th$", first)},
                           opens=r"^_{3,}[.,]|^U\.S\. Census\.|^\[[A-Z]|^De Danaan,|^[A-Z][\w’'\-]+(?: [A-Z][\w’'\-]+)*\. n\.d\."
                           r"|^[A-Z][\w’'\-]+(?:,| &| and)|^[A-Z][\w’'\-]+(?: (?:and )?[A-Z][\w’'\-]+)*\. \d{4}\.")
paper.rows = [row for row in paper.rows if row[2] != "reference"]
for entry, at in entries:
    paper.add("references", A, "reference", entry, "page %d" % at)
for start, end in ((addresses[0], addresses[1] - 1), (addresses[1], paper.last)):
    lines = [one for one in range(start, end + 1) if paper.text(one) and not paper.lines[one][2]
             and not re.fullmatch(r"\d{3}", paper.text(one))]
    paper.add("references", A, "note", paper.joined(lines), "page %d, the author's name and address" % paper.page(start))
paper.meta = {
    "title": "Twana and the difficult language belief",
    "byline": "Nile R. Thompson and C. Dale Sloat, Dushuyay Research / North Seattle Community College",
    "volume": "48",
    "whose": "The Twana forms of the tables are from Curtis (1913), N. Thompson (1979) and Elmendorf (1951), "
             "and the Puget Sound Salish cognates from Curtis (1913) and Kuipers (2002). The forms the prose "
             "quotes from Allen, Meeker and the census are theirs as those sources print them.",
    "letters": "The forms are in the Americanist orthography of Lushootseed and Twana: ə, ɔ, ɪ, ɛ, č, š, ǰ, ł, "
               "x̌, ƛ̓, ʔ, ʷ, ’ for glottalization after its letter and the acute of stress. The older spellings "
               "the prose quotes keep their letters, α, ´ after a letter for stress, • for length and a dot "
               "above for glottalization, ċ and ẏ.",
}
paper.write()
