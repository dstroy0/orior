# Context for Bailey Trotter, The past tense suffix and 2PCs in ʔayʔaǰuθəm (Comox-Sliammon). The
# examples come from Betty Wilson (BW), tagged sf with the date, and from Watanabe 2003, Huijsmans
# 2023 and Kroeber 2002. The engine took a cell of Table 2, 3 Ø, for the heading of section 3, and
# every row from there to the conclusion is given back to §2.2, §2.3 or §2.4. The lexical entries
# (3), (15) and (18), Tables 1 to 3, the derivations (21) and (22) and the references are read off
# the page again.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import PRIVATE  # noqa: E402
from workdir import WORK  # noqa: E402
import residue  # noqa: E402

STEM = "Trotter_ICSNL"
AUTHORS = "Bailey Trotter"
AY = "ʔayʔaǰuθəm"

TITLE = "The past tense suffix and 2PCs in ʔayʔaǰuθəm (Comox-Sliammon)"
BYLINE = "Bailey Trotter, The University of British Columbia"
# The stem does not carry the volume; the page header names ICSNL 60.
VOLUME = "60"

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
    for number in range(index, -1, -1):
        marker = re.match(r"^===== page (\d+) =====$", PAGE[number])
        if marker:
            return int(marker.group(1))
    return 0


def chained(anchor, rows):
    """Each row after the one before it, the first after anchor."""
    out = []
    for row in rows:
        out.append((anchor, row))
        anchor = (row[0], row[3])
    return out


def draft_form(where, opening):
    for one in DRAFT:
        if one[0] == where and one[3].startswith(opening):
            return one[3]
    raise SystemExit("no draft row %s opens with %s" % (where, opening))


def entry(number, page, head, allomorphs):
    """A lexical entry: the morpheme, its general allomorph and each specified one with the
    environment that selects it."""
    where = "(%d) line %%d" % number
    rows = [(where % 1, AUTHORS, "note", "allomorph environment",
             "page %d, the column heads of the lexical entry of %s" % (page, head))]
    for line, (form, environment) in enumerate(allomorphs, 2):
        gloss = "page %d, %s ⇔ %s, %s" % (page, head, form, "the general allomorph" if not environment
                                             else "selected in the environment " + environment)
        rows.append((where % line, AY, "cited affix", form, gloss))
    return rows


ENTRY_3 = entry(3, 4, "PST", (("/uɬ/", ""), ("/ʔu/", "__3SG.POSS"), ("/ʔuw/", "__2PL.POSS/3PL.POSS")))
ENTRY_15 = entry(15, 9, "1SG.SBJ", (("/čan/", ""), ("/č/", "]ω__ or Q__"), ("/tᶿ/", "]ω__FUT or Q__FUT")))
ENTRY_18 = entry(18, 10, "3ERG", (("/as/", ""), ("Ø", "__{RPT/FUT/3POSS/3SBJV}")))

TABLE_1 = [("Table 1", AUTHORS, "note", "Table 1: Allomorphs of the past tense with possessive subject clitics", "page 5, the caption"),
           ("Table 1", AUTHORS, "note", "POSS SBJ | √ + PST (-uɬ) + POSS SBJ", "page 5, the column heads")]
for _person, _clitic, _word in (("1SG", "(ʔə)tᶿ=", "(ʔə)tᶿ= √-uɬ"), ("2SG", "(ʔə)θ=", "(ʔə)θ= √-uɬ"),
                                ("1PL", "ʔəms=", "ʔəms= √-uɬ"), ("2PL", "=ap", "√-ʔuw=ap"),
                                ("3SG", "=s", "√-ʔu=s"), ("3PL", "=it", "√-ʔuw=it")):
    TABLE_1 += [("Table 1 %s" % _person, AY, "cited affix", _clitic, "page 5, the possessive subject clitic, POSS SBJ"),
                ("Table 1 %s" % _person, AY, "segmentation", _word, "page 5, √ + PST (-uɬ) + POSS SBJ")]

TABLE_2 = [("Table 2", AUTHORS, "note", "Table 2: Full and reduced forms of the indicative subject clitics (Huijsmans 2023)", "page 6, the caption"),
           ("Table 2", AUTHORS, "note", "Full | Reduced", "page 6, the column heads")]
for _person, _full, _reduced in (("1SG", "čan", "č"), ("2SG", "čaxʷ", "čxʷ"), ("1PL", "cat", "št"),
                                 ("2PL", "čap", None), ("3", "Ø", None)):
    if _reduced:
        TABLE_2 += [("Table 2 %s" % _person, AY, "cited affix", _full, "page 6, the full form"),
                    ("Table 2 %s" % _person, AY, "cited affix", _reduced, "page 6, the reduced form")]
    else:
        TABLE_2.append(("Table 2 %s" % _person, AY, "cited affix", _full, "page 6, one cell centered over both columns"))

TABLE_3 = [("Table 3", AUTHORS, "note", "Table 3: The subjunctive and ergative subjects (adapted from Watanabe 2003, p. 52)", "page 13, the caption"),
           ("Table 3", AUTHORS, "note", "SUBJUNCTIVE | ERGATIVE", "page 13, the column heads")]
for _person, _form in (("1SG", "an"), ("1PL", "at"), ("2SG", "axʷ"), ("2PL", "ap"), ("3", "as")):
    TABLE_3 += [("Table 3 %s" % _person, AY, "cited affix", "=" + _form, "page 13, the subjunctive subject clitic"),
                ("Table 3 %s" % _person, AY, "cited affix", "-" + _form, "page 13, the ergative subject suffix")]


def derivation(number, caption, segmented, glossed, english, steps):
    """A derivation: the example, then each step of lexical insertion a rule row with its name."""
    rows = [("(%d) line 1" % number, AUTHORS, "note", caption, "page 11, the caption"),
            ("(%da) line 1" % number, AY, "segmentation", segmented, "page 11"),
            ("(%da) line 2" % number, AY, "gloss", glossed, "page 11"),
            ("(%da) line 3" % number, AUTHORS, "translation", english, "page 11")]
    for letter, (form, step) in zip("bcde", steps):
        rows.append(("(%d%s) line 1" % (number, letter), AUTHORS, "rule", form, "page 11, the step, %s" % step))
    return rows


DERIVATION_21 = derivation(21, "No clitics", "hu k̓ʷə[n]-t-as-uɬ", "go see-CTR-3ERG-PST",
                           "‘He went to see her... (yesterday).’", (
                               ("ERG+SUFFIX PST+SUFFIX [hu]ω [kʷət]ω", "first-step of lexical insertion (L-match)"),
                               ("[hu]ω [kʷət]ω-PST-ERG", "linearization"),
                               ("[hu]ω [kʷət]ω-uɬ-as", "second-step of lexical insertion (Insert), allomorph selection"),
                               ("[hu]ω [kʷət-as-uɬ]ω", "metathesis")))
DERIVATION_22 = derivation(22, "With clitics", "yəm-əxʷ-Ø-uɬ=k̓ʷa...", "kick-NCTR-ERG-PST=RPT",
                           "‘He accidentally kicked... (a rock).’", (
                               ("RPT+ENCL ERG+SUF PST+SUF [yəm-əxʷ]ω", "first-step of lexical insertion (L-match)"),
                               ("[yəm-əxʷ]ω-PST-ERG=RPT", "linearization"),
                               ("[yəm-əxʷ]ω-uɬ-Ø=k̓ʷa", "second-step of lexical insertion (Insert), allomorph selection"),
                               ("[yəm-əxʷ-Ø-uɬ=k̓ʷa]ω", "metathesis")))


def references():
    """One reference a paragraph: the entries are parted by blank lines."""
    entries, current = [], None
    for index in range(at("Blake, Susan J."), len(PAGE)):
        text = PAGE[index]
        if text.startswith("=====") or text.isdigit():
            continue
        if not text:
            current = None
            continue
        if current is None:
            current = [text, page_of(index)]
            entries.append(current)
        else:
            current[0] += " " + text
    return [("references", AUTHORS, "reference", text, "page %d" % page) for text, page in entries]


REFERENCES = references()

# (23)'s lines, which the engine took for a rule, and its translation, left in the section; the
# engine's reference entries, which are read again whole below.
KIND_RULES = (
    (r"^\(23\) line 1$", r"^rule$", r".", "transcription", AY, "set as a bracketing of the relative clause"),
    (r"^\(23\) line 2$", r"^rule$", r".", "segmentation", AY, ""),
    (r"^\(23\) line 3$", r"^rule$", r".", "gloss", AY, ""),
    (r"^§3$", r"^note$", r"^‘I know the one you went and helped\.’", "translation", AUTHORS, "the translation of (23)"),
    (r"^references$", r"^note$", r".", "reference", AUTHORS, "read again below"),
)

FORMS = {
    AY: ("language", AUTHORS, "Comox-Sliammon, Central Salish, the language of the paper, its Mainland dialect"),
    "ʔu": ("cited affix", AY, "page 4, the past tense allomorph -ʔu"),
    "ʔuw": ("cited affix", AY, "page 4, the past tense allomorph -ʔuw"),
    "ʔuɬ": ("cited affix", AY, "page 4, footnote 3, the past tense as earlier documentation writes it, -ʔuɬ, its hyphen closing the line above"),
    "səm": ("cited affix", AY, "page 6, footnote 6, the future clitic"),
    "CəC": ("notation", AUTHORS, "page 7, a root of consonant, schwa, consonant"),
    "-oɬ": ("cited affix", AY, "page 15, the past marker in the title of Huijsmans 2024"),
    "Tromsø": ("place", AUTHORS, "the University of Tromsø, in a reference entry"),
    "Honoré": ("name", AUTHORS, "Honoré Watanabe, in a reference entry"),
}
DROP = ()

SPLIT = [
    ("front", "Abstract:", None, {"gloss": "page 1, the abstract"}),
    ("§1.1", "Lexical insertion refers to", {"where": "(1) line 1", "kind": "rule",
                                               "gloss": "page 2, the enclitic feature"}, {}),
    ("§1.1", "The suffix feature acts as a instruction", {"where": "(2) line 1", "kind": "rule",
                                                          "gloss": "page 3, the suffix feature"}, {}),
    ("§2.1", "The more specified allomorphs", None, {}),
    ("§2.1", "The possessive subject enclitics occur", None, {}),
    ("§3", "2.3 Transparency to ergative", None, {}),
    ("§3", "The third person ergative suffix -as is usually", {"where": "§2.3", "kind": "heading", "gloss": "page 9"},
     {"where": "§2.3"}),
    ("§3", "The problem in relation to the past tense", None, {}),
    ("§3", "Subjunctive subject clitics primarily", {"where": "§2.4", "kind": "heading", "gloss": "page 12"},
     {"where": "§2.4"}),
    ("§3", "The morphological features which distinguish", None, {}),
    ("footnote 3", "I have argued for an account", {"where": "§3", "kind": "heading", "gloss": "page 14"},
     {"where": "§3"}),
    ("(16c) line 3", "[Watanabe 2003, 57]", {}, {"kind": "citation", "who": AUTHORS,
                                                  "gloss": "the source at the right of the example"}),
    ("(27b) line 3", "[sf | BW.2025/04/14]", {}, {"kind": "citation", "who": AUTHORS,
                                                   "gloss": "the tag at the right of the example"}),
]

# The cells of the tables and entries the engine cited from the section, and the lines of the
# derivations it set as examples.
REMOVE = [("§3", "3 Ø"), ("§3", draft_form("§3", "past tense undergoes metathesis with the null")),
          ("§3", draft_form("§3", "taken into account when selecting"))]
REMOVE += [("§2.1", one) for one in ("/uɬ/", "/ʔu/", "/ʔuw/", "(ʔə)tᶿ=", "√-uɬ", "(ʔə)θ=", "ʔəms=",
                                      "√-ʔuw=ap", "√-ʔu=s", "√-ʔuw=it")]
REMOVE += [("§2.2", one) for one in ("čan", "č", "čaxʷ", "čxʷ", "št", "čap")]
REMOVE += [("§3", one) for one in ("/čan/", "/č/", "ω__", "/tᶿ/", "ω__FUT", "Ø", "=axʷ", "-axʷ")]
REMOVE += [(where, form) for where, who, kind, form, gloss in DRAFT if where == "references" and kind == "note"]
REMOVE_WHERE = r"^Table 2 line|^\(2[12][a-e]?\) line"

ADD = tuple(
    [(None, ("title", AUTHORS, "title", TITLE, "the paper's title, carrying the star of the acknowledgement footnote")),
     (None, ("title", AUTHORS, "name", AUTHORS, "author, The University of British Columbia")),
     (None, ("footnote *", AUTHORS, "name", "Betty Wilson", "the speaker, BW in the tags, of the Mainland dialect")),
     (None, ("footnote *", AUTHORS, "name", "Marianne Huijsmans", "thanked for her documentation and analysis")),
     (None, ("footnote *", AUTHORS, "name", "Henry Davis", "thanked")),
     (None, ("footnote *", AUTHORS, "name", "Gloria Mellesmoen", "thanked. Footnote 8 cites her")),
     ("§2.2", ("§2.2", AY, "cited affix", "=a", "page 8, the polar question clitic")),
     ("§2.3", ("§2.3", AY, "cited affix", "-as", "page 9, the third person ergative suffix")),
     (None, ("all", AUTHORS, "notation", "sf", "a sentence the author supplied and the speaker judged")),
     (None, ("all", AUTHORS, "notation", "*", "judged ill-formed")),
     (None, ("all", AUTHORS, "notation", "ω", "a prosodic word")),
     (None, ("all", AUTHORS, "notation", "μ", "a mora")),
     (None, ("all", AUTHORS, "notation", "Ft", "a foot"))]
    + chained(("§2.1", "There are three allomorphs of the past tense..."), ENTRY_3)
    + chained(("footnote 4", "4 When adjacent to possessive suffixes..."), TABLE_1)
    + chained(("§2.2", "The indicative subject clitics have two main forms..."), TABLE_2)
    + chained(("§3", "I argue contrary to Huijsmans (2023)..."), ENTRY_15)
    + chained(("(17) line 4", "[Kroeber 2002, 25]"), ENTRY_18)
    + chained(("§3", "The following examples show how..."), DERIVATION_21 + DERIVATION_22)
    + chained(("§3", "Exactly what triggers the metathesis..."), TABLE_3)
    + chained(("references", "References"), REFERENCES)
)

SET = {
    ("(1) line 1", "(1) [ENCLITIC] = attach following the next highest head in a span"):
        {"form": "[ENCLITIC] = attach following the next highest head in a span"},
    ("(2) line 1", "(2) [SUFFIX] = attach following the next lowest head in a span"):
        {"form": "[SUFFIX] = attach following the next lowest head in a span"},
    ("(10c) line 2", draft_form("(10c) line 2", "Suffixes that are spelled out")):
        {"where": "§2.2", "who": AUTHORS, "kind": "note", "gloss": "page 7",
         "form": draft_form("(10c) line 2", "Suffixes") + " " + draft_form("§3", "taken into account when selecting")},
    ("(22e) line 2", draft_form("(22e) line 2", "As the null ergative")):
        {"where": "§2.3", "who": AUTHORS, "kind": "note", "gloss": "page 11",
         "form": draft_form("(22e) line 2", "As the null") + " " + draft_form("§3", "past tense undergoes metathesis with the null")},
    ("(28b) line 7", "[sf | BW.2025/05/16]"): {"kind": "citation", "who": AUTHORS, "gloss": "the tag at the right of the example"},
    ("§3", "=k̓ʷa"): {"where": "§2.3", "kind": "cited affix", "gloss": "page 9, the reportative clitic"},
    ("§3", "-Ø"): {"where": "§2.3", "gloss": "page 9, the null allomorph of the third person ergative"},
    ("§3", "‘I know the one you went and helped.’"): {"where": "(23) line 4"},
    ("§3", "[Watanabe 2003, 131]"): {"where": "(23) line 4"},
}
# The prose after (18) and after Table 3, parted from the entry and the table the draft ran it into.
for _opening, _marker, _section in (("(18) allomorph environment", "The problem in relation", "§2.3"),
                                    ("Table 3:", "The morphological features which distinguish", "§2.4")):
    _text = draft_form("§3", _opening)
    SET[("§3", _text[_text.index(_marker):])] = {"where": _section}
for _person, _where in (("2PL", "(4)"), ("3SG", "(5)"), ("3PL", "(6)")):
    SET[("§2.1", "Adjacent to %s.POSS:" % _person)] = {"where": "%s line 0" % _where,
                                                        "gloss": "page 5, the heading over the example"}
# (9b) to (9d) and (10b), (10c) are the author's footings of the roots, not tiers of the language.
for _where, _who, _kind, _form, _gloss in DRAFT:
    if re.match(r"^\((9[bcd]|10[bc])\) line 1$", _where):
        SET[(_where, _form)] = {"kind": "rule", "who": AUTHORS, "gloss": _gloss + ", the author's footing of the root"}

# The rows from the cell 3 Ø on stand in §3 in the draft; each goes back to its section.
_section = None
for _where, _who, _kind, _form, _gloss in DRAFT:
    if _kind == "heading" and _form == "3 Ø":
        _section = "§2.2"
        continue
    if _form.startswith("(15) allomorph environment"):
        _section = "§2.3"
        continue
    if _form.startswith("2.4 Subjunctive"):
        _section = "§2.4"
        continue
    if _form.startswith("3 Conclusion"):
        break
    if _section and _where == "§3" and ("§3", _form) not in REMOVE and ("§3", _form) not in SET:
        SET[("§3", _form)] = {"where": _section}

# The words a footnote cites, which the engine left in the section around it.
_note = None
for _where, _who, _kind, _form, _gloss in DRAFT:
    _page = re.search(r"page (\d+)", _gloss)
    _page = int(_page.group(1)) if _page else 0
    if _kind == "note" and "footnote" in _gloss:
        _mark = re.match(r"^(\d{1,2}|\*) ", _form)
        _note = (_mark.group(1), _page) if _mark else None
        continue
    if _kind != "cited form" or not _note or _page != _note[1] or (_where, _form) in REMOVE:
        _note = None
        continue
    SET.setdefault((_where, _form), {})["where"] = "footnote %s" % _note[0]

WHOSE = (
    "The language is ʔayʔaǰuθəm, its Mainland dialect. The examples come from Watanabe 2003, "
    "Huijsmans 2023 and Kroeber 2002, each named in brackets at the right of the translation, and "
    "from the author's elicitation with Betty Wilson, tagged [sf | BW.date].\n\n"
    "The who for each tier of an example is the language, and for a translation the author. The "
    "lexical entries (3), (15) and (18) and the tables give their allomorphs to the language. The "
    "footings of (9) and (10), the steps of the derivations (21) and (22), the features (1) and (2) "
    "and the prose carry the author."
)

LETTERS = (
    "The transcription lines write the practical orthography with ɛ, ʊ and ɩ, hɛkʷ čɛ, and the "
    "segmentation lines the phonemic forms, hiɬ+kʷ=ča. ω marks a prosodic word, μ a mora, Ft a "
    "foot, and Ø a null allomorph."
)

PAGE_NOTES = (
    "The engine took the last cell of Table 2, 3 Ø, for the heading of section 3, and ran the "
    "headings of §2.3, §2.4 and §3 into the prose; the rows between are given back to their "
    "sections here. The lexical entries (3), (15) and (18) and Tables 1 to 3 are read off the page "
    "one cell a row, their columns placed by glyph position."
)
