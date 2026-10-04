"""The ops of 16-Robertson_ICSNL_final-34: David Douglas Robertson on the Lower Chehalis
(ɬəw̓ál̓məš) loans in Chinook Jargon beyond the 39 of Kinkade et al. (2010), the false positives
among them, and what the pidgin can give back to the revitalization of its lexifier.

Tables 1 to 14 set four columns, language, word, gloss and source, and each running on over pages
repeats the line that heads them. Each word is given to its column by where it stands. A line with
no language cell is a further word of the language above it, and a line with neither a language nor
a word runs its gloss and source on. The source cells carry the marks of notes 3, 4, 7 and 9 to 34
and the word and gloss cells those of notes 11 and 8; each note is written after the row that
carries its mark.
"""
import os
import collections
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
STEM = "16-Robertson_ICSNL_final-34"
LANGUAGE = "Lower Chehalis"
AUTHORS = ["David Douglas Robertson"]
paper = gen.Paper(STEM, authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Kinkade", "M. Dale Kinkade, the Lower Chehalis loans (2010) and the Upper Chehalis and Cowlitz dictionaries"),
         ("Terry and Larry Thompson", "ICSNL founders, the paper's dedication"),
         ("Earl Davis", "thanked, note 1"), ("Tony A. Johnson", "thanked, note 1"),
         ("Jay Powell", "thanked, note 1; Quileute pidgin teaching (1973)"),
         ("Jedd Schrock", "thanked, note 1"), ("Sam Sullivan", "thanked, note 1"), ("Henry Zenk", "thanked, note 1"),
         ("Gibbs", "George Gibbs, the CJ dictionary (1863a) and the Chinook vocabulary (1863b)"),
         ("Swan", "James G. Swan, the Shoalwater Bay vocabulary (1857)"),
         ("Eells", "Myron Eells, the Chinook Jargon (1894)"), ("Shaw", "the CJ compendium (1909)"),
         ("Le Jeune", "Chinook rudiments (1924)"), ("Boas", "Franz Boas, Chinook (1910) and notes (1890)"),
         ("Harrington", "John P. Harrington, field notes (1942)"),
         ("Gill", "John Kaye Gill, the CJ dictionary (1909)"),
         ("Samuel V. Johnson", "the computer assisted analysis of CJ (1978)"),
         ("F.N. Blanchet", "the 1853 CJ dictionary Gill republished"),
         ("Charles Cultee", "his use of míɬt, note 3"), ("Elmendorf", "William Elmendorf, word taboo (1951)"),
         ("Emma Luscier", "the 1942 reelicitations, note 30"), ("Victoria Howard", "narrations, note 31"),
         ("Charles Snow", "Kinkade's student, the phonology (1969)")]
LANGUAGES = [(LANGUAGE, "ɬəw̓ál̓məš, Tsamosan, Coast Salish"), ("Chinook Jargon", "the pidgin, CJ"),
             ("Chinookan", "the lower Columbia neighbor"), ("Upper Chehalis", "Tsamosan"),
             ("Cowlitz", "Tsamosan"), ("Quinault", "Tsamosan"), ("Tillamook", "Table 1"),
             ("Lushootseed", "Tables 2 and 3"), ("Twana", "Table 3's discussion; word taboo"),
             ("Haida", "Table 4"), ("Nuuchahnulth", "Table 5"), ("Kalapuyan", "Table 6"),
             ("Sahaptin", "Table 7"), ("Sechelt", "Table 7"), ("Quileute", "Tables 7 and 8"),
             ("Proto-Salish", "PS"), ("Proto-Interior Salish", "PIS"), ("Shuswap", "lahanʃut, putah"),
             ("Maritime Polynesian Pidgin", "Drechsel 2014"), ("Russenorsk", "Jahr 1996"),
             ("Klallam", "note 5"), ("Heiltsuk", "note 5"), ("Mandarin Chinese", "note 7"), ("Russian", "note 7"),
             ("Songish", "note 19"), ("Lillooet", "note 19"), ("Thompson", "note 19"),
             ("Kwak’wala", "pidgin syntax teaching"), ("Palawa Kani", "Tasmania")]

# The language cell of a table and the language of the words beside it: "CJ" and "Chinook Jargon" in
# quotes are Tables 10 and 11's Chinook Jargon mistakenly so called, the language row as printed.
CELL_LANGUAGE = {"Chinook Jargon": "Chinook Jargon", "“CJ”": "Chinook Jargon", "“Chinook Jargon”": "Chinook Jargon",
                 "Chinookan": "Chinookan", "ɬəw̓ál̓məš": L, "Upper Chehalis": "Upper Chehalis",
                 "Cowlitz": "Cowlitz", "Quinault": "Quinault", "Tillamook": "Tillamook",
                 "Lushootseed": "Lushootseed", "Haida": "Haida", "Nuuchahnulth": "Nuuchahnulth",
                 "Kalapuyan": "Kalapuyan", "Sahaptin": "Sahaptin", "Sechelt": "Sechelt", "Quileute": "Quileute",
                 "PS": "Proto-Salish", "PIS": "Proto-Interior Salish"}

# A source cell that closes on a note's mark set against an archive code, whose digits run on into
# the mark's: LHcs19670817.1503 and note 3. Notes 24 to 27 the page sets at the size of the code,
# ELjh1942.18.649 24, and the stream reads them as it reads the raised ones. A source that closes
# on a parenthesis, Gibbs (1863a)9, is read by its digits after it.
MARKED = {"LHcs19670817.15033": ("LHcs19670817.1503", "3"), "ISmk19781015.1667": ("ISmk19781015.166", "7"),
          "LHcs19670817.145512": ("LHcs19670817.1455", "12"), "ELjh1942.18.46613": ("ELjh1942.18.466", "13"),
          "ELjh1942.18.32816": ("ELjh1942.18.328", "16"), "ELjh1942.17.31017": ("ELjh1942.17.310", "17"),
          "ELjh1942.18.64924": ("ELjh1942.18.649", "24"), "ISmk19781015.10925": ("ISmk19781015.109", "25"),
          "ELjh1942.18.49026": ("ELjh1942.18.490", "26"), "ELjh1942.17.4628": ("ELjh1942.17.46", "28"),
          "NBcs19670512.29329": ("NBcs19670512.293", "29"), "LHcs19670619.19632": ("LHcs19670619.196", "32"),
          "ELjh1942.17.38134": ("ELjh1942.17.381", "34"),
          # Note 11 on the ? of ɬəw̓ál̓məš in Table 9, and note 8 on the gloss ‘Q’.
          "?11": ("?", "11"), "‘Q’8": ("‘Q’", "8")}
FULL_SIZE = {"24", "25", "26", "27"}

# A form in the prose is a form of the language the text names right ahead of it, CJ tsole-pat,
# Upper Chehalis √ɬə́n, PS *kʷú(l)-st(ə)w-m.
NAMED = dict(CELL_LANGUAGE)
NAMED.update({"CJ": "Chinook Jargon", "CW": "Chinook Jargon", "Proto-Salish": "Proto-Salish"})
NAMED.update({name: name for name, _ in LANGUAGES if name != LANGUAGE})
NAMED_AHEAD = re.compile(r"(?:^|[\s(])(%s)\s*[*(]*$" % "|".join(
    re.escape(one) for one in sorted(NAMED, key=len, reverse=True)))


def language_before(text):
    named = NAMED_AHEAD.search(text[-40:])
    return NAMED[named.group(1)] if named else None


paper.language_before = language_before

# The forms in the prose whose language the text names further off, and the forms the italics
# give short: the upright star of a reconstruction, *kʷú(l)-st(ə)w-m, the brackets of a definition,
# <hul>, the hyphen a line end breaks off, -il, and the root sign of a compound read as náw ‘big’.
# (where, form as read): (language, form as printed, gloss or None).
CJ = "Chinook Jargon"
FORMS = {("§1", "lahanʃut"): (CJ, None, None), ("§1", "máh-lie"): (CJ, None, None),
         ("§1", "√báli"): ("Lushootseed", None, None),
         ("footnote 9", "yáxwəl"): (CJ, None, None),
         ("footnote 9", "hul>/<whul>/xʷəl"): (CJ, "<hul>/<whul>/xʷəl", None),
         ("footnote 10", "kʷústm-"): ("Cowlitz", None, None),
         ("footnote 10", "*kʷú(l)-st(ə)w-m"): ("Proto-Salish", None, None),
         ("footnote 10", "√líl-"): ("Upper Chehalis", None, None),
         ("footnote 10", "č"): ("Upper Chehalis", "*k>č", None),
         ("footnote 10", "čó:yaʔ"): ("Upper Chehalis", None, None),
         ("footnote 11", "ǰ"): ("Lushootseed", None, None), ("footnote 11", "- il"): ("Lushootseed", "-il", None),
         ("footnote 11", "*yuʔ"): ("Proto-Salish", None, None),
         ("footnote 12", "*-m"): ("Proto-Salish", None, None),
         ("footnote 13", "*x>š"): (L, None, None),
         ("footnote 13", "*mix/mixʷ"): ("Proto-Salish", None, None),
         ("footnote 13", "a-"): ("Chinookan", None, None), ("footnote 13", "i-"): ("Chinookan", None, None),
         ("footnote 14", "s-"): (L, "s-√x̣il=á=q=mi(n)", None),
         ("footnote 17", "wən"): ("Upper Chehalis", None, None),
         ("footnote 17", "=iɬ=či"): ("Upper Chehalis", None, None),
         ("footnote 28", "-hæn"): (CJ, None, None), ("footnote 29", "klís-pʰáya"): (CJ, None, None),
         ("footnote 30", "=ál(=)s"): ("Upper Chehalis", None, None),
         ("footnote 30", "√nəw=ál(=)s"): ("Upper Chehalis", None, None),
         ("footnote 30", "náw"): (L, None, "page 24, in italics, ‘big’"),
         ("footnote 30", "s-√náw=ucn"): ("Upper Chehalis", None, None),
         ("footnote 30", "√naw=áy̓s"): ("Upper Chehalis", None, None),
         ("footnote 30", "√naw=áps"): ("Upper Chehalis", None, None),
         ("footnote 31", "p̓íʔns"): (CJ, None, None),
         ("footnote 31", "*p̓ə́n"): ("Proto-Salish", None, None),
         ("footnote 32", "/ə/"): (CJ, None, None)}
# The spaces the page text sets where the page sets none, read off renders: note 10's k>(k)x, note
# 14's s-√x̣il=á=q=mi(n) and Table 10's (s-)ʔac(‘)-=íl(‘)als; and the hyphen of note 11's -il the
# line end breaks from its letters.
TEXT = (("k> (k)x", "k>(k)x"), ("s-√x̣il=á =q=mi(n)", "s-√x̣il=á=q=mi(n)"),
        ("(s-)ʔac(‘)- =íl(‘)als", "(s-)ʔac(‘)-=íl(‘)als"), ("sequence - il", "sequence -il"))


def as_printed():
    """Set each prose form's language and printed form, and each row's text as the page sets it."""
    for row in paper.rows:
        where, _, kind, form, _ = row
        if kind == "cited form" and (where, form) in FORMS:
            language, printed, glossed = FORMS.pop((where, form))
            row[1], row[3], row[4] = language, printed or form, glossed or row[4]
        for wrong, right in TEXT:
            row[3] = row[3].replace(wrong, right)
    assert not FORMS, FORMS

RUNNING = paper.running_numbers_set()
HEADER = re.compile(r"^language\s+word\s+gloss\s+source$")
CAPTION = re.compile(r"^Table (\d+) ")
state = {"table": None, "rows": 0}
# Note 2's mark stands on a table's number, Table 1:2, where gen reads a digit after a colon as no
# mark; it is placed by hand, and the notes on the tables' cells by the rows that carry them.
TABLE_MARKS = [str(one) for one in (2, 3, 4, 7, 8) + tuple(range(9, 35))]
place = paper.place_footnotes
paper.place_footnotes = lambda marks, write, placed=(), **rest: place(marks, write, placed=tuple(placed) + tuple(TABLE_MARKS),
                                                                      **rest)


def note(mark):
    parts, at = FOUND[mark]
    paper.footnote(mark, parts, at, NAMES, LANGUAGES, gloss="page %d, footnote %s" % (at, mark))


def edges(header):
    """The left edges of the word, gloss and source columns. The line heading them does not stand
    over its cells: Table 5 sets source at 253pt and its sources at 273.7pt, and the gloss ‘go + try
    to get s.t. to eat from s.o.’ runs past 253pt. Each column's edge is where most of the words
    near its heading start, in the lines of the table under it."""
    found = paper.word_positions(header)
    assert [word for _, word in found] == ["language", "word", "gloss", "source"], found
    heads = [left for left, _ in found]
    rough = [left - 3 for left in heads[1:]]
    lefts = []
    number = header + 1
    while number <= paper.last:
        if number in RUNNING or paper.lines[number][2] or not paper.text(number).strip():
            number += 1
            continue
        if table_line(number, rough) is None:
            break
        lefts.extend(round(left, 1) for left, _ in paper.word_positions(number))
        number += 1
    bounds = []
    for column, head in enumerate(heads[1:], 1):
        near = collections.Counter(one for one in lefts if abs(one - head) <= 25)
        bounds.append(near.most_common(1)[0][0] - 3 if near else rough[column - 1])
    return bounds


def cells(number, bounds):
    """{column: text} for a table line, the words of each column joined."""
    found = paper.word_positions(number)
    assert found, "no glyph row for line %d: %s" % (number, paper.text(number))
    held = {}
    for left, word in found:
        column = sum(left >= bound for bound in bounds)
        held[column] = held[column] + " " + word if column in held else word
    return held


def table_line(number, bounds):
    """The cells of line number where it is a line of the table, else None: a line of prose opens
    at the margin on a word that is no language cell."""
    text = paper.text(number)
    if not text.strip() or CAPTION.match(text) or HEADER.match(text):
        return None
    held = cells(number, bounds)
    if 0 in held and held[0] not in CELL_LANGUAGE:
        return None
    return held


def unmarked(text):
    """A cell's text and the mark of the note it carries, or None."""
    if text in MARKED:
        return MARKED[text]
    mark = re.search(r"(?<=\))(\d+)$", text)
    if mark:
        return text[:mark.start()], mark.group(1)
    return text, None


def table(start, where):
    """A table's caption or the line that heads its columns on a page it runs on to, and each of
    its rows: the language cell a language row, the word a cited form of that language or a note
    where it is ? or idem, the gloss a translation and the source a citation."""
    text = paper.text(start)
    page = paper.page(start)
    number = start
    caption = CAPTION.match(text)
    if caption:
        state["table"], state["rows"] = caption.group(1), 0
        label = "Table %s" % state["table"]
        paper.add(label, A, "note", text, "page %d, the caption" % page)
        paper.mentions(label, text, LANGUAGES, "language")
        number += 1
    label = "Table %s" % state["table"]
    assert HEADER.match(paper.text(number)), paper.text(number)
    paper.add(label, A, "note", paper.text(number), "page %d, heads the columns" % page)
    bounds = edges(number)
    number += 1
    rows = []
    while number <= paper.last:
        if number in AT_FOOT or number in RUNNING or paper.lines[number][2]:
            number += 1
            continue
        held = table_line(number, bounds)
        if held is None:
            break
        if 0 in held or (1 in held and not (rows and rows[-1]["word"].endswith(","))):
            rows.append({"language": held.get(0), "word": held.get(1, ""), "gloss": held.get(2, ""),
                         "source": held.get(3, ""), "page": paper.page(number)})
        else:
            # A word cell broken after its comma, <ny-ee’-na>, <my-ee’-na>, and a gloss or a source
            # run on to the line under it.
            for column, key in ((1, "word"), (2, "gloss"), (3, "source")):
                if column in held:
                    rows[-1][key] = (rows[-1][key] + " " + held[column]).strip()
        number += 1
    language = None
    for row in rows:
        state["rows"] += 1
        at = "%s row %d" % (label, state["rows"])
        page = row["page"]
        marks = []
        if row["language"]:
            language = row["language"]
            paper.add(at, A, "language", language, "page %d, %s, language" % (page, label))
            paper.mentioned.add(("language", CELL_LANGUAGE[language]))
        who = CELL_LANGUAGE[language]
        word, mark = unmarked(row["word"])
        marks.append(mark)
        if word in ("?", "idem"):
            paper.add(at, A, "note", word, "page %d, %s, word%s" % (
                page, label, ", carries footnote " + mark if mark else ""))
        elif word:
            for form in re.split(r"(?<=[,;]) ", word):
                paper.add(at, who, "cited form", form.rstrip(",;"), "page %d, %s, word" % (page, label))
        gloss, mark = unmarked(row["gloss"])
        marks.append(mark)
        if gloss:
            paper.add(at, A, "translation", gloss, "page %d, %s, gloss%s" % (
                page, label, ", carries footnote " + mark if mark else ""))
        source, mark = unmarked(row["source"])
        marks.append(mark)
        if source:
            paper.add(at, A, "citation", source, "page %d, %s, source%s" % (
                page, label, ", carries footnote %s%s" % (mark, ", set full size" if mark in FULL_SIZE else "")
                if mark else ""))
        for one in marks:
            if one:
                note(one)
    return number


def table_lines():
    """The lines of every table, its caption and the line heading its columns included: the tables
    are set at the size of the notes, and a note run on from the page before would take in the
    table set above it, Table 9 on page 8 under note 5."""
    held = set()
    for number in range(1, paper.last + 1):
        if not HEADER.match(paper.text(number)):
            continue
        held.add(number)
        if CAPTION.match(paper.text(number - 1)):
            held.add(number - 1)
        bounds = edges(number)
        number += 1
        while number <= paper.last:
            if number in RUNNING or paper.lines[number][2] or not paper.text(number).strip():
                number += 1
                continue
            if table_line(number, bounds) is None:
                break
            held.add(number)
            number += 1
    return held


FOUND = paper.page_footnotes(stops=table_lines())
paper.page_footnotes = lambda *args, **kwargs: FOUND
AT_FOOT = {one for parts, _ in FOUND.values() for one in parts}


def place_by_hand(mark, pattern):
    """Write a note's rows after the first row of the text that matches pattern."""
    at = next(index for index, row in enumerate(paper.rows)
              if row[2] == "note" and not row[0].startswith("footnote") and re.search(pattern, row[3]))
    held, paper.rows = paper.rows, []
    note(mark)
    paper.rows = held[:at + 1] + paper.rows + held[at + 1:]


blocks = {}
for number in range(1, paper.last + 1):
    if number in AT_FOOT:
        continue
    text = paper.text(number)
    if CAPTION.match(text) and HEADER.match(paper.text(number + 1)) or \
            HEADER.match(text) and not CAPTION.match(paper.text(number - 1)):
        blocks[number] = table

# Section 1's title holds a stop, P.S., and 2.3's opens on a quote; both are read by hand.
HEADINGS = {}
for pattern, label in ((r"^1\s+Introduction: P\.S\.", "1"), (r"^2\s+Beware of false positives$", "2"),
                       (r"^2\.1 Sorry, wrong language$", "2.1"), (r"^2\.2 Long-term Chinookan", "2.2"),
                       (r"^2\.3 “Mistaken CJ”", "2.3"), (r"^3\s+But there is still much more", "3"),
                       (r"^4\s+And now, the best part", "4")):
    HEADINGS[paper.find(pattern)] = label
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, headings=HEADINGS)
place_by_hand("2", r"shown in Table 1:2(?!\d)")
as_printed()
paper.write()
