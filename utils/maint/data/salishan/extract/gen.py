"""The shared half of a paper's generator: its lines, its paragraph starts, its footnotes, its
references, and the ops file the rows become.

A paper's own generator imports this module and states only what is the paper's: which lines are
headings, how its examples are laid out, which names and languages it holds.

    import gen
    paper = gen.Paper("ICSNL56_Zenk_final")
    paper.add("front", gen.A, "title", paper.text(2), "page 1")
    ...
    paper.write()                     # ops/<stem>.ops, the rows chained in order
"""
import os
import re
import sys
import time
import unicodedata

import pypdfium2 as pdfium

HERE = os.path.dirname(os.path.abspath(__file__))
# residue puts a public tree's corpus_script_extraction ahead on the path, and its page_text.py is
# another module of the same name. This directory's own is imported first and kept.
sys.path.insert(0, HERE)
from workdir import PRIVATE  # noqa: E402
import page_text  # noqa: E402,F401
import residue  # noqa: E402
# CORPUS is read by the generators as gen.CORPUS and ORACLES by tier_check.py as gen.ORACLES.
from workdir import CORPUS, ORACLES  # noqa: E402,F401
A, L = "@a", "@l"
# A numbered example's first line, (12) and the words after it; a footnote numbers its own (i), (ii).
EXAMPLE = re.compile(r"^\((\d{1,3}|[ivx]{1,4})\)(?:\s+(.*))?$")
# A sub-example's letter, a. and the words after it. A judgment can stand against the letter,
# a.? t̕sux̱w'idi Simon in Sardinha's second paper.
SUB = re.compile(r"^([a-z])\.(?:\s+|(?=[?*#]))(.*)$")
# The raised letters of the orthographies, by the plain letter an italic run can read one as.
RAISED = {"w": "ʷ", "h": "ʰ", "y": "ʸ", "j": "ʲ"}
# What stands right of a translation: a parenthesis or bracket, one level of nesting inside it,
# with a language tag before it or not, G (VG), CT (Forbes 2018), (Independent), [JS 74].
TAG = r"[A-Z]{1,3}(?:\+[A-Z]{1,3})?"
PIECE = r"(?:%s\s+)?(?:\((?:[^()]|\([^()]*\))*\)|\[[^\[\]]*\])\.?" % TAG
PIECES = re.compile(r"^(?:\s*%s)+\s*$" % PIECE)
SOURCE = PIECES
# A transcription with its source at its right, a starred form given no tiers:
# * Hasagathl miyup.   G (Rigsby 1986:286)
TAGGED = re.compile(r"^(.*\S)\s+(%s\s+\([^()]*\))$" % TAG)


def pieces_of(rest):
    """The pieces of what stands right of a translation, each a source or a note."""
    rest = rest.strip()
    if not rest:
        return []
    found = re.findall(PIECE, rest)
    return found if found and PIECES.match(rest) else [rest]


def is_source(piece):
    """Whether a piece right of a translation names where the example comes from: a language tag
    before it, a year, a page or line, a text's title in brackets, or a speaker's initials."""
    inner = piece.strip()
    if re.match(r"^%s(?:\s|$)" % TAG, inner):
        return True
    inner = inner.strip("()[]. ")
    # A label in capitals in brackets, [PREVIOUS DIRECT EVIDENCE], names what the example shows.
    label = re.fullmatch(r"\[[A-Z]+(?:[ :,-]+[A-Z]+)+\]", piece.strip())
    # A year can run on to its month and day, (20160712 VF) in Sardinha's eventuality types.
    return bool(re.search(r"\b(?:1[89]|20)\d\d(?:[01]\d[0-3]\d)?\b|:\s*\d|\bline\b|\bp\.\s*\d|\bin prep\b|\bet al\b|\bp\.c\.|"
                          r"\bpers(?:onal|\.) comm", inner)
                or re.fullmatch(r"[A-Z]{2,3}(?:[,;/ ]+[A-Z]{2,3})*", inner)
                # A field notebook's page, (W2.88), (EP4.44.7), (ECH.ED.90.CD.l21) in Kinkade's
                # Nxaʔamxčín notes.
                or re.fullmatch(r"[A-Z]{1,4}\d*(?:\.\w+)+", inner)
                or piece.strip().startswith("[") and not label)


def split_translation(line):
    """(translation, pieces right of it) for a line opening on a quote. The translation closes at
    the last closing quote after which only pieces stand; a translation printed without its
    closing quote ends where the pieces begin. Returns None where the line ends mid-translation."""
    # A translation of quoted speech opens and closes on double quotes, "Just keep going!".
    closing = [index for index, symbol in enumerate(line) if symbol == ("”" if line.startswith("“") else "’")]
    for at in reversed(closing):
        # A footnote mark on the closing quote stays with the translation, ‘I cannot figure it out.’1
        mark = re.match(r"^\d{1,2}(?=\s|$)", line[at + 1:])
        if mark:
            at += len(mark.group(0))
        rest = line[at + 1:].strip()
        # A footnote mark on a piece's closing parenthesis stays with the piece, (i.e., my name
        # is Qwa7yán'ak)13 under H. Davis's (56c).
        bare = re.sub(r"(?<=\))\d{1,2}$", "", rest)
        if not rest or PIECES.match(rest) or re.fullmatch(TAG, rest) or bare != rest and PIECES.match(bare):
            return line[:at + 1], pieces_of(rest)
    # A source that is more than pieces, CT Boas (1911:385), cited in Mulder (1989), stands whole.
    if closing and re.match(r"^\s*(?:%s\s|[(\[])" % TAG, line[closing[-1] + 1:]):
        return line[:closing[-1] + 1], [line[closing[-1] + 1:].strip()]
    trailing = re.match(r"^(.*?\S)\s*((?:%s\s*)+)$" % PIECE, line)
    if trailing:
        return trailing.group(1), pieces_of(trailing.group(2))
    if line.rstrip().endswith((".", "?", "!")):
        return line, []
    return None


# A consultant's comment set under an example, Consultant's comment: You own your hair, ..., or
# under the consultant's initials, HH: It's like the book has arms in Forbes's transitivity.
COMMENT = re.compile(r"^(?:(?:Consultant|Speaker)(?:’s|'s)? comment|Comment):|^[A-Z]{2,3}:\s")
# The context an example is given in, Context: or Context 2: where it is given two.
CONTEXT = re.compile(r"^Context(?: \d+)?:")
# An exchange set as an example, Interviewer: ‘What about ...’ / Consultant: ‘...’.
DIALOGUE = re.compile(r"^(Interviewer|Consultant):\s")
# A part of a lettered part, i. Context: ... under a. Stative.
ROMAN_PART = re.compile(r"^(i{1,3}|iv|vi{0,3})\.\s+(.*)$")
# A translation judged, ?? ‘The vase is touched.’
JUDGED_QUOTE = re.compile(r"^[?#*]+\s*‘")
# A translation under a label, Intended interpretation: ‘It’s this man that owns the dog.’ A
# consultant's comment in quotes, Comment: ‘That’s not true.’ in Martin, is no translation.
LABELED_QUOTE = re.compile(r"^(?!Interviewer:|Consultant:|Comment:)[A-Z][a-z]+(?: [a-z]+){0,2}:\s*‘")
# A lettered part's heading over its own parts, a. Stative.
PART_HEAD = re.compile(r"^[A-Z][a-z]+(?: [a-z]+){0,2}$")


def is_gloss(line):
    """Whether a tier is a gloss: it holds a grammatical label in capitals, DET, 3POSS, 1SG.SBJ,
    CTR\\STAT, where a transcription or a segmentation holds forms of the language. A clitic's join
    bounds a label too, Nater's PREP˽ART and Lyon and Czaykowska-Higgins's Q‿IPFV."""
    return bool(re.search(r"(?:^|[\s=\-.~+\\<\[(˽‿])(?:[123](?:SG|PL|POSS|ERG|OBJ|SBJ)|[A-Z]{2,})"
                          r"(?=$|[\s=\-.~+\\>\])˽‿])", line))


def sentences(body):
    """A text in the language cut into its sentences, after each full stop that stands outside a
    quotation or closes one: a quotation stays whole with the words that say who spoke it,
    "χəpǰišθ ga!" natəm k̓ʷa tə titol ǰɛnxʷ."""
    out, current, depth = [], "", 0
    for at, letter in enumerate(body):
        current += letter
        depth += {"“": 1, "”": -1}.get(letter, 0)
        ended = depth == 0 and (letter == "." or letter == "”" and body[at - 1:at] == ".")
        if ended and body[at + 1:at + 2] in (" ", "") and not (letter == "." and body[at + 1:at + 2] == "”"):
            out.append(current.strip())
            current = ""
    return out + ([current.strip()] if current.strip() else [])


# A section heading: its number and a title with no closing stop. The title can open on a bracket,
# 3 (Regular) Demonstratives in Huijsmans and Reisinger's clausal demonstratives.
HEADING = re.compile(r"^(\d+(?:\.\d+)*)\.?\s+([A-Z‘“ʔ(][^.]{0,80}[^.:,;])$")


def successor(label, previous):
    """Whether section label may follow section previous: 2 after 1.3, 1.2 after 1.1, 1.1 after 1."""
    now = [int(one) for one in label.split(".")]
    if previous is None:
        return now in ([1], [0])
    before = [int(one) for one in previous.split(".")]
    if len(now) == len(before) + 1:
        return now[:-1] == before and now[-1] == 1
    if len(now) > len(before) + 1:
        return False
    return now[:-1] == before[:len(now) - 1] and now[-1] == before[len(now) - 1] + 1


def orthographic(run):
    """Whether an italic run is a cited form of the language: a letter outside plain English, or
    an affix's hyphen or a root's √ at its edge. A run of four words or more, most of them plain
    English letters, is an italicized sentence that names a language, not a form of it."""
    words = run.split()
    if len(words) >= 4 and sum(bool(re.fullmatch(r"[A-Za-z()\-,.;:]+", one)) for one in words) > 0.6 * len(words):
        return False
    if any(ord(symbol) > 127 and symbol not in "‘’“”–—…" for symbol in run):
        return True
    # Two shapes of one word across a tilde, Shuswap qm~qem-t in Nater's old records, in plain letters.
    if re.fullmatch(r"[\w()-]+~[\w()-]+", run):
        return True
    return run.startswith(("-", "√")) or run.endswith("-")


def letters(text):
    """text with its spaces taken out, the key a page-text line and a glyph row share."""
    return "".join(text.split())


def indented_rows(stem, step=10.0, reach=40.0):
    """The glyph rows of each page whose first glyph stands step to reach points right of the
    page's common left edge, as {page: [letters of the row]}: the first lines of paragraphs, which
    a text layer keeps no indent for. The common edge is the first quartile of the rows' left edges,
    as indents.py reads it."""
    document = page_text.paper_document(stem)[0]
    found = {}
    for number in range(len(document)):
        textpage = document[number].get_textpage()
        rows = {}
        for index in range(textpage.count_chars()):
            symbol = textpage.get_text_range(index, 1)
            if not symbol or symbol.isspace():
                continue
            left, bottom, right, top = textpage.get_charbox(index, loose=True)
            rows.setdefault(round(bottom / 3.0), []).append((left, symbol))
        lefts = sorted(min(one for one, _ in row) for row in rows.values())
        margin = lefts[len(lefts) // 4] if lefts else 0
        for key in sorted(rows, reverse=True):
            row = sorted(rows[key])
            if margin + step <= row[0][0] < margin + reach:
                found.setdefault(number + 1, []).append("".join(symbol for _, symbol in row))
    return found


def row_sizes(stem):
    """{page: (body size, [(letters of the row, font size)])}: each glyph row's median font size
    over its letters, top to bottom, and the paper's body size, the size most of its letters have.
    The body size is the paper's and not the page's: a first page can hold more abstract and
    footnote than body."""
    import statistics
    document = page_text.paper_document(stem)[0]
    found, counts = {}, {}
    for number in range(len(document)):
        textpage = document[number].get_textpage()
        rows = {}
        for index in range(textpage.count_chars()):
            symbol = textpage.get_text_range(index, 1)
            if not symbol or symbol.isspace():
                continue
            left, bottom, right, top = textpage.get_charbox(index, loose=True)
            # A paper that sets its type at size 1 under a scaling matrix reads 1 everywhere;
            # page_text.glyph_size takes the matrix's scale.
            size = round(page_text.glyph_size(textpage, index), 1)
            rows.setdefault(round(bottom / 3.0), []).append((left, symbol, size))
            if symbol.isalpha():
                counts[size] = counts.get(size, 0) + 1
        listed = []
        for key in sorted(rows, reverse=True):
            row = sorted(rows[key])
            sizes = [one[2] for one in row if one[1].isalpha()]
            if sizes:
                listed.append(("".join(one[1] for one in row), statistics.median(sizes)))
        found[number + 1] = listed
    body = max(counts, key=counts.get) if counts else 0
    return {number: (body, listed) for number, listed in found.items()}


# A footnote's opening mark: a number, or a symbol for a note on the title.
# The note's text opens on a capital, a quote, a bracket or a digit; a table row set small above
# the notes opens on a lowercase form, 2 səsaʔliʔ.
# A note may open on a glottal stop, 7 ʔayʔaǰuθəm has, or on a lowercase form set flush against its
# number, 9yɩm- /yəm-/; a table row has a space there. It may open on a phonemic or morphological
# form, 3 /ʔəm/ → [ʔam], 5 {C1V1-C1ə-}. A note may be a bracketed address alone, 6
# <http://academic.uprm.edu/~sbischoff/COLRC/texts/> in Bischoff et al. It may open on an accented
# capital, 1Áístainskiaakii in Aistainskiaakii et al.
MARK = re.compile(r"^(\d{1,2}|[*∗†‡§])(?:\s*(?=[A-ZÀ-ÖØ-Þ‘’“(\[\dʔ/{<])|(?=[a-zɐ-ʯ]))")
# The marks of a note on the title.
TITLE_MARKS = "∗*†"


def title_mark(text):
    """The note mark closing a title, or None."""
    found = re.search("[%s]+$" % re.escape(TITLE_MARKS), text)
    return found.group(0) if found else None
# The kinds of row that hold forms of the language, where a footnote mark is looked for only at the
# row's end.
FORM_KINDS = ("transcription", "phonetic", "phonemic", "underlying")
# The key page_footnotes gives the small lines at the foot of the title's page that carry no mark.
UNMARKED = "unmarked"


class Paper(object):
    """A paper's page text, a line to an entry, and the rows its generator adds."""

    def __init__(self, stem, authors=None, language=None):
        self.stem = stem
        self.authors, self.language = authors, language
        repair = residue.paper_repair(stem)
        # lines[n] is (text, page, is_page_marker) for line n of the page text, 1-based.
        self.lines = [("", 0, True)]
        # spaced[n] is line n as the page text sets it, its runs of spaces kept for the columns.
        self.spaced = [""]
        page = 0
        for raw in open(residue.source_path(stem), encoding="utf-8").read().split("\n"):
            marked = re.match(r"^===== page (\d+) =====$", raw.strip())
            self.spaced.append("" if marked else repair(raw).strip())
            if marked:
                page = int(marked.group(1))
                self.lines.append(("", page, True))
                continue
            self.lines.append((" ".join(repair(raw).split()), page, False))
        self.last = len(self.lines) - 1
        self.rows = []
        self.used = set()
        self._starts = None
        self._italics = None
        # The kind of an example's first line: a transcription over its segmentation and gloss, or
        # the segmentation itself where the paper sets two tiers.
        self.opening = "transcription"
        self.cited_done = set()
        self.mentioned = set()

    # The page text.

    def text(self, number):
        return self.lines[number][0]

    def page(self, number):
        return self.lines[number][1]

    def joined(self, numbers):
        """The lines' text run together with a space, or with none after a line that ends on a
        hyphen where the paper sets hyphen_ends_join and the next line opens on a letter, a digit or
        a closing bracket: the paper breaks a line only at a hyphen it prints, taboo- / driven for
        taboo-driven and (morpho- / )syntax. An affix's hyphen before its gloss, *-mi- / '2SG.OBJ',
        keeps the space. Where the paper sets address_ends_join, a line ending inside a web address
        joins with none to a line opening on a lower-case letter or &, muckleshoot08m. / html."""
        out = ""
        for one in numbers:
            text = self.text(one)
            if not text:
                continue
            closes = getattr(self, "hyphen_ends_join", False) and out.endswith("-") and re.match(r"[^\W_]|\)", text)
            closes = closes or getattr(self, "address_ends_join", False) and \
                re.search(r"(?:https?://|www\.)\S*$", out) and re.match(r"[a-z&]", text)
            glue = "" if not out or closes else " "
            out += glue + text
        return out

    def find(self, pattern, start=1, end=None):
        """The first line number from start whose text matches pattern, or None. An end past the
        paper's last line stops at it, front()'s 200 in Abraham's page text of 106 lines."""
        for number in range(start, min(end or self.last, self.last) + 1):
            if re.search(pattern, self.text(number)):
                return number
        return None

    def running_numbers(self):
        """The line numbers holding only a printed page number, 3 digits alone on a line."""
        return {number for number in range(1, self.last + 1)
                if re.fullmatch(r"\d{1,3}", self.text(number))}

    def paragraph_starts(self, step=10.0):
        """The line numbers whose page prints them indented: a paragraph's first line."""
        if self._starts is None:
            rows = indented_rows(self.stem, step)
            self._starts = set()
            for number in range(1, self.last + 1):
                text = self.text(number)
                key = letters(text)[:24]
                if len(key) < 6:
                    continue
                if any(letters(row).startswith(key) or key.startswith(letters(row)[:24])
                       for row in rows.get(self.page(number), ()) if len(letters(row)) >= 6):
                    self._starts.add(number)
        return self._starts

    # Footnotes.

    def footnotes(self, marks, start=1, end=None, stops=()):
        """{mark: (line numbers, page)} for each footnote, found in order of marks: a footnote
        opens on a line that begins with its mark and a word, and runs to the next mark's line, the
        page's end, or a line in stops. Marks are strings, "*" or "1" and on."""
        found, expect = {}, 0
        end = end or self.last
        number = start
        while number <= end and expect < len(marks):
            mark = marks[expect]
            if re.match(r"^%s ?[A-Z‘“(\[a-z]" % re.escape(mark), self.text(number)) and not self.lines[number][2]:
                parts = [number]
                run = number + 1
                following = marks[expect + 1] if expect + 1 < len(marks) else None
                while run <= end and not self.lines[run][2] and run not in stops:
                    if following and re.match(r"^%s ?[A-Z‘“(\[a-z]" % re.escape(following), self.text(run)):
                        break
                    if re.fullmatch(r"\d{1,3}", self.text(run)):
                        break
                    parts.append(run)
                    run += 1
                found[mark] = ([one for one in parts if self.text(one)], self.page(number))
                expect += 1
                number = run
                continue
            number += 1
        missing = [one for one in marks if one not in found]
        if missing:
            print("# footnotes not found:", " ".join(missing), file=sys.stderr)
        return found

    def small_lines(self, below=0.4):
        """The line numbers whose glyph row is set more than below points under its page's body
        size: footnotes, table notes, the abstract where it is set small."""
        if getattr(self, "_small", None) is None:
            sizes = row_sizes(self.stem)
            self._small = set()
            # The lines matched to a glyph row at all; a line no row matches has no size.
            self._sized = set()
            for number in range(1, self.last + 1):
                key = letters(self.text(number))
                if len(key) < 3 or self.lines[number][2]:
                    continue
                body, rows = sizes.get(self.page(number), (0, []))

                def matches(key, row_key):
                    # A short line, -an. closing a footnote, matches only a row of the same letters;
                    # a longer one a row of about its length, never the short affiliation Salish
                    # School of Spokane its note opens with.
                    return (row_key == key) if len(key) < 8 else (
                        (row_key[:20] in key[:28] or key[:20] in row_key[:28]) and
                        abs(len(row_key) - len(key)) <= max(8, len(key) // 3))

                def shared(key, row_key):
                    return next((at for at, (one, other) in enumerate(zip(key, row_key)) if one != other),
                                min(len(key), len(row_key)))

                # The rows can leave out a note's raised mark, 3 opening Martin's note on Hindle and
                # Rigsby (1973), and the mark in a body line, (2)2: the line's letters are matched
                # without its opening mark, then without its digits, and of the rows that match, the
                # one sharing the longest opening with the line wins, never a body line naming the
                # same authors.
                mark = re.match(r"^(?:\d{1,2}|[∗*†‡§¶])\s+(?=\S)", self.text(number))
                bare = letters(self.text(number)[mark.end():]) if mark else key
                best = None
                for trial in (lambda text: text, lambda text: re.sub(r"\d", "", text)):
                    line_key = trial(bare)
                    for row, size in rows:
                        row_key = trial(letters(row))
                        if len(row_key) < 3 or not matches(line_key, row_key):
                            continue
                        if best is None or shared(line_key, row_key) > best[0]:
                            best = (shared(line_key, row_key), size)
                    if best:
                        break
                if best:
                    self._sized.add(number)
                    if best[1] < body - below:
                        self._small.add(number)
        return self._small

    def volume_header(self):
        """The line numbers of the volume's header, Papers for the International Conference ...
        through the line that closes on the year or UBCWPL, three lines at most. Volume 53 opens
        its header on In Papers for."""
        first = self.find(r"^(?:In )?Papers (?:for|of) the International Conference", 1, min(self.last, 200))
        if not first:
            return []
        for number in range(first, min(first + 3, self.last + 1)):
            if re.search(r"UBCWPL|\b(?:19|20)\d\d\.?$", self.text(number)):
                return list(range(first, number + 1))
        return [first]

    def page_footnotes(self, stops=(), symbols_on=(1,)):
        """{mark: (line numbers, page)} for the footnotes read from the type size: on each page,
        the run of small lines that ends the page (before its number), from its first line opening
        on a mark, a new note at each line opening on the next number in sequence or on a symbol.
        A symbol mark counts only on the pages of symbols_on, where a note on the title stands.
        The volume's header, set small wherever the page puts it, is left out, and so are the
        lines of skip. The numbers run from the paper's first_footnote, 1 where it sets none: 2 in
        Mellesmoen and Andreotti, whose title carries a mark 1 with no note. A paper's
        body_size_notes, where it names them, are the lines of a note set at the body size and read
        as small ones: Black's note 4, under the rule at the foot of page 4."""
        small = self.small_lines() | set(getattr(self, "body_size_notes", ()))
        running = self.running_numbers_set()
        header = self.volume_header()
        found, expect, current = {}, getattr(self, "first_footnote", 1), None
        by_page = {}
        for number in range(1, self.last + 1):
            if self.lines[number][2] or not self.text(number) or number in running or number in header \
                    or number in stops:
                continue
            by_page.setdefault(self.page(number), []).append(number)
        for page in sorted(by_page):
            numbers = by_page[page]
            # The run ends at a line set at the body size; a line no glyph row matches is kept.
            tail = []
            for number in reversed(numbers):
                if number not in small and number in self._sized:
                    break
                tail.insert(0, number)
            # A note whose example the page sets at the body size, (iv) under a note 15, breaks the
            # run: a note opens at the first small line of the page with the next number and a
            # space after it, and runs to the page's end.
            opener = next((one for one in numbers if one in small and re.match(r"^%d\s" % expect, self.text(one))),
                          None)
            if opener is not None and (not tail or opener < tail[0]):
                tail = numbers[numbers.index(opener):]
            # A table set small over the notes with its caption under it, Table 1 of Sardinha's 2011
            # paper over her note 4: the notes open at the opener past the caption.
            if opener in tail and any(re.match(r"^Table \d+[:.]", self.text(one)) for one in tail[:tail.index(opener)]):
                tail = tail[tail.index(opener):]
            # A note's own example that it names, (iii), set in the lower half of the next page
            # over the notes there, runs on too.
            if current in found and tail:
                parts = found[current][0]
                named = set(re.findall(r"\(([ivx]{1,4})\)", self.joined(parts))) - \
                    {EXAMPLE.match(self.text(one)).group(1) for one in parts if EXAMPLE.match(self.text(one))}
                head = next((one for one in numbers[len(numbers) // 2:] if one < tail[0]
                             and EXAMPLE.match(self.text(one)) and EXAMPLE.match(self.text(one)).group(1) in named),
                            None)
                if head is not None:
                    tail = numbers[numbers.index(head):]
            # A note the page before left mid-sentence runs on at the head of this page's notes.
            last_text = self.text(found[current][0][-1]).rstrip() if found and current in found else ""
            # So does one whose example, (iii), the page sets at the head of its notes.
            opened = EXAMPLE.match(self.text(tail[0])) if tail else None
            example_of_last = opened and current in found and not opened.group(1).isdigit() and \
                "(%s)" % opened.group(1) in self.joined(found[current][0])
            if not (last_text and not re.search(r"[.!?)\]’”]$|https?://\S+$|www\.\S+$", last_text)) \
                    and not example_of_last:
                current = None
            for number in tail:
                # A table's caption the layer reads after the notes, Table 4 of Pincott over its note 6, ends the notes.
                if re.match(r"^Table \d+[:.]", self.text(number)):
                    break
                mark = MARK.match(self.text(number))
                opens = mark and (mark.group(1) == str(expect) or not mark.group(1).isdigit() and page in symbols_on)
                # A note that opens on its number and a word in lower case or a sign, 2 bmnac.org.au
                # in Webb, 44 ¬ in Lyon and Czaykowska-Higgins.
                if not opens and number == opener:
                    mark, opens = re.match(r"^(%d)" % expect, self.text(number)), True
                # A mark the glyph rows set on a line of its own over its note, 1 over Many thanks on
                # page 1 of Urbanczyk's word-based morphology, reads as a page number; the note opens
                # on the line under it, the first small line of the page's foot, opening on a capital.
                if not opens and number == next((one for one in tail if one in small), None) and \
                        number - 1 in running and self.text(number - 1) == str(expect) and \
                        re.match(r"^[A-Z]", self.text(number)):
                    mark, opens = re.match(r"^(%d)$" % expect, self.text(number - 1)), True
                # A note that opens on a lowercase form, 17 g̱a̱l- in Sardinha's first paper, opens on
                # the next number when the note over it ended its sentence.
                if not opens and current in found and re.match(r"^%d\s+[^\s\d]" % expect, self.text(number)) \
                        and re.search(r"[.!?)’”]$", self.text(found[current][0][-1]).rstrip()):
                    mark = re.match(r"^(%d)" % expect, self.text(number))
                    opens = True
                if opens:
                    current = mark.group(1)
                    found[current] = ([number], page)
                    if current.isdigit():
                        expect += 1
                elif current:
                    found[current][0].append(number)
                elif page in symbols_on and not SUB.match(self.text(number)) and any(
                        MARK.match(self.text(one)) and MARK.match(self.text(one)).group(1) in (str(expect), *TITLE_MARKS)
                        for one in tail):
                    # Small lines over the first note of the title's page with no mark of their
                    # own, the author's thanks and e-mail: a note on the paper as a whole. The note
                    # under them opens on the next number or a symbol; a section set small at the
                    # page's foot, 3 ʔayʔaǰuθəm Text, is no note, and neither is an example's
                    # lettered line set small, (1) a. ɬíc̓ət on page 1 of Mellesmoen and Urbanczyk.
                    found.setdefault(UNMARKED, ([], page))[0].append(number)
        return found

    @staticmethod
    def body_of(text, mark):
        """A footnote's text with its mark taken off."""
        return re.sub(r"^%s ?" % re.escape(mark), "", text, count=1)

    # References.

    def references(self, first, last,
                   opens=r"^[A-Z][\w’'\-]+(?:,| &| and)|^[A-Z][\w’'\-]+(?: [A-Z][\w’'\-]+)*\. \d{4}\."
                         r"|^[A-Z][\w’'\-]+ \(\d{4}\)\.",
                   skip=()):
        """The entries of a reference list from line first to last, each (text, page): an entry
        opens on a line matching opens after a line that ends one (a stop, a URL's slash, a closing
        parenthesis or bracket, a digit, a web address). The lines in skip, a footnote at the foot
        of a page of references, are no entry's. A paper's reference_lines_run_on, where it names
        them, continue the entry above though they open like one: Black's Washington, Government
        Printing Office, the place of Boas 1911."""
        entries, previous = [], "."
        run_on = set(getattr(self, "reference_lines_run_on", ()))
        for number in range(first, last + 1):
            text = self.text(number)
            if not text or number in skip or self.lines[number][2] or re.fullmatch(r"\d{1,3}", text):
                continue
            # An entry ends on a web address too, http://bmnac.org.au in Webb.
            # An editor's initial ends no entry: edited by J. over Almog, J. Perry, and H. K.
            # Wettstein in H. Davis's proper names.
            ended = (previous.rstrip()[-1:] in ".)]/" + "0123456789" or re.search(r"https?://\S+$", previous)) and \
                not re.search(r"\bedited by\s+(?:[A-Z]\.\s*)+$", previous)
            if not entries or (re.match(opens, text) and ended and number not in run_on):
                entries.append([text, self.page(number)])
            else:
                glue = "" if entries[-1][0].endswith("-") and text[:1].islower() is False else " "
                # A web address broken at the line's end, where the paper sets address_ends_join.
                if getattr(self, "address_ends_join", False) and \
                        re.search(r"(?:https?://|www\.)\S*$", entries[-1][0]) and re.match(r"[a-z&]", text):
                    glue = ""
                entries[-1][0] += glue + text
            previous = text
        return [tuple(one) for one in entries]

    def merge_references(self, opens=r"^[A-Z][\w’'\-]+, [A-Z]"):
        """Run each reference row onto the entry above it where the row opens no entry by opens, or
        the entry above ends on an initial. The reference reader opens an entry on any line after a
        stop that starts on a capitalized word and a comma. That misreads a wrap after an editor's
        initial, In T. Honma, M. / Okazaki in Brown's heavy syllables, and a wrap onto a place,
        Ussery. / Amherst, MA: GLSA. in Turner. A paper whose every entry opens Surname, X. merges
        them back with the default; one that also gives the year, Turner's, can require it."""
        merged = []
        for row in self.rows:
            if row[2] == "reference" and merged and merged[-1][2] == "reference" and (
                    re.search(r"\b[A-Z]\.$", merged[-1][3]) or not re.match(opens, row[3])):
                merged[-1][3] += " " + row[3]
                continue
            merged.append(row)
        self.rows = merged

    # Headings, cited forms and names.

    def headings(self, first, last, skip=()):
        """{line: label} for the numbered section headings from line first to last: a line that
        opens on a section number and a title with no closing stop, taken only where its number
        follows the heading before it (2 after 1.3, 2.1 after 2), which keeps out numbered footnotes
        and table rows."""
        found, previous = {}, None
        for number in range(first, last + 1):
            if number in skip or self.lines[number][2]:
                continue
            matched = HEADING.match(self.text(number))
            # A table row that opens on a numeral, 4   PS *mus   86% in Denzer-King's (2), sets a
            # column gap after its first cell as well; a heading sets one after its number alone.
            if matched and re.search(r"\S\s{3,}\S.*\S\s{3,}\S", self.spaced[number]):
                continue
            if matched and successor(matched.group(1), previous):
                found[number] = matched.group(1)
                previous = matched.group(1)
        return found

    def italics(self):
        """{page: [italic runs]} of the paper, read once."""
        if self._italics is None:
            import italic_runs
            self._italics = {}
            # The runs come off the glyphs decomposed, n(i)snáq with its acute apart, and the page
            # text is composed; a run is matched in the text as the text defines it. A run that sets
            # forms side by side is each of them: an alternation, c̓uwá ~ suwá, the languages of a
            # table's row, naʔ / cəná ~ səná, and the stages of a change, *kúkum > *kúkʷum. A glyph
            # the paper's font keeps in the private use area reads as the page text reads it, the
            # k̓ of k̓ʷək̓ʷʔitas in Mellesmoen and Huijsmans's footnote 24.
            private = page_text.PRIVATE_USE.get(self.stem, {})
            # A paper whose font declares its letters at codes of its own names them in
            # italic_letters, the GenSal ¯ of Thompson and Sloat's q̇ʷélo.
            private = dict(private, **getattr(self, "italic_letters", {}))
            for number, run in italic_runs.italic_runs(self.stem):
                run = "".join(private.get(one, one) for one in run)
                run = unicodedata.normalize("NFC", " ".join(run.split()))
                # A run can open on the sign, the > of Madeline > smətle:n18 in Gerdts and Peter,
                # where the form before it is upright.
                self._italics.setdefault(number, []).extend(
                    one for one in re.split(r"(?:^|\s)(?:~|/|>) ", run) if one)
        return self._italics

    def footnote_marks(self):
        """The marks of the paper's footnotes, read once."""
        if getattr(self, "_footnote_marks", None) is None:
            self._footnote_marks = set(self.page_footnotes())
        return self._footnote_marks

    def cited(self, where, body, pages, skip=()):
        """A cited form row for each italic run of pages that body holds as a word and that is a
        form of the language (orthographic), once in the paper; its gloss is the quoted gloss the
        text gives right after it, where it gives one."""
        marks = r"[̀-ͯʰ-˿ᴬ-ᶿ]"
        for number in pages:
            # The italic reader keeps the spaces the layer sets at a combining mark, sč ̓an̓us and
            # x̩ aƛ ̓ in Griffin's 2019 appendix and *t ̓sa̱x'id in Sardinha's first paper, where the
            # page text closes the word from the glyph positions; each run is defined as the body
            # defines it, without the dash of a phrase set after it, U+2014 kn̓ čk̓ʕam̓ in Johnson's thanks.
            # A raised letter the reader takes for a plain one standing apart, ɫp̓úlex w tn for
            # ɫp̓úlexʷtn in McKay's footnote 8, is looked for as either.
            spelled = {}
            for run in self.italics().get(number, ()):
                if re.search(r"%s\s|\s%s|\S\s[%s](?:\s|$)" % (marks, marks, "".join(RAISED)), run):
                    loose = r"\s?".join("(?:%s|%s)" % (one, RAISED[one]) if one in RAISED else re.escape(one)
                                        for one in re.sub(r"^[—–]\s+", "", run).split())
                    match = re.search(r"(?<![\w’])%s(?![\w’]|%s)" % (loose, marks), body)
                    # Or one a footnote's mark ends, č ̓an̓o for č̓an̓o2 on Mellesmoen's page 2.
                    if not match and not re.search(r"\d", run):
                        match = next((one for one in re.finditer(
                            r"(?<![\w’])%s(?=(\d{1,2})(?:[\s,.;:)]|$))" % loose, body)
                            if one.group(1) in self.footnote_marks()), None)
                    if match:
                        spelled[run] = match.group(0)
            italics = [spelled.get(run, run) for run in self.italics().get(number, ())]
            # Two runs the text sets as one word across an upright tilde are one form, nuχʷ~nχʷ and
            # *spəl~eləm in Nater's old records, or the tilde operator, siwilaayin∼siwilaak'in in
            # Forbes's transitivity, and so are two across an upright hyphen, si-wok̲ there.
            runs = []
            for run in italics:
                joined = runs and next((sign for sign in "~∼-" if re.search(
                    r"(?<![\w’])%s%s%s(?![\w’])" % (re.escape(runs[-1]), sign, re.escape(run)), body)), None)
                if joined:
                    runs[-1] += joined + run
                else:
                    runs.append(run)
            for run in runs:
                # A paper can name the language of its italic runs, where its forms are defined in
                # plain letters, Arapaho's To'uu3eebexookee, or its English examples are cited too.
                language = self.form_language(run) if getattr(self, "form_language", None) else \
                    L if orthographic(run) else None
                if run in self.cited_done or run in skip or not language or \
                        run in getattr(self, "language_names", ()):
                    continue
                # A run that stops before a combining mark is the front of a longer letter, the
                # t'əx of *t'əx̌ on Denzer-King's page 7, whose caron the stream carries at the line's
                # end.
                matches = list(re.finditer(r"(?<![\w’])%s(?![\w’]|%s)" % (re.escape(run), marks), body))
                # A glottal apostrophe set upright after the italic letters ends the run short,
                # -ɢəm’ and łic’ in Nater's Bella Coola; the form in the text takes it.
                if not matches and not run.endswith("’") and "‘" not in run:
                    matches = list(re.finditer(r"(?<![\w’])%s(?![\w’])" % re.escape(run + "’"), body))
                    if matches:
                        self.cited_done.add(run)
                        run += "’"
                        if run in self.cited_done:
                            continue
                # A footnote's mark the text sets on the form, č̓an̓o2 on Mellesmoen's page 2, ends it
                # there; a mark is a number the paper's footnotes carry, before a space or a stop.
                if not matches and not re.search(r"\d", run):
                    matches = [one for one in re.finditer(r"(?<![\w’])%s(?=(\d{1,2})(?:[\s,.;:)]|$))"
                                                          % re.escape(run), body)
                               if one.group(1) in self.footnote_marks()]
                # And one set upright before them begins it, Shuswap 'úpəkst on Denzer-King's page 8.
                if not matches and not run.startswith("’") and "‘" not in run:
                    matches = list(re.finditer(r"(?<![\w’])%s(?![\w’]|%s)" % (re.escape("’" + run), marks), body))
                    if matches:
                        self.cited_done.add(run)
                        run = "’" + run
                        if run in self.cited_done:
                            continue
                if not matches:
                    continue
                # A form that stands inside a longer run of the page is that run's, k̓ʷzús-əm in
                # s.k̓ʷzús-əm ‘job’ and -us in n-..-us ‘-fold’ in van Eijk; the form's own place is
                # outside it, where there is one.
                covered = [(one.start(), one.end()) for longer in italics
                           if run in longer and longer != run
                           for one in re.finditer(re.escape(longer), body)]
                matches = [one for one in matches
                           if not any(start <= one.start() and one.end() <= end for start, end in covered)] or matches
                # A form cited on its own after a compound that holds it, ts’íl-xíl-em ... xíl-em
                # ‘do (like)’, is the free standing one.
                found = next((one for one in matches if body[one.start() - 1:one.start()] != "-"), matches[0])
                # Or on its own after a compound it opens, sl-aq’-nk ... (sl ‘to cut, slice’ in Nater's
                # Tsimshianic vestiges.
                if body[found.end():found.end() + 1] == "-":
                    found = next((one for one in matches if body[one.start() - 1:one.start()] != "-" and
                                  body[one.end():one.end() + 1] != "-"), found)
                # And a root on its own after a compound that holds it, q'ʷum-sx-iwa ... √q'ʷum in
                # Nater's Tsimshianic vestiges.
                found = next((one for one in matches if body[one.start() - 1:one.start()] == "√"), found)
                # An italic prefix hyphened to an upright word, the kʷ of clausal kʷ-demonstratives
                # in Reisinger's modals, is part of that word; the paper holds no form of it alone.
                # The body closes a line's hyphen on the next line, √łaq-ANTIP in Nater's
                # Tsimshianic vestiges, where the paper holds √łaq- alone; the word after the hyphen
                # has to be lower-case English.
                after = re.match(r"-([a-z]{3,})\b", body[found.end():])
                if after and after.group(1) not in "".join(italics):
                    continue
                self.cited_done.add(run)
                bare = run
                start, end = found.start(), found.end()
                # A parenthesis the upright type opens and the italics close, (ʔ)ay̓, taken before
                # the dash ahead of it, -(x)tła in Sardinha's first paper.
                if run.count(")") > run.count("(") and body[start - 1:start] == "(":
                    run = "(" + run
                    start -= 1
                # An affix's hyphen the italics leave upright, -ət with ət in italics, and an en dash
                # the italic reader loses, –ayqʷp in Pincott, with a mutation mark after it, -°x̱tłe
                # in Sardinha's first paper. A dash after a letter or a digit is a range or a
                # compound, 1993–x. A dash after a sign set in parentheses is a polarity sign, the
                # (?)-čát-t̓iqi-m̓ɬ ‘policeman’ of Robertson's puns, and no hyphen.
                # The non-breaking hyphen of Sardinha's ‑°tłe’ is a hyphen too, and a clitic's sign
                # set upright is taken the same way, the =ńd.s and =nis/=ńis of Sardinha's 2011 page 14.
                lead = re.search(r"[-–‑=][°!]?$", body[max(0, start - 2):start])
                if lead and not run.startswith(("-", "–", "‑", "=")) and \
                        not body[start - len(lead.group(0)) - 1:start - len(lead.group(0))].isalnum() and \
                        not re.search(r"\([?+]\)$", body[:start - len(lead.group(0))]):
                    run = lead.group(0) + run
                if body[end:end + 1] == "-" and not run.endswith("-"):
                    run += "-"
                    end += 1
                # A root sign set upright before an italic root, √ʔwm in Nater's Tsimshianic vestiges.
                if body[start - 1:start] == "√" and not run.startswith("√"):
                    run = "√" + run
                    start -= 1
                # And the star of an unattested form, set upright against it, *s.qáy:qəyxʷ in van Eijk,
                # both of the salmon paper's **ŋ, and the raised letter that can stand before the star,
                # ᵖ*kʷ naʔ nhampátkʷ in Lyon and Czaykowska-Higgins' footnote 43.
                # An ellipsis between the star and the form stands for the part left out, *…k̭ən in
                # Nater's old records.
                if body[start - 2:start] == "*…" and not run.startswith("…"):
                    run = "…" + run
                    start -= 1
                if not run.startswith("*"):
                    while body[start - 1:start] == "*":
                        run = "*" + run
                        start -= 1
                    if run.startswith("*") and start and unicodedata.category(body[start - 1]) == "Lm" and \
                            (start == 1 or body[start - 2] in " (‘“"):
                        run = body[start - 1] + run
                        start -= 1
                # A paper that sets forms of several languages side by side names the language ahead
                # of each form, Kwakwala √ʔwm in Nater's Tsimshianic vestiges.
                if getattr(self, "language_before", None):
                    language = self.language_before(body[:start]) or language
                # The Symbol font's empty set after an affix's dash, -∅, is set upright.
                if body[end:end + 1] == "∅":
                    run += "∅"
                    end += 1
                # An apostrophe inside the gloss, ‘bereft of one’s tail’ in Nater's old records, has a
                # letter after it; the closing quote does not.
                gloss = re.match(r"\s*(‘(?:[^’]|’(?=[^\W\d_]))+’)", body[end:])
                # A parenthesis the italics open and the upright type closes, scəlacəɬdat(il).
                if run.count("(") > run.count(")") and body[found.end():found.end() + 1] == ")":
                    run += ")"
                # A run that the star or a dash above made into a form already cited at the same
                # place, the bare tqačiʔ of *tqačiʔ on Denzer-King's page 6, is that form. The same
                # form cited again elsewhere stays, Nater's -nix in Figure 1 and in §1.
                why = "page %d, in italics%s" % (number, ", " + gloss.group(1) if gloss else "")
                if run != bare and [where, language, "cited form", run, why] in self.rows:
                    continue
                self.add(where, language, "cited form", run, why)

    def mentions(self, where, body, pairs, kind, who=A):
        """A row of kind for each (name, why) of pairs that body holds, once in the paper."""
        for name, why in pairs:
            if (kind, name) not in self.mentioned and name in body:
                self.mentioned.add((kind, name))
                self.add(where, who, kind, name, why)

    # Examples.

    def example(self, start, last=None, skip=(), resume=None, prefix=""):
        """Read the numbered interlinear example opening on line start and return the line after
        it. An example is one sentence or lettered sub-examples, a. b. c., each laid out the same:
        its first line the transcription, the lines under it a segmentation and its gloss in turn
        until a translation in single quotes, which may run over two lines. What stands right of
        the translation, or on the line under it, is a citation where it names a source (a
        language tag, a year, a page, a speaker's initials) and a note where it does not
        ((Independent), (Lit: ...)). A line over the sub-examples that closes on a colon, and a
        Context: line, are notes. A starred form given with its source and no tiers is a
        transcription and its citation. Page breaks, page numbers and the lines of skip are passed
        over. With resume, an example number, line start is a lettered part of that example the
        page sets after a table or figure that broke into it."""
        last = last or self.last
        number_label = resume or EXAMPLE.match(self.text(start)).group(1)
        running = self.running_numbers_set()
        state = {"label": number_label, "count": 0, "mode": "start", "tiers": 0}

        def row(who, kind, form, number, gloss=None):
            state["count"] += 1
            self.add("%s(%s) line %d" % (prefix, state["label"], state["count"]), who, kind, form,
                     "page %d%s" % (self.page(number), ", " + gloss if gloss else ""))

        def next_line(number):
            number += 1
            while number <= last and (self.lines[number][2] or not self.text(number) or number in running
                                      or number in skip):
                number += 1
            return number

        def done(number):
            # A translation the reader opened and never closed, one whose line ends on a label
            # split_translation does not know, is dropped with no row; say so, and the generator
            # can read the example in a block of its own.
            if state["mode"] == "open":
                said, at = state["open"]
                print("gen: %s(%s) leaves its translation open at line %d, %s" % (prefix, state["label"], at, said[:70]),
                      file=sys.stderr)
            return number

        def translation(said, number):
            split = split_translation(said)
            if split is None:
                return False
            said, pieces = split
            row(A, "translation", said, number)
            for piece in pieces:
                if is_source(piece):
                    row(A, "citation", piece, number, "the tag or source at the right of the translation")
                else:
                    row(A, "note", piece, number, "at the right of the translation")
                    # A note whose parenthesis the line leaves open, (Literally; ‘...that one who
                    # was Paul in H. Davis's proper names, runs on to the line under it.
                    if piece.count("(") > piece.count(")"):
                        state["paren"] = len(self.rows) - 1
            state["mode"] = "after"
            return True

        def close_paren(text, number):
            """Run the open note on with text to its closing parenthesis and the mark after it,
            Spintlum')12, and write what stands after as its citations or notes."""
            index = state.pop("paren")
            depth = self.rows[index][3].count("(") - self.rows[index][3].count(")")
            for at, symbol in enumerate(text):
                depth += {"(": 1, ")": -1}.get(symbol, 0)
                if depth == 0:
                    break
            closed = re.match(r"\d*", text[at + 1:]).end() + at + 1
            extend(index, text[:closed])
            rest = text[closed:].strip()
            if depth > 0:
                state["paren"] = index
            elif rest and SOURCE.match(rest):
                for piece in pieces_of(rest):
                    row(A, "citation" if is_source(piece) else "note", piece, number, "under the translation")
            elif rest:
                extend(index, rest)

        def continues_context(number):
            """Whether line number runs on the context over it: more lines stand between it and
            the gloss than the tiers over a gloss, a transcription and a segmentation (one, a
            segmentation, where the paper opens on it)."""
            over = 1 if self.opening == "segmentation" else 2
            seen = 0
            while number <= last:
                text = self.text(number)
                # The lines between a context and the lettered part under it, a. ʔɛ, qəǰɛč ..., are
                # all the context's: the part sets its own tiers.
                if SUB.match(text):
                    return seen > 0
                if EXAMPLE.match(text) or text.startswith("‘") or JUDGED_QUOTE.match(text):
                    return False
                if is_gloss(text):
                    return seen > over
                seen += 1
                number = next_line(number)
            return False

        def lines_to_translation(number):
            """The lines from number to the translation under them, or 0 where none follows."""
            count = 0
            while number <= last:
                text = self.text(number)
                if text.startswith("‘") or JUDGED_QUOTE.match(text) or LABELED_QUOTE.match(text):
                    return count
                if EXAMPLE.match(text) or SUB.match(text) or number in skip:
                    return 0
                count += 1
                number = next_line(number)
            return 0

        def extend(index, text):
            self.rows[index][3] += " " + text

        def open_comment():
            """The row of a comment the line before left mid-sentence, or None."""
            index = state.get("comment")
            if index is None or re.search(r"[.!?’”)]$", self.rows[index][3]):
                return None
            return index

        number = start
        text = self.text(start) if resume else EXAMPLE.match(self.text(start)).group(2) or ""
        while True:
            sub = SUB.match(text)
            roman = ROMAN_PART.match(text)
            # A part of a lettered part, a. Stative / i. Context: ..., its label (21ai); i. after
            # h. is a letter.
            if roman and state.get("letter") and not (roman.group(1) == "i" and state["letter"] == "h"):
                state.update(label=number_label + state["letter"] + roman.group(1), count=0, mode="start",
                             tiers=0, context=None, comment=None)
                text = roman.group(2)
            elif sub:
                state.update(label=number_label + sub.group(1), letter=sub.group(1), count=0, mode="start",
                             tiers=0, context=None, comment=None)
                text = sub.group(2)
                roman = ROMAN_PART.match(text)
                if roman:
                    state["label"] += roman.group(1)
                    text = roman.group(2)
            if state["mode"] == "start":
                following = self.text(next_line(number)) if next_line(number) <= last else ""
                if state.get("context") is not None and continues_context(number):
                    extend(state["context"], text)
                # A caption over the example can carry a note's mark, Kalispel-Spokane-Flathead:1,
                # and a lettered part under it, a. ... s-tariɁ-CVC-m, is no gloss of it. A paper can
                # name its captions (captions), for one printed without its colon.
                elif CONTEXT.match(text) or \
                        re.search(r":[*∗†]?\d{0,2}$", text) and not (is_gloss(following) and not SUB.match(following)) \
                        or re.sub(r":?[*∗†]?\d{0,2}$", "", text) in getattr(self, "captions", ()):
                    row(A, "note", text, number, "over the example")
                    state["context"] = len(self.rows) - 1 if CONTEXT.match(text) else None
                elif PART_HEAD.match(text) and ROMAN_PART.match(following):
                    row(A, "note", text, number, "heading the parts under it")
                elif DIALOGUE.match(text):
                    speaker = DIALOGUE.match(text).group(1)
                    row(A, "speaker comment" if speaker == "Consultant" else "note", text, number,
                        "the consultant's answer, the consultant not named" if speaker == "Consultant"
                        else "the interviewer's question")
                    state["mode"] = "after"
                elif text.startswith("‘") or JUDGED_QUOTE.match(text):
                    if not translation(text, number):
                        state["mode"], state["open"] = "open", (text, number)
                elif text:
                    tagged = TAGGED.match(text)
                    if tagged and (text[:1] in "*?#%" or SUB.match(following) or EXAMPLE.match(following)):
                        row(L, "transcription", tagged.group(1), number)
                        row(A, "citation", tagged.group(2), number, "the tag or source at the right")
                        state["mode"] = "after"
                    else:
                        # A paper that sets the sentence segmented, over its gloss, opens on the
                        # segmentation (opening = "segmentation"); the tiers then run gloss first.
                        # With opening = "auto" each example opens on its segmentation where the
                        # line under it is the gloss, and on a transcription where it is not.
                        opening = self.opening
                        if opening == "auto":
                            # A form with only its gloss between it and the translation, however
                            # the gloss is set (he.gets.up secondary), is the segmented tier.
                            below = next_line(next_line(number))
                            under = self.text(below) if below <= last else ""
                            opening = "segmentation" if is_gloss(following) or \
                                under.startswith("‘") or JUDGED_QUOTE.match(under) else "transcription"
                        row(L, opening, text, number)
                        state["mode"] = "tiers"
                        state["tiers"] = 1 if opening == "segmentation" else 0
                        state["words"] = len(text.split())
                        state["kind"] = state["opened"] = opening
                        # A paper that sets its glosses in capitals, DET=dog, lets each tier's kind
                        # be read off the gloss under it.
                        after = next_line(next_line(number))
                        state["caps"] = is_gloss(following) or after <= last and is_gloss(self.text(after))
            elif state["mode"] == "open":
                said, at = state["open"]
                if not translation(said + " " + text, at):
                    state["open"] = (said + " " + text, at)
            elif text.startswith("‘") or JUDGED_QUOTE.match(text) or LABELED_QUOTE.match(text) or \
                    text.startswith("“") and state["tiers"] >= 2:
                if not translation(text, number):
                    state["mode"], state["open"] = "open", (text, number)
            elif state["mode"] == "after":
                if state.get("paren") is not None:
                    close_paren(text, number)
                elif open_comment() is not None:
                    extend(open_comment(), text)
                elif SOURCE.match(text):
                    for piece in pieces_of(text):
                        row(A, "citation" if is_source(piece) else "note", piece, number,
                            "under the translation")
                elif CONTEXT.match(text):
                    row(A, "note", text, number, "the context")
                elif COMMENT.match(text) or DIALOGUE.match(text):
                    # The source at the right of a comment is its citation, (HH).
                    tagged = re.match(r"^(.*[.!?’”])\s+(\([^()]+\))$", text)
                    said = tagged.group(1) if tagged and is_source(tagged.group(2)) else text
                    row(A, "speaker comment", said, number, "under the example, the consultant not named")
                    state["comment"] = len(self.rows) - 1
                    if said != text:
                        row(A, "citation", tagged.group(2), number, "the tag or source at the right of the comment")
                else:
                    return number
            elif state["tiers"] >= 2 and state["tiers"] % 2 == 0 and SOURCE.match(text):
                # A line in parentheses where a translation would stand, (same) under (43b): the
                # translation of the part before holds.
                for piece in pieces_of(text):
                    row(A, "citation" if is_source(piece) else "note", piece, number, "where the translation stands")
                state["mode"] = "after"
            elif state["tiers"] >= 2 and state["tiers"] % 2 == 0 and COMMENT.match(text):
                # A starred sentence takes no translation; its consultant's comment stands where
                # one would, under (56a) in H. Davis's proper names.
                row(A, "speaker comment", text, number, "where the translation stands, the consultant not named")
                state["comment"], state["mode"] = len(self.rows) - 1, "after"
            else:
                kind = "segmentation" if state["tiers"] % 2 == 0 else "gloss"
                if state.get("caps"):
                    # The gloss holds capitals; the line over a gloss is its segmentation, and the
                    # line over that the transcription where the example wraps all three tiers.
                    # Otherwise a tier is the kind the one before it is not: a gloss the page sets
                    # over its own segmentation, (51c) in H. Davis's infinitives, is read as printed.
                    one = next_line(number)
                    two = next_line(one)

                    def glossed(at):
                        # A translation is no gloss for its label in capitals, [PREVIOUS DIRECT
                        # EVIDENCE].
                        line = self.text(at)
                        return at <= last and is_gloss(line) and not line.startswith(("‘", "“")) \
                            and not JUDGED_QUOTE.match(line)
                    if is_gloss(text):
                        kind = "gloss"
                    elif glossed(one):
                        kind = "segmentation"
                    elif glossed(two) and state["kind"] == "gloss":
                        kind = "transcription"
                    elif state["kind"] == "gloss" and state.get("opened") == "transcription" and \
                            lines_to_translation(number) == 3:
                        # The last wrap of the three tiers, skʷiǰoɬ. / skʷiǰuɬ / morning in (6),
                        # whose gloss holds no capitals: three lines over the translation.
                        kind = "transcription"
                    elif state["kind"] == "transcription":
                        kind = "segmentation"
                    else:
                        kind = "segmentation" if state["kind"] == "gloss" else "gloss"
                row(L, kind, text, number)
                state["kind"] = kind
                state["tiers"] += 1
            number = next_line(number)
            if number > last:
                return done(number)
            text = self.text(number)
            if EXAMPLE.match(text) or number in skip:
                return done(number)
            if state["mode"] == "after" and not (SUB.match(text) or SOURCE.match(text) or COMMENT.match(text)
                                                 or DIALOGUE.match(text) or CONTEXT.match(text)
                                                 or ROMAN_PART.match(text) and state.get("letter")
                                                 or text.startswith("‘") or JUDGED_QUOTE.match(text)
                                                 or LABELED_QUOTE.match(text) or open_comment() is not None
                                                 or state.get("paren") is not None):
                return number
            # A tier has about as many words as the sentence over it; a line of prose after a
            # starred form given no tiers, * Hlimooyidiithl us., has many more. A tier's count takes
            # the words that open on neither = nor -, three in l(a̱) ='m =ux̱ Shelly =(a̱)x̱ ḵ̕ux̱ -t̕sa̱w
            # -(x)'id -a under la̱'mux̱ Shelliya̱x̱ ḵ̕ux̱t̕sox'ida in Sardinha's second paper.
            if state["mode"] == "tiers" and not SUB.match(text) and not text.startswith("‘") and \
                    not COMMENT.match(text) and len([one for one in text.split() if one[0] not in "=-"]) > 2 * state.get("words", 0) + 2:
                return number

    def place_footnotes(self, marks, write, placed=(), spaced=None, glued=None, kinds=(), only=None):
        """Put each footnote's rows after the first note, heading, translation or citation that
        carries its mark, the marks taken in order and each looked for only after the one before
        it: a mark stands on the end of a word or a stop, orders.2 There, and not after a space, a
        digit or a parenthesis. Two marks set together, permitted.8,9, stand on the same row, the
        second after the first and a comma. spaced maps a mark the page sets a space after its
        word to the text before that space, and glued a mark set on a form whose orthography holds
        digits of its own, lhel=ki=qú7=a5 in Davis's count-mass paper, to that form. write(mark)
        adds the footnote's rows. kinds names row kinds that can carry a mark besides these, a
        reference, and only, where given, the kinds alone that are looked in. Returns the marks no
        row carries."""
        spaced = spaced or {}
        glued = glued or {}
        out, done = [], set(placed)
        waiting = [one for one in marks if one not in done]
        previous = None
        # An orthography with no digit among its letters, no sk7am or 7a, holds none after a word
        # either, and a mark glued to a form there is a mark: snatolh6, {hiL-a7 səm.
        lettered = not any(re.search(r"[^\W\d_]\d[^\W\d_]|(?:^|\s)\d[^\W\d_]", row[3]) for row in self.rows
                           if row[2] == "transcription")
        for row in self.rows:
            out.append(row)
            while waiting and not row[0].startswith("footnote") and (only is None or row[2] in only) and row[2] in (
                    "note", "heading", "translation", "citation", "speaker comment", "gloss",
                    "segmentation") + FORM_KINDS + tuple(kinds) or (row[2] == "cited form" and waiting and waiting[0] in glued):
                mark = re.escape(waiting[0])
                # A cited form carries only a glued mark, smelhmúlhats8 in Davis's list (17).
                if row[2] == "cited form":
                    alone = None
                elif row[2] in FORM_KINDS + ("segmentation",):
                    # A language row holds digits of its own, wa7 with 7 the glottal stop; a mark
                    # there ends the row after a stop, a comma or a closing bracket, tə ʔatnopɛl.12,
                    # xigap,8 in Davis's future certainty, [šyεʔtams]8, or stands apart after the form, tlemlhayem 1. A segmentation is
                    # read the same way, -wa̱ł/-uł2 in Table 1 of Sardinha's second paper. Where no
                    # letter is a digit the mark can follow a letter or its mark below, Shelliya̱x̱9.
                    alone = re.search(r"(?:(?<=[.!?,\]}/])|(?<=\s))%s$" % mark, row[3]) or \
                        (lettered and re.search(r"(?<=[^\W\d_]|[̀-ͯ])%s(?=\s|$)" % mark, row[3]))
                else:
                    # A digit after a capital standing alone is a name's, L2 teachers in Sardinha's
                    # first paper, and no mark; so is a section's number, begin in §2 by and
                    # analysis (§3) in Martin.
                    alone = re.search(r"(?<=.)(?<![\s\d(=\-§])(?<!\d[.,:])(?<!\b[A-Z])%s(?:\s|$|[,.;:)])" % mark,
                                      row[3]) or \
                        re.search(r"(?:(?<=\s\d\.)|(?<=\s\d\d\.)|(?<=\s\d{4}\.))%s(?:\s+(?=[A-Z])|$)" % mark, row[3])
                    # A gloss tier set in columns can hold the mark a column's gap after its last
                    # word, go we ga IMPF-walk-PL-VERB ga 4.
                    if row[2] == "gloss" and not alone:
                        alone = re.search(r"(?<=[^\W\d]\s)%s$" % mark, row[3])
                    # Or glue it to a label before the label's hyphen, CAUS10-you in Davis's future
                    # certainty.
                    if row[2] == "gloss" and not alone:
                        alone = re.search(r"(?<=[A-Z])%s(?=-)" % mark, row[3])
                    # A note can set the mark a space after its sentence's stop, of its existence. 13
                    # The differences. The stop can close a quote or a bracket, symbolized by ‘⊕’. 41
                    # The difference, and (58a). 43 But in Lyon and Czaykowska-Higgins. The mark can
                    # end the row, the accusative case. 3 in Sardinha's second paper.
                    if row[2] == "note" and not alone:
                        alone = re.search(r"(?<=[a-z’)\]][.!?]\s)%s(?=\s+[A-Z]|$)" % mark, row[3])
                    # And a translation a space after its closing quote, (one on the boat).' 12.
                    if row[2] == "translation" and not alone:
                        alone = re.search(r"(?<=[’”]\s)%s$" % mark, row[3])
                    # A source held whole in its parentheses, a notebook's page (EP4.44.7), carries
                    # its digits and no mark inside them.
                    if row[2] == "citation" and re.fullmatch(r"\(\S+\)", row[3]):
                        alone = None
                # The second form, a mark on a number that closes a sentence, shown in Figure 8.6 As
                # with, or on a year, published in 1917.1 The last in Bischoff et al.; a section
                # number opens its heading's row and is never read so. The second of
                # two marks can stand a space after the comma, position.10, 11.
                paired = previous and row[2] not in FORM_KINDS and \
                    re.search(r"(?:(?<=\D%s,)|(?<=\D%s, ))%s(?:\s|$|[,.;:)])" %
                              (re.escape(previous), re.escape(previous), mark), row[3])
                # A mark the page sets a space after its word, typed form, 5 and in Bischoff et al.,
                # is found where the generator names the text before it.
                if not (alone or paired) and waiting[0] in spaced:
                    alone = spaced[waiting[0]] + " " + waiting[0] in row[3]
                if not (alone or paired) and waiting[0] in glued:
                    # The mark can stand before a stop or a comma, the 5-82, of Denzer-King's §2, or
                    # before a clitic's sign, the lala12=oχ of Sardinha's 2011 (25).
                    alone = re.search(r"(?:^|\s)%s%s(?=[\s,.;:)=]|$)" % (re.escape(glued[waiting[0]]), mark), row[3])
                if not (alone or paired):
                    break
                held, self.rows = self.rows, []
                write(waiting[0])
                out.extend(self.rows)
                self.rows = held
                previous = waiting.pop(0)
                done.add(previous)
        self.rows = out
        return waiting

    def display(self, start, count, who=A, kind="note", gloss="set as an example", per_line=False):
        """Write the count printed lines of a display from line start, a template, a list or a
        generalization set as an example: a row to each lettered item with its wrapped lines run
        on, or one row where it has no letters. With per_line, a tree, each printed line is a row
        of its own. Returns the line after it."""
        label = EXAMPLE.match(self.text(start)).group(1)
        number, done, items = start, 0, []
        running = self.running_numbers_set()
        # count is the display's printed lines, or a pattern for the first line after it.
        until = re.compile(count) if isinstance(count, str) else None
        while (done < count if until is None else not (done and until.search(self.text(number)))):
            text = self.text(number)
            if not (self.lines[number][2] or not text or number in running):
                done += 1
                if done == 1:
                    text = EXAMPLE.match(text).group(2) or ""
                if done == 1 or per_line or SUB.match(text):
                    items.append([text, number])
                else:
                    items[-1][0] += " " + text
            number += 1
        for index, (text, at) in enumerate(items):
            letter = None if per_line else SUB.match(text)
            where = "(%s) line %d" % (label, index + 1) if per_line else \
                "(%s%s) line 1" % (label, letter.group(1) if letter else "")
            self.add(where, who, kind, letter.group(2) if letter else text, "page %d, %s" % (self.page(at), gloss))
        return number

    def footnote(self, mark, parts, page, names=(), languages=(), where=None, gloss=None):
        """Write a footnote from its lines: its prose a note to each stretch between the examples it
        sets, the examples read as the body's are, and the cited forms, names and languages of each
        stretch after it."""
        where = where or "footnote %s" % mark
        prose, index = [], 0

        def flush(first):
            if not prose:
                return
            body = self.joined(prose)
            if first:
                body = Paper.body_of(body, mark)
            self.add(where, A, "note", body, gloss or "page %d, footnote %s" % (page, mark))
            # A note that runs on at the foot of the next page cites forms there too, the =χ of
            # Sardinha's 2011 note 11 on page 16.
            self.cited(where, body, sorted({page} | {self.page(one) for one in prose}))
            self.mentions(where, body, names, "name")
            self.mentions(where, body, languages, "language")
            del prose[:]

        first = True
        while index < len(parts):
            number = parts[index]
            # A sentence can wrap onto a body example's number, not offered for (33) and / (34)
            # probably reflects, in note 11 of Davis, Griffin, Huijsmans and Mellesmoen: that
            # number opens no example.
            opened = EXAMPLE.match(self.text(number))
            if opened and opened.group(1).isdigit() and \
                    re.search(r"(?:\b(?:and|or|nor|for|of|than|with)|,)$", self.text(parts[index - 1]).rstrip()):
                opened = None
            if index and opened:
                flush(first)
                first = False
                after = self.example(number, parts[-1], prefix="footnote %s " % mark)
                while index < len(parts) and parts[index] < after:
                    index += 1
                continue
            prose.append(number)
            index += 1
        flush(first)

    def glossed(self, start, skip=()):
        """Whether a gloss stands in the two lines of text under line start, before another
        example opens."""
        running = self.running_numbers_set()
        seen, number = 0, start + 1
        while seen < 2 and number <= self.last:
            text = self.text(number)
            if not (self.lines[number][2] or not text or number in running or number in skip):
                if EXAMPLE.match(text):
                    return False
                if is_gloss(text):
                    return True
                seen += 1
            number += 1
        return False

    def glyph_rows(self, page):
        """The glyphs of a page grouped into printed rows, each a list of (left edge, glyph) in
        reading order: a glyph joins the row whose baseline lies within 2.5 points of its own."""
        if not hasattr(self, "_glyph_rows"):
            self._glyph_rows = {}
        if page not in self._glyph_rows:
            if not hasattr(self, "_document"):
                self._document = page_text.paper_document(self.stem)[0]
            textpage = self._document[page - 1].get_textpage()
            glyphs = []
            for index in range(textpage.count_chars()):
                symbol = textpage.get_text_range(index, 1)
                if symbol and not symbol.isspace():
                    left, bottom, right, top = textpage.get_charbox(index, loose=True)
                    glyphs.append((bottom, left, symbol))
            rows, baseline = [], None
            for bottom, left, symbol in sorted(glyphs, key=lambda one: -one[0]):
                if baseline is None or baseline - bottom > 2.5:
                    rows.append([])
                    baseline = bottom
                rows[-1].append((left, symbol))
            self._glyph_rows[page] = [sorted(row) for row in rows]
        return self._glyph_rows[page]

    def word_positions(self, number):
        """[(left edge, word)] for the words of line number, each word's edge that of its first
        glyph on the page: the line is aligned to the printed row that holds most of its letters,
        marks and superscripts aside, and a column's words can be told by where they stand."""
        import difflib
        import unicodedata

        def key(text):
            return [one for one in unicodedata.normalize("NFD", text)
                    if not one.isspace() and not unicodedata.combining(one) and unicodedata.category(one) != "Lm"]

        text = self.text(number)
        line = key(text)
        best, ratio = None, 0.0
        for row in self.glyph_rows(self.page(number)):
            glyphs = [(left, one) for left, symbol in row for one in key(symbol)]
            if not glyphs or abs(len(glyphs) - len(line)) > max(6, len(line) // 2):
                continue
            matcher = difflib.SequenceMatcher(None, line, [one for _, one in glyphs], autojunk=False)
            if matcher.ratio() > ratio:
                best, ratio = (glyphs, matcher), matcher.ratio()
        if best is None or ratio < 0.6:
            return None
        glyphs, matcher = best
        at = {}
        for first, second, size in matcher.get_matching_blocks():
            for offset in range(size):
                at[first + offset] = glyphs[second + offset][0]
        found, index = [], 0
        for word in text.split():
            count = len(key(word))
            edges = [at[one] for one in range(index, index + count) if one in at]
            found.append((edges[0] if edges else None, word))
            index += count
        # A word none of whose letters was matched stands just right of the word before it.
        for place, (left, word) in enumerate(found):
            if left is None:
                found[place] = ((found[place - 1][0] + 0.1) if place else 0.0, word)
        return found

    def columns(self, start, where=None, skip=(), language=L, label=None):
        """Read an example set in columns from line start and return the line after it: a header
        line of lettered forms, (13) a. sasaxosem b. sasaxost c. saxwat, or of numbered ones,
        (5) hasem (6) kwetem, then the tiers under them, each word given to the column it stands
        under on the page (word_positions). A column's text on a line is a row of the kind its
        shape gives: [..] phonetic, /../ phonemic, {..} underlying, ‘..’ a translation, (..) a
        note, a label in capitals or a form broken by hyphens a gloss, the header's forms the
        transcription. Lettered parts set in a later header line, c. t’oz’.’em Joe, open new
        columns. The example ends at a line standing left of its first column, prose at the margin
        or on a paragraph's indent, or at a new example. label names an example the paper numbers
        twice, 35:2 for the second (35)."""
        number = label or EXAMPLE.match(self.text(start)).group(1)
        running = self.running_numbers_set()
        counts = {}

        def header(line):
            """[(left edge, label, [words])] for a header line, the markers dropped."""
            found = []
            for left, word in self.word_positions(line):
                marker = re.fullmatch(r"\((\d{1,3})\)|([a-h])\.", word)
                if marker:
                    numbered = marker.group(1) and (label if len(found) == 0 and label else marker.group(1))
                    found.append([None, numbered or number + marker.group(2), []])
                elif found:
                    found[-1][2].append(word)
                    # A column stands where its first form does.
                    found[-1][0] = found[-1][0] if found[-1][0] is not None else left
                else:
                    found.append([left, number, [word]])
            # The example's number over lettered columns is no column of its own.
            return [one for one in found if one[2]]

        def kind(text, first):
            if first:
                return language, "transcription"
            if text.startswith("["):
                return language, "phonetic"
            if text.startswith("/"):
                return language, "phonemic"
            if text.startswith("{"):
                return language, "underlying"
            if text.startswith(("‘", "’", "“")):
                return A, "translation"
            if text.startswith("("):
                return A, "note"
            if is_gloss(text) or "-" in text or len(text.split()) <= 3 and not text.endswith("."):
                return language, "gloss"
            return A, "note"

        last = {}

        def add(label, text, line, first):
            # A translation or note that wraps inside its column, ‘several people kicking
            # unspecified / object(s); playing soccer’, runs on from the line it opened on.
            before = last.get(label)
            if not first and before is not None and (
                    before[3].startswith("‘") and not re.search(r"’\W*$", before[3]) or
                    before[3].startswith("(") and before[3].count("(") > before[3].count(")")):
                before[3] += " " + text
                return
            counts[label] = counts.get(label, 0) + 1
            who, what = kind(text, first)
            self.add("(%s) line %d" % (label, counts[label]), who, what, text, "page %d, in columns" % self.page(line))
            last[label] = self.rows[-1]

        columns = header(start)
        for left, label, words in columns:
            add(label, " ".join(words), start, True)
        # Prose stands at the edge the example's number does, or a paragraph's indent in from it;
        # the tiers stand under the first column.
        margin = min(one[0] for one in columns) - 10
        line = start + 1
        while line <= self.last:
            text = self.text(line)
            if self.lines[line][2] or not text or line in running or line in skip:
                line += 1
                continue
            if EXAMPLE.match(text):
                return line
            if SUB.match(text):
                columns = header(line)
                for left, label, words in columns:
                    add(label, " ".join(words), line, True)
                line += 1
                continue
            positions = self.word_positions(line)
            if positions is None or positions[0][0] < margin:
                return line
            pieces = {}
            for left, word in positions:
                owner = max((one for one in columns if one[0] <= left + 6), key=lambda one: one[0], default=columns[0])
                pieces.setdefault(owner[1], []).append(word)
            for left, label, words in columns:
                if label in pieces:
                    add(label, " ".join(pieces[label]), line, False)
            line += 1
        return line

    def indented_runs(self, start, end, skip=()):
        """[[line, ...]] for each block quotation from line start to end: a run of two lines or
        more, every one of them set 10 points or more in from the edge most of the body's lines
        start at. A paragraph sets in its first line alone. A quotation runs on over a page break,
        its running number and the page's blank lines."""
        running = self.running_numbers_set() | set(self.volume_header())
        skip = set(skip) | running
        edges, lefts = {}, {}
        for number in range(start, end + 1):
            if number in skip or self.lines[number][2] or not self.text(number).strip():
                continue
            found = self.word_positions(number)
            if found:
                lefts[number] = found[0][0]
                edges[round(found[0][0])] = edges.get(round(found[0][0]), 0) + 1
        margin = max(edges, key=edges.get)
        runs, current = [], []
        for number in range(start, end + 1):
            if number in lefts and lefts[number] >= margin + 10:
                current.append(number)
                continue
            if number in skip or number not in lefts and not self.text(number).strip() and not current:
                continue
            if current and number not in lefts:
                # A blank line: the run goes on where the next line is set in on the next page.
                after = next((one for one in range(number + 1, end + 1) if one in lefts), None)
                if after is not None and self.page(after) != self.page(current[-1]) and \
                        lefts[after] >= margin + 10 and \
                        all(one in skip or self.lines[one][2] or not self.text(one).strip()
                            for one in range(number, after)):
                    continue
            if len(current) >= 2:
                runs.append(current)
            current = []
        if len(current) >= 2:
            runs.append(current)
        return runs

    def running_numbers_set(self):
        if not hasattr(self, "_running"):
            self._running = self.running_numbers()
        return self._running

    # The body.

    def flow(self, first, last, where, headings=None, skip=(), blocks=None, names=(), languages=(),
             restarts=None):
        """Walk the body from line first to last and add its rows: a heading row at each line of
        headings ({line: label}), a numbered example wherever one opens, a block's own rows where
        blocks ({line: function(line, where) returning the line after the block}) has one, and the
        prose between them as a note to a paragraph, split at the lines the page indents. Each
        paragraph's cited forms, names and languages follow it. Lines in skip (footnotes, table
        notes) and page numbers are passed over. restarts ({heading label: prefix}) names the
        sections whose examples number from (1) again, each example's rows put under the prefix,
        §3.3 (1) line 1. Returns the section it ends in."""
        headings, blocks, restarts = headings or {}, blocks or {}, restarts or {}
        starts = self.paragraph_starts()
        running = self.running_numbers_set()
        paragraph = []
        state = {"where": where}

        def flush():
            if not paragraph:
                return
            body = self.joined(paragraph)
            pages = sorted({self.page(one) for one in paragraph})
            self.add(state["where"], A, "note", body, "page %d" % pages[0])
            self.cited(state["where"], body, pages)
            self.mentions(state["where"], body, names, "name")
            self.mentions(state["where"], body, languages, "language")
            del paragraph[:]

        number = first
        while number <= last:
            text = self.text(number)
            if number in skip or self.lines[number][2] or not text or number in running:
                number += 1
                continue
            if number in headings:
                flush()
                state["where"] = "§" + headings[number]
                if headings[number] in restarts:
                    state["highest"], state["prefix"] = 0, restarts[headings[number]]
                self.add(state["where"], A, "heading", text, "page %d" % self.page(number))
                number += 1
                continue
            if number in blocks:
                flush()
                opened = EXAMPLE.match(text)
                if opened and opened.group(1).isdigit():
                    # A block can set a second example beside the first, the trees (32) and (33)
                    # side by side in Forbes's transitivity; the next in sequence follows the last.
                    beside = re.findall(r"(?:^|\s{3,})\((\d+)\)", self.spaced[number])
                    state["highest"] = max([state.get("highest", 0)] + [int(one) for one in beside])
                number = blocks[number](number, state["where"])
                state["broken"] = state.get("example")
                continue
            # A roman-numbered line in the body is a list item, a paragraph of its own; a footnote's
            # own examples are numbered (i), (ii) and read in footnote().
            # Its wrapped lines hang indented under it, and the first line at the margin after it
            # opens the next paragraph.
            if re.match(r"^\([ivx]{1,4}\)", text):
                flush()
                paragraph.append(number)
                state["item"] = True
                number += 1
                continue
            if state.get("item"):
                if number in starts and not EXAMPLE.match(text) and number not in headings and number not in blocks:
                    paragraph.append(number)
                    number += 1
                    continue
                flush()
                state["item"] = False
                if number not in headings and number not in blocks and not EXAMPLE.match(text):
                    paragraph.append(number)
                    number += 1
                    continue
            # Examples are numbered in order; a line of prose that wraps to open on a reference,
            # (4) features a subjunctive ..., names a number already set. One that names a number
            # ahead, (67) or an argument (68) modifier, has no gloss under it before the next
            # example opens; a number past the next in sequence counts only where one does.
            opened = EXAMPLE.match(text)
            # A line that carries on a sentence the paragraph left open, the speaker utters / (26)
            # while still seeing the bear, is prose even with the next number in sequence. A stop
            # can carry a footnote's mark, the human series:4, set a space off in Forbes's (34b). 13.
            carries_on = paragraph and number not in starts and \
                not re.search(r"[.:;!?)\]’”][*∗†]?(?: ?\d{1,2}(?:,\d{1,2})*)?\s*$", self.text(paragraph[-1])) and \
                all(self.text(one).strip() for one in range(paragraph[-1] + 1, number)
                    if one not in skip and one not in running)
            if opened and opened.group(1).isdigit() and int(opened.group(1)) > state.get("highest", 0) and \
                    not carries_on and \
                    (int(opened.group(1)) == state.get("highest", 0) + 1 or self.glossed(number, skip)):
                flush()
                state["example"] = opened.group(1)
                state["highest"] = int(opened.group(1))
                number = self.example(number, last, skip, prefix=state.get("prefix", ""))
                state["broken"] = None
                continue
            # A lettered part right after a block, an example the block broke into.
            if state.get("broken") and not paragraph and SUB.match(text):
                number = self.example(number, last, skip, resume=state["broken"], prefix=state.get("prefix", ""))
                state["broken"] = None
                continue
            state["broken"] = None
            if number in starts:
                flush()
            paragraph.append(number)
            number += 1
        flush()
        return state["where"]

    # A whole paper.

    def front(self, authors, languages=()):
        """The front matter of page 1: the title, the lines it wraps onto, each author a name row
        and each line under an author (a university, an e-mail) a note, the abstract to the
        keywords, the keywords, and the volume's header. The paper's languages follow. Returns the
        keywords' last line."""
        header = set(self.volume_header())
        keywords = self.find(r"^Key ?words?:", 1, 200)
        abstract = self.find(r"^Abstract[:.]", 1, keywords or 200)
        # An abstract with no keywords under it, Webb's, runs to the first section heading.
        close = keywords or (abstract and self.find(HEADING.pattern, abstract + 1, abstract + 60))
        # A paper with neither, Ignace, Ignace and Lyon's W7éyle, sets its front matter over the first
        # section heading.
        top = abstract or keywords or self.find(HEADING.pattern, 1, 60) or 1
        lines = [one for one in range(1, top) if self.text(one)
                 and not self.lines[one][2] and one not in header]
        named = [one for one in lines if any(self.text(one).startswith(name) for name in authors)]
        first_author = named[0] if named else lines[-1]
        title = [one for one in lines if one < first_author]
        mark = title_mark(self.joined(title))
        self.add("front", A, "title", self.joined(title)[:-len(mark)].strip() if mark else self.joined(title),
                 "page 1%s" % (", carries footnote " + mark if mark else ""))
        current = None
        done = set()
        # An abstract set under the author with no label, JeongEun Lee's and Peter Jacobs's in the
        # 2012 volume: five lines of prose or more, each but the last 35 characters long or more, run
        # to the first section heading. It is one note, as a labeled abstract is. It never opens on
        # the author's department, Andie Diane Palmer's in the 2013 volume, which stays a note of its own.
        plain = []
        if not abstract and not keywords:
            tail = [one for one in lines if one > first_author]
            for index in range(len(tail)):
                run = tail[index:]
                # Nor on a line set in columns, Gerdts and Peter's Simon Fraser University beside
                # Quamichan First Nation in the 2011 volume.
                if len(run) >= 5 and all(len(self.text(one)) >= 35 for one in run[:-1]) and \
                        not re.match(r"(?:Department|University|College|Institute)\b", self.text(run[0])) and \
                        not re.search(r"\S\s{3,}\S", self.spaced[run[0]]) and \
                        not any(name in self.text(one) for one in run for name in authors):
                    plain = run
                    break
        done.update(plain)
        for number in lines:
            if number < first_author or number in done:
                continue
            text = self.text(number)
            names = [name for name in authors if name in text]
            # Authors set side by side, each over a column of their own: John Lyon over California
            # State University, Fresno and Ewa Czaykowska-Higgins over University of Victoria. Each
            # line under them splits at its wide gaps into a piece for each author.
            under = []
            for below in lines[lines.index(number) + 1:]:
                if any(name in self.text(below) for name in authors):
                    break
                under.append(below)
            pieces = [re.split(r"\s{3,}", self.spaced[one]) for one in under]
            # The lines in columns end where the abstract opens under them, unlabeled, as in Gerdts
            # and Peter's.
            if len(names) > 1 and plain:
                kept = next((at for at, one in enumerate(pieces) if len(one) != len(names)), len(pieces))
                under, pieces = under[:kept], pieces[:kept]
            if len(names) > 1 and under and all(len(one) == len(names) for one in pieces):
                names.sort(key=text.index)
                for column, name in enumerate(names):
                    self.add("front", A, "name", name, "author")
                    self.mentioned.add(("name", name))
                    for piece in pieces:
                        self.add("front", A, "note", piece[column], "page 1, under %s" % name)
                done.update(under)
                current = names[-1]
                continue
            if names:
                for name in names:
                    self.add("front", A, "name", name, "author")
                    self.mentioned.add(("name", name))
                # A line holding more than the names, their commas and marks, prints it all.
                rest = text
                for name in names:
                    rest = rest.replace(name, "")
                if re.sub(r"[\s,&*∗†\d]|\band\b", "", rest):
                    self.add("front", A, "note", text, "page 1, the author line")
                current = names[-1]
            else:
                self.add("front", A, "note", text, "page 1, under %s" % current)
        if abstract:
            self.add("front", A, "note", self.joined(range(abstract, close or abstract + 1)), "page 1, the abstract")
        if plain:
            self.add("front", A, "note", self.joined(plain), "page 1, the abstract")
        # Keywords that end a line on a comma run onto the next, modal-temporal interactions under
        # Reisinger's, and so do keywords whose next line opens in lower case, lexical and structural
        # / copying in Nater's Tsimshianic vestiges.
        wrapped = keywords
        while wrapped and wrapped < self.last and (self.text(wrapped).endswith(",") or
                                                   self.text(wrapped + 1)[:1].islower()):
            wrapped += 1
        if keywords:
            self.add("front", A, "note", self.joined(range(keywords, wrapped + 1)), "page 1, the keywords")
        if header:
            self.add("front", A, "note", self.joined(sorted(header)), "page %d, the volume's header" % self.page(min(header)))
        for name, why in languages:
            self.add("front", A, "language", name, why)
            self.mentioned.add(("language", name))
        return wrapped or (close - 1 if close else abstract) or max(lines)

    def standard(self, authors, names=(), languages=(), front_languages=1, displays=None, blocks=None,
                 references=r"^References$", notes_title=tuple(TITLE_MARKS), restarts=None, appendix=None,
                 headings=None, spaced=None, glued=None):
        """Write the whole paper the common way: the front matter, the notes on the title, the body
        from the keywords to the references with its headings, examples, displays and blocks, each
        footnote after the paragraph that carries its mark, and the references an entry a row.
        glued passes to place_footnotes().
        displays maps an example number to its printed line count; restarts passes to flow(), and
        spaced to place_footnotes(). With
        appendix, a pattern, the references end at the first line after them it matches, and the
        generator writes what follows. Returns the footnotes found."""
        # A language's name the paper sets in italics, St'át'imcets in Davis, Huijsmans and
        # Mellesmoen's second paper, is a language row and no cited form.
        self.language_names = {name for name, _ in languages}
        start = self.front(authors, languages[:front_languages])
        found = self.page_footnotes()
        skip = {one for parts, _ in found.values() for one in parts} | set(self.volume_header())
        end = self.find(references, start) or self.last + 1

        def write(mark):
            parts, at = found[mark]
            self.footnote(mark, parts, at, names, languages,
                          gloss="page %d, footnote %s%s" % (at, mark,
                                                            ", on the title" if mark in notes_title else ""))

        for mark in found:
            if mark in notes_title:
                write(mark)
            elif mark == UNMARKED:
                parts, at = found[mark]
                self.footnote(mark, parts, at, names, languages, where="front",
                              gloss="page %d, the note at the foot of the page with no mark" % at)
        blocks = dict(blocks or {})
        for number in range(start + 1, end):
            opened = EXAMPLE.match(self.text(number))
            if opened and displays and opened.group(1) in displays and number not in skip:
                spec = displays[opened.group(1)]
                count, per_line = spec if isinstance(spec, tuple) else (spec, False)
                blocks[number] = (lambda count, per_line: lambda first, where: self.display(
                    first, count, per_line=per_line))(count, per_line)
        headings = headings if headings is not None else self.headings(start + 1, end - 1, skip=skip)
        self.flow(start + 1, end - 1, "front", headings=headings, skip=skip, blocks=blocks,
                  names=names, languages=languages, restarts=restarts)
        missing = self.place_footnotes([one for one in found if one not in notes_title and one != UNMARKED], write,
                                       spaced=spaced, glued=glued)
        if end <= self.last:
            tail = (self.find(appendix, end) if appendix else None) or self.last + 1
            self.add("references", A, "heading", self.text(end), "page %d" % self.page(end))
            for entry, at in self.references(end + 1, tail - 1, skip=skip):
                self.add("references", A, "reference", entry, "page %d" % at)
            # A mark no row of the body carries can stand on a reference, Toronto.18 after
            # Mackenzie's Voyages in Nater's old records; its footnote follows that reference.
            if missing:
                missing = self.place_footnotes(missing, write, placed=[one for one in found if one not in missing],
                                               spaced=spaced, glued=glued, kinds=("reference",), only=("reference",))
        if missing:
            print("# footnotes not placed:", missing, file=sys.stderr)
        return found

    def languages_by_tag(self, tags, speakers=None):
        """Set the language of each example's own rows from the tag at the right of its
        translation, or from a consultant's initials there: {tag: language}, {initials: language}.
        Every lettered part takes the language of the example. Returns the examples left untagged."""
        found = {}
        # An example's own label, with the footnote it stands in: footnote 17 (ii), (12).
        label = re.compile(r"^((?:footnote \S+ )?\((?:\d+|[ivx]+))[a-z]?\) line")
        for where, who, kind, form, gloss in self.rows:
            example = label.match(where)
            # A bare language name at the right, (Lillooet), is a note and names the language too.
            if example and kind in ("citation", "note"):
                # A tag's end is no word's inside, (Nsyilxcn) with its parenthesis as Lillooet.
                tag = re.match(r"^(%s)(?!\w)" % "|".join(re.escape(one) for one in sorted(tags, key=len, reverse=True)), form)
                initials = [one for one in re.findall(r"\b[A-Z]{2,3}\b", form) if speakers and one in speakers]
                if tag:
                    found.setdefault(example.group(1), tags[tag.group(1)])
                elif initials:
                    found.setdefault(example.group(1), speakers[initials[0]])
        for row in self.rows:
            example = label.match(row[0])
            if example and row[1] == L and example.group(1) in found:
                row[1] = found[example.group(1)]
        return sorted({label.match(row[0]).group(1) + ")" for row in self.rows
                       if row[1] == L and label.match(row[0])})

    def languages_by_caption(self, names):
        """Set the language of each example's own rows from the caption over it, St'át'imcets:,
        where names ({caption: language}) holds the caption without its colon. Returns the examples
        left uncaptioned."""
        label = re.compile(r"^((?:footnote \S+ )?(?:§\S+ )?\((?:\d+|[ivx]+))[a-z]*\) line")
        found = {}
        for where, who, kind, form, gloss in self.rows:
            example = label.match(where)
            if example and kind == "note" and form.endswith(":") and form[:-1] in names:
                found.setdefault(example.group(1), names[form[:-1]])
        for row in self.rows:
            example = label.match(row[0])
            if example and row[1] == L and example.group(1) in found:
                row[1] = found[example.group(1)]
        return sorted({label.match(row[0]).group(1) + ")" for row in self.rows
                       if row[1] == L and label.match(row[0])})

    # Rows.

    def add(self, where, who, kind, form, gloss):
        form = " ".join(form.split())
        if form:
            self.rows.append([where, who, kind, form, gloss])

    def ops(self):
        out = []
        if self.authors:
            out.append("meta authors %s" % self.authors)
        if self.language:
            out.append("meta lang %s" % self.language)
        # The rest of the context make_md.py writes from: title, byline, volume, whose and letters.
        for key, value in getattr(self, "meta", {}).items():
            out.append("meta %s %s" % (key, " ".join(value.split())))
        out.append("removewhere .")
        chain = ("", "")
        for where, who, kind, form, gloss in self.rows:
            form = form.replace("|", "\\|")
            out.append("add %s | %s | %s | %s | %s | %s | %s" % (
                chain[0], chain[1], where, who, kind, form, gloss.replace("|", "\\|")))
            chain = (where, form)
        return out

    def write(self):
        """Write ops/<stem>.ops and say how many rows it adds. An ops file whose ops are unchanged is left
        as it stands, its stamp line included, and the closed corpus's inventory does not move."""
        target = os.path.join(PRIVATE, "ops", self.stem + ".ops")
        body = "\n".join(self.ops()) + "\n"
        held = None
        if os.path.isfile(target):
            with open(target, encoding="utf-8", newline="") as handle:
                held = handle.read()
        if held is None or held.split("\n", 1)[-1] != body:
            with open(target, "w", encoding="utf-8", newline="\n") as handle:
                handle.write("# written by a generator over gen.py, %s\n" % time.strftime("%Y-%m-%d %H:%M"))
                handle.write(body)
        print("%s: %d rows" % (target, len(self.rows)), file=sys.stderr)
