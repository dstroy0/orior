# Context for Mellesmoen, Innovations on Classic Salish Morphology: Glottal Stop Codas in Nuxalk and
# Halq̓eméylem. The examples are St’át’imcets (4, 5), Nuxalk (6 to 10, 16 to 18) and two dialects of
# Halkomelem, Halq̓eméylem (19, 22, 26) and hən̓q̓əmin̓əm̓ (20, 21, 27, 29). The five tableaux are
# rebuilt one candidate a row with its violation marks beside it; the definitions (1), (2), (11) to
# (13), (25) and (28), which the text layer ran into the prose, are cut out of the draft rows.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import PRIVATE  # noqa: E402
from workdir import WORK  # noqa: E402
import residue  # noqa: E402

STEM = "MellesmoenICSNL60_BCHk"
AUTHORS = "Gloria Mellesmoen"
NX = "Nuxalk"
ST = "St’át’imcets"
HQ = "Halq̓eméylem"
HN = "hən̓q̓əmin̓əm̓"
MP = "McCarthy & Prince 1994"
# The glyph the page draws as a pointing hand at the winning candidate of a tableau.
HAND = ""

TITLE = "Innovations on Classic Salish Morphology: Glottal Stop Codas in Nuxalk and Halq̓eméylem"
BYLINE = "Gloria Mellesmoen, University of British Columbia"

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


def header_16():
    """The input and constraint columns of tableau (16), which the page sets over nine lines, a
    constraint name broken after its hyphen."""
    start = PAGE.index(line("(16) Stratum 1"))
    text = ""
    for one in PAGE[start + 1:start + 10]:
        text += one if text.endswith("-") else " " + one
    return text.strip()


def translated(where, translation, source, page, note=""):
    """A translation row and the source at its right, as finish.py splits one."""
    return [(where, source, "translation", translation, "page %d%s" % (page, note)),
            (where, AUTHORS, "citation", "(%s)" % source, "the tag or source at the right of the translation")]


R17 = draft_form("§2", "The relative markedness")
R72 = draft_form("§3.1", "`fir tree needles")
R310 = draft_form("§4.2", "A high-ranked constraint")
TRIPLES = [one for one in DRAFT if re.match(r"^\(5[a-e]\) line 1$", one[0])]
PROSE_5E = draft_form("(5e) line 2", "Within the Generalised") + " " + draft_form("§2.1", "same morphological classification")

FORMS = {
    HQ: ("language", AUTHORS, "Upriver Halkomelem, whose imperfectives the paper analyses in Section 4"),
    HN: ("language", AUTHORS, "Downriver Halkomelem, Musqueam, Suttles 2004"),
    "Hul’q’umi’num’": ("language", AUTHORS, "Island Halkomelem, Hukari 1984"),
    ST: ("language", AUTHORS, "Lillooet, whose diminutive reduplication and triplication Section 2.1 cites"),
    "St’´at’imcets": ("language", AUTHORS, "St’át’imcets, in the title of Davis & Mellesmoen 2023, its acute set as a spacing accent"),
    "St’át’imc": ("name", AUTHORS, "in the Upper St’át’imc Language Authority, Davis et al. in prep"),
    "St’´at’imc": ("name", AUTHORS, "St’át’imc, in the title of Davis et al. in prep, its acute set as a spacing accent"),
    "Nxaʔamxcin": ("language", AUTHORS, "Moses-Columbia Salish, in the title of Czaykowska-Higgins 1993"),
    "Nqwal’uttenlhk´alha": ("cited form", ST, "the title of the dictionary of Davis et al. in prep, its acute set as a spacing accent"),
    "Bermúdez-Otero": ("name", AUTHORS, "Ricardo Bermúdez-Otero, cited for GNLA and Stratal OT"),
    "Gonzàlez": ("name", AUTHORS, "the editor of the proceedings in McCarthy & Prince 1994"),
    "ʔ": ("notation", AUTHORS, "the glottal stop"),
    "/ʔ/": ("notation", AUTHORS, "the glottal stop phoneme"),
    "ə": ("notation", AUTHORS, "schwa"),
    "ɛ": ("notation", AUTHORS, "a vowel Galloway 2009 hears in ʔə́ɬtəl, footnote 11"),
    "əʔ": ("notation", AUTHORS, "the sequence hən̓q̓əmin̓əm̓ ablaut avoids in a syllable"),
    "x̣": ("notation", AUTHORS, "the Nuxalk fricative Nater 1990 writes x, footnote 6"),
    "xʸ": ("notation", AUTHORS, "Galloway 2009's letter for [x], footnote 12"),
    "μ": ("notation", AUTHORS, "a mora"),
    "σμμ": ("notation", AUTHORS, "a heavy syllable, Zimmermann 2017"),
    "ʔə": ("notation", AUTHORS, "Zimmermann 2017's constraint *ʔə, against a syllable [ʔə]"),
    "MAX-μ": ("notation", AUTHORS, "a constraint, defined in (12)"),
    "ʔ]σ": ("notation", AUTHORS, "the constraint *ʔ]σ against a coda glottal stop, defined in (11)"),
    "ʔ]σS̓": ("notation", AUTHORS, "the constraint *ʔ]σS̓, defined in (28)"),
    "[ʔ]-epenthesis": ("notation", AUTHORS, "epenthesis of a glottal stop"),
    "/ʔ/-epenthesis": ("notation", AUTHORS, "epenthesis of a glottal stop"),
    "/ʔ/-infixation": ("notation", AUTHORS, "infixation of a glottal stop"),
    "/ʔiʔk̓ʷ/": ("cited form", HQ, "page 13, the input to the second stratum of s-ʔiːk̓ʷ ‘lost’"),
    "héːy̓": ("cited form", HN, "page 14, ‘be making a canoe’"),
    "híːlt": ("cited form", HN, "page 12, ‘roll it over’, Suttles 2004: 147"),
    "híʔəl̓t": ("cited form", HN, "page 12, ‘be rolling it over’, Suttles 2004: 147"),
    "ʔáːm̓əst": ("cited form", HN, "page 15, ‘be giving it to them (sg.)’, Suttles 2004: 148"),
    "ʔáməst": ("cited form", HN, "page 15, the perfective of ʔáːm̓əst, Suttles 2004: 148"),
    "ʔiʔəməx": ("cited form", HQ, "page 15, a starred candidate in Zimmermann 2017"),
    "ʔiːməx": ("cited form", HQ, "page 15, ‘walking’, Zimmermann 2017"),
    "kasmiw": ("cited form", NX, "page 6, ‘golden eagle’, footnote 5, Nater 1990: 44"),
}

# English and the halves of words a line end or a bracket broke.
DROP = ("non-[ə", "α", "f(α", "Max-μ.9", "/ʔ/-", "/ʔ/-initial", "ʔ-")

INITIALS = {}

SPLIT = (
    ("front", "Gloria Mellesmoen University", None, {}),
    ("front", "Abstract:", None, {}),
    ("§2", "(1) REDk", {}, {"where": "(1) line 1", "who": MP}),
    ("(1) line 1", "(McCarthy & Prince 1994, as cited",
     {"form": cut(R17, "REDk", " (McCarthy & Prince 1994, as cited"),
      "gloss": "page 2, the definition of a reduplicative morpheme, quoted"},
     {"who": AUTHORS, "kind": "citation", "gloss": "the source of the definition"}),
    ("(1) line 1", "Two types of morphological", {}, {"where": "§2", "kind": "note", "gloss": "page 2"}),
    ("§2", "(2) ROOT-FAITH", {}, {"where": "(2) line 1", "who": MP}),
    ("(2) line 1", "Four different constraint",
     {"form": "ROOT-FAITH >> AFFIX-FAITH", "gloss": "page 2, the universal ranking of root and affix faithfulness"},
     {"where": "§2", "who": AUTHORS, "gloss": "page 2"}),
    ("(3d) line 1", "1 I cite Urbanczyk", {}, {"where": "footnote 1", "gloss": "page 2, footnote"}),
    ("footnote 1", "In fact, a central assumption", {}, {"where": "§2", "gloss": "page 3"}),
    ("(15e) line 1", "Tableau (16) shows", {}, {"where": "§3.2", "gloss": "page 8"}),
    ("§3.2", "8 I use DEP-ONSET", {}, {"where": "footnote 8", "gloss": "page 8, footnote"}),
    ("§4.2", "(28) *ʔ]σS̓:", {}, {"where": "(28) line 1"}),
    ("(28) line 1", "The tableau in (29) shows",
     {"form": cut(R310, "*ʔ]σS̓: Assign", " The tableau in (29)"), "gloss": "page 14, the constraint *ʔ]σS̓"},
     {"where": "§4.2", "gloss": "page 14"}),
)

REMOVE = (
    ("front", "Innovations on Classic Salish Morphology:"),
    ("§3.1", "s-xʷpa<p>ni<ː>ɬ-i"), ("§3.1", "sxʷpaniɬ"), ("§3.1", "`fir tree needles..."),
    ("§2.1", "same morphological classification..."),
    ("(17e) line 3", "compensatory lengthening from..."), ("(17e) line 4", "analysis within a Parallel OT..."),
    ("§3.2", "most straightforward analysis..."),
    ("(26e) line 3", "in codas, as shown in (27)."),
    ("(29c) line 3", "analysed as [ʔ]-infixation..."), ("§4.2", "Urbanczyk 2020); they share..."),
    ("§4.2", "(2017: 220); she analyses..."),
)
# The triplications of (5), each set on one line with its translation and source, and the lines of
# tableau (16)'s header, which the rows below give again whole.
REMOVE_WHERE = r"^\(5[a-e]\) line|^\(16\) line ([2-9]|10)$"

EXAMPLES_OF = ((r"^\(4[a-c]\) line", ST, "a St’át’imcets example"),
               (r"^\((19[a-e]|22[ab]|26[a-e])\) line", HQ, "a Halq̓eméylem example"),
               (r"^\((20[a-e]|21[ab]|27[a-e]|29[a-c])\) line", HN, "a hən̓q̓əmin̓əm̓ example"))
WHO_RULES = tuple((where, kind, ".", who, gloss) for where, who, gloss in EXAMPLES_OF
                  for kind in ("segmentation", "gloss", "transcription"))

KIND_RULES = (
    (r"^\(3[a-d]\) line 1$", ".", ".", "note", AUTHORS, "a ranking of faithfulness and markedness"),
    (r"^\(15[a-e]\) line", ".", ".", "note", AUTHORS, "a constraint definition"),
    (r"^\((14|7b)\) line", "phonemic", ".", "phonemic", NX, "an input or output of the stratum"),
    (r"^\(24[ab]\) line", "phonemic", ".", "phonemic", "Halkomelem", "an input or output of the stratum"),
    (r"^\((16|17|26|27|29)\) line 1$", ".", ".", "note", AUTHORS, "the caption of the tableau"),
    (r"^\((17|26|27|29)\) line 2$", ".", ".", "note", AUTHORS, "the input and the constraint columns of the tableau"),
    (r"^\((19|30)\) line 1$", ".", ".", "note", AUTHORS, "the caption"),
    (r"^\(30[ab]\) line [12]$", ".", ".", "note", AUTHORS, "a ranking at the second stratum"),
    (r"^\(2[0-2][a-e]\) line", "note", r"^\((Suttles|Galloway) ", "citation", AUTHORS,
     "the source at the right of the translation"),
)

SET = {
    ("§3.2", "dominates s (Kirchner 2010: 232)."): {"where": "(15a) line 2", "gloss": "page 8, a constraint definition"},
    ("(17e) line 2", draft_form("(17e) line 2", "The reranking")):
        {"where": "§3.2", "who": AUTHORS, "kind": "note", "gloss": "page 9",
         "form": " ".join([draft_form("(17e) line 2", "The reranking"), draft_form("(17e) line 3", "compensatory"),
                           draft_form("(17e) line 4", "analysis within"), draft_form("§3.2", "most straightforward")])},
    ("(26e) line 2", draft_form("(26e) line 2", "In hən̓q̓əmin̓əm̓")):
        {"where": "§4.2", "who": AUTHORS, "kind": "note", "gloss": "page 14",
         "form": draft_form("(26e) line 2", "In hən̓q̓əmin̓əm̓") + " " + draft_form("(26e) line 3", "in codas")},
    ("(29c) line 2", draft_form("(29c) line 2", "Imperfective morphology")):
        {"where": "§4.2", "who": AUTHORS, "kind": "note", "gloss": "page 15",
         "form": " ".join([draft_form("(29c) line 2", "Imperfective morphology"),
                           draft_form("(29c) line 3", "analysed as"), draft_form("§4.2", "Urbanczyk 2020); they")])},
    ("(30b) line 3", draft_form("(30b) line 3", "An alternate analysis")):
        {"where": "§4.2", "who": AUTHORS, "kind": "note", "gloss": "page 15",
         "form": draft_form("(30b) line 3", "An alternate analysis") + " " + draft_form("§4.2", "(2017: 220); she")},
    ("(14) line 1", "a. First Stratum: MAX-[C.G.], MAX-μ >> *ʔ]σ"):
        {"where": "(14a) line 1", "form": "First Stratum: MAX-[C.G.], MAX-μ >> *ʔ]σ"},
    ("§4.2", "(McCarthy 1995, as cited in Kager 1999: 269)."):
        {"where": "(25) line 4", "kind": "citation", "gloss": "the source of the definition"},
}
ADD_ROWS = []
_translation = {}
for _where, _who, _kind, _form, _gloss in DRAFT:
    # The definitions (11) to (13) and (25), set in the prose rows of Section 3.2 and 4.2.
    opened = re.match(r"^\((11|12|13|25)\) (.*)$", _form)
    if opened and _kind == "note":
        SET[(_where, _form)] = {"where": "(%s) line 1" % opened.group(1), "form": opened.group(2),
                                "who": "McCarthy 1995" if opened.group(1) == "25" else AUTHORS,
                                "gloss": "%s, a constraint definition" % _gloss}
    if _where == "§4.2" and _form in ("if α is monomoraic, then f(α) is monomoraic.",
                                      "if α is bimoraic, then f(α) is bimoraic."):
        SET[(_where, _form)] = {"where": "(25) line %d" % (2 if "mono" in _form else 3), "who": "McCarthy 1995",
                                "gloss": "%s, a constraint definition" % _gloss}
    # The rest of (14a) and all of (14b), which the engine numbered (7b).
    if _where == "(14) line 2":
        SET[(_where, _form)] = {"where": "(14a) line 2"}
    if _where in ("(7b) line 1", "(7b) line 2"):
        SET[(_where, _form)] = {"where": _where.replace("(7b)", "(14b)")}
    # A footnote mark set after the closing bracket of the cf. form, (cf. nax̣nx̣ ‘mallard duck’)6.
    marked = re.match(r"^(.*\))(\d{1,2})$", _form)
    if marked and _kind == "segmentation":
        SET[(_where, _form)] = {"form": marked.group(1),
                                "gloss": "%s, carries footnote %s, written here without its digit" % (_gloss, marked.group(2))}
    # The source of a hən̓q̓əmin̓əm̓ or Halq̓eméylem translation, set as a note after it.
    if _kind == "translation" and re.match(r"^\(2[0-2][a-e]\) line", _where):
        _translation[_where] = _form
    source = re.match(r"^\(((?:Suttles|Galloway) \d{4}: \d+)\)$", _form)
    if source and _where in _translation:
        SET[(_where, _translation[_where])] = {"who": source.group(1)}
    # A tableau candidate, the form and the violation marks after it; the winner opens on the hand.
    tableau = re.match(r"^\((16|17|26|27|29)[a-f]\) line 1$", _where)
    if tableau and _kind == "transcription":
        tokens = _form.replace(HAND, " ").split()
        marks = []
        while tokens and tokens[-1] in ("*", "*!"):
            marks.insert(0, tokens.pop())
        language = NX if tableau.group(1) in ("16", "17") else HQ if tableau.group(1) == "26" else HN
        winner = ", the winning candidate, marked on the page with a pointing hand" if HAND in _form else ""
        SET[(_where, _form)] = {"form": " ".join(tokens), "who": language, "gloss": _gloss + winner}
        if marks:
            ADD_ROWS.append(((_where, _form), (_where, AUTHORS, "note", " ".join(marks),
                             "%s, the violation marks of the candidate, * a violation and *! a fatal one, "
                             "under the columns of the header" % _gloss.split(",")[0])))

# The triplications of (5): form, translation, source, and the paragraph after them, which the
# engine read as a gloss line of (5e).
TRIPLE_ROWS = []
for _where, _who, _kind, _form, _gloss in TRIPLES:
    parts = re.match(r"^(.*?) (‘[^’]*’) \((.*)\)$", _form)
    form, note = parts.group(1), ""
    if form.endswith("ál3"):
        form, note = form[:-1], ", carries footnote 3, written here without its digit"
    TRIPLE_ROWS += [(_where, ST, "segmentation", form, "page 4, a St’át’imcets example%s" % note)]
    TRIPLE_ROWS += translated(_where, parts.group(2), parts.group(3), 4)
TRIPLE_ROWS += [("§2.1", AUTHORS, "note", PROSE_5E, "page 4")]

ADD = tuple(
    [(None, ("title", AUTHORS, "title", TITLE, "the paper's title, set over two lines, carrying the star of footnote *")),
     (None, ("title", AUTHORS, "name", "Gloria Mellesmoen", "the author, University of British Columbia")),
     (None, ("§1", AUTHORS, "language", NX, "Bella Coola, whose diminutives the paper analyses in Section 3")),
     (None, ("§3.2", NX, "cited form", "kasmwi", "page 6, footnote 5, the diminutive of kasmiw ‘golden eagle’, without reduplication, Nater 1990: 44")),
     (("(2) line 1", "ROOT-FAITH >> AFFIX-FAITH"),
      ("(2) line 1", AUTHORS, "citation", "(McCarthy & Prince 1994)", "the source of the ranking")),
     ] +
    display(("§2.1", "There is also a triplication..."), TRIPLE_ROWS) +
    ADD_ROWS +
    display("(7a) line 2", translated("(7a) line 3", cut(R72, "`fir", " (Bagemihl"), "Bagemihl 1991: 599", 6) + [
        ("(7b) line 1", NX, "segmentation", cut(R72, "s-xʷpa", " NMLZ"), "page 6"),
        ("(7b) line 2", NX, "gloss", cut(R72, "NMLZ", " `small"), "page 6"),
    ] + translated("(7b) line 3", cut(R72, "`small", " (Nater"), "Nater 1990: 107", 6) + [
        ("§3.1", AUTHORS, "note", cut(R72, "The three exponents"), "page 6"),
    ]) +
    display("(16) line 1", [("(16) line 2", AUTHORS, "note", header_16(),
                             "page 9, the input and the constraint columns of the tableau, set over nine lines")]) +
    display("(20c) line 2", translated("(20c) line 3", "‘be throwing it away", "Suttles 2004: 147", 11,
                                       ", printed without its closing quote")))

WHOSE = (
    "The paper's data are cited from published sources, and each translation's who is the source "
    "its tag names, (Nater 1990: 107) or (Galloway 2009: 10). The St’át’imcets examples (4) and (5) "
    "come from Van Eijk 1997 and 2013, Davis & Mellesmoen 2023 and Davis et al. in prep; the Nuxalk "
    "examples (6) to (9) and (18) from Nater 1978 and 1990 and Bagemihl 1991; the Halq̓eméylem "
    "examples (19) and (22) from Galloway 2009; and the hən̓q̓əmin̓əm̓ examples (20) and (21) from "
    "Suttles 2004. The who of each tier is the language of the example.\n\n"
    "The tableaux (16), (17), (26), (27) and (29) are the author's: each candidate is a row whose who "
    "is the language it is a candidate form of, and its violation marks are a note of the author's "
    "beside it. The definition (1) and the ranking (2) are McCarthy and Prince's, and (25) is "
    "McCarthy's as cited in Kager 1999. The other constraints, the rankings, the lexical entries (10) "
    "and (23), the prose, the headings and the notes carry Gloria Mellesmoen."
)

LETTERS = (
    "The Nuxalk, St’át’imcets and Halkomelem forms are in the Americanist orthography of their "
    "sources, with the glottalization of a consonant as U+0313, the dot below of the St’át’imcets "
    "retracted consonants as U+0323, a syllabic sonorant with U+0329, and length as ː, U+02D0. An "
    "infix and a copied segment stand in angle brackets, <ː> for length and <ʔ> for a glottal stop. "
    "Moras are μ and syllables σ; the constraint names are small capitals on the page and ASCII "
    "capitals in the text layer."
)

PAGE_NOTES = (
    "The page marks the winning candidate of each tableau with a pointing hand, U+F046 in the text "
    "layer; the table drops it and says so in the candidate's gloss. The text layer sets a space after "
    "a glottalized letter, k̓ ʷ for k̓ʷ, and the table closes it. The page quotes the translations of (7) "
    "with a backtick and a straight quote, `small deer', gives (20c)'s translation no closing quote, "
    "and prints k̓̓ with two commas above in (19d); each is kept."
)
