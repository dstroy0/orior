# Context for Mellesmoen, Reduce, Reuse, Reduplicate: "Wrong Side" Reduplication in Twana. The
# word lists (1) to (7) are read again whole off the page, one form, translation and source to an
# item; the tableaux (9), (13), (17), (18), (20), (22), (24) and (25) are rebuilt one candidate a
# row with its violation marks beside it; the definitions and the paragraphs the engine read as
# tiers are cut out of the draft rows and joined.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import PRIVATE  # noqa: E402
from workdir import WORK  # noqa: E402
import residue  # noqa: E402

STEM = "MellesmoenICSNL60_Twana"
AUTHORS = "Gloria Mellesmoen"
TW = "Twana"
TI = "Tillamook"
# The glyph the page draws as a pointing hand at the winning candidate of a tableau.
HAND = ""

TITLE = "Reduce, Reuse, Reduplicate: “Wrong Side” Reduplication in Twana"
BYLINE = "Gloria Mellesmoen, University of British Columbia"

_repair = residue.paper_repair(STEM)
with open(os.path.join(PRIVATE, "pagetext", STEM + ".txt"), encoding="utf-8") as handle:
    PAGE = [" ".join(_repair(one).split()) for one in handle.read().split("\n")]

with open(os.path.join(WORK, STEM + ".draft.tsv"), encoding="utf-8") as handle:
    DRAFT = [one.rstrip("\n").split("\t") for one in handle][1:]


def at(opening, after=0):
    """The index of the page line opening on these words, the first after index after."""
    for number, text in enumerate(PAGE):
        if number >= after and text.startswith(opening):
            return number
    raise SystemExit("no page line opens with %s" % opening)


def page_of(index):
    """The page a page line index falls on."""
    for number in range(index, -1, -1):
        marker = re.match(r"^===== page (\d+) =====$", PAGE[number])
        if marker:
            return int(marker.group(1))
    return 0


def draft_form(where, opening):
    """The form of the draft row at where opening on these words."""
    for one in DRAFT:
        if one[0] == where and one[3].startswith(opening):
            return one[3]
    raise SystemExit("no draft row %s opens with %s" % (where, opening))


def cut(text, first, last=None):
    """The words of text from first up to last, or to its end."""
    start = text.index(first)
    end = text.index(last, start) if last else len(text)
    return text[start:end].strip()


def display(anchor, rows):
    """ADD entries for rows (where, who, kind, form, gloss), each placed after the one before it,
    the first after anchor."""
    added = []
    for row in rows:
        added.append((anchor, row))
        anchor = (row[0], row[3])
    return added


def joined(*parts):
    return " ".join(parts)


ITEM = re.compile(r"^(?:\((\d+)\) )?(?:([a-y])\. )?(\S+) (‘[^’]*’) \(([^()]*)\)$")


def word_list(number):
    """The items of word list (number), each a form, its translation and its source, read off
    the page lines from the one opening (number) until a line that is not an item."""
    rows = []
    index = at("(%d) " % number)
    count = 0
    while index < len(PAGE):
        text = PAGE[index]
        if not text or text.startswith("=====") or text.isdigit():
            index += 1
            continue
        item = ITEM.match(text)
        if not item or (item.group(1) and int(item.group(1)) != number):
            break
        count += 1
        letter = item.group(2)
        where = "(%d%s) line 1" % (number, letter) if letter else "(%d) line %d" % (number, count)
        source = item.group(5)
        language = TI if source.endswith(TI) else TW
        who = re.sub(r", (Twana|Tillamook)$", "", source)
        page = page_of(index)
        rows += [(where, language, "segmentation", item.group(3), "page %d, a %s example" % (page, language)),
                 (where, who, "translation", item.group(4), "page %d" % page),
                 (where, AUTHORS, "citation", "(%s)" % source, "the source at the right of the translation")]
        index += 1
    return rows


def header(number):
    """The input and constraint columns of tableau (number), which the page sets over several
    lines, a constraint name broken before its bracket or after a hyphen or comma. The page
    stacks *BRANCHO over [SV] with no hyphen and writes the name nowhere else that way, and the
    two stay apart."""
    index = at("(%d) " % number) + 1
    text = ""
    while not PAGE[index].startswith("a. "):
        one = PAGE[index]
        if one:
            closed = text.endswith(("-", ",")) or (one.startswith("[") and one != "[SV]")
            text += one if closed else " " + one
        index += 1
    return text.strip()


def table(opening, count):
    """The caption line of a table and the count lines after it."""
    index = at(opening)
    return [PAGE[index + step] for step in range(count + 1)]


def table_4():
    """Table 4's caption, its column heads and its three rows, each row's cells wrapped over
    lines the page breaks inside a cell."""
    index = at("Table 4: Realisations")
    caption, heads = PAGE[index], PAGE[index + 1]
    rows = []
    index += 2
    while PAGE[index]:
        if re.match(r"^Cə?C?- ", PAGE[index]):
            rows.append(PAGE[index])
        else:
            rows[-1] += " " + PAGE[index]
        index += 1
    return caption, heads, rows


PAGE_OF_TABLEAU = {"9": 7, "13": 9, "17": 11, "18": 11, "20": 12, "22": 12, "24": 13, "25": 14}

R4 = draft_form("front", "Reduce, Reuse")
R39 = draft_form("§2", "obstruent), unless")
R78 = draft_form("§2", "is called “wrong side”")
R180 = draft_form("§2", "segments separating the reduplicated")
R183 = draft_form("§2", "2 breaks the voiceless")
R189 = draft_form("§2", "Table 1: Realisations")
R205 = draft_form("(8c) line 1", "*FLOAT:")
R215 = draft_form("§3.2", "Theory, which is an alternate")
R243 = draft_form("§4.1", "relative to the other constraints")
R256 = draft_form("(16a) line 1", "ALIGN-R-O[-GRAVE]:")
R258 = draft_form("§4.2.1", "2 Labial, dorsal")
R268 = draft_form("§4.2.1", "with O[-grave]O[-grave]")
R306 = draft_form("§4.2.2", "(21) *BRANCHINGONSET")
R340 = draft_form("§4.2.3", "reduplication as repairs in other")
R363 = draft_form("(26) line 1", "ONSET, DEP")
R365 = draft_form("§4.2.3", "ALIGN-R-ONSET[-GRAVE]")

FORMS = {
    "əC-": ("notation", AUTHORS, "the prefix the first stratum yields, a schwa and a copy of the second root consonant"),
    "əC": ("notation", AUTHORS, "the onsetless syllable that fills the prosodic affix σN+μ"),
    "ə": ("notation", AUTHORS, "schwa"),
    "Ceʔ": ("notation", AUTHORS, "the surface C1C2 reduplicant before a root glide /y/"),
    "Coʔ": ("notation", AUTHORS, "the surface C1C2 reduplicant before a root glide /w/"),
    "/Cəy/": ("notation", AUTHORS, "the reduplicant that surfaces as Ceʔ"),
    "/Cəw/": ("notation", AUTHORS, "the reduplicant that surfaces as Coʔ"),
    "CəC-": ("notation", AUTHORS, "the C1C2 realisation of the reduplicant, a copied consonant, schwa and a copied consonant"),
    "Cə-": ("notation", AUTHORS, "the “wrong side” realisation with schwa, the metathesis repair"),
    "CəC-/C-": ("notation", AUTHORS, "a cell of Table 1 where both realisations are attested"),
    "/p̓/": ("notation", AUTHORS, "the glottalized labial stop, one of the two labials of Twana"),
    "p_p̓": ("notation", AUTHORS, "a root with /p/ as C1 and /p̓/ as C2, unattested"),
    "p̓_p": ("notation", AUTHORS, "a root with /p̓/ as C1 and /p/ as C2, unattested"),
    "σN+μ": ("notation", AUTHORS, "the underlying form of plural reduplication, a syllable with a nucleus and a mora"),
    "STRUC-σ": ("notation", AUTHORS, "the constraint *STRUC-σ, defined in (19)"),
    "əSCVS": ("notation", AUTHORS, "the input to the second stratum of a CS root"),
    "əOSVO": ("notation", AUTHORS, "the input to the second stratum of an SO root"),
    "[ə]-deletion": ("notation", AUTHORS, "deletion of schwa"),
    "C1]σ[C2": ("notation", AUTHORS, "a heterosyllabic sequence, in the definition of SYLLCON (23)"),
    "qəbə́qsəd": ("cited form", TW, "page 4, ‘nose (C1C2)’, (7a), with its three underlined segments"),
    "s-x̣p̓x̣ə́p̓ab": ("cited form", TW, "page 5, ‘cockles’, Drachman 1969: 61"),
    "s-x̣p̓ab": ("cited form", TW, "page 5, ‘cockle’, the base of s-x̣p̓x̣ə́p̓ab, Drachman 1969: 61"),
    "ʔəs-q̓x̣əq̓": ("cited form", TW, "page 8, ‘landed (C1C2)’, given in (6l)"),
    "ʔas-x̣əq̓": ("cited form", TW, "page 8, ‘landed’, the unreduplicated word"),
    "əƛ̓šóƛ̓": ("cited form", TW, "page 9, starred on the page, the output of the first stratum for šoƛ̓ ‘grind’"),
    "šáw̓": ("cited form", TW, "page 12, ‘bone’"),
    "/əw̓šáw̓/": ("cited form", TW, "page 12, the output of the first stratum for šáw̓ ‘bone’"),
    "/šəw̓šáw̓/": ("cited form", TW, "page 13, the output of the second stratum for šáw̓ ‘bone’"),
    "šoʔšáw̓": ("cited form", TW, "page 13, the surface form after glide vocalisation, ‘bone (C1C2)’"),
    "Bermúdez-Otero": ("name", AUTHORS, "Ricardo Bermúdez-Otero, cited for GNLA and Stratal OT"),
    "Gonzàlez": ("name", AUTHORS, "the editor of the proceedings in McCarthy & Prince 1994"),
    "Nxaʔamxcín": ("language", AUTHORS, "Moses-Columbia Salish, whose stress Czaykowska-Higgins 1993b describes"),
    "Nxaʔamxcin": ("language", AUTHORS, "Moses-Columbia Salish, in the title of Czaykowska-Higgins 1993b"),
    "Nɬeʔkepmxcín": ("language", AUTHORS, "Thompson River Salish, whose reduplication Jimmie 1994 analyses"),
    "Nłek̉epmx": ("language", AUTHORS, "Thompson River Salish, in the title of Jimmie 1994"),
    "Hutyéyu": ("language", AUTHORS, "Tillamook, in the title of Egesdal & Thompson 1998"),
    "St’át’imcets": ("language", AUTHORS, "Lillooet, in the title of Matthewson 1994"),
}

# The halves of kt̓ə́keʔəs, which the text layer split where the page underlines its segments.
DROP = ("kt̓ə́", "keʔəs")

INITIALS = {}

ONLY = cut(R189, "Only voiceless", " The choice between CəC-, C-, and Cə-")

SPLIT = (
    ("front", "Gloria Mellesmoen University", None, {}),
    ("front", "Abstract:", None, {"gloss": "page 1, the abstract"}),
    # Table 1, the paragraph under it and the page-6 sentence, run into one row; the start of the
    # paragraph joins its end, which the engine took for footnote 2 on the digit of Table 2.
    ("§2", "Only voiceless obstruent-voiceless", None, {}),
    ("§2", "The choice between CəC-, C-, and Cə-", None, {"gloss": "page 6"}),
    ("footnote 2", "Table 2: Realisations",
     {"where": "§2", "form": ONLY + " " + cut(R183, "2 breaks", " Table 2: Realisations"), "gloss": "page 5"},
     {"where": "§2"}),
    ("§2", "The patterns concerning place", None, {"gloss": "page 5"}),
    ("§3.2", "Reduplication is the process of fission",
     {"gloss": "page 7, the caption of Figure 1, a drawing of the two mappings the text layer does not carry"}, {}),
    ("(8b) line 1", "(Kager 1999: 68)", {"who": "Kager 1999: 68", "gloss": "page 7, a constraint definition"},
     {"who": AUTHORS, "kind": "citation", "form": "(Kager 1999: 68)", "gloss": "the source of the definition"}),
    ("(9) line 1", "/σ + C1V2C3/", {"gloss": "page 7, the caption of the tableau"},
     {"where": "(9) line 2", "gloss": "page 7, the input and the constraint columns of the tableau"}),
    ("§3.2", "REDk is a morpheme lexically", None, {"where": "(10) line 1"}),
    ("(10) line 1", "(McCarthy & Prince 1994, as cited",
     {"who": "McCarthy & Prince 1994",
      "gloss": "page 8, the definition of a reduplicative morpheme in Base-Reduplicant Correspondence Theory, quoted"},
     {"who": AUTHORS, "kind": "citation", "gloss": "the source of the definition"}),
    ("(10) line 1", "A crucial assumption", {}, {"where": "§3.2", "kind": "note", "gloss": "page 8"}),
    ("(12b) line 1", "The tableau in (13) shows", {}, {"where": "§4.1", "gloss": "page 9"}),
    ("(12b) line 1", "(Blake 2000: 244).", {"who": "Blake 2000: 244", "gloss": "page 8, a constraint definition"},
     {"who": AUTHORS, "kind": "citation", "form": "(Blake 2000: 244)", "gloss": "the source of the definition"}),
    ("§4.2", "Table 4: Realisations", {}, {}),
    ("§4.2", "Each repair corresponds", None, {"gloss": "page 10"}),
    ("(15a) line 1", "(McCarthy & Prince 1995: 16)",
     {"who": "McCarthy & Prince 1995: 16", "gloss": "page 10, a constraint definition"},
     {"who": AUTHORS, "kind": "citation", "gloss": "the source of the definition"}),
    ("(15b) line 1", "For repairs to occur", {}, {"where": "§4.2", "gloss": "page 10"}),
    ("(15b) line 1", "(McCarthy & Prince 1995: 123)",
     {"who": "McCarthy & Prince 1995: 123", "gloss": "page 10, a constraint definition"},
     {"who": AUTHORS, "kind": "citation", "gloss": "the source of the definition"}),
    ("footnote 2", "b. ALIGN-L-O[+GRAVE,-CONT]:", {},
     {"where": "(16b) line 1", "form": cut(R258, "ALIGN-L-O[+GRAVE,-CONT]: Any"),
      "gloss": "page 11, a constraint definition"}),
    ("§4.2.3", "(23) SYLLCON", {}, {"where": "(23) line 1"}),
    ("(23) line 1", "SYLLCON: In the", None, {}),
    ("(23) line 1", "(Urbanczyk 1996: 177) Ranking",
     {"who": "Urbanczyk 1996: 177", "gloss": "page 13, a constraint definition"},
     {"who": AUTHORS, "kind": "citation", "gloss": "the source of the definition"}),
    ("(23) line 1", "Ranking ONSET", {}, {"where": "§4.2.3", "kind": "note", "gloss": "page 13"}),
    ("§4.2.3", "One question for future work", None, {}),
)

REMOVE = (
    ("§3.2", "dominates s (Kirchner 2010: 232)."),
    ("§4.2.1", "right edge of the onset."),
    ("(21)", "(21)"),
)
# The word lists, read again whole off the page; the lines of each tableau's header, given again
# whole; and the lines of the paragraphs the engine read as tiers, joined into their first line.
REMOVE_WHERE = (r"^\(([1-7])[a-y]?\) line|^\((13|17|18|20|22|24|25)\) line ([2-9]|1[0-6])$"
                r"|^\(9c\) line [2-4]$|^\(13f\) line 2$|^\(17c\) line 2$|^\(18c\) line [3-6]$"
                r"|^\(19\) line [24-6]$|^\(24d\) line 2$")

WHO_RULES = ()

KIND_RULES = (
    (r"^\((13|17|18|20|22|24|25)\) line 1$", ".", ".", "note", AUTHORS, "the caption of the tableau"),
    (r"^\((8a|12a)\) line 1$", ".", ".", "note", AUTHORS, "a constraint definition"),
)


def lines_of(where):
    return [one[3] for one in DRAFT if one[0] == where]


SET = {
    ("§2", R39): {"form": joined(*lines_of("(3j) line 2"), *lines_of("(3j) line 3"), R39)},
    ("§2", R78): {"form": joined(*lines_of("(5m) line 7"), R78)},
    ("§2", R180): {"form": joined(*lines_of("(7i) line 2"), R180)},
    ("(8c) line 1", R205): {"form": R205 + " " + cut(draft_form("§3.2", "dominates s"), "dominates s", " (Kirchner"),
                            "who": "Kirchner 2010: 232", "kind": "note", "gloss": "page 7, a constraint definition"},
    ("§3.2", R215): {"form": joined(*lines_of("(9c) line 2"), *lines_of("(9c) line 3"), *lines_of("(9c) line 4"), R215)},
    ("§4.1", R243): {"form": joined(*lines_of("(13f) line 2"), R243)},
    ("§4.1", "(14) *FLOAT, DEP-C >> INT-C, DEP >> ONSET"):
        {"where": "(14) line 1", "form": "*FLOAT, DEP-C >> INT-C, DEP >> ONSET",
         "gloss": "page 9, the crucial ranking at the first stratum"},
    ("(16a) line 1", R256): {"form": R256 + " right edge of the onset.", "kind": "note", "who": AUTHORS,
                             "gloss": "page 10, a constraint definition"},
    ("§4.2.1", R268): {"form": joined(*lines_of("(17c) line 2"), R268)},
    ("(18c) line 2", lines_of("(18c) line 2")[0]):
        {"where": "§4.2.1", "who": AUTHORS, "kind": "note",
         "form": joined(*[lines_of("(18c) line %d" % one)[0] for one in range(2, 7)]).replace(
             "in the output. 3 *STRUC", "in the output. *STRUC"),
         "gloss": "page 11, carries footnote 3, written here without its digit"},
    ("(19) line 1", lines_of("(19) line 1")[0]):
        {"form": joined(lines_of("(19) line 1")[0], lines_of("(19) line 2")[0]), "who": AUTHORS, "kind": "note",
         "gloss": "page 11, a constraint definition, modified from Zoll 1996"},
    ("(19) line 3", lines_of("(19) line 3")[0]):
        {"where": "§4.2.1", "who": AUTHORS, "kind": "note",
         "form": joined(*[lines_of("(19) line %d" % one)[0] for one in range(3, 7)]), "gloss": "page 11"},
    ("§4.2.2", R306): {"where": "(21) line 1", "form": R306[len("(21) "):], "gloss": "page 12, a constraint definition"},
    ("§4.2.3", R340): {"form": joined(*lines_of("(24d) line 2"), R340)},
    ("(26) line 1", R363): {"form": R363 + " " + cut(R365, "ALIGN-R-ONSET", " One question"),
                            "gloss": "page 14, the ranking at the second stratum"},
    ("§5", "Figure 2: Deriving Twana Reduplication at Two Strata"):
        {"gloss": "page 15, the caption of Figure 2, a drawing the text layer does not carry"},
}

ADD_ROWS = []
for _where, _who, _kind, _form, _gloss in DRAFT:
    # A tableau candidate, the form and the violation marks after it; the winner opens on the hand.
    tableau = re.match(r"^\((9|13|17|18|20|22|24|25)[a-f]\) line 1$", _where)
    if tableau and _kind == "transcription":
        tokens = _form.replace(HAND, " ").split()
        marks = []
        while tokens and re.fullmatch(r"[*!]+", tokens[-1]):
            marks.insert(0, tokens.pop())
        winner = ", the winning candidate, marked on the page with a pointing hand" if HAND in _form else ""
        change = {"form": " ".join(tokens), "gloss": _gloss.split(",")[0] + winner}
        if tableau.group(1) == "9":
            change.update({"who": AUTHORS, "kind": "notation"})
            change["gloss"] += ", a schematic candidate, C a consonant and V a vowel"
        SET[(_where, _form)] = change
        if marks:
            ADD_ROWS.append(((_where, _form), (_where, AUTHORS, "note", " ".join(marks),
                             "%s, the violation marks of the candidate, * a violation and ! a fatal one, "
                             "in the order of the columns they fall under" % _gloss.split(",")[0])))

TABLE_1 = table("Table 1: Realisations", 3)
TABLE_2 = table("Table 2: Realisations", 4)
TABLE_3 = table("Table 3: Realisations", 3)
TABLE_4 = table_4()


def table_rows(name, caption, heads, rows, page, heads_gloss):
    return ([(name, AUTHORS, "note", caption, "page %d, the caption" % page),
             (name, AUTHORS, "note", heads, "page %d, %s" % (page, heads_gloss))] +
            [("%s line %d" % (name, number), AUTHORS, "note", row, "page %d, a row of the table" % page)
             for number, row in enumerate(rows, 1)])


ADD = tuple(
    [(None, ("title", AUTHORS, "title", TITLE, "the paper's title, carrying the star of footnote *")),
     (None, ("title", AUTHORS, "name", "Gloria Mellesmoen", "the author, University of British Columbia")),
     (None, ("§1", AUTHORS, "language", TW, "the Central Salish language whose plural reduplication the paper analyses")),
     (None, ("§1", AUTHORS, "language", TI, "the Salish language whose “wrong side” reduplication (1b) and (2) cite")),
     (("§2", R180), ("§2", TW, "cited form", "kt̓ə́keʔəs",
                     "page 4, ‘basket (C1C2)’, (6a), the text layer splitting it where the page underlines two segments")),
     ] +
    # Each word list goes after the prose the page sets before it.
    display(("§1", "Optimality Theory (OT) has proven..."), word_list(1)) +
    display(("§1", "The Twana pattern was analysed..."), word_list(2)) +
    display(("§2", "Twana is a Central Salish..."), word_list(3)) +
    display(("§2", "obstruent), unless..."), word_list(4)) +
    display(("§2", "obstruent, as in (3) and (4)..."), word_list(5)) +
    display(("§2", "is called “wrong side”..."), word_list(6) + word_list(7)) +
    ADD_ROWS +
    display(("§2", "Table 1 shows that the choice..."),
            table_rows("Table 1", TABLE_1[0], TABLE_1[1], TABLE_1[2:], 5,
                       "the column heads, C1 down the side and C2 across the top")) +
    display(("§2", "Only voiceless obstruent-voiceless..."),
            table_rows("Table 2", TABLE_2[0], TABLE_2[1], TABLE_2[2:], 5,
                       "the column heads, C1 down the side and C2 across the top") +
            table_rows("Table 3", TABLE_3[0], TABLE_3[1], TABLE_3[2:], 5,
                       "the column heads, C1 down the side and C2 across the top")) +
    display(("§4.2", "The input to the second stratum..."),
            table_rows("Table 4", TABLE_4[0], TABLE_4[1], TABLE_4[2], 9, "the column heads")) +
    [add for number in ("13", "17", "18", "20", "22", "24", "25") for add in
     display("(%s) line 1" % number, [("(%s) line 2" % number, AUTHORS, "note", header(int(number)),
                                       "page %d, the input and the constraint columns of the tableau"
                                       % PAGE_OF_TABLEAU[number])])])

WHOSE = (
    "The paper's data are cited from published sources. Every Twana form comes from Drachman 1969 "
    "and each Tillamook form from Egesdal and Thompson 1998, and each translation's who is the "
    "source its tag names, (Drachman 1969: 41). The who of each form is its language.\n\n"
    "The tableaux (9), (13), (17), (18), (20), (22), (24) and (25) are the author's: each candidate "
    "is a row whose who is Twana, except the schematic candidates of (9), which are the author's "
    "notation, and its violation marks are a note of the author's beside it. The definition (10) is "
    "McCarthy and Prince's as cited in Urbanczyk 1996; DEP (8b) is Kager's, *FLOAT (8c) Kirchner's, "
    "ONSET (12b) Blake's, MAX and LINEARITY (15) McCarthy and Prince's, and SYLLCON (23) Urbanczyk's. "
    "The other constraints, the rankings, the tables, the prose, the headings and the notes carry "
    "Gloria Mellesmoen."
)

LETTERS = (
    "The Twana and Tillamook forms are in the Americanist orthography of Drachman 1969 and Egesdal "
    "and Thompson 1998, with the glottalization of a consonant as U+0313, the dot below of a "
    "uvular as U+0323, and stress as an acute accent on the vowel. A tilde joins the reduplicant to "
    "the root, s-q~téqaw, and a hyphen sets off a prefix. The analysis writes C for a consonant, V "
    "for a vowel, S for a sonorant or voiced obstruent and O for a voiceless obstruent; σ is a "
    "syllable and μ a mora. The constraint names are small capitals on the page and ASCII capitals "
    "in the text layer."
)

PAGE_NOTES = (
    "The page marks the winning candidate of each tableau with a pointing hand, U+F046 in the text "
    "layer; the table drops it and says so in the candidate's gloss. The text layer sets a space "
    "after a glottalized letter, q̓ ʷ for q̓ʷ, and the table closes it. The page underlines the "
    "segments between a copied consonant and its source in kt̓ə́keʔəs and qəbə́qsəd; the text layer "
    "breaks kt̓ə́keʔəs at the underline and writes C1C2 in its translation as C 1C2. Figures 1 and 2 "
    "are drawings, and only their captions are in the text layer."
)
