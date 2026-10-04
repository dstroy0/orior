# Context for Lyon, Change-of-State in Nsyilxcn Roots and Beyond. The word tables (1) to (11) and
# footnote 14's table are the engine's, relabeled where a letter cell stood in the first row. The
# rows the text layer ran into prose, (1e), (2e), the rest of (6), (24), (29b) to (29d), (53), (71)
# and Table 1, are read off the page text or cut out of the draft row that holds them.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import PRIVATE  # noqa: E402
from workdir import WORK  # noqa: E402
import residue  # noqa: E402

STEM = "LyonICSNL60_CS"
AUTHORS = "John Lyon"
L = "nsyilxcn"
ST = "St’át’imcets"
DAVIS = "Davis in prep."
BKG = "Beavers & Koontz-Garboden 2020:45"
KL = "Kennedy & Levin 2008"

TITLE = "Change-of-State in Nsyilxcn Roots and Beyond"
BYLINE = "John Lyon, University of British Columbia – Okanagan"

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


def joined(first, last, after=0):
    """The page lines from the one opening on first to the one opening on last, as one text."""
    start = PAGE.index(line(first, after))
    end = PAGE.index(line(last, start))
    return " ".join(one for one in PAGE[start:end + 1] if one)


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


def table_6():
    """The rows of (6), the positive adjectives with -t, set as root → adjective ‘gloss’. The list
    runs from page 4 onto page 5, past footnote 4."""
    texts = [line("(6) √x̌ʷup"), line("√ʔilxʷ →")]
    start = PAGE.index(line("√ʔayx̌ʷ →"))
    texts += [one for one in PAGE[start:start + 9]]
    rows = []
    for number, text in enumerate(texts, 1):
        parts = re.match(r"^(?:\(6\) )?(√\S+) → (.+?) (‘[^’]*’)$", text)
        if not parts:
            raise SystemExit("(6) line %d does not read root → form ‘gloss’: %s" % (number, text))
        page = "page 4" if number <= 2 else "page 5"
        where = "(6) line %d" % number
        rows += [(where, L, "root", parts.group(1), page),
                 (where, L, "transcription", parts.group(2), page),
                 (where, AUTHORS, "translation", parts.group(3), page)]
    return rows


def typology():
    """Table 1 on pages 32 and 33: the column heads, set two words a line, and one row for each
    language, its number, its name and a √ or * under each column."""
    start = PAGE.index(line("Table 1. Towards a Typology"))
    heads = " ".join(one for one in PAGE[start + 1:start + 13] if one)
    rows = [("Table 1", AUTHORS, "note", PAGE[start], "page 32, the caption"),
            ("Table 1", AUTHORS, "note", heads, "page 32, the column heads, as the text layer runs them together")]
    for number in range(1, 8):
        text = line("%d %s" % (number, "Nsyilxcn" if number == 1 else ST if number == 2 else "predicted"))
        rows.append(("Table 1 line %d" % number, AUTHORS, "note", text,
                     "page %d, a row of the typology, √ where the language has the property and * where it lacks it"
                     % (32 if number <= 4 else 33)))
    return rows


# The two rows of (1) and (2) whose e. line carries the denotations, one for each column.
E1 = draft_form("§1", "e. λdλxλsλe")
E2 = draft_form("§1", "e. λdλxλs.hard")
# The rows of (29b) to (29d), which the text layer set among footnotes 16 and 17 across a page.
R587 = draft_form("§3", "(2023) to posit")
R590 = draft_form("§3", "already STAT-tear-INCH")
R593 = draft_form("§3", "STAT-finish-CMPD-straight")
R594 = draft_form("§3", "already STAT-white<INCH>")
# (24a)'s translation and all of (24b), run together.
R542 = draft_form("§3", "# ‘When you put")
R543 = draft_form("§3", "that when LOC-put.in")
# (53), run into the paragraph after it.
R813 = draft_form("§5", "(53) a. Measure of change")
# (71a)'s denotation and all of (71b), run into the paragraph after them.
R922 = draft_form("§6", "λxλt∃s∃e.[fixedΔ(x,e,s) ≥")
# (54b)'s continuation, run into the paragraph after it.
R819 = draft_form("§5", "the amount that x changes in the state s")

FORMS = {
    ST: ("language", AUTHORS, "Northern Interior Salish, whose bare change-of-state roots the paper compares, Davis 2024"),
    "St’at’imcets": ("language", AUTHORS, "St’át’imcets, printed here without its acute"),
    "Secwepemctsín": ("language", AUTHORS, "Northern Interior Salish, Nederveen 2023 and Kuipers 1974"),
    "Skwxwú7mesh": ("language", AUTHORS, "Squamish, Bar-el 2005"),
    "ʔayʔaǰuθəm": ("language", AUTHORS, "Comox-Sliammon, Huijsmans 2022"),
    "ʔayʔaǰúθəm": ("language", AUTHORS, "Comox-Sliammon, in the title of Davis et al. 2020"),
    "nxaʔamxčín": ("language", AUTHORS, "Moses-Columbian, Kinkade 1989"),
    "ɬk̓mxnalqs": ("name", L, "the nsyilxcn name of Delphine Derrickson-Armstrong, footnote *"),
    "c̓skʕáknaʔ": ("name", L, "the nsyilxcn name of Dave Michele, footnote *"),
    "En’owkin": ("name", AUTHORS, "the En’owkin Centre, a supporter of the work, footnote *"),
    "Piñon": ("name", AUTHORS, "Christopher Piñon, cited for the verbal positive, Piñon 2005"),
    "Schäfer": ("name", AUTHORS, "a coauthor of Alexiadou et al. 2015, in the references"),
    "Wöllstein-Leisten": ("name", AUTHORS, "an editor of Event arguments, in Kratzer 2005"),
    "Calderón-Corona": ("name", AUTHORS, "an editor of the SULA 12 proceedings, in Nederveen 2023"),
    "Tübingen": ("place", AUTHORS, "the place of publication of Kratzer 2005"),
    "aufpump-": ("cited form", "German", "page 17, ‘to get inflated’, a German target state participle stem, Kratzer 2000"),
    "mΔ": ("notation", AUTHORS, "a measure-of-change function, Kennedy & Levin 2008"),
    "δs": ("notation", AUTHORS, "the scalar dimension of a state s, Beavers & Koontz-Garboden 2020"),
    "δ": ("notation", AUTHORS, "a scalar dimension, Beavers & Koontz-Garboden 2020"),
}

# The pieces of formulas the engine took for cited forms, the table cells glued into prose, and the
# (ə)c- a footnote mark was glued onto, which §1.2 holds without it.
DROP = ("λdλxλsλe.cutΔ(x,e,s", "λxλs∃e.cutΔ(x,e,s", "stnd(cutΔ", "λxλe∃s.cutΔ(x,e,s", "max(cutΔ",
        "λdλxλs.hard(x,s", "λxλs.hard(x,s", "λxλe∃s.hardΔ(x,e,s", "min(hardΔ", "λxλe.mm(x)(init(e",
        "λxλt∃s∃e.[fixedΔ(x,e,s", "stnd(fixedΔ", "√melt⟧", "√/", "(ə)c-12", "so-called")

# The initials that open the speaker comments, and the consultant of (43b), whose comment Davis 2024
# reports.
INITIALS = {"DD": "Delphine Derickson-Armstrong", "DM": "Dave Michele", "Speaker’s": "Davis 2024:310"}

SPLIT = (
    ("front", "John Lyon University", None, {}),
    ("front", "Abstract:", None, {}),
    ("footnote *", "Contact:", {}, {"where": "front", "gloss": "page 1, the author's contact line, at the foot of the page"}),
    ("§1", "Nsyilxcn CS roots require",
     {"where": "(2e) line 1", "kind": "rule", "form": E2[3:E2.index("Nsyilxcn CS roots")].strip(),
      "gloss": "page 2, the denotations of (2), one for each column"}, {}),
    ("§1", "The basic outline of my argument",
     {"where": "Figure 1", "gloss": "page 3, the captions of Figures 1, 2 and 3, trees the text layer does not hold"}, {}),
    ("§1.1", "I assume, following Davis’", None, {}),
    ("(15) line 1", "15 Gloss abbreviations",
     {}, {"where": "footnote 15", "gloss": "page 9, footnote, set in the middle of (15)"}),
    ("§2.1", "As Beavers & Koontz-Garboden (2020) discuss",
     {"where": "Figure 4", "gloss": "page 8, the captions of Figures 4 and 5, trees the text layer does not hold"}, {}),
    ("§3", "Nsyilxcn v is not always realized",
     {"where": "Figure 6", "gloss": "page 14, the caption of Figure 6, a tree the text layer does not hold"}, {}),
    ("§3", "To be clear, PC roots like those", None, {}),
    ("§3", "In the absence of inchoative marking",
     {"where": "Figure 3", "gloss": "page 15, the caption of Figure 3, repeated"}, {}),
    ("§3", "What then is null v",
     {"where": "Figure 2", "gloss": "page 16, the caption of Figure 2, repeated"}, {}),
    ("§5", "Beavers & Koontz-Garboden’s (2020) analysis of result roots",
     {"where": "Figure 1", "gloss": "page 25, the captions of Figures 1, 2 and 3, repeated"}, {}),
    ("§5", "The truth conditions for Beavers", None, {}),
    ("§5", "In a nutshell",
     {"where": "(54b) line 2", "who": AUTHORS, "form": cut(R819, "the amount", " In a nutshell")[:-2],
      "gloss": "page 26, the rest of (54b), carries footnote 32, written here without its digit"}, {}),
    ("§6", "Resultant state intepretations", None, {}),
)

# The glued rows the ADD rows below replace, the letter cells of tables (9) to (11) and footnote 14,
# and Table 1, whose rows the footnote rule took for footnotes 1 to 4 and a heading of §7.
TABLE_CELLS = ("√ʔayx̌ʷ", "ʔayx̌ʷ-t", "√taɬ", "(təɬ)•táɬ-t", "√t̓ʕas", "t̓əs•t̓ʕas-t", "√nʕas", "(nəs)•nʕas-t",
               "√xʷəl", "xʷəl•xʷál-t", "√xaʔ", "x̌aʔ•x̌áʔ", "√x̌as", "x̌as-t", "√ham", "həm•hám-t", "√c̓aɬ", "c̓aɬ-t")
GLUED = ("ixíʔ", "ɬaʔ", "n-wt-nt-ixʷ", "iʔ", "knəxnáx", "uɬ", "n<ʔ>ʕas", "way̓", "(əc)-t̓l-ap", "q̓əy̓mín",
         "(əc)-wiʔ-s-təɬ•áɬ", "(əc)-p<ʔ>aq", "citxʷ")
LETTERS_OF = {"(11d) line %d" % number: letter for number, letter in zip(range(2, 10), "abcdefgh")}

REMOVE = tuple(
    [("§1.1", form) for form in TABLE_CELLS] +
    [("§3", form) for form in GLUED] +
    [(where, letter + ".") for where, letter in LETTERS_OF.items()] +
    [("(9) line 1", "a."), ("(10) line 1", "a."), ("(11) line 1", "a."),
     ("(2c) line 1", "•nʕas-t"),
     ("(6) line 1", "√x̌ʷup → x̌ʷup-t ‘weak’"), ("(6) line 2", "√ʔilxʷ → ʔilxʷ-t ‘hungry’"),
     ("(3a) line 1", "əc-nik’..."), ("(3b) line 1", "əc-nik̓•ək̓..."),
     ("(4a) line 1", "*əc-qʷin..."), ("(4b) line 1", "əc-qʷ<ʔ>in..."),
     ("§3", "# ‘When you put..."), ("§3", "that when LOC-put.in..."),
     ("footnote 16", "16 These are not always..."), ("§3", "(2023) to posit..."),
     ("§3", "already STAT-tear-INCH..."), ("§3", "STAT-finish-CMPD-straight..."),
     ("§2.2", "than the degree held..."),
     ("footnote 1", "1 Nsyilxcn..."), ("footnote 2", "2 St’át’imcets √..."),
     ("footnote 3", "3 predicted..."), ("footnote 4", "4 predicted..."),
     ("§6", "Table 1. Towards a Typology..."), ("§7", "7 predicted...")])

# The forms the paper cites from St'át'imcets, in the examples Davis (in prep.) gives.
ST_EXAMPLES = r"^\((43b|61|66a|67|70a|70b|71a|71b)\) line"
WHO_RULES = tuple(
    (ST_EXAMPLES, kind, ".", ST, "a St’át’imcets example") for kind in ("segmentation", "gloss", "transcription")
) + (
    (r"^\((61|66a|67|70a|70b|71a|71b)\) line", "translation", r"\(Davis, in prep", DAVIS,
     "the translation of Davis (in prep.), as the tag says"),
    (r"^\(43b\) line", "translation", ".", "Davis 2024:310", "the translation of Davis 2024:310"),
    (r"^\(64\) line", "transcription", ".", ST, "a St’át’imcets form, Davis 2024:311"),
    (r"^\(64\) line", "translation", ".", "Davis 2024:311", "the translation of Davis 2024:311"),
)

KIND_RULES = (
    # The first cell of a row of (1), (2), (8) and footnote 14's table is the root, starred where the
    # bare root is not a patient-oriented predicate.
    (r"^\((1[a-e]|2[a-e]|8[a-l]|11d)\) line", "transcription", r"^\*?√", "root", L,
     "the root of the row, starred where it is not a predicate by itself"),
    (r"^\(21a\) line", ".", ".", "note", BKG, "the truth conditions of BECOME(s,e), quoted"),
    (r"^\(21b\) line", ".", ".", "note", "Beavers 2012", "the Figure/Path Relation, quoted"),
    (r"^\(54a\) line [23]$", ".", ".", "rule", AUTHORS, "the measure-of-change function of Nsyilxcn CS roots"),
    (r"^\(54[ab]\) line", ".", ".", "note", AUTHORS, "the measure-of-change function of Nsyilxcn CS roots"),
    (r"^\(43b\) line 1$", ".", ".", "note", AUTHORS, "the language of the example"),
)

SET = {
    ("§1", E1): {"where": "(1e) line 1", "kind": "rule", "form": E1[3:],
                 "gloss": "page 2, the denotations of (1), one for each column"},
    ("(2c) line 1", "(nəs)"): {"kind": "transcription", "who": L, "form": "(nəs)•nʕas-t",
                              "gloss": "page 2, the text layer breaks it after (nəs)"},
    ("(8k) line 1", "‘get unravelled"): {"kind": "translation", "who": AUTHORS,
                                        "gloss": "page 7, printed without its closing quote"},
    ("(7g) line 1", "x̌<ʔ>ʕal *x̌ʕal-p"): {"form": "x̌<ʔ>ʕal"},
    ("(7h) line 1", "*h<ʔ>am ham-áp"): {"form": "*h<ʔ>am"},
    ("(7i) line 1", "*k̓<ʔ>im k̓m-áp"): {"form": "*k̓<ʔ>im"},
    ("(7j) line 1", "*t<ʔ>aɬ tɬ-ap"): {"form": "*t<ʔ>aɬ"},
    ("(7l) line 1", "*x̌<ʔ>as *x̌as-p"): {"form": "*x̌<ʔ>as"},
    ("(7m) line 1", "*y<ʔ>us *yus-p"): {"form": "*y<ʔ>us"},
    ("§2.1", "(12) a. The knife was sharpened again."):
        {"where": "(12a) line 1", "who": "English", "kind": "transcription", "form": "The knife was sharpened again.",
         "gloss": "page 8, an English example, Beavers & Koontz-Garboden 2020"},
    ("§2.1", "b. John sharpened the knife again."):
        {"where": "(12b) line 1", "who": "English", "kind": "transcription", "form": "John sharpened the knife again.",
         "gloss": "page 8, an English example, Beavers & Koontz-Garboden 2020"},
    ("§2.1", "(13) a. #The ice-cream was melted again."):
        {"where": "(13a) line 1", "who": "English", "kind": "transcription", "form": "#The ice-cream was melted again.",
         "gloss": "page 9, an English example, Rappaport Hovav & Levin 2010"},
    ("§2.1", "b. #Kim melted the ice-cream again."):
        {"where": "(13b) line 1", "who": "English", "kind": "transcription", "form": "#Kim melted the ice-cream again.",
         "gloss": "page 9, an English example, Rappaport Hovav & Levin 2010"},
    ("§2.2", "containing d′ and whose maximal degree is ds."):
        {"where": "(21a) line 7", "who": BKG, "gloss": "page 11, the truth conditions of BECOME(s,e), quoted"},
    ("(21b) line 1", draft_form("(21b) line 1", "Figure/Path Relation:")):
        {"form": draft_form("(21b) line 1", "Figure/Path Relation:") + " e.",
         "gloss": "page 11, the Figure/Path Relation, quoted; its last word, e., is set alone on page 12"},
    ("(11e) line 1", draft_form("(11e) line 1", "Overall, “change of state")):
        {"where": "§2.2", "who": AUTHORS, "kind": "note",
         "form": draft_form("(11e) line 1", "Overall, “change of state") + " " + draft_form("§2.2", "than the degree held"),
         "gloss": "page 12"},
    ("(27d) line 3", "‘I got cut by a knife.’"): {"who": AUTHORS},
    ("(ia) line 1", draft_form("(ia) line 1", "⟦vBECOME⟧")):
        {"where": "footnote 24 (iia) line 1", "who": "Beavers & Koontz-Garboden 2020",
         "gloss": "page 19, footnote 24, the vBECOME of Beavers & Koontz-Garboden"},
    ("(ib) line 1", draft_form("(ib) line 1", "⟦√melt⟧")):
        {"where": "footnote 24 (iib) line 1", "who": "Beavers & Koontz-Garboden 2020",
         "gloss": "page 19, footnote 24, the result root of Beavers & Koontz-Garboden"},
    ("(ic) line 1", draft_form("(ic) line 1", "⟦vBECOME √melt⟧")):
        {"where": "footnote 24 (iic) line 1", "who": "Beavers & Koontz-Garboden 2020",
         "gloss": "page 19, footnote 24, vBECOME applied to the result root"},
    ("§7", draft_form("§7", "In conclusion")): {"where": "§6"},
}
# The first row of each of the tables (9), (10) and (11) opens on a letter cell, a., and the engine
# numbered the row after the example. The second table of (11) repeats the letters a. to d., and
# footnote 14's table numbers its rows (i) a. to h.
_second = False
for _where, _who, _kind, _form, _gloss in DRAFT:
    if _where in ("(9) line 1", "(10) line 1", "(11) line 1") and _form != "a.":
        SET[(_where, _form)] = {"where": _where.replace(") line", "a) line")}
    if (_where, _form) == ("(11a) line 1", "xʷl•al"):
        _second = True
    if _second and re.match(r"^\(11[a-d]\) line 1$", _where):
        SET[(_where, _form)] = {"where": _where.replace("line 1", "line 2")}
    if _where in LETTERS_OF and not re.fullmatch(r"[a-h]\.", _form):
        SET[(_where, _form)] = {"where": "footnote 14 (i%s) line 1" % LETTERS_OF[_where]}
    # A St'át'imcets or Nsyilxcn line of (61) to (71) closes on the name of its language. The
    # footnote rule takes the 7 of the auxiliary wa7 for a footnote mark, and the row is found
    # without it and given it back.
    named = re.match(r"^(.*\S) (St’át’imcets|Nsyilxcn)$", _form)
    if named and _kind == "segmentation" and re.match(r"^\((6[1-9]|7[01])[ab]?\) line", _where):
        SET[(_where, re.sub(r"^wa7 ", "wa ", _form))] = {
                                "form": named.group(1), "who": ST if named.group(2) == ST else L,
                                "gloss": "%s, the page names the language, %s, at the right of the line"
                                % (_gloss, named.group(2))}

# The stative and inchoative cells the engine took for one cell, each with its own row.
SPLIT_CELLS = (("(7g) line 1", "x̌<ʔ>ʕal *x̌ʕal-p"), ("(7h) line 1", "*h<ʔ>am ham-áp"),
               ("(7i) line 1", "*k̓<ʔ>im k̓m-áp"), ("(7j) line 1", "*t<ʔ>aɬ tɬ-ap"),
               ("(7l) line 1", "*x̌<ʔ>as *x̌as-p"), ("(7m) line 1", "*y<ʔ>us *yus-p"))

ADD = tuple(
    [(None, ("title", AUTHORS, "title", TITLE, "the paper's title, which carries the star of footnote *")),
     (None, ("title", AUTHORS, "name", "John Lyon", "the author, University of British Columbia – Okanagan")),
     (None, ("footnote *", AUTHORS, "name", "Delphine Derickson-Armstrong",
             "of Westbank reserve, ɬk̓mxnalqs; footnote * spells her Derrickson-Armstrong, and DD in the speaker comments")),
     (None, ("footnote *", AUTHORS, "name", "Dave Michele",
             "of Westbank reserve, c̓skʕáknaʔ; DM in the speaker comments, and Dave Michel in some tags")),
     (None, ("footnote *", AUTHORS, "place", "Westbank reserve", "home of Delphine Derickson-Armstrong and Dave Michele")),
     (None, ("footnote *", AUTHORS, "name", "En’owkin Centre", "a supporter of the work")),
     (None, ("§1", AUTHORS, "language", "Nsyilxcn", "the language of the paper, a.k.a. Okanagan, ISO 639-3 oka, Southern Interior Salish")),
     (None, ("§1", AUTHORS, "language", "Okanagan", "another name for Nsyilxcn")),
     ] +
    [(one, (one[0], L, "transcription", one[1].split()[1], "page 5, a cell of its own, set beside the one before it"))
     for one in SPLIT_CELLS] +
    display(("§1", "The basic outline of my argument..."), [
        ("(3a) line 1", L, "transcription", "əc-nik’", "page 3, printed with a straight apostrophe for the glottalization"),
        ("(3a) line 1", AUTHORS, "translation", "‘already cut’", "page 3"),
        ("(3a) line 1", AUTHORS, "note", "CS root - target stative", "page 3, what the form is"),
        ("(3b) line 1", L, "transcription", "əc-nik̓•ək̓", "page 3"),
        ("(3b) line 1", AUTHORS, "translation", "‘already cut’", "page 3"),
        ("(3b) line 1", AUTHORS, "note", "CS root - resultant stative", "page 3, what the form is"),
        ("(4a) line 1", L, "transcription", "*əc-qʷin", "page 3"),
        ("(4a) line 1", AUTHORS, "translation", "‘already made green’", "page 3"),
        ("(4a) line 1", AUTHORS, "note", "PC root - target stative", "page 3, what the form is"),
        ("(4b) line 1", L, "transcription", "əc-qʷ<ʔ>in", "page 3"),
        ("(4b) line 1", AUTHORS, "translation", "‘already made green", "page 3, printed without its closing quote"),
        ("(4b) line 1", AUTHORS, "note", "PC root - resultant stative", "page 3, what the form is"),
    ]) +
    display(("(5) line 7", "‘straight’"), table_6()) +
    display("(24a) line 2", [
        ("(24a) line 3", AUTHORS, "translation", cut(R542, "# ‘When", " b. "), "page 13, the tag on the line below"),
        ("(24b) line 1", L, "segmentation", cut(R542, "ixíʔ"), "page 13"),
        ("(24b) line 2", L, "gloss", cut(R543, "that when", " ‘When"), "page 13"),
        ("(24b) line 3", AUTHORS, "translation", cut(R543, "‘When"), "page 13"),
    ]) +
    display(("§3", "wiʔ"), [
        ("(29b) line 1", L, "segmentation", cut(R587, "way̓"), "page 15"),
        ("(29b) line 2", L, "gloss", cut(R590, "already", " ‘The"), "page 15"),
        ("(29b) line 3", AUTHORS, "translation", cut(R590, "‘The", " c. "), "page 15"),
        ("(29c) line 1", L, "segmentation", cut(R590, "(əc)-wiʔ"), "page 15"),
        ("(29c) line 2", L, "gloss", cut(R593, "STAT-finish", " ‘The"), "page 15"),
        ("(29c) line 3", AUTHORS, "translation", cut(R593, "‘The", " d. "), "page 15"),
        ("(29d) line 1", L, "segmentation", cut(R593, "way̓"), "page 15"),
        ("(29d) line 2", L, "gloss", cut(R594, "already", " ‘The"), "page 15"),
        ("(29d) line 3", AUTHORS, "translation", cut(R594, "‘The", " To be clear"), "page 15"),
    ]) +
    [("(29a) line 3",
      ("footnote 16", AUTHORS, "note", joined("16 These are not always", "away from these issues"),
       "page 14, footnote"))] +
    display(("§5", "Beavers & Koontz-Garboden’s (2020) analysis of result roots..."), [
        ("(53a) line 1", KL, "note", "Measure of change", "page 25, the heading of the definition"),
        ("(53a) line 2", KL, "rule", cut(R813, "For any measure function m", " b. “"),
         "page 25, the measure-of-change function, its ↑ set on the line below"),
        ("(53b) line 1", "Kennedy & Levin 2008:18-19", "note", cut(R813, "“A measure", " (Kennedy & Levin"),
         "page 25, the prose description, quoted"),
        ("(53b) line 1", AUTHORS, "citation", "(Kennedy & Levin 2008:18-19)", "the source of the quotation"),
    ]) +
    display("(71a) line 3", [
        ("(71a) line 4", AUTHORS, "rule", cut(R922, "λxλt", " b. wa7"), "page 30, the denotation of (71a)"),
        ("(71b) line 1", ST, "segmentation", cut(R922, "wa7", " St’át’imcets"),
         "page 30, a St’át’imcets example, the page names the language at the right of the line"),
        ("(71b) line 2", ST, "gloss", cut(R922, "IPFV", " ‘My car"), "page 30, a St’át’imcets example"),
        ("(71b) line 3", DAVIS, "translation", cut(R922, "‘My car", " λxλt"),
         "page 30, the translation of Davis (in prep.), as the tag says"),
        ("(71b) line 4", AUTHORS, "rule", cut(R922, "λxλt∃s∃e.[fixedΔ(x,e,s) ≥ stnd(fixedΔ) ∧ t", " Resultant state"),
         "page 31, the denotation of (71b)"),
    ]) +
    display(("§6", "The theory put forward in this paper predicts..."), typology()))

WHOSE = (
    "The language is Nsyilxcn, Okanagan, and its examples come from Delphine Derickson-Armstrong and "
    "Dave Michele of Westbank reserve, whom footnote * thanks by their nsyilxcn names too, ɬk̓mxnalqs "
    "and c̓skʕáknaʔ. The tag at the right of each translation names the speaker, and VF in it marks a "
    "volunteered form. Examples without VF were built by the author and judged by the speakers. The "
    "speaker comments are signed DD and DM, and each is the speaker's it names. The paper spells her "
    "Derrickson-Armstrong in footnote * and some tags, and him Dave Michel in some tags.\n\n"
    "The who of every tier of a Nsyilxcn example and of every word of the tables is nsyilxcn, and "
    "the translations and the column glosses are John Lyon's. The St’át’imcets examples (43b), (61), "
    "(66a), (67), (70) and (71) and the table (64) are cited from Davis (in prep.) and Davis 2024, "
    "and their tiers are St’át’imcets and their translations the source's; the consultant's comment "
    "on (43b) is cited from Davis 2024:310. The English examples (12) and (13) are English. The "
    "definitions (21) are Beavers and Koontz-Garboden's and Beavers', (53) is Kennedy and Levin's, and "
    "footnote 24's (ii) is Beavers and Koontz-Garboden's. The other denotations, the prose, the "
    "contexts, the headings and the notes carry John Lyon."
)

LETTERS = (
    "The forms are in the Americanist orthography the Nsyilxcn literature uses, with the "
    "glottalization of a consonant as U+0313, the uvular of x̌ with a caron, U+030C, and the raised w "
    "of kʷ as U+02B7. The bullet • marks C2 reduplication and <ʔ> the inchoative infix. The "
    "denotations use the double brackets ⟦ ⟧, U+27E6 and U+27E7, and some variables are set in "
    "mathematical italic in the text layer, 𝑑𝑥𝑠 in (21a) and (54b). The gloss labels are ASCII "
    "capitals in the text layer."
)

PAGE_NOTES = (
    "The text layer leaves a space after a glottalized letter, nik̓ •ək̓ for nik̓•ək̓, and the table "
    "closes it. It runs DET into the noun after it in (46e) and (49a), DETwind and DETrope, and the "
    "table puts the space back. The page prints totarget in Section 5 and intepretation in Section 6, "
    "labels the last row of (9) h., and gives (8k) and (4b) no closing quote; each is kept. The trees "
    "of Figures 1 to 6 are drawings the text layer does not hold, and only their captions are rows."
)
