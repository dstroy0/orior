# Context for Davis, A How-To Guide to Control Infinitives in St'át'imcets.
# The text layer spaces inside words; the page text closes them from the glyph positions. The
# English examples and the displays are read off the page text below by the words each line
# opens with.
import os
import re

from workdir import PRIVATE  # noqa: E402

AUTHORS = "Henry Davis"
L = "St’át’imcets"
C = "ʔayʔaǰuθəm"

TITLE = "A How-To Guide to Control Infinitives in St’át’imcets"
BYLINE = "Henry Davis, The University of British Columbia"

with open(os.path.join(PRIVATE, "pagetext",
                       "DavisICSNL60.txt"), encoding="utf-8") as handle:
    # The Symbol font's codes read as residue.CORRECTIONS reads them.
    PAGE = [" ".join(line.replace("", "*").replace("", "λ").split())
            for line in handle.read().split("\n")]


def line(opening, after=0):
    """The page line opening on these words, the first after line number after."""
    for number, text in enumerate(PAGE):
        if number >= after and text.startswith(opening):
            return text
    raise SystemExit("no page line opens with %s" % opening)


def display(anchor, rows):
    """ADD entries for rows (where, who, kind, form, gloss), each placed after the one before it,
    the first after anchor."""
    added = []
    for row in rows:
        added.append((anchor, row))
        anchor = (row[0], row[3])
    return added


ENGLISH = "an English example, the author's"
AT_39 = PAGE.index(line("(39) a."))
AT_49 = PAGE.index(line("(49) a."))

FORMS = {
    "St’át’imcets": ("language", AUTHORS, "the language of the paper, a.k.a. Lillooet, ISO 639-3 lil, Northern Interior Salish"),
    "nɬeʔkepmxcín": ("language", AUTHORS, "a.k.a. Thompson (River) Salish, ISO 639-3 thp, the other Northern Interior Salish language with infinitives"),
    "Nɬeʔkepmxcín": ("language", AUTHORS, "in the title of Hall 2023"),
    "ʔayʔaǰuθəm": ("language", AUTHORS, "a.k.a. Comox-Sliammon, ISO 639-3 coo, Central Salish, where infinitives were found recently"),
    "St’átimcets": ("language", AUTHORS, "in the title of Roberts 1994, printed without its second apostrophe"),
    "St’át’imc": ("name", AUTHORS, "the Upper St’át’imc Language Authority, an author of the dictionary Davis et al. in prep."),
    "English-St’át’imcets": ("note", AUTHORS, "in the title of the dictionary Davis et al. in prep."),
    "St’át’imcets-": ("note", AUTHORS, "St’át’imcets-English, in the title of the dictionary Davis et al. in prep., broken at the hyphen"),
    "Qwa7yán’ak": ("name", AUTHORS, "the St’át’imcets name of Carl Alexander, the consultant"),
    "ucwalmícwts": ("language", AUTHORS, "the language of the people of the land, St’át’imcets, as the translations write it"),
    "Brugè": ("name", AUTHORS, "Laura Brugè, an editor in Constantini and Laskova 2009"),
    "Sqwéqwel": ("cited form", L, "‘stories’, Sqwéqwel’ in the title of Alexander 2016, the page's apostrophe after it cut by the text layer"),
    "múta7": ("cited form", L, "‘and’, in the title of Alexander 2016"),
    "Múta7": ("cited form", L, "‘and’, in the title of Alexander 2025"),
    "Sqwéqwel’s": ("cited form", L, "in the title of Edwards et al. 2017"),
    "Skelkekla7lhálha": ("cited form", L, "in the title of Edwards et al. 2017"),
    "Nqwal’uttenlhkálha": ("cited form", L, "the title of the dictionary Davis et al. in prep."),
    "λ-operator": ("notation", AUTHORS, "the operator that binds PRO, footnote 14; the Symbol font's lambda, U+F06C in the text layer"),
}

# The lambda of the readings of (49), read as a form.
DROP = ("λx[x",)

SPLIT = (
    ("front", "Henry Davis The University", None, {}),
    ("front", "Abstract:", None, {}),
    ("§3.4", "All of these verbs select", None, {}),
    # The end of footnote 9 sits at the foot of page 14: the translation of its (ii) and a
    # sentence.
    ("§4.1", "‘I’m learning how to ride a pony.’", {},
     {"where": "footnote 9 (ii) line 3", "kind": "translation",
      "gloss": "page 14, at the foot of the page, the translation of footnote 9's (ii) carried over from page 13"}),
    ("footnote 9 (ii) line 3", "If a covert version", {},
     {"where": "footnote 9", "kind": "note", "gloss": "page 14, the end of footnote 9, carried over from page 13"}),
    ("§4.3", "This is exactly the same contrast", None, {}),
    ("§4.3", "(54) The OC signature", {}, None),
)

REMOVE = (
    ("§4.3", "(49) a. Only Peteri claimed [that hei was the winner]."),
    ("§4.3", "(i) Only Peter λx[x claimed x is the winner] (ii) Only Peter λx[x claimed Peter is the winner]."),
    ("(49b) line 1", "Only Peteri claimed [PROi to be the winner]."),
    ("(49b) line 2", "(i) Only Peter λx[x claimed x is the winner]"),
    ("(49b) line 3", "(ii) # Only Peter λx[x claimed Peter is the winner]."),
    ("§4.3", "a. The controller(s) X must be (a) co-dependent(s) of S."),
    ("(39b) line 1", "PRO (or part of it)..."),
    ("§4.5", "(66) It is dangerous for babiesi [PROarb to smoke around themi]."),
    ("(67a) line 1", "* Maryi didn’t know..."),
    ("(67b) line 1", "* Suei asked..."),
    ("§4.5", "c. Maryi didn’t know [where one should hide heri]."),
    ("§4.5", "d. Suei asked [what one should buy heri in Rome]."),
)

# The consultant is Carl Alexander, whose are all unattributed examples and judgements, and the
# interviewer is the author.
INITIALS = {"Consultant": "Carl Alexander", "Interviewer": AUTHORS}

WHO_RULES = (
    # The examples of (40), (41) and footnote 9 are ʔayʔaǰuθəm, from Betty Wilson and Molly Harry.
    (r"^(\((40|41)\)|footnote 9 \((i|ii)\)) line", "segmentation", ".", C, "the language of the example, footnote 8"),
    (r"^(\((40|41)\)|footnote 9 \((i|ii)\)) line", "gloss", ".", C, "the language of the example, footnote 8"),
    (r"^(\((40|41)\)|footnote 9 \((i|ii)\)) line", "transcription", ".", C, "the language of the example, footnote 8"),
)

SET = {
    ("(39b) line 2", "Under this definition, non-obligatory control (NOC) is really just the converse of OC: NOC PRO"):
        {"where": "§4.3", "kind": "note", "gloss": "page 21, the first line of a paragraph, its rest in the next row"},
    ("(49b) line 4", "The (a) case with a finite complement clause is ambiguous between readings (i) and (ii). Reading"):
        {"where": "§4.3", "kind": "note", "gloss": "page 18, the first line of a paragraph, its rest in the next row"},
    ("§4.2", "zəwát-ən.13"): {"form": "zəwát-ən", "gloss": "‘know’, transitive, which the consultant corrected (47a) and (48a) to, carries footnote 13, written here without its digit"},
}

ADD = tuple(
    [(None, ("title", AUTHORS, "title", TITLE, "the paper's title")),
     (None, ("title", AUTHORS, "name", "Henry Davis", "the author, The University of British Columbia")),
     (None, ("footnote *", AUTHORS, "name", "Carl Alexander", "Qwa7yán’ak, the consultant, whose are all unattributed St’át’imcets examples and judgements; Alexander 2016 and 2025")),
     (None, ("footnote *", AUTHORS, "name", "Betty Wilson", "provided the ʔayʔaǰuθəm data")),
     (None, ("footnote *", AUTHORS, "name", "Molly Harry", "provided the ʔayʔaǰuθəm data")),
     (None, ("footnote *", AUTHORS, "name", "Daniel Reisinger", "elicited the ʔayʔaǰuθəm data")),
     (None, ("footnote *", AUTHORS, "name", "Marianne Huijsmans", "elicited the ʔayʔaǰuθəm data")),
     (None, ("footnote *", AUTHORS, "name", "Lisa Matthewson", "gave feedback")),
     (None, ("footnote *", AUTHORS, "name", "Salish Working Group", "gave feedback")),
     (None, ("§1", AUTHORS, "language", "Lillooet", "another name for St’át’imcets")),
     (None, ("§1", AUTHORS, "language", "Thompson (River) Salish", "another name for nɬeʔkepmxcín")),
     (None, ("§1", AUTHORS, "language", "Comox-Sliammon", "another name for ʔayʔaǰuθəm")),
     (None, ("§1", AUTHORS, "language", "Salish", "the family, where infinitives are not common")),
     (None, ("all", AUTHORS, "notation", "Ø", "the null third person object, U+00D8, in the segmentations, zəwát-ən-Ø")),
     (None, ("all", AUTHORS, "notation", "gloss labels", "the page draws the gloss labels as small capitals, and the text layer holds them as ASCII capitals, IPFV. The table keeps the text layer")),
     (None, ("all", AUTHORS, "notation", "subscripts", "the indices of the English examples, Peteri, PROi+, are set as subscripts on the page and written on the line here")),
     ]
    + display(("§3.4", "In this respect, it is worth noting..."), [
        ("(39%s) line 1" % letter, AUTHORS, "cited example", re.sub(r"^(?:\(39\) )?[a-h]\.\s*", "", PAGE[at]),
         "page %d, %s: the verb with to, and after it the same verb with how to, ≠ where the two differ" % (12 if letter in "abc" else 13, ENGLISH))
        for letter, at in zip("abcdefgh", [AT_39, AT_39 + 1, AT_39 + 2, AT_39 + 6, AT_39 + 7, AT_39 + 8, AT_39 + 9, AT_39 + 10])])
    + display(("§4.3", "It turns out one is readily available..."), [
        ("(49a) line 1", AUTHORS, "cited example", PAGE[AT_49][len("(49) a. "):], "page 18, %s, a finite complement" % ENGLISH),
        ("(49a) line 2", AUTHORS, "notation", PAGE[AT_49 + 1], "page 18, the sloppy reading"),
        ("(49a) line 3", AUTHORS, "notation", PAGE[AT_49 + 2], "page 18, the strict reading"),
        ("(49b) line 1", AUTHORS, "cited example", PAGE[AT_49 + 3][len("b. "):], "page 18, %s, an infinitive" % ENGLISH),
        ("(49b) line 2", AUTHORS, "notation", PAGE[AT_49 + 4], "page 18, the sloppy reading"),
        ("(49b) line 3", AUTHORS, "notation", PAGE[AT_49 + 5], "page 18, the strict reading, marked # as unavailable"),
    ])
    + display(("§4.3", "Here, the sloppy reading is available..."), [
        ("Table 1", AUTHORS, "note", line("Table 1: Strict"), "page 20, the caption"),
        ("Table 1", AUTHORS, "note", line("finite clause Infinitival"), "page 20, the column heads"),
        ("Table 1", AUTHORS, "note", line("strict √"), "page 20, the strict reading: available with a finite clause, not with an infinitive"),
        ("Table 1", AUTHORS, "note", line("sloppy √"), "page 20, the sloppy reading: available with both"),
    ])
    + display(("§4.3", "We now have the evidence we need..."), [
        ("(54) line 1", "Landau 2013:29", "note", line("(54) The OC")[5:], "page 21, the caption, Landau's term"),
        ("(54) line 2", "Landau 2013:29", "note", line("In a control construction") + " " + line("subject of the clause S:"),
         "page 21, the definition, set over two lines"),
        ("(54a) line 1", "Landau 2013:29", "note", line("a. The controller(s)")[3:], "page 21, the first part of the signature"),
        ("(54b) line 1", "Landau 2013:29", "note", line("b. PRO (or part")[3:].rstrip("0123456789"),
         "page 21, the second part of the signature, carries footnote 15, written here without its digit"),
    ])
    + display(("§4.5", "Externally to St’át’imcets..."), [
        ("(66) line 1", "Landau 2013:159", "cited example", line("(66) It is")[5:], "page 26, an English example, from Landau (2013:159)"),
    ])
    + display(("§4.5", "Here, PROarb gets its reference..."), [
        ("(67a) line 1", "Landau 2013", "cited example", line("(67) a. *")[len("(67) a. "):].rstrip("0123456789"),
         "page 26, an English example, Landau's paradigm, carries footnote 18, written here without its digit"),
        ("(67b) line 1", "Landau 2013", "cited example", line("b. * Suei")[3:], "page 26, an English example, Landau's paradigm"),
        ("(67c) line 1", "Landau 2013", "cited example", line("c. Maryi")[3:], "page 26, an English example, Landau's paradigm"),
        ("(67d) line 1", "Landau 2013", "cited example", line("d. Suei")[3:], "page 26, an English example, Landau's paradigm"),
    ])
)

TITLE_ROW = TITLE

WHOSE = (
    "The language is St’át’imcets, Northern Interior Salish. All unattributed examples and "
    "judgements come from Carl Alexander, Qwa7yán’ak, whom the acknowledgement thanks, and the "
    "Consultant of the comments is him. Other examples are cited from texts and a dictionary: "
    "Alexander 2016, Matthewson 2005b, Mitchell 2022, van Eijk and Williams 1981 and Davis et al. in "
    "prep. The examples (40) and (41) and those of footnote 9 are ʔayʔaǰuθəm, from Betty Wilson and "
    "Molly Harry.\n\n"
    "The who for each tier of an example is its language. The who for a translation is the source "
    "its tag cites, and Henry Davis for an untagged one. The comments signed Consultant are Carl "
    "Alexander's, and the Interviewer's question is the author's. The English examples of (39) and "
    "(49) are the author's, and (54), (66) and (67) are Landau's. The prose, the headings and the "
    "notes carry Henry Davis."
)

LETTERS = (
    "St’át’imcets is written in the variant of the North American Phonetic Alphabet footnote 5 "
    "names, with the glottalization of a resonant as a comma above, U+0313, and the null object "
    "as Ø. The gloss labels are ASCII capitals in the text layer."
)

PAGE_NOTES = (
    "The text layer sets spaces inside words, wh ich and 200 5a, and the page text closes the 181 "
    "lines the glyph positions contradict. A space the layer sets where the face or size changes, "
    "EXCL get.forgotten, stays. The page itself prints some glosses run together, NTSDET and "
    "IPFVget, and the table keeps them as printed."
)
