# Context for Henry Davis, Central Salish from a Nooksack Perspective. The paper compares sixteen
# grammatical traits across seven Central Salish languages in sixteen tables, each a row of language
# heads over rows of traits; the text layer ran every table into the prose around it. Each table is
# read again from the glyph positions (grid_table.py), a cell under the head its middle stands
# nearest: the cells that are affixes or particles are cited forms of their column's language, and a
# table of marks (√, U+2014, yes, no) is a row a trait with each language's mark in the gloss. The
# examples are Nooksack but for (4) and (8), Upriver Halkomelem, (9), Squamish, (29) and (33),
# nɬeʔkepmxcín, and (30) and (34), St'át'imcets, each named with its source on the page.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import WORK  # noqa: E402
import grid_table  # noqa: E402

STEM = "ICSNL59_Davis_final"
AUTHORS = "Henry Davis"
NK = "Nooksack"
UH = "Upriver Halkomelem"
HL = "Halkomelem"
SQ = "Squamish"
LU = "Lushootseed"
TH = "nɬeʔkepmxcín"
LI = "St’át’imcets"

TITLE = "Central Salish from a Nooksack Perspective"
BYLINE = "Henry Davis, University of British Columbia"
VOLUME = "59"

with open(os.path.join(WORK, STEM + ".draft.tsv"), encoding="utf-8") as handle:
    DRAFT = [one.rstrip("\n").split("\t") for one in handle][1:]

# The column heads, as §2 and the tables define them.
HEADS = {"LU": LU, "NK": NK, "UH": UH, "NSS": "Northern Straits Salish", "SS": "Straits Salish",
         "SQ": SQ, "SE": "Sechelt", "CX": "Comox"}


def chained(anchor, rows):
    """Each row after the one before it, the first after anchor."""
    out = []
    for row in rows:
        out.append((anchor, row))
        anchor = (row[0], row[3])
    return out


def read(page, bottom, top=None):
    """The heads and cells of the table on page whose head line stands under top."""
    heads, rows = grid_table.rows(STEM, page, "LU", bottom, top)
    return heads, [cells for label, cells in rows]


def forms(number, page, caption, labels, bottom, top=None, fixes=None):
    """A table of affixes or particles: the caption, then a row a trait and a cited form a cell,
    given to the language of the cell's column. A dash is an empty cell."""
    heads, cells = read(page, bottom, top)
    where = "Table %d" % number
    out = [(where, AUTHORS, "note", caption, "page %d, the caption" % page)]
    for label, row in zip(labels, cells):
        out.append((where, AUTHORS, "note", label, "page %d, the head of a row" % page))
        for head, cell in zip(heads, row):
            cell = (fixes or {}).get((label, head), cell)
            # The page text sets a tilde between spaces and the plus of (+-as) against its hyphen.
            cell = re.sub(r"\s*~\s*"," ~ ", cell or "").replace("(+ -", "(+-")
            if cell in ("", "—", "_—"):
                continue
            kind = "cited affix" if re.search(r"^\(?[-=]|=\)?$", cell) else "cited form"
            # An empty cell marked (DEM), a demonstrative in the place of the pronoun, holds no form.
            who = HEADS[head]
            if cell.startswith("—"):
                kind, who = "note", AUTHORS
            out.append(("%s %s" % (where, head), who, kind, cell,
                        "page %d, %s, the %s column" % (page, label, head)))
    return out


def marks(number, page, caption, labels, bottom, top=None):
    """A table of marks: the caption, then a row a trait, its form the trait and the marks as the
    page sets them, the gloss naming each language's mark."""
    heads, cells = read(page, bottom, top)
    where = "Table %d" % number
    out = [(where, AUTHORS, "note", caption, "page %d, the caption" % page)]
    for label, row in zip(labels, cells):
        # The glyph words run PROG (?) of Table 1 together; the page text keeps its space.
        row = [cell.replace("_", "").replace("PROG(?)", "PROG (?)") for cell in row]
        printed = " ".join(row)
        out.append((where, AUTHORS, "note", ("%s %s" % (label, printed)).strip(),
                    "page %d, %s" % (page, ", ".join("%s %s" % (head, cell.replace("_", "")) for head, cell in zip(heads, row)))))
    return out


TABLE_1 = marks(1, 5, "Table 1: Imperfective marking across Central Salish",
                ("C1 reduplication", "‘actual’", "auxiliary", "prefix/proclitic"), 330)
TABLE_2 = forms(2, 7, "Table 2: Third person transitive subject marking across Central Salish",
                ("main clause", "subordinate clause", "subjunctive clause", "nominalized clause"), 240,
                fixes={(label, head): None for label in () for head in ()})
TABLE_3 = marks(3, 9, "Table 3: *3 > 2 in Central Salish", ("*3 > 2",), 340)
TABLE_4 = marks(4, 10, "Table 4: Passive in Central Salish", ("non-promotional", "promotional"), 240)
TABLE_5 = marks(5, 11, "Table 5: Distribution of the oblique determiner across Central Salish",
                ("ergative proper noun subjects", "oblique proper noun subjects", "oblique common noun subjects"), 380)
TABLE_6 = forms(6, 12, "Table 6: The general oblique marker/preposition across Central Salish",
                ("oblique marker",), 440)
TABLE_7 = marks(7, 12, "Table 7: Pre-predicative subjects in Central Salish", ("SV(O)",), 200, top=230)
TABLE_8 = marks(8, 13, "Table 8: Distribution of post-verbal DPs in Central Salish", ("",), 490)
TABLE_9 = forms(9, 14, "Table 9: Intransitive Markers across Central Salish",
                ("active intransitive", "developmental", "autonomous", "middle"), 530)
# The relic rows of Table 10 set a raised hyphen inside the parenthesis and a raised y after x; the
# page at 250 dpi prints (-ns), (-ləs), (-aš ~ axy) and (-əxy), and the text layer codes the y on
# the line.
TABLE_10 = forms(10, 15, "Table 10: Transitivizers across Central Salish",
                 ("control", "limited control", "causative", "(transitive)", "(purposive)"), 390,
                 fixes={("(transitive)", "NK"): "(-ns)", ("(transitive)", "UH"): "(-ləs)",
                        ("(purposive)", "NK"): "(-aš ~ axy )", ("(purposive)", "UH"): "(-əxy)",
                        ("(purposive)", "NSS"): "-as ~ -əs", ("(purposive)", "SE"): "(-aš ~ iš)"})
TABLE_11 = forms(11, 17, "Table 11: Applicatives across Central Salish",
                 ("redirective", "relational", "indirective"), 600,
                 fixes={("redirective", "NK"): "-ši-t ~ -xyi-t"})
TABLE_12 = marks(12, 18, "Table 12: Clausal negation across Central Salish", ("Type A", "Type B", "Type B’", "Type C"), 370)
TABLE_13 = marks(13, 19, "Table 13: Invariant and alternating independent pronoun systems in Central Salish", ("",), 400)
TABLE_14 = marks(14, 20, "Table 14: Use of 1st- and 2nd- person independent pronouns as arguments in Central Salish",
                 ("free use of independent pronouns as arguments",), 640)
TABLE_15 = forms(15, 20, "Table 15: Form of the clefting predicate and the 3rd-person independent pronoun in Central Salish",
                 ("clefting particle", "3rd-person independent pronoun"), 200, top=270,
                 fixes={("3rd-person independent pronoun", "NK"): "ƛ̓u", ("3rd-person independent pronoun", "UH"): "ƛ̓a",
                        ("3rd-person independent pronoun", "SQ"): "— (DEM)", ("3rd-person independent pronoun", "SE"): "niɬ (?) (DEM)",
                        ("3rd-person independent pronoun", "CX"): "— (DEM)"})
TABLE_16 = marks(16, 21, "Table 16: Grammatical variation across Central Salish from a NK perspective", (
    "imperfective auxiliary", "oblique proper noun determiner", "no oblique marker",
    "3 ergative suffix in indicative clauses", "no 3 ergative suffix in subjunctive clauses",
    "3 ergative suffix in nominalized clauses", "*3 > 2", "promotional passive", "SV(O) word order",
    "unmarked VOS word order", "use of *-xi-t redirective", "use of -ni-t rather than -min-t",
    "pattern B negation", "alternating independent pronouns", "free use of independent pronouns as arguments",
    "use of ƛ̓u/ƛ̓a as clefting predicate"), 200)
TABLES = (TABLE_1, TABLE_2, TABLE_3, TABLE_4, TABLE_5, TABLE_6, TABLE_7, TABLE_8, TABLE_9, TABLE_10,
          TABLE_11, TABLE_12, TABLE_13, TABLE_14, TABLE_15, TABLE_16)

# Figure 1, the family tree of Central Salish, a node a row with its branch in the gloss.
FIGURE = [("Figure 1", AUTHORS, "note", "Figure 1: Central Salish", "page 3, the caption of the tree")]
for _node, _under in (("*Proto-Central Salish", "the root, carrying footnote 3"),
                      ("North Georgia", "under *Proto-Central Salish"), ("South Georgia", "under *Proto-Central Salish"),
                      ("Puget", "under *Proto-Central Salish"), ("Comox-Sliammon", "under North Georgia"),
                      ("Pentlatch", "under North Georgia"), ("Sechelt", "under North Georgia"),
                      ("Squamish", "under South Georgia"), ("Nooksack", "under South Georgia, in bold italics"),
                      ("Halkomelem", "under South Georgia, with Straits"), ("Straits", "under South Georgia, with Halkomelem"),
                      ("Northern Straits", "under Straits"), ("Klallam", "under Straits"),
                      ("Lushootseed", "under Puget"), ("Twana", "under Puget")):
    FIGURE.append(("Figure 1", AUTHORS, "language", _node, "page 3, a node of the tree, %s" % _under))

# Table 16 is a row of its own in the draft, and Tables 1 and 4 lines of their own.
REMOVE = [(one[0], one[3]) for one in DRAFT if one[2] == "note" and one[3].startswith("Table 16: ")]
REMOVE_WHERE = r"^Table [14] line"
# The table cells the engine offered as candidates that no prose sentence prints; the table rows
# read again hold them.
REMOVE += [("§3.2.1", form) for form in ("=Ø", "-əs")]
REMOVE += [("§3.5.1", form) for form in ("-alikʷ", "-áls", "-im̓", "-ʔəm", "-il̓", "-íl", "-əl", "-iʔ", "-agʷil", "-ŋ")]
REMOVE += [("§3.5.2", form) for form in ("-(ə)t", "-dxʷ", "-nəxʷ", "-ləxʷ", "-naxʷ", "-(n)xʷ", "-taxʷ", "-stxʷ",
                                         "-sxʷ", "-ləs", "-əxy", "-əs", "iš", "aš")]
REMOVE += [("§3.5.3", form) for form in ("-ši-t", "-mə-t", "-ŋi-t")]
REMOVE += [("§3.7.3", "hiɬ"), ("§4", "ƛ̓u/ƛ̓a")]

SPLIT = [
    ("front", "Henry Davis University", {"where": "title", "kind": "title", "gloss": "page 1, the title, its star the acknowledgement footnote's"}, {}),
    ("front", "Abstract:", {"where": "title", "gloss": "page 1, the author and his affiliation"}, {"gloss": "page 1, the abstract"}),
    ("§2", "The division in Figure 1", None, {}),
    ("§3.1", "There are four patterns.", None, {}),
    ("§3.2.1", "Four types of clause", None, {}),
    ("§3.2.2", "Here NK is part of a core", None, {}),
    ("§3.3.1", "As can be seen in the table, the distribution", None, {}),
    ("§3.3.2", "Table 6: The general", {}, {}),
    ("§3.3.2", "As the table shows, loss", None, {}),
    ("§3.4.1", "As the table shows, subject-initial", None, {}),
    ("§3.4.1", "Table 8 shows the distribution", {"kind": "heading", "gloss": "page 13, the heading, numbered 3.4.1 on the page"}, {}),
    ("§3.4.1", "Table 8: Distribution", {}, {}),
    ("§3.4.1", "There are three attested patterns", None, {}),
    ("§3.5.1", "All of the intransitive markers", None, {}),
    ("§3.5.2", "Of the five transitivizers", None, {}),
    ("§3.5.3", "Table 11: Applicatives", {}, {}),
    ("§3.5.3", "Looking first at the redirectives", None, {}),
    ("§3.6", "The distribution of Type B is disjoint", None, {}),
    ("§3.7.1", "The pattern shown here", None, {}),
    ("§3.7.2", "As the table shows, this development", None, {}),
    ("§3.7.3", "Table 15: Form of", {}, {}),
    ("§3.7.3", "In the form of the clefting predicate", None, {}),
]

PS = "Proto-Salish"
IS = "Interior Salish"
CS = "Central Salish"
FORMS = {
    "lə=": ("cited affix", LU, "the progressive marker, a proclitic by Beck (2018)"),
    "ʔay": ("cited form", NK, "the imperfective auxiliary"),
    "ʔe:y": ("cited form", UH, "the intransitive aspectual verb Galloway glosses in UH, the source he gives for ʔay"),
    "ʔux̌ʷ": ("cited form", NK, "the prospective auxiliary, from the verb ‘go’, borrowed from LU"),
    "ƛ̓=": ("cited affix", NK, "the oblique proper noun determiner of NK, HL and SQ"),
    "tɬ=": ("cited affix", "Northern Straits Salish", "the oblique determiner, tɬ= ~ ƛ̓="),
    "(ʔ)ə=": ("cited affix", CS, "the general oblique marker of most of CS"),
    "ʔə=": ("cited affix", NK, "oblique ʔə=, of which the NK corpus has no case"),
    "-əls": ("cited affix", NK, "the active intransitive, -als ~ -əls in NK, HL and NSS"),
    "-iyiš": ("cited affix", "Comox", "the autonomous suffix, in footnote 20"),
    "-(ə)m": ("cited affix", CS, "the middle suffix, whose glottalized version the northern languages use for the active intransitive"),
    "ikʷ": ("cited form", LU, "the increment [ikʷ] of the LU active intransitive"),
    "-ílx": ("cited affix", IS, "the autonomous, -ílx ~ -ləx, with a lexical reflexive meaning"),
    "-ləx": ("cited affix", IS, "the autonomous, -ílx ~ -ləx"),
    "-wíl̓x": ("cited affix", IS, "the developmental, a lexically stressed suffix with an inchoative meaning"),
    "gʷ": ("notation", AUTHORS, "the initial [w] ~ [gʷ] of the autonomous suffix in NK and LU"),
    "š": ("notation", AUTHORS, "the segment [š]"),
    "-txʷ": ("cited affix", NK, "the causative"),
    "-stəxʷ": ("cited affix", PS, "the causative *-stəxʷ, whose [s] NK deletes"),
    "-nəs": ("cited affix", PS, "the relic transitivizer *-nəs of the South Georgia languages"),
    "-aš": ("cited affix", PS, "the purposive transitivizer *-aš ~ -iš"),
    "-iš": ("cited affix", PS, "the purposive transitivizer *-aš ~ -iš"),
    "-n(ə)s": ("cited affix", NK, "the relic transitivizer of (20)"),
    "-(a)š": ("cited affix", NK, "the purposive as a less UH-influenced NK would pronounce it, in footnote 23"),
    "-əɬc-t": ("cited affix", HL, "the redirective, an innovation"),
    "-əs-t": ("cited affix", HL, "the redirective, an innovation"),
    "-ʔəm-t": ("cited affix", "Sechelt and Comox", "the redirective of the North Georgia region"),
    "-min̓-t": ("cited affix", SQ, "the relational, marginal in SQ"),
    "min̓-t": ("cited affix", SQ, "three stems with min̓-t in Kuipers (1967:78)"),
    "diɬ": ("cited form", LU, "the clefting predicate"),
    "ƛ̓u": ("cited form", NK, "the clefting predicate and third-person independent pronoun"),
    "ƛ̓a": ("cited form", HL, "the clefting predicate, ƛ̓u after the u → a shift"),
    "niɬ": ("cited form", CS, "the clefting particle of the languages with a reflex of PS *niɬ"),
    "cədiɬ": ("cited form", LU, "the third-person independent pronoun"),
    "cəniɬ": ("cited form", PS, "the third-person independent pronoun, reconstructed by Newman (1977)"),
    "cəníɬ": ("cited form", PS, "the third-person independent pronoun"),
    "wə-": ("cited affix", NK, "the “perfective” prefix of Louisa George's grammar, cognate with LU u-"),
    "-nwéɬən": ("cited affix", TH, "the non-control intransitive morpheme"),
    "-waɬən": ("cited affix", NK, "the first-person plural object marker"),
    "-nwaɬən": ("cited affix", PS, "*-nwaɬən, which became an object marker in NK"),
    "ƛ̓uʔ": ("cited form", TH, "the exclusive particle, ‘so, just, but, only’"),
    "cukʷ": ("cited form", TH, "‘finish’"),
    "/k̓/": ("notation", AUTHORS, "the ejective velar of the PS series */k/ /k̓/ /x/"),
    "/k̓y/": ("notation", AUTHORS, "a palatalized ejective velar of mainland HL"),
    "/č/": ("notation", AUTHORS, "the palato-alveolar affricate"),
    "/č̓/": ("notation", AUTHORS, "the ejective palato-alveolar affricate"),
    "/š/": ("notation", AUTHORS, "the palato-alveolar fricative"),
    "ʔiɬ": ("cited form", NK, "the auxiliary"),
    "ʔeyɬ": ("cited form", TH, "the adverb ‘now, next, then’"),
    "ʔayɬ": ("cited form", LI, "the adverb ‘now, next, then’"),
    "ʔi": ("cited form", UH, "the common auxiliary Galloway derives ʔiɬ from"),
    "uɬ": ("cited affix", UH, "the past tense enclitic =uɬ"),
    "Sḵwxwúmesh": ("language", AUTHORS, "Squamish, in the title of Bar-el 2005"),
    "Xweysás": ("cited form", LI, "in the title of the volume Davis 2012 stands in, Wa7 Xweysás i Nqwal’úttensa i Ucwalmícwa"),
    "Nqwal’úttensa": ("cited form", LI, "in the title Wa7 Xweysás i Nqwal’úttensa i Ucwalmícwa"),
    "Ucwalmícwa": ("cited form", LI, "in the title Wa7 Xweysás i Nqwal’úttensa i Ucwalmícwa"),
    "St’át’imc": ("language", AUTHORS, "in a reference title"),
    "Nqwal’uttenlhkálha": ("cited form", LI, "the title of the Upper St’át’imcets dictionary in preparation"),
    "St’át’imcets": ("language", AUTHORS, "St’át’imcets, Lillooet"),
    "ʔayʔaǰuθəm": ("language", AUTHORS, "Comox-Sliammon, in the title of Davis and Huijsmans 2017"),
    "Nɬeʔkepmxcín": ("language", AUTHORS, "Thompson River Salish, in the title of Koch and Zimmermann 2009"),
    "nɬeʔkepmxcín": ("language", AUTHORS, "Thompson River Salish"),
    "SENĆOŦEN": ("language", AUTHORS, "Saanich, in the title of Montler 2018"),
    "Honoré": ("name", AUTHORS, "Honoré Watanabe, in a reference entry"),
}
# The engine's candidates with a parenthesis or a footnote digit run on, or a slash left over from
# the phonemes of footnote 33, each held again by a row of its own.
DROP = ("(ʔay).6", "ʔay).7", "/ʔeyɬ", ">*čshift", "k̓/")

SET = {
    ("front", "Halq’eméylem/Upriver"): {"form": "Halq’eméylem", "kind": "language", "who": AUTHORS, "gloss": "page 1, Upriver Halkomelem"},
    ("§1", "Halq’eméylem/Upriver"): {"form": "Halq’eméylem", "kind": "language", "who": AUTHORS, "gloss": "page 2, Upriver Halkomelem"},
    ("front", "nɬeʔkepmxcín/Thompson"): {"form": "nɬeʔkepmxcín", "kind": "language", "who": AUTHORS, "gloss": "page 1, Thompson River Salish"},
    ("§5", "nɬeʔkepmxcín/Thompson"): {"form": "nɬeʔkepmxcín", "kind": "language", "who": AUTHORS, "gloss": "page 22, Thompson River Salish"},
    ("§3", "ʔayʔaǰuθəm/Comox-Sliammon"): {"form": "ʔayʔaǰuθəm", "kind": "language", "who": AUTHORS, "gloss": "page 4, Comox-Sliammon, in footnote 4"},
    ("§3.3.1", "SENĆOTEN/Saanich"): {"form": "SENĆOTEN", "kind": "language", "who": AUTHORS, "gloss": "page 11, Saanich"},
    ("§3.3.2", "St’át’imcets/Lillooet"): {"form": "St’át’imcets", "kind": "language", "who": AUTHORS, "gloss": "page 11, Lillooet, in footnote 17"},
    ("§3.5.2", "-nəs"): {"form": "*-nəs"},
    ("§3.5.2", "-stəxʷ"): {"form": "*-stəxʷ"},
    ("§3.5.2", "-aš"): {"form": "*-aš"},
}
# A translation closing on a footnote digit, ‘...if my pay parents agree to it.’11.
for _where, _who, _kind, _form, _gloss in DRAFT:
    _mark = re.match(r"^(.*’)(\d{1,2})$", _form)
    if _kind == "translation" and _mark:
        SET[(_where, _form)] = {"form": _mark.group(1), "gloss": "%s, carries footnote %s, written here without its digit" % (_gloss, _mark.group(2))}
# The translation of (30) closes on its source tag, which finish.py does not part from a literal
# gloss in parentheses.
_THIRTY = "‘I only see a hand there.’ (literally ‘It’s only a hand that I see there.’)"
SET[("(30) line 3", _THIRTY + " (LI: CA)")] = {"form": _THIRTY}
# (ii) of §3.2.3, which the engine read as an example line under footnote 13.
for _where, _who, _kind, _form, _gloss in DRAFT:
    if _where == "footnote 13 (ii) line 1":
        SET[(_where, _form)] = {"where": "§3.2.3", "form": "(ii) " + _form, "gloss": "page 10"}

# Two paragraphs the engine broke at a number in parentheses, reading the rest as an example: the
# paragraph after (28), broken at "(see examples (3)", and the one before (26), at "(2004)". Each
# is one row again.
_OPENS = [one for one in DRAFT if one[0] == "§5.2" and one[3].startswith("This opens up the possibility")][0]
_REST = [one for one in DRAFT if re.match(r"^\(3\) line \d$", one[0]) and "page 25" in one[4]]
_SECOND = [one for one in DRAFT if one[0] == "§3.7.2" and one[3].startswith("The second parameter concerns")][0]
_WILTSCHKO = [one for one in DRAFT if one[0] == "(2004) line 1"][0]
REMOVE += [(one[0], one[3]) for one in [_OPENS, _SECOND, _WILTSCHKO] + _REST]
MERGED = [
    (("§5.2", "ʔiɬ"), ("§5.2", AUTHORS, "note", _OPENS[3] + " (3)" + _REST[0][3] + " " + " ".join(one[3] for one in _REST[1:]),
                       "pages 24 and 25")),
    (("§3.7.2", "3.7.2 Free use..."), ("§3.7.2", AUTHORS, "note", _SECOND[3] + " (2004) " + _WILTSCHKO[3], "page 19")),
]

# The examples of other languages, named with their sources on the page.
# The engine gave the translations of (29), (33) and (34) to their source tags; the English is the
# author's.
WHO_MATCH = ((r"^(TH|LI): ", r"^translation$", AUTHORS, None),)
WHO_RULES = []
for _example, _language, _source in (("(4)", UH, "Galloway's UH, which the text names"),
                                     ("(8a)", UH, "UH, from Galloway 1993a:187"), ("(8b)", UH, "UH, from Galloway 1993a:187"),
                                     ("(9a)", SQ, "SQ, from Kuipers 1967:89"), ("(9b)", SQ, "SQ, from Kuipers 1967:89"),
                                     ("(29)", TH, "TH, from Koch & Zimmermann 2009:242"),
                                     ("(30)", LI, "LI, from the consultant CA"), ("(33)", TH, "TH, from Thompson & Thompson 1996:7"),
                                     ("(34)", LI, "LI, from Davis et al. in prep.")):
    for _kind in ("transcription", "segmentation", "gloss"):
        WHO_RULES.append((r"^%s line" % re.escape(_example), _kind, r".", _language, _source))

ADD = tuple(
    [(("title", "Henry Davis University..."), ("title", AUTHORS, "name", "Henry Davis", "author, University of British Columbia"))]
    + MERGED
    + [(("(30) line 3", "‘I only see a hand there.’ (literally..."),
        ("(30) line 3", AUTHORS, "citation", "(LI: CA)", "the tag or source at the right of the translation"))]
    + chained(("§2", "In Salish historical linguistics..."), FIGURE)
    + chained(("§3.1", "While perfect is uniformly unmarked..."), TABLE_1)
    + chained(("§3.2.1", "Table 2 gives the distribution..."), TABLE_2)
    + chained(("§3.2.2", "The distribution of the *3 > 2 ban..."), TABLE_3)
    + chained(("§3.2.3", "The distribution of promotional and non-promotional passive..."), TABLE_4)
    + chained(("§3.3.1", "Table 5 shows the distribution..."), TABLE_5)
    + chained(("§3.3.2", "The form and distribution of the general oblique marker..."), TABLE_6)
    + chained(("§3.4.1", "The distribution of pre-predicative subjects..."), TABLE_7)
    + chained(("§3.4.1", "Table 8 shows the distribution..."), TABLE_8)
    + chained(("§3.5.1", "Table 9 compares intransitive markers..."), TABLE_9)
    + chained(("§3.5.2", "Table 10 shows the basic..."), TABLE_10)
    + chained(("§3.5.3", "There are three main applicative..."), TABLE_11)
    + chained(("§3.6", "The distribution of these patterns..."), TABLE_12)
    + chained(("§3.7.1", "The distribution of these systems..."), TABLE_13)
    + chained(("§3.7.2", "The distribution of this pattern..."), TABLE_14)
    + chained(("§3.7.3", "Table 15 shows the form..."), TABLE_15)
    + chained(("§4", "Table 16 summarizes..."), TABLE_16)
    + [(None, ("all", AUTHORS, "notation", "*", "a reconstructed form, or with a person sequence an ungrammatical one"))]
)

WHOSE = (
    "The examples are Nooksack, from the corpus of George Swanaset, Sindick Jimmy and Louisa George, "
    "but for (4) and (8), Upriver Halkomelem from Galloway, (9), Squamish from Kuipers, (29) and (33), "
    "nɬeʔkepmxcín, and (30) and (34), St’át’imcets, each with its source on the page. The cells of the "
    "tables that are affixes or particles are given to the language of their column, and the prose's "
    "forms to the language it names; the proto-forms are the author's reconstructions."
)

LETTERS = (
    "Each Nooksack example sets Galloway's practical orthography over an Americanist line, with ʔ, "
    "ɬ, ƛ̓, x̌, xʷ, č, š and θ; the stressed vowel carries an acute accent. The tables and the prose "
    "write affixes with a hyphen and clitics with an equals sign, and the proto-forms with a star."
)

PAGE_NOTES = (
    "The text layer ran each of the sixteen tables into the prose; the tables are read from the glyph "
    "positions a cell a column. It drops the caron of x̌ and sets a space where it stood, in twelve "
    "examples, and sets /k̓/ and /ky/ of footnote 33 with a space inside the slashes; each is read off "
    "the page at 250 dpi."
)
