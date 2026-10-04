"""The ops of 17-Zenk_revised: Henry Zenk on the credits to John Kirk Townsend in the draft of
Horatio Hale's sketch of the Chinuk Wawa "Jargon", on Townsend's own 1835 word-lists, and on the
simplified Chinookan their phrases show.

The page text is the glyph rows (page_text.py rows), which keep the gaps between the columns of the
examples and of the appendix. Examples (1) to (6) are Hale's sentences, each a transcription over
its gloss and Hale's source translation. Examples (7) to (12) are sentences of the Chinookan text
corpus and of the Catholic missionary corpus, each a form over a gloss and a translation in quotes
with its source. Example (13) sets Chinuk Wawa words beside their Chinookan sources. Examples (14)
to (30) open on a phrase of Townsend's and his English, and set the Chinuk Wawa or Chinookan
comparisons under it; from (23) on the comparisons stand in two columns, Chinookan and CW.

The appendix lists the items of Hale's draft marked T, one entry a row of five columns: the gloss
and definition in Hale 1846, the draft's definition, Townsend's 1835 definition, the CW form and the
Chinookan one. Each word is given to its column by where it stands; a line whose first column holds
no gloss runs the entry above it on. Pages 16 to 27 are turned on their sides and the rows set the
digits of their numbers, 270 to 281, before a line's first word; a digit left of the first column
is no cell's. The eight notes of the appendix are numbered apart from the three of the body, and
each is written after the entry that carries its mark.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
STEM = "17-Zenk_revised"
LANGUAGE = "Chinuk Wawa"
AUTHORS = ["Henry Zenk"]
paper = gen.Paper(STEM, authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Horatio Hale", "the Jargon sketch of 1846 and its draft of ca. 1841"),
         ("John Kirk Townsend", "the American naturalist, 1809 to 1851, and his 1835 word-lists"),
         ("George Lang", "the presentation of note 1 (Zenk and Lang 2012); Lang 2008"),
         ("Ives Goddard", "personal communication 2011"),
         ("Boas", "Franz Boas, Chinook texts (1894), Kathlamet texts (1901) and Chinook (1911)"),
         ("Sapir", "Edward Sapir, Wishram texts (1909)"), ("Dyk", "Walter Dyk, a grammar of Wishram (1933)"),
         ("Hymes", "Dell Hymes, the language of the Kathlamet Chinook (1955)"),
         ("Demers", "Modeste Demers, the Chinook dictionary, catechism, prayers and hymns (1871)"),
         ("Thomason", "Sarah Thomason (1983)"), ("Samarin", "William Samarin (1986, 1996)"),
         ("Grant", "Anthony Grant (1996)"), ("Tony Johnson", "Zenk and Johnson 2013"),
         ("Ross Clark", "personal communication 2010, note 3 of the appendix"),
         ("Gibbs", "George Gibbs (1863), note 5 of the appendix")]
LANGUAGES = [(LANGUAGE, "Chinook Jargon, CW, the pidgin of the lower Columbia"),
             ("Chinookan", "CW's principal lexifier"), ("Nootka Jargon", "the seafarers' pidgin"),
             ("Nootkan", "the source of the Nootka Jargon items"), ("English", "a CW lexifier"),
             ("French", "a CW lexifier"), ("Wishram", "Upper Chinookan; Sapir 1909 and Dyk 1933"),
             ("Kathlamet", "Chinookan; Boas 1901 and Hymes 1955"), ("Lower Chinook", "LC, the appendix"),
             ("Upper Chinook", "UC, the appendix"), ("Salish", "S, the appendix"),
             ("Kalapuyan", "note 4 of the appendix"), ("Hajda", "note 3 of the appendix, as printed")]
CHINOOKAN = "Chinookan"
HALE, DRAFT, TOWNSEND = "Hale (1846)", "Hale (ca. 1841)", "Townsend (1835)"

# The lines come repaired by residue.py's corrections, the Symbol font's bullet among them; the
# words word_positions reads off the glyphs are repaired the same way.
REPAIR = gen.residue.paper_repair(STEM)
BULLET = "\u2022"

REFERENCES = paper.find(r"^References$")
APPENDIX = paper.find(r"^Appendix\s")
RUNNING = paper.running_numbers_set()
# The appendix's notes are numbered 1 to 8 apart from the body's 1 to 3; the body's are read with
# the appendix left out.
FOUND = paper.page_footnotes(stops=set(range(APPENDIX, paper.last + 1)))
assert sorted(FOUND) == ["1", "2", "3"], sorted(FOUND)
paper.page_footnotes = lambda *args, **kwargs: FOUND
SKIP = {one for parts, _ in FOUND.values() for one in parts} | set(paper.volume_header())

HEADINGS = {}
for pattern, label in ((r"^1\s+Introduction", "1"), (r"^2\s+Hale and Townsend$", "2"),
                       (r"^3\s+Chinuk Wawa versus Chinookan$", "3"), (r"^4\s+Morphologically simplified", "4"),
                       (r"^5\s+Evidence of simplified Chinookan", "5"), (r"^6\s+Concluding note$", "6")):
    HEADINGS[paper.find(pattern)] = label


def content(number):
    return not (paper.lines[number][2] or not paper.text(number) or number in RUNNING or number in SKIP)


def next_line(number):
    number += 1
    while number <= paper.last and not content(number):
        number += 1
    return number


def prose(where, numbers, gloss):
    """A note of the lines and the cited forms, names and languages it holds."""
    body = paper.joined(numbers)
    paper.add(where, A, "note", body, gloss)
    pages = sorted({paper.page(one) for one in numbers})
    paper.cited(where, body, pages)
    paper.mentions(where, body, NAMES, "name")
    paper.mentions(where, body, LANGUAGES, "language")


# The body's blocks: the quotations set off from the text, the two bulleted lists, the second line
# of section 5's heading and the examples.

CLOSING = re.compile(r"^(.*\S)\s+(\((?:Hale|Zenk and Johnson) [^()]*\)\.)$")


def quotation(start, where):
    """A quotation set off from the text, to the line that closes on its source: a note and the
    source a citation."""
    numbers, number = [start], start
    while not CLOSING.match(paper.text(number)):
        number = next_line(number)
        numbers.append(number)
    said, source = CLOSING.match(paper.joined(numbers)).groups()
    pages = sorted({paper.page(one) for one in numbers})
    paper.add(where, A, "note", said, "page %d, a quotation set off from the text" % pages[0])
    paper.add(where, A, "citation", source, "page %d, the source of the quotation" % pages[-1])
    paper.cited(where, said, pages)
    paper.mentions(where, said, NAMES, "name")
    paper.mentions(where, said, LANGUAGES, "language")
    return next_line(number)


def bullets(start, where):
    """A bulleted list, a note to each item with its wrapped lines run on; the Symbol font's round
    bullet is left off."""
    end = LISTS[start]
    items, number = [], start
    while number < end:
        if paper.text(number).startswith(BULLET):
            items.append([])
        items[-1].append(number)
        number = next_line(number)
    for numbers in items:
        body = paper.joined(numbers).lstrip(BULLET + " ")
        paper.add(where, A, "note", body, "page %d, an item of a bulleted list" % paper.page(numbers[0]))
        paper.cited(where, body, sorted({paper.page(one) for one in numbers}))
        paper.mentions(where, body, NAMES, "name")
        paper.mentions(where, body, LANGUAGES, "language")
    return end


def heading_wrap(start, where):
    """Section 5's heading, which wraps onto a second line."""
    paper.rows[-1][3] += " " + paper.text(start)
    return next_line(start)


class Rows:
    """The rows of one example, each labeled (N) line K as gen labels them."""

    def __init__(self, label):
        self.label, self.count = label, 0

    def __call__(self, who, kind, form, number, gloss=None):
        self.count += 1
        paper.add("(%s) line %d" % (self.label, self.count), who, kind, form,
                  "page %d%s" % (paper.page(number), ", " + gloss if gloss else ""))


def opening(start):
    matched = gen.EXAMPLE.match(paper.text(start))
    return matched.group(1), matched.group(2)


def form_kind(form):
    return "segmentation" if re.search(r"[‑-]", form) else "transcription"


def run_on(number):
    """The text from line number on, joined over the lines that close the parentheses it opens.
    Returns the text and its last line."""
    said = paper.text(number)
    while said.count("(") > said.count(")"):
        number = next_line(number)
        said += " " + paper.text(number)
    return said, number


QUOTED = re.compile(r"^(‘.*?’)(?=\s*(?:\(|$))\s*(.*)$")
PIECE = re.compile(r"\((?:[^()]|\([^()]*\))*\)\.?")


def translation(row, said, number):
    """A translation in quotes and the parenthesized pieces after it, each a citation where it
    names a year and a note where it does not."""
    quoted, rest = QUOTED.match(said).groups()
    row(A, "translation", quoted, number)
    pieces = PIECE.findall(rest)
    assert not PIECE.sub("", rest).strip(), (said, pieces)
    for piece in pieces:
        # The year may run into the page on a length mark, Demers et al. 1871ː65.
        if re.search(r"\b1[89]\d\d(?!\d)", piece):
            row(A, "citation", piece, number, "the source at the right of the translation")
        else:
            row(A, "note", piece, number, "at the right of the translation")


def hale(start, where):
    """Examples (1) to (6), Hale's sentences: the transcription and its gloss in turn, a second
    gloss under a starred word, *how? under *qata in (6), and the translation of the source after
    its label, source translation:, with the page of Hale 1846 under it in (6)."""
    label, first = opening(start)
    row = Rows(label)
    row(L, "transcription", first, start)
    kind, number = "transcription", next_line(start)
    while True:
        text = paper.text(number)
        if text.startswith("source translation:"):
            said, at = text[len("source translation:"):].strip(), number
            while not re.search(r"[.?!]$", said):
                number = next_line(number)
                said += " " + paper.text(number)
            row(A, "translation", said, at, "the source's translation, after its label source translation:")
            number = next_line(number)
            if re.fullmatch(r"\(Hale \d{4}:\d+\)", paper.text(number)):
                row(A, "citation", paper.text(number), number, "under the translation")
                number = next_line(number)
            return number
        kind = "gloss" if kind == "transcription" or text.startswith("*") else "transcription"
        row(L, kind, text, number)
        number = next_line(number)


# The language of the corpus forms of each example, by the source its translation cites: Boas's
# Chinook texts (1894) are Lower Chinook, his Kathlamet texts (1901) Kathlamet, and Sapir's texts
# (1909) and Dyk's grammar (1933) Wishram. The missionary corpus of (11) and (12) is CW.
SOURCE_LANGUAGE = {"7": "Lower Chinook", "8": "Lower Chinook", "9": "Kathlamet", "10": "Kathlamet",
                   "11": L, "12": L, "18": "Kathlamet", "19": "Wishram", "20": "Wishram",
                   "21": "Kathlamet", "22": "Kathlamet"}
CLASSES = re.compile(r"^(?:\(\w+\)\s*)+$")


def corpus(start, where):
    """Examples (7) to (12): a form, a line of word classes under it in (7) and (8), its gloss,
    and the translation in quotes with its source."""
    label, first = opening(start)
    who = SOURCE_LANGUAGE[label]
    row = Rows(label)
    row(who, form_kind(first), first, start)
    last, number = "form", next_line(start)
    while True:
        text = paper.text(number)
        if CLASSES.match(text):
            row(A, "note", text, number, "the word class of each word over it")
        elif text.startswith("‘"):
            said, end = run_on(number)
            translation(row, said, number)
            return next_line(end)
        elif last == "gloss":
            row(who, form_kind(text), text, number)
            last = "form"
        else:
            row(who, "gloss", text, number)
            last = "gloss"
        number = next_line(number)


def chinookan_pairs(start, where):
    """Example (13): under its column heads, each CW word, the Chinookan word it is from and the
    English."""
    label, heads = opening(start)
    row = Rows(label)
    row(A, "note", heads, start, "heads the columns")
    number = next_line(start)
    while number not in HEADINGS:
        cw, chinookan, english = re.split(r"\s{3,}", paper.spaced[number].strip())
        row(L, "transcription", cw, number, "the Chinuk Wawa column")
        noted = re.match(r"^(\S+)\s+(\(.*\))$", chinookan)
        row(CHINOOKAN, form_kind(chinookan), noted.group(1) if noted else chinookan, number, "the Chinookan column")
        if noted:
            row(A, "note", noted.group(2), number, "beside the Chinookan word")
        row(A, "translation", english, number)
        number = next_line(number)
    return number


def townsend(row, start):
    """The line opening an example of Townsend's phrases: each phrase a transcription and the
    English beside it a translation, two of them set apart by a slash in (21). Phrases of his
    mixed language list are CW, those of his Chenook list Chinookan."""
    label = opening(start)[0]
    who = CHINOOKAN if label in ("21", "22") or int(label) >= 27 else L
    listed = "the Chenook list" if who == CHINOOKAN else "the mixed language list"
    line = re.sub(r"^\(\d+\)\s*", "", paper.spaced[start].strip())
    for pair in re.split(r"\s+/\s+", line):
        cells = re.split(r"\s{3,}", pair)
        row(who, "transcription", " ".join(cells[:-1]), start, "Townsend's phrase, from %s" % listed)
        row(A, "translation", cells[-1], start, "Townsend's English")


PROSE = re.compile(r"^[A-Z][a-z]+\b")
COMPARE = re.compile(r"^\(cf\. [^()]*:\)$")
GLOSSED = re.compile(r"^(\S+)\s+(‘[^’]*’)$")
NOTED = re.compile(r"^(.*?)\s+(\([^()]*\s[^()]*\))$")


def townsend_list(start, where):
    """Examples (14) to (22): Townsend's phrase and its English, then the comparisons under
    (cf. CW:) or (cf. Chinookan:), each a form over its gloss or its translation in quotes, and a
    note that runs on from (literally."""
    label = opening(start)[0]
    who = SOURCE_LANGUAGE.get(label, L)
    row = Rows(label)
    townsend(row, start)
    last, number = "translation", next_line(start)
    while number <= paper.last:
        text = paper.text(number)
        if gen.EXAMPLE.match(text) or number in HEADINGS or PROSE.match(text):
            return number
        if COMPARE.match(text):
            row(A, "note", text, number, "heads the comparison under it")
            last = "note"
        elif text.startswith("(literally"):
            said, number = run_on(number)
            row(A, "note", said, number)
            last = "note"
        elif text.startswith("‘"):
            said, end = run_on(number)
            translation(row, said, number)
            number, last = end, "translation"
        elif GLOSSED.match(text):
            form, gloss = GLOSSED.match(text).groups()
            row(who, form_kind(form), form, number)
            row(A, "translation", gloss, number)
            last = "translation"
        elif last == "form" and text.isascii():
            row(who, "gloss", text, number)
            last = "gloss"
        else:
            noted = NOTED.match(text)
            row(who, form_kind(noted.group(1) if noted else text), noted.group(1) if noted else text, number)
            if noted:
                row(A, "note", noted.group(2), number, "beside the form")
            last = "form"
        number = next_line(number)
    return number


# The left edge of the CW column of examples (23) to (30), by page.
CW_EDGE = {11: 240, 12: 220}
CELL = re.compile(r"^(?:(cf\.\?|\?)\s+)?(\S+)(?:\s+(‘[^’]*’\S*|‘\S+|\d[^()]*?))?(?:\s+(\(.*\)))?$")


def cell_rows(row, text, who, number, column):
    """A cell of the two columns: a doubt set before the form (? or cf.?), the form, its gloss in
    quotes or in capitals, and the notes in parentheses after it."""
    doubt, form, gloss, noted = CELL.match(text).groups()
    if doubt:
        row(A, "note", doubt, number, "%s, set before the form" % column)
    row(who, form_kind(form), form, number, column)
    if gloss:
        row(A, "translation", gloss, number, column + (", carries footnote 3" if gloss.endswith("’3") else ""))
    if noted:
        row(A, "note", noted, number, column)


def townsend_pairs(start, where):
    """Examples (23) to (30): Townsend's phrase and its English, the two column heads, and each
    line's Chinookan and CW words."""
    label = opening(start)[0]
    row = Rows(label)
    townsend(row, start)
    number = next_line(start)
    for head in re.split(r"\s{3,}", paper.spaced[number].strip()):
        row(A, "note", head, number, "heads the column under it")
    number = next_line(number)
    while number <= paper.last:
        text = paper.text(number)
        if gen.EXAMPLE.match(text) or number in HEADINGS or PROSE.match(text):
            return number
        edge = CW_EDGE[paper.page(number)]
        found = paper.word_positions(number)
        left = " ".join(REPAIR(" ".join(word for at, word in found if at < edge)).split())
        right = " ".join(REPAIR(" ".join(word for at, word in found if at >= edge)).split())
        assert " ".join(filter(None, (left, right))) == text, (number, left, right, text)
        if left:
            cell_rows(row, left, CHINOOKAN, number, "the Chinookan column")
        if right:
            cell_rows(row, right, L, number, "the CW column")
        number = next_line(number)
    return number


LISTS = {paper.find(r"^%s\s*The verbal sentence, consisting" % BULLET): paper.find(r"^While a Chinookan verbal sentence"),
         paper.find(r"^%s\s*The basic CW lexicon" % BULLET): paper.find(r"^Examples \(11\) and \(12\) show")}
blocks = {start: bullets for start in LISTS}
for number in range(1, REFERENCES):
    text = paper.text(number)
    if not content(number):
        continue
    opened = gen.EXAMPLE.match(text)
    if opened and opened.group(1).isdigit():
        example = int(opened.group(1))
        blocks[number] = hale if example <= 6 else corpus if example <= 12 else chinookan_pairs if example == 13 \
            else townsend_list if example <= 22 else townsend_pairs
for first in (r"^As a evidence that this Jargon", r"^It should also be noted that the Methodist",
              r"^As the Jargon is to be spoken", r"^One striking feature of Chinuk Wawa"):
    blocks[paper.find(first)] = quotation
blocks[paper.find(r"^and “Chenook tribe” word-lists$")] = heading_wrap
assert None not in blocks, blocks
assert sorted(int(opening(one)[0]) for one, block in blocks.items()
              if block not in (bullets, quotation, heading_wrap)) == list(range(1, 31)), sorted(blocks)

# The appendix.

# The right edges of the gloss and definition in Hale 1846, of Hale ca. 1841, of Townsend's 1835 list
# and of CW: the columns stand at 55, 177, 266, 371 and 466 on every page.
BOUNDS = (170, 260, 360, 460)
COLUMN = ("Hale 1846", "Hale ca. 1841", "Townsend's 1835 list", "CW", "Chinookan")
WHO = (HALE, DRAFT, TOWNSEND, L, CHINOOKAN)
# The appendix's notes, by the words each opens on.
NOTE_OPENS = ("The KC and LC stems", "Supposed to be derived", "Ross Clark", "Of uncertain origin",
              "I was unable to find", "Cf. also:", "Hale confuses two words", "Oregon grape")
TAGS = {"LC": "Lower Chinook", "KC": "Kathlamet", "UC": "Upper Chinook", "Chn": CHINOOKAN}
# The tags before a Chinookan form; a tag list, KC, UC ɬəl, splits into items.
TAGGED = re.compile(r"^((?:(?:cf\??|LC|KC|UC|Chn)(?:,?\s+|$))+)(.*)$")
# An entry opens on a line whose first column sets the gloss before a spaced slash.
ENTRY = re.compile(r"\S\s+/(?:\s|$)")
PAREN = re.compile(r"\((?:[^()]|\([^()]*\))*\)")
# A form with its gloss after it, in parentheses, in quotes or in double quotes.
GLOSS_AFTER = re.compile(r"(.*?\S)\s+([(‘“].*)")


def appendix_notes():
    """{mark: (lines, page)} for the eight notes of the appendix, each at the foot of its page."""
    found = {}
    for index, opens in enumerate(NOTE_OPENS, 1):
        first = paper.find(r"^%d %s" % (index, re.escape(opens)), APPENDIX)
        parts, number = [first], first + 1
        while number <= paper.last and content(number):
            parts.append(number)
            number += 1
        found[str(index)] = (parts, paper.page(first))
    return found


NOTES = appendix_notes()
AT_FOOT = {one for parts, _ in NOTES.values() for one in parts}


def cells(number):
    """{column: text} for an appendix line: each word to the column its left edge stands in, and a
    page number's digits left of the first column dropped."""
    held = {}
    for left, word in paper.word_positions(number):
        if left < 50:
            assert word.isdigit(), (number, word)
            continue
        column = sum(left >= bound for bound in BOUNDS)
        held[column] = held[column] + " " + word if column in held else word
    return {column: " ".join(REPAIR(text).split()) for column, text in held.items()}


def balanced(text):
    return text.count("(") <= text.count(")") and text.count("[") <= text.count("]") and \
        text.count("“") <= text.count("”")


def items(text):
    """The items of a cell set apart by commas and semicolons outside brackets and quotes, each
    without the comma or semicolon after it."""
    found, depth, quoted, current = [], 0, False, ""
    for index, one in enumerate(text):
        depth += one in "([“"
        depth -= one in ")]”"
        # A single quote opens a gloss after a space and closes it at the next ’; a ’ inside a
        # word, Townsend’s, is an apostrophe.
        if one == "‘" and text[index - 1:index] in (" ", ""):
            quoted = True
        elif one == "’" and quoted:
            quoted = False
        if one in ",;" and depth <= 0 and not quoted and text[index + 1:index + 2] in (" ", ""):
            found.append(current.strip())
            current = ""
            continue
        current += one
    found.append(current.strip())
    return [one for one in found if one]


def entry_rows(entry, state):
    """The rows of one appendix entry: the gloss a translation, each definition of each column a
    transcription of its source, a definition Hale's draft marks T (Townsend's) said so in its
    gloss, the cross references and the parenthesized notes of a line notes, and the note of each
    mark an item carries after them."""
    state["count"] += 1
    label = "appendix #%d" % state["count"]
    page = entry["page"]
    pieces = {column: [] for column in range(5)}
    for number, held in entry["lines"]:
        for column, text in held.items():
            pieces[column].append([text, number])
    gloss, spelling = [one.strip() for one in pieces[0][0][0].split("/", 1)] if "/" in pieces[0][0][0] \
        else (pieces[0][0][0], "")
    paper.add(label, A, "translation", gloss, "page %d, the appendix, the gloss in Hale 1846" % page)
    opened = pieces[0][0][0]
    pieces[0][0][0] = spelling
    marks = []
    for column in range(5):
        # A piece that leaves a bracket or a quote open runs on to the next line's.
        joined = []
        for text, number in pieces[column]:
            if joined and not balanced(joined[-1][0]):
                joined[-1][0] += " " + text
            else:
                joined.append([text, number])
        who, tags = WHO[column], ""
        for text, number in joined:
            for item in items(text):
                where_page = paper.page(number)
                mark = str(state["mark"])
                if re.search(r"(?<=[^\d\s])%s$" % mark, item):
                    item = item[:-len(mark)].rstrip(",;")
                    marks.append(mark)
                    state["mark"] += 1
                carries = ", carries note %s of the appendix" % mark if marks and marks[-1] == mark and \
                    state["mark"] == int(mark) + 1 and item else ""
                tagged = TAGGED.match(item) if column == 4 else None
                if tagged:
                    tags = (tags + ", " if tags else "") + tagged.group(1).strip().rstrip(",")
                    item = tagged.group(2)
                    if not item:
                        continue
                    # A form after two tags is of both, and Chinookan.
                    names = {TAGS[one] for one in re.findall(r"\b(?:LC|KC|UC|Chn)\b", tags)}
                    who = names.pop() if len(names) == 1 else CHINOOKAN
                about = "page %d, the appendix, %s" % (where_page, COLUMN[column])
                if column == 4 and tags:
                    # A cf or cf? before the tags compares the form after it, a note.
                    doubt = re.match(r"(cf\??)\s+", tags)
                    if doubt:
                        paper.add(label, A, "note", doubt.group(1), about + ", a comparison set before the form")
                        tags = tags[doubt.end():]
                    about += ", after %s" % tags.rstrip(",")
                    tags = ""
                # No definition in Hale 1846: the first cell as printed, the gloss and a star.
                if item == "*":
                    assert column == 0 and re.fullmatch(r".*\S\s+/\s+\*", opened), (label, opened)
                    paper.add(label, A, "note", opened, about + ", the gloss and a star, an item only in Hale ca. 1841"
                              + carries)
                    continue
                for part in re.split(r"\s+(?=\(T[\s?)])", item):
                    marked = re.fullmatch(r"\((T\??)\s+(.*)\)", part) or re.fullmatch(r"(T)\s+(.*)", part)
                    if marked:
                        for form in items(marked.group(2)):
                            paper.add(label, who, "transcription", form, about + ", marked %s" % marked.group(1) + carries)
                        continue
                    after = re.fullmatch(r"(.*\S)\s+\((T\??)\)", part)
                    if after:
                        paper.add(label, who, "transcription", after.group(1), about + ", marked %s" % after.group(2) + carries)
                    elif re.fullmatch(r"‘[^‘]*’", part):
                        paper.add(label, A, "translation", part, about + ", the gloss of the form above it" + carries)
                    elif PAREN.fullmatch(part) or part.startswith("(") and part.count("(") > part.count(")"):
                        paper.add(label, A, "note", part, about + carries)
                    elif GLOSS_AFTER.fullmatch(part):
                        form, said = GLOSS_AFTER.fullmatch(part).groups()
                        paper.add(label, who, "transcription", form, about + carries)
                        paper.add(label, A, "translation" if said.startswith("‘") else "note", said,
                                  about + ", after the form" + carries)
                    else:
                        paper.add(label, who, "transcription", part, about + carries)
    for mark in marks:
        parts, at = NOTES[mark]
        paper.footnote(mark, parts, at, NAMES, LANGUAGES, where="appendix footnote %s" % mark,
                       gloss="page %d, footnote %s of the appendix" % (at, mark))


def appendix():
    """The appendix's heading, the lines heading its columns, which each page repeats, and its
    entries."""
    paper.add("appendix", A, "heading", paper.joined([APPENDIX, APPENDIX + 1]), "page %d" % paper.page(APPENDIX))
    heads, number = [], next_line(APPENDIX + 1)
    while not ENTRY.search(cells(number).get(0, "")):
        heads.append(paper.text(number))
        for cell in re.split(r"\s{3,}", paper.spaced[number].strip()):
            paper.add("appendix", A, "note", cell, "page %d, heads the columns, and every page of the appendix repeats it"
                      % paper.page(number))
        number = next_line(number)
    entries = []
    for number in range(number, paper.last + 1):
        if not content(number) or number in AT_FOOT or paper.text(number) in heads:
            continue
        held = cells(number)
        if not held:
            continue
        first = held.get(0, "")
        if ENTRY.search(first) or first == "here (this)":
            entries.append({"page": paper.page(number), "lines": [(number, held)]})
        else:
            entries[-1]["lines"].append((number, held))
    state = {"count": 0, "mark": 1}
    for entry in entries:
        entry_rows(entry, state)
    assert state["mark"] == 9, state


# The forms in the prose that are not CW, by the language the text or the example it discusses
# gives them, and the affixes whose hyphen the italics leave upright.
# (where, form as read): (language, form as printed or None).
LC, KC = "Lower Chinook", "Kathlamet"
FORMS = {("§3", "χ"): (CHINOOKAN, None), ("§3", "tχl"): (CHINOOKAN, None), ("§3", "tʊ̆qéχ"): (CHINOOKAN, None),
         ("§3", "ƛuɬ"): ("Nootkan", None),
         ("§4", "ɬ‑ʔɑ́gil"): (LC, None), ("§4", "ɬɑ́‑kikɑl"): (LC, None), ("§4", "ɬ‑"): (LC, None),
         ("§4", "ɬɑ‑"): (LC, None), ("§4", "‑mǝqt"): (LC, None), ("§4", "tqʼiχ"): (LC, None),
         ("§4", "‑χ"): (LC, None), ("§4", "g-"): (LC, None), ("§4", "ɬ-"): (LC, None), ("§4", "gɑ‑"): (LC, None),
         ("§4", "‑kiwisx"): (LC, None), ("§4", "‑kúsɑit"): (KC, None), ("§4", "i‑"): (KC, None),
         ("§4", "χ‑"): (KC, None), ("§4", "š"): (KC, "š‑"), ("§4", "‑kusɑit"): (KC, None),
         ("§4", "iɑ"): (KC, "iɑ‑"), ("§6", "-x̣"): (CHINOOKAN, None),
         ("appendix footnote 1", "-qʼuču"): (KC, None), ("appendix footnote 1", "–(ʔ)uču"): (LC, None),
         ("appendix footnote 1", "u-"): (LC, None), ("appendix footnote 1", "i-"): (LC, None),
         ("appendix footnote 3", "hiˑluˑ"): ("Haida", None),
         ("appendix footnote 4", "-pdu"): ("Kalapuyan", None), ("appendix footnote 4", "-pduʔ"): ("Kalapuyan", None),
         ("appendix footnote 4", "wɑ-"): ("Upper Chinook", None),
         ("appendix footnote 6", "wɑwɑ"): ("Nootka Jargon", None)}


def as_printed():
    """Set each prose form's language and printed form."""
    for row in paper.rows:
        where, _, kind, form, _ = row
        if kind == "cited form" and (where, form) in FORMS:
            language, printed = FORMS.pop((where, form))
            row[1], row[3] = language, printed or form
    assert not FORMS, FORMS


paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, headings=HEADINGS, appendix=r"^Appendix\s")
appendix()
as_printed()
paper.write()
