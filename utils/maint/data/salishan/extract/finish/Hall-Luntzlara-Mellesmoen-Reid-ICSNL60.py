# Context for Hall, Luntzlara, Mellesmoen and Reid, The Long Schwa Paper: Stressed Schwa Epenthesis in
# nɬeʔkepmxcín. Every example is nɬeʔkepmxcín. The sixteen OT tableaux of Section 3, the tables and
# the constraint definitions are read off the page text below, the way residue.py reads it, and the
# violation marks of each tableau are put under their constraints by the glyph positions of the page
# (table_cells.py), which the text layer does not keep.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import PRIVATE  # noqa: E402
from workdir import WORK  # noqa: E402
import residue  # noqa: E402
import table_cells  # noqa: E402

STEM = "Hall-Luntzlara-Mellesmoen-Reid-ICSNL60"
AUTHORS = "Brent Hall, Noah Luntzlara, Gloria Mellesmoen and Danica Reid"
N = "nɬeʔkepmxcín"

TITLE = "The Long Schwa Paper: Stressed Schwa Epenthesis in nɬeʔkepmxcín"
BYLINE = ("Brent Hall, Noah Luntzlara and Gloria Mellesmoen, University of British Columbia; "
          "Danica Reid, Simon Fraser University")

_repair = residue.paper_repair(STEM)
with open(os.path.join(PRIVATE, "pagetext", STEM + ".txt"), encoding="utf-8") as handle:
    PAGE = [" ".join(_repair(one).split()) for one in handle.read().split("\n")]

with open(os.path.join(WORK, STEM + ".draft.tsv"), encoding="utf-8") as handle:
    DRAFT = [one.rstrip("\n").split("\t") for one in handle][1:]


def line(opening, after=0):
    """The page line opening on these words, the first after line number after."""
    for number, text in enumerate(PAGE):
        if number >= after and text.startswith(opening):
            return text
    raise SystemExit("no page line opens with %s" % opening)


def at(opening, after=0):
    """The index in PAGE of the line opening on these words."""
    for number, text in enumerate(PAGE):
        if number >= after and text.startswith(opening):
            return number
    raise SystemExit("no page line opens with %s" % opening)


def page_of(index):
    """The page a PAGE line stands on."""
    for number in range(index, -1, -1):
        found = re.match(r"^===== page (\d+) =====$", PAGE[number])
        if found:
            return int(found.group(1))
    return 1


def prose(first, last):
    """The page lines of one paragraph from the one opening on first to the one opening on last, as
    one text, leaving out the footnotes and the page break a paragraph runs over. The footnotes of a
    page follow an empty line, and the next page opens with its marker, an empty line and its
    number."""
    start = at(first)
    end = at(last, start)
    words = []
    skipping = False
    for number in range(start, end + 1):
        text = PAGE[number]
        if text.startswith("===== page"):
            skipping = "number"
            continue
        if skipping == "number":
            if re.fullmatch(r"\d{2,3}", text):
                skipping = False
            continue
        if not text:
            skipping = True
            continue
        if skipping:
            continue
        words.append(text)
    return " ".join(words)


def display(anchor, rows):
    """ADD entries for rows (where, who, kind, form, gloss), each placed after the one before it,
    the first after anchor."""
    added = []
    for row in rows:
        added.append((anchor, row))
        anchor = (row[0], row[3])
    return added


SPEAKERS = {"BP": "Bev Phillips", "CMA": "Marty Aspinall", "KBG": "Bernice Garcia"}


def source_who(source):
    """The who of a translation, from the source its example cites: the speakers by the initials of
    Section 2.1, or the work. T&T is Thompson and Thompson."""
    source = source.strip("() ")
    if re.fullmatch(r"[A-Z]{2,3}(, [A-Z]{2,3})*", source):
        names = [SPEAKERS[one] for one in source.split(", ")]
        return names[0] if len(names) == 1 else ", ".join(names[:-1]) + " and " + names[-1]
    return source.replace("T&T", "Thompson & Thompson")


def marks_of(text):
    """The violation marks of a text, as one string."""
    return "".join(re.findall(r"[*!]+", text))


TABLEAUX = {69: "(69) Onset clusters of more", 70: "(70) Complex onsets", 71: "(71) Onset clusters containing",
            72: "(72) Complex codas are", 73: "(73) Both syllabic", 74: "(74) Syllabic glides",
            75: "(75) Vocalized glides", 76: "(76) Syllabic liquids", 77: "(77) Superheavy",
            78: "(78) Complex codas with", 84: "(84) Rightmost vowel", 85: "(85) Leftmost input",
            86: "(86) Leftmost vowel does", 87: "(87) Leftmost vowel position", 89: "(89) Schwa is inserted and",
            90: "(90) Schwa is inserted exactly"}
AFTER = ("Nasal vocalization:", "Glide vocalization:", "Unstressable morphology:")


def spelled(named, printed):
    """The heads named off the glyphs, each defined the way the text layer prints it. A glyph can
    drop a hyphen the layer keeps, L over ANCH(FV) for L-ANCH(FV) in (86), and each name takes the
    stretch of the printed heads that matches it with the hyphens left out."""
    rest = "".join(printed.split())
    out = []
    for left, right, name in named:
        bare = name.replace("-", "")
        for end in range(1, len(rest) + 1):
            if rest[:end].replace("-", "") == bare:
                break
        else:
            raise SystemExit("no printed head %s in %s" % (name, rest))
        out.append((left, right, rest[:end]))
        rest = rest[end:]
    if rest:
        raise SystemExit("printed heads left over: %s" % rest)
    return out


def columns(number, page, count):
    """The constraint heads of tableau number on its page, left to right, each (left, right, name),
    and the violation marks of each candidate line, from the glyph positions.

    A head the page breaks over two or three lines, DEP- over μ(N), is the words of those lines
    that stand over one another. The Symbol font's star and exclamation mark come through the text
    layer as U+F02A and U+F021.
    """
    lines = [" ".join(word[2] for word in words) for baseline, words in table_cells.word_lines(STEM, page)]
    rows = [words for baseline, words in table_cells.word_lines(STEM, page)]
    title = next(index for index, text in enumerate(lines) if text.startswith("(%d) " % number))
    head = next(index for index in range(title, len(rows)) if rows[index][0][2].startswith("/"))
    # A candidate's footnote number stands raised above its line, 20 over (85a); it is no head.
    parts = list(rows[head][1:])
    index = head + 1
    while not re.fullmatch(r"[a-h]\.", rows[index][0][2]):
        parts.extend(word for word in rows[index] if not word[2].isdigit())
        index += 1
    heads = []
    for word in sorted(parts, key=lambda one: one[0]):
        if heads and word[0] <= heads[-1][1] + 4:
            heads[-1] = [heads[-1][0], max(heads[-1][1], word[1]), heads[-1][2] + [word]]
        else:
            heads.append([word[0], word[1], [word]])
    named = []
    for left, right, words in heads:
        # The parts of a head stand in the order of the lines, top to bottom.
        order = {id(one): place for place, one in enumerate(parts)}
        name = "".join(one[2] for one in sorted(words, key=lambda one: order[id(one)]))
        named.append((left, right, name.replace("­-", "-").replace("­", "-")))
    # A star can stand a few points above the line of its candidate and fall into a line of its
    # own, and each mark goes to the candidate whose baseline is nearest, and the marks of one cell
    # join left to right: * and ! of *! apart.
    baselines = [baseline for baseline, words in table_cells.word_lines(STEM, page)]
    starts = []
    while len(starts) < count:
        if re.fullmatch(r"[a-h]\.", rows[index][0][2]):
            starts.append(index)
        index += 1
    found = [[] for one in starts]
    for number in range(starts[0] - 1, min(len(rows), starts[-1] + 3)):
        for word in rows[number]:
            text = word[2].replace("", "*").replace("", "!")
            if not re.fullmatch(r"[*!]+", text):
                continue
            nearest = min(range(len(starts)), key=lambda one: abs(baselines[starts[one]] - baselines[number]))
            if abs(baselines[starts[nearest]] - baselines[number]) > 8:
                continue
            found[nearest].append((word[0], table_cells.column_of(word, named), text))
    candidates = []
    for marks in found:
        cells = []
        for left, column, text in sorted(marks):
            if cells and cells[-1][0] == column:
                cells[-1] = (column, cells[-1][1] + text)
            else:
                cells.append((column, text))
        candidates.append(cells)
    return named, candidates


def tableau(number):
    """The rows of one tableau: its title, the attested form with its gloss and source, the rankings
    it establishes, its input and constraint heads, each candidate with its violations, and the
    derivation lines printed under it."""
    start = at(TABLEAUX[number])
    page = page_of(start)
    where = "(%d)" % number
    rows = []
    title = PAGE[start][len(where) + 1:]
    index = start + 1
    while not PAGE[index].startswith("["):
        title += " " + PAGE[index]
        index += 1
    rows.append((where + " line 1", AUTHORS, "note", title, "page %d, the title of the tableau" % page))
    surface = re.match(r"^(\[[^\]]+\])(\d{0,2}) (‘.*’) ?(.*)$", PAGE[index])
    if not surface:
        raise SystemExit("tableau %d: no attested form in %s" % (number, PAGE[index]))
    form, mark, english, source = surface.groups()
    gloss = "page %d, the attested form the tableau derives" % page
    if mark:
        gloss += ", carries footnote %s, written here without its digit" % mark
    rows.append((where + " line 2", N, "phonemic", form, gloss))
    rows.append((where + " line 2", source_who(source) if source else AUTHORS, "translation", english,
                 "page %d" % page + ("" if source else ", printed with no source")))
    if source:
        rows.append((where + " line 2", AUTHORS, "citation", source, "the source of the example"))
    index += 1
    tier = 3
    while not PAGE[index].startswith("/"):
        rows.append((where + " line %d" % tier, AUTHORS, "notation", PAGE[index],
                     "page %d, a ranking the tableau establishes" % page))
        tier += 1
        index += 1
    head = re.match(r"^(/[^ ]+/)(\d{0,2}) (.*)$", PAGE[index])
    if not head:
        raise SystemExit("tableau %d: no input in %s" % (number, PAGE[index]))
    underlying, mark = head.group(1), head.group(2)
    gloss = "page %d, the input of the tableau" % page
    if mark:
        gloss += ", carries footnote %s, written here without its digit" % mark
    rows.append((where + " line %d" % tier, N, "phonemic", underlying, gloss))
    # The heads as the text layer runs them, over as many lines as the page breaks them.
    printed = [head.group(3)]
    index += 1
    while not re.match(r"^[a-h]\. ", PAGE[index]):
        if PAGE[index]:
            printed.append(PAGE[index])
        index += 1
    first = index
    lines = []
    while re.match(r"^[a-h]\. ", PAGE[index]):
        lines.append(PAGE[index])
        index += 1
    named, marks = columns(number, page, len(lines))
    named = spelled(named, " ".join(printed))
    rows.append((where + " line %d" % tier, AUTHORS, "note", " ".join(printed),
                 "page %d, the constraints heading the columns, in ranked order: %s" % (
                     page, ", ".join(one[2] for one in named))))
    for text, found in zip(lines, marks):
        candidate = re.match(r"^([a-h])\. (☞ |\?\? )?(\[.*\])(\d{0,2})( .*)?$", text)
        if not candidate:
            raise SystemExit("tableau %d: no candidate in %s" % (number, text))
        letter, sign, form, mark, tail = candidate.groups()
        if marks_of(tail or "") != "".join(one[1] for one in found):
            raise SystemExit("tableau %d%s: the page text has marks %s and the glyphs %s" % (
                number, letter, marks_of(tail or ""), found))
        if " " in form:
            raise SystemExit("tableau %d%s: a space inside %s" % (number, letter, form))
        gloss = "page %d, candidate %s" % (page, letter)
        who = AUTHORS
        if sign and sign.startswith("☞"):
            gloss += ", the optimal candidate, marked ☞"
            who = N
        elif sign:
            gloss += (", marked ??, one of the two candidates with the same violations the analysis "
                      "does not choose between")
        violations = ["%s %s" % (named[column][2], text) for column, text in found]
        gloss += "; violations: " + ("; ".join(violations) if violations else "none")
        if "!" in "".join(one[1] for one in found):
            gloss += " (! marks a fatal violation)"
        if mark:
            gloss += "; carries footnote %s, written here without its digit" % mark
        rows.append(("%s line 1" % (where[:-1] + letter + ")"), who, "phonemic", form, gloss))
    tier += 1
    while index < len(PAGE) and PAGE[index].startswith(AFTER):
        text = PAGE[index]
        index += 1
        while text.count("(") > text.count(")"):
            text += " " + PAGE[index]
            index += 1
        rows.append((where + " line %d" % tier, AUTHORS, "note", text,
                     "page %d, the derivation from the optimal candidate to the surface form" % page))
        tier += 1
    if index - first > 30:
        raise SystemExit("tableau %d ran on" % number)
    return rows


def table_2():
    """Table 2 on page 15, one row to a line; a line with no category of its own is under the one
    above it."""
    start = at("Table 2. Accentedness")
    rows = [("Table 2", AUTHORS, "note", PAGE[start + 1] + " " + PAGE[start + 2],
             "page 15, the column heads: Category, Morpheme Type, Accented?, Vowels in UR?, Stressable?")]
    category = None
    for number, text in enumerate(PAGE[start + 3:start + 10], 1):
        tokens = text.split()
        if len(tokens) == 5:
            category = tokens.pop(0)
        morpheme, accented, vowels, stressable = tokens
        rows.append(("Table 2 line %d" % number, AUTHORS, "note", text,
                     "page 15, category %s, morpheme type %s, accented? %s, vowels in UR? %s, stressable? %s"
                     % (category, morpheme, accented, vowels, stressable)))
    return rows


def table_a1():
    """Table A1 on page 40, each cell an English label over a suffix, the cells in rows of four
    under Strong, Ambivalent and Weak, which are stressable, and Unstressable (the glyph positions
    put the four columns at 96, 204, 312 and 420 points)."""
    start = at("Table A1: sample")
    heads = ("strong", "ambivalent", "weak", "unstressable")
    rows = [("Table A1", AUTHORS, "note", PAGE[start + 1] + " " + PAGE[start + 2],
             "page 40, the column heads: Strong, Ambivalent and Weak under Stressable, and Unstressable")]
    cells = PAGE[start + 3:start + 35]
    for number in range(16):
        label, form = cells[2 * number], cells[2 * number + 1]
        if not form.startswith("/"):
            raise SystemExit("Table A1 cell %d: %s %s" % (number, label, form))
        rows.append(("Table A1 line %d" % (number // 4 + 1), N, "cited affix", form,
                     "page 40, Table A1, %s, ‘%s’" % (heads[number % 4], label)))
    return rows


def draft_form(where, opening):
    """The form of the draft row of this where opening on these words."""
    for one in DRAFT:
        if one[0] == where and one[3].startswith(opening):
            return one[3]
    raise SystemExit("no draft row %s opens with %s" % (where, opening))


# The sources of the examples, for the who of each translation. An example's source is its
# citation row, or a note or a transcription row the engine gave the tag.
SOURCES = {}
for _where, _who, _kind, _form, _gloss in DRAFT:
    _number = re.match(r"^\((\d+)\)", _where)
    if _number and (_kind == "citation" or re.fullmatch(r"\(T&T [\d:]+\)|[A-Z]{2,3}(, [A-Z]{2,3})*", _form)):
        SOURCES[_number.group(1)] = _form
SOURCES["100"] = "BP"

INTRODUCTION = ("ʔesʔúmecms kʷaɬtèzetkʷuʔ tuɬe c̓əɬétkʷu wéʔe ncítxʷ ƛ̓uʔ wéʔec ʔéx netíyxs "
                "scwéw̓xmx ƛ̓uʔ tékm xéʔe ne nɬeʔképmx e tmíxʷs")
TRANSLATION = "‘My traditional name is kʷaɬtèzetkʷuʔ, my home is in Coldwater of Nicola of the nɬeʔképmx lands.’"

FORMS = {
    N: ("language", AUTHORS, "a.k.a. Thompson River Salish, ISO 639-3 thp, Northern Interior Salish, the language of the paper"),
    "St’át’imcets": ("language", AUTHORS, "a.k.a. Lillooet, Northern Interior Salish"),
    "nɬeʔkepmxcín-to-English": ("language", AUTHORS, "the dictionary of Thompson and Thompson 1996"),
    "Nɬeʔkepmx": ("language", AUTHORS, "in the title of Jimmie 1994"),
    "Nɬeʔkepmxcín": ("language", AUTHORS, "in the title of Khalaji Pirbaluti 2023"),
    "Cupeño": ("language", AUTHORS, "in the title of Alderete 2001, a Uto-Aztecan language"),
    "hən’q’əmin’əm": ("language", AUTHORS, "hən’q’əmin’əm’ (Musqueam) Salish, in the title of Shaw et al. 1999"),
    "c̓úʔsinek": ("name", AUTHORS, "the nɬeʔkepmxcín name of Marty Aspinall, CMA"),
    "kʷaɬtèzetkʷuʔ": ("name", AUTHORS, "the nɬeʔkepmxcín name of Bernice Garcia, KBG"),
    "kʷukʷstéyp": ("cited form", N, "‘thank you’, the thanks of footnote *"),
    "nɬab": ("name", AUTHORS, "the nɬeʔkepmxcín lab at the University of British Columbia, footnote *"),
    "Jóhannsdóttir": ("name", AUTHORS, "K. Jóhannsdóttir, an editor in Shahin 2007"),
    "xʷíʔ": ("cited form", N, "in the title of Hall & Phillips 2024, xʷíʔ kʷ páq (You Will Be Sorry)"),
    "kʷ": ("cited form", N, "in the title of Hall & Phillips 2024, xʷíʔ kʷ páq (You Will Be Sorry)"),
    "páq": ("cited form", N, "in the title of Hall & Phillips 2024, xʷíʔ kʷ páq (You Will Be Sorry)"),
    "ɬ": ("cited form", N, "in the title of Hall & Phillips this volume, ɬ cutés us ɬ qəɬmín ɬ tmíxʷ (When Old One Created the Earth)"),
    "cutés": ("cited form", N, "in the title of Hall & Phillips this volume, ɬ cutés us ɬ qəɬmín ɬ tmíxʷ"),
    "qəɬmín": ("cited form", N, "in the title of Hall & Phillips this volume, ɬ cutés us ɬ qəɬmín ɬ tmíxʷ"),
    "tmíxʷ": ("cited form", N, "in the title of Hall & Phillips this volume, ɬ cutés us ɬ qəɬmín ɬ tmíxʷ"),
    "ʔeskəɬxə̣́n": ("cited form", "Bev Phillips", "page 7, ‘have one’s shoes off’, Figure 1, produced by BP"),
    "slə̣́k": ("cited form", "Bev Phillips", "page 7, ‘turn’, Figure 2, produced by BP"),
    "ɬkép": ("cited form", "Bev Phillips", "page 10, ‘pot/pan’, Figure 4, produced by BP"),
    "ʔustíyxs": ("cited form", "Bev Phillips", "page 11, ‘they discarded it’, Figure 5, produced by BP"),
    "séytknmx": ("cited form", "Bev Phillips", "page 12, ‘Indigenous person/people’, Figure 6, produced by BP"),
    "sqʷə̣́m": ("cited form", "Bev Phillips", "page 40, ‘mountain’, e sqʷə̣́m, Figure B1, produced by BP"),
    "sʕʷəsʕʷə̣́sts": ("cited form", "Bev Phillips", "page 41, ‘…and it shined’, ʔe sʕʷəsʕʷə̣́sts, Figure B2, produced by BP"),
    "xʷə̣́st": ("cited form", "Bev Phillips", "page 41, ‘return home’, Figure B3 produced by BP and Figure B6 by CMA"),
    "cwə̣́m": ("cited form", "Marty Aspinall", "page 42, ‘do/make’, Figure B4, produced by CMA"),
    "scúw": ("cited form", "Marty Aspinall", "page 42, ‘task/work’, tk scúw, Figure B5, produced by CMA"),
    "nsc̓óqʷ": ("cited form", "Bernice Garcia", "page 43, ‘my paper’, Figure B7, produced by KBG"),
    "səxsə̣́x": ("cited form", "Bernice Garcia", "page 44, ‘mistaken’, Figure B8, produced by KBG"),
    "qʷzə̣́z": ("cited form", "Bev Phillips", "page 44, ‘get used’, change-of-state reduplication, Figure B9"),
    "cwúw": ("cited form", "Bev Phillips", "page 45, ‘get made/grow’, change-of-state reduplication, Figure B10"),
    "méƛ̓əƛ̓": ("cited form", "Bev Phillips", "page 45, ‘get mixed’, change-of-state reduplication, Figure B11"),
    "ʔúsəs": ("cited form", "Bev Phillips", "page 46, ‘get discarded’, change-of-state reduplication, the second Figure B10"),
}

# The words of Bernice Garcia's introduction, a row of their own below; the forms the tables, the
# tableaux and footnote 9 hold, rows of their own; and the pieces the text layer or the formulas
# leave in the candidates.
DROP = tuple(INTRODUCTION.split()) + ("ʔe", "nɬeʔkepmxcín9", "nɬeʔkepmxcín.16")

# The consonant clusters Section 2.2 writes in brackets, a hyphen standing for the rest of the
# syllable, and the illicit forms it stars, the syllabifications the language avoids.
CLUSTERS = ("kɬx-", "mƛ̓-", "ʕʷy-", "wm-", "sl-", "py-", "kɬ", "ɬk", "-ƛ̓qt", "-tn", "-kʷlxʷ", "-nm",
            "-ʔnxʷ", "kɬ-", "cx̣ʷ-", "ɬk-", "ɬx̣ʷ")
ILLICIT = ("ʔes.kɬxə́n", "mƛ̓ə́q̓ʷ", "ʕʷyə́p", "wméx", "slə́k", "pyépst", "máʕ.xetn", "ʔíkʷlxʷ", "kénm",
           "qʷúʔnxʷ", "ké.nmN", "slN.ke.tés")
for _form in CLUSTERS:
    FORMS[_form] = ("cited form", AUTHORS, "the consonant cluster [%s]%s" % (
        _form, ", a hyphen standing for the rest of its syllable" if "-" in _form else ""))
for _form in ILLICIT:
    FORMS[_form] = ("cited form", AUTHORS, "the illicit form *[%s], a syllabification the language avoids" % _form)

# Footnote 4's table of `schwa colouring`, a row of its own to a line below.
COLOURING = [PAGE[number] for number in range(at("[e] / __ʔ"), at("[o] / __qʷ") + 1)]
VOWELS = (("Table 1 line 1", "i", "front, a primary vowel"), ("Table 1 line 1", "ị", "front, retracted"),
          ("Table 1 line 1", "u", "back, a primary vowel"), ("Table 1 line 2", "e", "front, a primary vowel"),
          ("Table 1 line 2", "ə", "central, a primary vowel"), ("Table 1 line 2", "ə̣", "central, retracted"),
          ("Table 1 line 2", "o", "back, retracted"), ("Table 1 line 3", "a", "central, retracted"))

SPLIT = (
    ("front", "Brent Hall University", None, {}),
    ("front", "Abstract:", None, {}),
    ("§1.1", "The retracted vowels are less common", {}, {"gloss": "page 2"}),
    ("§1.1", "Table 1. Thompson", {}, None),
    ("footnote 4", "[e] / __ʔ", {}, None),
    ("§2.2.6", "Examples (18)–(24)", None, {}),
    ("footnote 9", "i. /k̓ʷinex/", {}, None),
    ("§2.3", "Between these categories", None, {}),
    ("§2.3", "strong suffix preferred over strong root:", {},
     {"where": "(32) heading", "gloss": "page 15, the heading over (32)"}),
    # Section 3.2.1: the definitions run into the prose around them.
    ("§3.2.1", "(61) FULLVOWELWEIGHT", {}, {"where": "(61)", "gloss": "page 21, the definition of a constraint"}),
    ("§3.2.1", "The constraint in (62) prohibits", {}, {"gloss": "page 21"}),
    ("§3.2.1", "(63) *BRANCHONSET", {}, {"where": "(63)", "gloss": "page 21, the definition of a constraint"}),
    ("§3.2.1", "The constraint in (65) is violated",
     {"where": "(64)", "gloss": "page 21, the definition of a constraint"}, {}),
    ("§3.2.1", "a. *NUC/GLIDE", {"where": "(66)", "gloss": "page 22, the definitions of a family of constraints"},
     {"where": "(66a)", "gloss": "page 22"}),
    ("§3.2.1", "The basic faithfulness", {"where": "(67)", "gloss": "page 22, the definition of a constraint"}, {}),
    ("§3.2.1", "Before presenting the tableaux, we give a preview of the crucial rankings to",
     {"where": "(68b)", "gloss": "page 22"}, {}),
    ("§3.2.1", "Instead of building", {}, {}),
    ("§3.2.1", "• DEP-µ(N), ALIGN-R", {"kind": "notation", "gloss": "page 22, a crucial ranking of Section 3.2"},
     {"kind": "notation", "gloss": "page 22, a crucial ranking of Section 3.2"}),
    # Section 3.2.2: the tableaux run into the prose; the prose is cut out whole and the tableaux
    # are rows of their own below.
    ("§3.2.2", "The tableau in (70) shows", None, {}),
    ("§3.2.2", "(70) Complex onsets", {}, None),
    ("§3.2.2", "(72) Complex codas", {}, None),
    ("§3.2.2", "The tableau in (73) shows", None, {}),
    ("§3.2.2", "The tableau in (74) shows", None, {}),
    ("§3.2.2", "The tableau in (75) shows", None, {}),
    ("§3.3.1", "(79) CULMINATIVITY", {}, {"where": "(79)", "gloss": "page 27, the definition of a constraint"}),
    ("§3.3.1", "The constraint in (81) ensures", {"where": "(80)", "gloss": "page 27, the definition of a constraint"},
     {"gloss": "page 27"}),
    ("§3.3.1", "(83) L-ANCHOR", {}, {"where": "(83)", "gloss": "page 28, the definition of a constraint"}),
    ("(83)", "Before presenting the tableaux", {}, {"where": "§3.3.1", "gloss": "page 28"}),
    ("§3.3.1", "• FVWT, L-ANCH(FV)", {"kind": "notation", "gloss": "page 28, a crucial ranking of Section 3.3"},
     {"kind": "notation", "gloss": "page 28, a crucial ranking of Section 3.3"}),
    ("§3.3.2", "Tableau (85) demonstrates", None, {}),
    ("§3.3.2", "The tableau in (85) also", None, None),
    ("§3.3.2", "The tableau in (86) demonstrates", None, {}),
    ("§3.3.2", "The tableau in (87) demonstrates", None, {}),
    ("§3.4.1", "• *HDNUC/C, ALIGN-R", {"kind": "notation", "gloss": "page 31, a crucial ranking of Section 3.4"},
     {"kind": "notation", "gloss": "page 31, a crucial ranking of Section 3.4"}),
    ("appendix", "Stressable Unstressable", {}, None),
    ("appendix", "Figure B2.", {}, {}),
    ("appendix", "Figure B3.", {}, {}),
    ("appendix", "Figure B4.", {}, {"gloss": "page 42"}),
    ("appendix", "Figure B5.", {}, {"gloss": "page 42"}),
    ("appendix", "Figure B6.", {"gloss": "page 42"}, {"gloss": "page 43"}),
    ("appendix", "Figure B7.", {}, {"gloss": "page 43"}),
    ("appendix", "Figure B8.", {}, {"gloss": "page 44"}),
    ("appendix", "The following graphics show", {}, {"gloss": "page 44"}),
    ("appendix", "Figure B10. Waveform and spectrogram showing the presence of schwa in change-of-state reduplication for the word cwúw",
     {}, {"gloss": "page 45"}),
    ("appendix", "Figure B11.", {}, {"gloss": "page 45"}),
    ("appendix", "Figure B10. Waveform and spectrogram showing the presence of schwa in change-of-state reduplication for the word ʔúsəs",
     {}, {"gloss": "page 46, numbered B10 a second time"}),
)

REMOVE = tuple(("§1.1", form) for form in ("ə/", "/ị", "ə̣/1", "ị", "ə", "ə̣o", "ə̣")) + tuple(
    ("§2.2", form) for form in ("__ʔ", "y̓", "__kʷ", "k̓ʷ", "w̓", "xʷ", "q̓", "x̣", "ʕ", "ʕ̓", "__qʷ", "q̓ʷ", "x̣ʷ",
                                "ʕʷ", "ʕ̓ʷ")) + tuple(
    ("appendix", form) for form in ("/-íyxs/", "/-nwéɬn/", "/-nwén̓/", "/-ʔúy/")) + tuple(
    ("§2.3", form) for form in ("/k̓ʷinex/", "k̓ʷí.nex", "/k̓ʷinex-esq̓t/", "k̓ʷi.ne.xésq̓t")) + (
    ("§2.3", "Category Morpheme Type Accented? Vowels in UR? Stressable?"),
    ("(38) line 1", "and (39)."),
    ("(31) line 1", "older brother’"),
    ("(84) line 1", "and (84))."),
    ("§3.2.3", "resonant in non-initial position..."),
    ("§3.4.1", "two moras."),
    ("(70)", "(70)"), ("(78)", "(78)"), ("(83)", "(83)"), ("(89)", "(89)"), ("(90)", "(90)"),
    ("§3.2.2", "(69) Onset clusters..."), ("§3.2.2", "[təq.tés]..."), ("§3.2.2", "b. ☞ [təqµ..."),
    ("§3.2.2", "[kɬə́p] ‘become..."), ("§3.2.2", "[səkp.stés]..."), ("§3.2.2", "c. ?? [skəpµsµ..."),
    ("§3.2.2", "(73) Both..."), ("§3.2.2", "[máʕ.xe.tn]..."), ("§3.2.2", "(74) Syllabic..."),
    ("§3.2.2", "Glide vocalization:..."), ("§3.2.2", "(76) Syllabic..."),
    ("§3.2.3", "(78) Complex..."), ("§3.2.3", "[ʔíkʷ.ləxʷ]..."),
    ("§3.3.2", "(84) Rightmost..."), ("§3.3.2", "[wi.kn.wen̓..."), ("§3.3.2", "c. ☞ [wiµ..."),
    ("§3.3.2", "f. [wiµ..."), ("§3.3.2", "g. [wiµ..."), ("§3.3.2", "h. [wiµ..."),
    ("§3.3.2", "Nasal vocalization:..."), ("§3.3.2", "(85) Leftmost..."), ("§3.3.2", "[kəɬ.pék.stms]..."),
    ("§3.3.2", "(86) Leftmost..."), ("§3.3.2", "[ʔes.kəɬ.pek..."), ("§3.3.2", "Unstressable morphology:..."),
    ("§3.3.2", "(87) Leftmost..."), ("§3.3.2", "[cu.xi..."),
    ("§3.4.2", "(89) Schwa..."), ("§3.4.2", "(90) Schwa..."), ("§3.4.2", "[sʕʷə.yə́ps]..."),
    ("§3.5", "{Unviolated..."),
) + tuple(
    # The pieces of the tableaux and the constraint names the engine took for cited forms.
    (where, form) for where, who, kind, form, gloss in DRAFT
    if kind == "cited form" and re.match(r"^§3\.[2-5]", where))

REMOVE_WHERE = r"^\((71|75|77|84|88)[a-z]?\)( line \d+)?$"

WHO = {}
KIND_RULES = ()
WHO_RULES = ()

SET = {
    ("§2.2.3", "[-ʔnxʷ]6"): {"form": "-ʔnxʷ", "who": AUTHORS, "gloss": "the consonant cluster [-ʔnxʷ], a hyphen standing for the "
                                                     "rest of its syllable, carries footnote 6, written here without its digit"},
    ("(31) line 1", "‘your (guys’"): {"form": "‘your (guys’) older brother’", "who": "Thompson & Thompson 1992:43"},
    ("(31) line 2", "older.brother-2PL.POSS"): {"kind": "gloss", "who": N},
    ("(31) line 2", "(T&T 1992:43)"): {"kind": "citation"},
    ("(101) line 1", "CMA"): {"kind": "citation", "who": AUTHORS, "gloss": "the source of the example"},
    ("§2.3", draft_form("§2.3", "When multiple suffixes")):
        {"form": prose("When multiple suffixes of the same", "(38) and (39).")},
    ("§2.2.6", draft_form("§2.2.6", "2.2.6 Coda")): {"form": draft_form("§2.2.6", "2.2.6 Coda") + " tolerated"},
    ("§2.3", "strong root preferred over ambivalent suffix:"): {"where": "(33) heading"},
    ("§2.3", "ambivalent suffix preferred over weak suffix:"): {"where": "(34) heading"},
    ("§2.3", "weak suffix preferred over weak root:"): {"where": "(35) heading"},
    ("§3.3.2", draft_form("§3.3.2", "The tableau in (84) demonstrates")):
        {"form": prose("The tableau in (84) demonstrates", "(84) and (84)).")},
    ("§3.2.1", draft_form("§3.2.1", "(60) DEP")): {"where": "(60)", "gloss": "page 20, the definition of a constraint"},
    ("§3.2.1", draft_form("§3.2.1", "(62) ONSBINMAX")): {"where": "(62)", "gloss": "page 21, the definition of a constraint"},
    ("§3.2.1", draft_form("§3.2.1", "(65) *σ")): {"where": "(65)", "gloss": "page 21, the definition of a constraint"},
    ("§3.2.1", draft_form("§3.2.1", "b. *NUC/NASAL")): {"where": "(66b)"},
    ("§3.2.1", draft_form("§3.2.1", "c. *NUC/LIQUID")): {"where": "(66c)"},
    ("§3.2.1", "(68) General faithfulness constraints."): {"where": "(68)", "gloss": "page 22, the heading of two definitions"},
    ("§3.2.1", "a. MAX-IO: Do not delete segments."): {"where": "(68a)"},
    ("§3.3.1", draft_form("§3.3.1", "(81) HEADNUCLEUSWEIGHT")): {"where": "(81)", "gloss": "page 27, the definition of a constraint"},
    ("§3.3.1", draft_form("§3.3.1", "(82) *HEADNUCLEUS")): {"where": "(82)", "gloss": "page 27, the definition of a constraint"},
}
for _where, _who, _kind, _form, _gloss in DRAFT:
    _number = re.match(r"^\((\d+)\) line", _where)
    if _number and _kind == "translation" and _number.group(1) in SOURCES and _form != "‘your (guys’":
        SET[(_where, _form)] = {"who": source_who(SOURCES[_number.group(1)])}
    if re.match(r"^\((9[89]|10\d|11[01])\) line 1$", _where) and _kind == "note" and _form.startswith("(T&T"):
        SET[(_where, _form)] = {"kind": "citation", "gloss": "the source of the example"}
    if _kind == "note" and re.match(r"^\(\d+\) line", _where):
        pass

WHOSE = (
    "Every example is nɬeʔkepmxcín, and the who of each form, gloss and root is the language. The "
    "who of a translation is the source its example cites: the speakers who worked on the project, "
    "Bev Phillips (BP), Marty Aspinall, c̓úʔsinek (CMA), and Bernice Garcia, kʷaɬtèzetkʷuʔ (KBG), or "
    "Thompson and Thompson's grammar (1992) and dictionary (1996), written T&T, or the two papers of "
    "Hall and Phillips. The words of the figures of Appendix B carry the speaker who produced them. "
    "Bernice Garcia's introduction of herself in footnote * is hers.\n\n"
    "The analysis of Section 3 is the authors': the constraint definitions, the rankings and the "
    "sixteen tableaux. In a tableau the input and the optimal candidate, marked ☞, are the "
    "language's; the losing candidates are forms the analysis builds and carry the authors. Each "
    "candidate's gloss gives its violation marks under the constraint heading each column, read off "
    "the glyph positions of the page. The prose, the headings and the notes carry the four authors."
)

LETTERS = (
    "The forms are in the orthography of Thompson and Thompson (1992; 1996), with the underlying "
    "representation between slashes and the surface form between square brackets, a period between "
    "syllables. An acute marks accent in the input and primary stress in the output; a subscript "
    "number ties an underlying segment to its surface correspondent, as n1 and e1; a subscript N "
    "marks a nucleus, as nN; a tie bar under two letters marks a diphthong, as í͜yN; µ marks a mora in "
    "the tableaux; a dot below marks a retracted vowel or consonant."
)

PAGE_NOTES = (
    "The subscripts, N and the correspondence numbers, come through the text layer as full-size "
    "letters and digits and are kept that way. The violation marks of tableaux (69), (72) and (73) "
    "are partly in the Symbol font, U+F02A and U+F021 in the text layer, read as * and ! at 200 dpi; "
    "the column each mark stands in is read off the glyph positions. The bold of the stressed "
    "syllables in the tableau candidates does not come through the text layer. The layer doubles the "
    "acute of tí͜y, sets every printed hyphen of the phonological notation as a soft hyphen, and puts "
    "spaces inside words after a vowel with two marks (petə̣́leʔ) and before a mora (kə́µɬµpµ); all "
    "are repaired. The italic face of footnote 25 and page 33 prints no dot below the stressed "
    "schwa of petə̣́leʔ, stə̣́nwn, sxʷə̣́seʔ and sxʷsə̣́l̓ec, where the text layer has it, and the dot "
    "is kept as typed. The page prints its broken cross-references to candidates, (69)(69), and "
    "numbers two figures B10; both are kept."
)

ADD = tuple(
    [(None, ("title", AUTHORS, "title", TITLE, "the paper's title, which carries footnote *")),
     (None, ("title", AUTHORS, "name", "Brent Hall", "an author, University of British Columbia")),
     (None, ("title", AUTHORS, "name", "Noah Luntzlara", "an author, University of British Columbia")),
     (None, ("title", AUTHORS, "name", "Gloria Mellesmoen", "an author, University of British Columbia")),
     (None, ("title", AUTHORS, "name", "Danica Reid", "an author, Simon Fraser University")),
     (None, ("footnote *", AUTHORS, "name", "Bev Phillips", "a speaker of nɬeʔkepmxcín, BP")),
     (None, ("footnote *", AUTHORS, "name", "Marty Aspinall", "c̓úʔsinek, a speaker of nɬeʔkepmxcín, CMA")),
     (None, ("footnote *", AUTHORS, "name", "Bernice Garcia",
             "kʷaɬtèzetkʷuʔ, a Kamloops Indian Residential School speaker of nɬeʔkepmxcín who is relearning her language, KBG")),
     (None, ("footnote *", AUTHORS, "name", "Lisa Matthewson", "a member of nɬab, thanked in footnote *")),
     ] +
    display(("footnote *", "* We’d like to thank..."), [
        ("footnote *", "Bernice Garcia", "transcription", INTRODUCTION,
         "page 1, footnote *, how Bernice Garcia introduces herself in nɬeʔkepmxcín"),
        ("footnote *", "Bernice Garcia", "translation", TRANSLATION, "page 1, footnote *, the English of her introduction"),
    ]) +
    display(("§1.1", "This paper provides an analysis..."), [
        ("§1.1", N, "cited form", "/i, u, e, ə/", "page 2, the primary vowels, after Thompson and Thompson 1992"),
        ("§1.1", N, "cited form", "/ị, o, a, ə̣/", "page 2, their retracted counterparts, after Thompson and "
                                                  "Thompson 1992, carries footnote 1, written here without its digit"),
        ("Table 1", AUTHORS, "note", line("Table 1. Thompson"), "page 2, the caption"),
    ] + [(where, N, "cited form", vowel, "page 2, Table 1, " + place) for where, vowel, place in VOWELS]) +
    display(("footnote 4", "4 The environments that condition..."), [
        ("footnote 4 line %d" % number, AUTHORS, "note", text,
         "page 5, footnote 4, a schwa colour and after the slash the consonants it comes before")
        for number, text in enumerate(COLOURING, 1)]) +
    display(("§2.2.2", "pyépst"), [
        ("§2.2.2", AUTHORS, "cited form", form, FORMS[form][2] + ", page 7") for form in ("wm-", "sl-", "py-")]) +
    display(("§2.2.3", "Figure 3. Waveform..."), [
        ("§2.2.3", "Bev Phillips", "cited form", "kénm", "page 9, ‘What happened? / why?’, Figure 3, produced by BP")]) +
    display(("§2.2.3", "máʕ.xetn"), [
        ("§2.2.3", AUTHORS, "cited form", form, FORMS[form][2] + ", page 8") for form in ("-tn", "-nm")]) +
    display(("footnote 9", "9 Consider the example below..."), [
        ("footnote 9 (i) line 1", N, "phonemic", "/k̓ʷinex/", "page 14, footnote 9, a weak root with full vowels"),
        ("footnote 9 (i) line 1", N, "phonemic", "[k̓ʷí.nex]", "page 14, footnote 9, stress on the initial vowel"),
        ("footnote 9 (i) line 1", "Thompson & Thompson 1996:133", "translation", "‘how many/much?’", "page 14, footnote 9"),
        ("footnote 9 (i) line 1", AUTHORS, "citation", "(T&T 1996:133)", "the source of the example"),
        ("footnote 9 (ii) line 1", N, "phonemic", "/k̓ʷinex-esq̓t/", "page 14, footnote 9"),
        ("footnote 9 (ii) line 1", N, "phonemic", "[k̓ʷi.ne.xésq̓t]", "page 14, footnote 9, stress on the ambivalent suffix"),
        ("footnote 9 (ii) line 1", "Thompson & Thompson 1996:133", "translation", "‘how many days?’", "page 14, footnote 9"),
        ("footnote 9 (ii) line 1", AUTHORS, "citation", "(T&T 1996:133)", "the source of the example"),
    ]) +
    display(("§2.3", "Table 2. Accentedness..."), table_2()) +
    display(("(100) line 1", "‘blackcap berry’"), [
        ("(100) line 1", AUTHORS, "citation", "BP", "the source of the example"),
    ]) +
    display(("§3.2.2", "The tableau in (69) shows..."), tableau(69)) +
    display(("§3.2.2", "The tableau in (70) shows..."), tableau(70)) +
    display(("§3.2.2", "Tableau (71) shows..."), tableau(71)) +
    display(("§3.2.2", "The tableau in (72) shows..."), tableau(72)) +
    display(("§3.2.2", "The tableau in (73) shows..."), tableau(73)) +
    display(("§3.2.2", "The tableau in (74) shows..."), tableau(74)) +
    display(("§3.2.2", "The tableau in (75) shows..."), tableau(75)) +
    display(("§3.2.2", "The tableau in (76) shows..."), tableau(76)) +
    display(("§3.2.3", "The tableau in (77) shows..."), tableau(77) + [
        ("§3.2.3", AUTHORS, "note", prose("The tableau in (78) shows", "A syllabic liquid is not"), "page 26"),
    ] + tableau(78)) +
    display(("§3.3.2", "The tableau in (84) demonstrates..."), tableau(84)) +
    display(("§3.3.2", "Tableau (85) demonstrates..."), tableau(85) + [
        ("§3.3.2", AUTHORS, "note", prose("The tableau in (85) also", "(31) /qéckm1p/"), "page 29"),
    ]) +
    display(("§3.3.2", "The tableau in (86) demonstrates..."), tableau(86)) +
    display(("§3.3.2", "The tableau in (87) demonstrates..."), tableau(87)) +
    display(("§3.4.1", "In addition to the constraints..."), [
        ("(88)", AUTHORS, "note", prose("(88) HEADSYLLABLEWEIGHT", "two moras."), "page 31, the definition of a constraint"),
    ]) +
    display(("§3.4.2", "The tableau in (89) shows..."), tableau(89)) +
    display(("§3.4.2", "The tableau in (90) demonstrates..."), tableau(90)) +
    display(("§3.5", "The ranking required..."), [
        ("§3.5", AUTHORS, "notation", prose("{Unviolated constraints}", "*NUC/NAS†"), "page 33, the total ranking"),
        ("§3.5", AUTHORS, "note", "(† indicates no crucial rankings below)", "page 33, under the total ranking"),
        ("§3.5", AUTHORS, "note", "Unviolated constraints", "page 33, the heading of the list below"),
    ] + [("§3.5", AUTHORS, "notation", PAGE[number], "page 33, an unviolated constraint")
         for number in range(at("Unviolated constraints") + 1, at("4 Discussion"))]) +
    display(("appendix", "Table A1: sample..."), table_a1())
)
