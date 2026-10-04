# Context for Hill and Matthewson, Gidaxan aa? Yagayt halaayin / You asked a question? You already
# know it. Every example is Gitksan, tagged with the initials of the speakers who gave or judged it
# and volunteered where they gave it themselves. The forms the prose cites are set in italics and
# are read off the page's italic runs (italic_runs.py); the rows below are keyed on the draft's own
# forms, read off DRAFT.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import WORK  # noqa: E402
import italic_runs  # noqa: E402

STEM = "HillMatthewsonICSNL60"
AUTHORS = "Hector Hill and Lisa Matthewson"
G = "Gitksan"

TITLE = "Gidaxan aa? Yagayt halaayin"
BYLINE = "Hector Hill, Gitsegukla Nation; Lisa Matthewson, University of British Columbia"

with open(os.path.join(WORK, STEM + ".draft.tsv"), encoding="utf-8") as handle:
    DRAFT = [one.rstrip("\n").split("\t") for one in handle][1:]


def draft_form(where, opening):
    """The form of the draft row at where holding these words."""
    for one in DRAFT:
        if one[0] == where and opening in one[3]:
            return one[3]
    raise SystemExit("no draft row %s holds %s" % (where, opening))


def between(where, opening, first, last=None):
    """The text of the draft row at where holding opening, from first up to last."""
    form = draft_form(where, opening)
    start = form.index(first)
    return form[start:form.index(last, start)].strip() if last else form[start:].strip()


def display(anchor, rows):
    """ADD entries for rows (where, who, kind, form, gloss), each placed after the one before it,
    the first after anchor."""
    added = []
    for row in rows:
        added.append((anchor, row))
        anchor = (row[0], row[3])
    return added


INITIALS = {"BS": "Barbara Sennott", "HH": "Hector Hill", "JH": "Jeanne Harris", "RJ": "Ray Jones",
            "VG": "Vincent Gogag"}
# The two authors ask and answer each other under (11) and (14).
WHO = {"Lisa": "Lisa Matthewson", "Hector": "Hector Hill"}

FORMS = {
    "Nisga’a": ("language", AUTHORS, "the other Interior Tsimshianic language, mutually intelligible with Gitksan"),
    "Sm’algyax": ("language", AUTHORS, "for Hector ‘original language’, which can include Gitksan; the language of Brown 2024"),
    "Büring": ("name", AUTHORS, "Daniel Büring, Büring and Gunlogson 2000"),
    "Šafářová": ("name", AUTHORS, "Marie Šafářová, 2005, footnote 11"),
    "sigweyn": ("cited form", G, "page 26, in Dim lipk sigweyn ‘You will find out.’"),
    "/ʔe•ʔ/": ("cited form", "Rigsby 1986:296", "page 26, footnote 16, ‘yes’, Ee'e. in Rigsby's phonemic writing"),
}

# The runs the prose sets in italics that are no cited form: the authors' names over their
# sections, a heading, an English word set in italics for stress, and the lines of examples
# written out below. haa, yaa of footnote 6 is two forms, added below.
ITALIC_SKIP = ("Lisa", "Hector", "Two perspectives", "not", "haa, yaa", "Nee dii yee'y",
               "Oo,) k'ap/ap siipxw 'nid aa?8", "K'ap neehl siipxwd aa", "Oo,) k'ap/ap nee dii siipxwd aa",
               "Saksxwhl ha'niiwan/ha'niiwen tun aa", "Noxs Katie 'niin", "Yukwhl wis",
               "Guut gan wil ma gidaxt", "Oo guuhl dii halaa'an", "K'am ha'nii goodi'y na halaaxt")
# An italic form the engine already offers at the same place is kept once.
_offered = {(one[0], one[3]) for one in DRAFT if one[2] == "cited form"}
ITALIC = [entry for entry in italic_runs.cited_rows(STEM, DRAFT, G, ITALIC_SKIP)
          if (entry[1][0], entry[1][3]) not in _offered]

A1 = draft_form("§2.2", "A1: (Oo,)")
A1_GLOSS = draft_form("§2.2", "(oh) VERUM sick 3.III=Q")
A2_GLOSS = draft_form("§2.2", "VERUM NEG=CN sick-3.II=Q")
A3_GLOSS = draft_form("§2.2", "(oh) VERUM NEG=FOC sick-3.II=Q")
TEN_OPENING = between("§2.2", "(10) Another set", "(10) Another set", "Context: A mother")
TEN_CONTEXT = between("§2.2", "(10) Another set", "Context: A mother", "Saksxwhl")
FOURTEEN_C_QUOTES = between("§2.3", "“Couldn’t use that, no.”", "“Couldn’t use that", "Another type of scenario")

DROP = ()

SPLIT = (
    # The front matter runs the title, its English, the bylines and the abstract together.
    ("front", "Abstract:", None, {}),
    ("front", "– You already know this.", {"who": "Hector Hill", "kind": "transcription", "gloss": "page 1, under the abstract"},
     {"who": "Hector Hill", "kind": "translation", "gloss": "page 1, its English after a dash"}),
    # Footnote 5's (i) closes on page 5, where the prose runs on to its segmentation.
    ("§2.1", "Nee=dii yee-'y.", {}, {"where": "footnote 5 (i) line 2", "who": G, "kind": "segmentation",
                                     "gloss": "page 5, footnote 5"}),
    ("§2.1", "‘I didn’t go.’", {"where": "footnote 5 (i) line 3", "who": G, "kind": "gloss", "gloss": "page 5, footnote 5"},
     {"where": "footnote 5 (i) line 4", "who": "Rigsby 1986:200", "kind": "translation", "gloss": "page 5, footnote 5"}),
    ("footnote 5 (i) line 4", "(Rigsby 1986:200)", {},
     {"who": AUTHORS, "kind": "citation", "gloss": "the source of the example"}),
    # (10) sets a paragraph after its number, then its context, then its tiers, and the prose runs
    # on after its tag.
    ("§2.2", "(10) Another set of cases", None, {"where": "(10) line 1", "gloss": "page 12, the paragraph the page sets after the number of (10)"}),
    ("(10) line 1", "Context: A mother asks", {}, {"where": "(10) line 2", "gloss": "page 13, carries footnote 9, written here without its digit"}),
    ("(10) line 2", "Saksxwhl ha'niiwan", {}, {"where": "(10) line 3", "who": G, "kind": "transcription", "gloss": "page 13"}),
    ("§2.2", "‘This floor is clean?’", {"where": "(10) line 5", "who": G, "kind": "gloss", "gloss": "page 13"},
     {"where": "(10) line 6", "kind": "translation", "gloss": "page 13"}),
    ("(10) line 6", "(volunteered HH, VG)", {}, {"kind": "citation", "gloss": "the tag at the right of the translation"}),
    ("(10) line 6", "There is one last type", {}, {"where": "§2.2", "kind": "note", "gloss": "page 13"}),
    # (14c) has no line in the orthography; its glosses, translation, tag and the comments on it
    # run together.
    ("§2.3", "‘Are you not married?’ (BS, VG)", {"where": "(14c) line 2", "who": G, "kind": "gloss", "gloss": "page 17"},
     {"where": "(14c) line 3", "kind": "translation", "gloss": "page 17"}),
    ("(14c) line 3", "(BS, VG)", {}, {"kind": "citation", "gloss": "the tag at the right of the translation"}),
    ("(14c) line 3", "Comments on (14)c:", {}, {"where": "(14c) line 4", "who": "Vincent Gogag and Barbara Sennott",
                                               "kind": "speaker comment", "gloss": "page 17"}),
    ("§2.3", "Another type of scenario where nee", {"where": "(14c) line 5"}, {}),
    # The segmentation and the glosses of (21a) run on to a second line, with its translation.
    ("§2.4", "‘Doesn’t Charlie not know", {"where": "(21a) line 5", "who": G, "kind": "gloss",
                                          "gloss": "page 23, the glosses go on to a second line"},
     {"where": "(21a) line 6", "kind": "translation", "gloss": "page 23"}),
    ("(21a) line 6", "(volunteered BS, HH, JH)", {}, {"kind": "citation", "gloss": "the tag at the right of the translation"}),
    # (25) and (26) set their sentence on the context's line.
    ("(25) line 1", "# Yukwhl wis?", {}, {"who": G, "kind": "transcription",
                                         "gloss": "page 24, with the note in brackets the page sets beside it"}),
    ("(26) line 1", "# Noxs Katie 'niin?", {"gloss": "page 25, carries footnote 15, written here without its digit"},
     {"who": G, "kind": "transcription", "gloss": "page 25"}),
    ("(26) line 1", "# Nox-s Katie 'niin?", {}, {"who": G, "kind": "segmentation",
                                                "gloss": "page 25, with the note in brackets the page sets beside it"}),
    ("(26) line 1", "mother[-3.II]=PN Katie", {}, {"who": G, "kind": "gloss", "gloss": "page 25"}),
    # The summary list (27) runs its first two points together over footnote 16.
    ("§4.1", "(27) a. If you believe", {}, {"where": "(27a)", "gloss": "page 26, the summary of the paper"}),
    ("(27a)", "b. If you believe the answerer", {}, {"where": "(27b)", "gloss": "page 27"}),
    # Hector's closing words, each Gitksan sentence and its English after a dash.
    ("§4.2", "– Why did you ask that?", {"who": "Hector Hill", "kind": "transcription", "gloss": "page 27"},
     {"who": "Hector Hill", "kind": "translation", "gloss": "page 27"}),
    ("§4.2", "– What do you know?", {"who": "Hector Hill", "kind": "transcription", "gloss": "page 27"},
     {"who": "Hector Hill", "kind": "translation", "gloss": "page 27"}),
    ("§4.2", "– I think I know it.", {"who": "Hector Hill", "kind": "transcription", "gloss": "page 27"},
     {"who": "Hector Hill", "kind": "translation", "gloss": "page 27"}),
)

REMOVE = (
    ("§2.2", A1),
    ("§2.2", A1_GLOSS),
    ("§2.2", A2_GLOSS),
    ("§2.2", "# K'ap nee=hl siipxw-d=aa?"),
    ("§2.2", "#/? (Oo,) k'ap/ap nee=dii siipxw-d=aa?"),
    ("§2.1", "(i) Nee dii yee'y."),
    ("(14c) line 5", FOURTEEN_C_QUOTES),
)

SET = {
    ("front", "– You already know this."): {"form": "You already know this."},
    ("(10) line 1", TEN_OPENING): {"form": TEN_OPENING[len("(10) "):]},
    ("(10) line 2", TEN_CONTEXT): {"form": re.sub(r"9$", "", TEN_CONTEXT)},
    ("§2.2", draft_form("§2.2", "Saks-xw=hl ha-'nii-wan")): {
        "where": "(10) line 4", "who": G, "kind": "segmentation", "gloss": "page 13"},
    ("(14b) line 1", "Nee=hl naks-in=aa?"): {"kind": "segmentation", "gloss": "page 17, (14b) has no line in the orthography"},
    ("§2.3", "c. # Nee=dii naks-in=aa?"): {"where": "(14c) line 1", "who": G, "kind": "segmentation",
                                          "form": "# Nee=dii naks-in=aa?", "gloss": "page 17, (14c) has no line in the orthography"},
    ("(14c) line 4", "Comments on (14)c:"): {"form": "Comments on (14)c: " + FOURTEEN_C_QUOTES},
    ("§2.4", "Gitxsanimx/Gitsenimux/Giyaanimx=aa?"): {
        "where": "(21a) line 4", "who": G, "kind": "segmentation", "gloss": "page 23, the segmentation goes on to a second line"},
    ("(24a) line 1", "Is it raining? ORDINARY QUESTION"): {
        "who": AUTHORS, "kind": "note", "gloss": "page 24, an English question and the type the page names beside it"},
    ("(24b) line 1", "It’s raining? DECLARATIVE QUESTION"): {
        "who": AUTHORS, "kind": "note", "gloss": "page 24, an English question and the type the page names beside it"},
    ("(26) line 1", between("(26) line 1", "Context:", "Context:", " # Noxs")): {
        "form": re.sub(r"15$", "", between("(26) line 1", "Context:", "Context:", " # Noxs"))},
    ("§4.2", "– Why did you ask that?"): {"form": "Why did you ask that?"},
    ("§4.2", "– What do you know?"): {"form": "What do you know?"},
    ("§4.2", "– I think I know it."): {"form": "I think I know it."},
}
for _letter in "cde":
    _form = next(one[3] for one in DRAFT if one[0] == "§4.1" and one[3].startswith(_letter + ". "))
    SET[("§4.1", _form)] = {"where": "(27%s)" % _letter, "gloss": "page 27"}


def names(where, entries):
    """ADD entries for the names at where, each (kind, form, gloss)."""
    return [(where, (where, AUTHORS, kind, form, gloss)) for kind, form, gloss in entries]


ADD = tuple(
    [(None, ("title", AUTHORS, "title", TITLE, "the paper's title, in Gitksan")),
     (None, ("title", AUTHORS, "title", "You asked a question? You already know it",
             "the English of the title, which carries footnote *")),
     (None, ("title", AUTHORS, "name", "Hector Hill", "an author, Gitsegukla Nation, HH in the tags, Gy'ax")),
     (None, ("title", AUTHORS, "name", "Lisa Matthewson", "an author, University of British Columbia")),
     ]
    + names("footnote *", [
        ("name", "Vincent Gogag", "a speaker, VG in the tags, from Git-anyaaw (Gitanyow), speaks Giyaanimx"),
        ("name", "Jeanne Harris", "a speaker, JH in the tags, from Ansba'yaxw (Kispiox), speaks Gitxsanimx; Jeannie Harris in footnote *"),
        ("name", "Ray Jones", "a speaker, RJ in the tags, from Gijigyukwhla (Gitsegukla), speaks Gitsenimux"),
        ("name", "Barbara Sennott", "a speaker, BS in the tags, from Ansba'yaxw (Kispiox), speaks Gitxsanimx"),
        ("name", "Colin Brown", "of the Gitksan Research Lab at UBC, thanked for discussion"),
        ("name", "Henry Davis", "of the Gitksan Research Lab at UBC, thanked for discussion and for comments on a draft"),
        ("name", "Michael Schwan", "of the Gitksan Research Lab at UBC, thanked for discussion"),
        ("cited form", "Ha'miyaa!", "‘thank you’, Gitksan, in footnote *"),
    ])
    + names("§1.2", [
        ("place", "Gitsegukla", "Hector Hill's village, Gijigyukwhla in Gitksan, British Columbia"),
        ("name", "WiiSeeks", "Hector Hill's House"),
        ("name", "Gy'ax", "Hector Hill's hereditary name, Earthquake/Earth Tremor"),
        ("place", "Kitwangax", "where Hector Hill worked at a treatment centre"),
        ("name", "Wilp Si'Satxw", "the Community Healing Centre at Kitwangax"),
        ("language", "Gitsenimuxw", "Hector Hill's word for the language, ‘our Gitsenimuxw’"),
        ("name", "Randy Barnetson", "the pastor Hector Hill assisted"),
        ("name", "Myrna Aksidan", "a speaker Lisa Matthewson has worked with in Gitksan territory"),
        ("name", "Frank Benson", "a speaker Lisa Matthewson has worked with in Gitksan territory"),
        ("name", "Rena Benson", "a speaker Lisa Matthewson has worked with in Gitksan territory"),
        ("name", "Thelma Blackstock", "a speaker Lisa Matthewson has worked with in Gitksan territory"),
        ("name", "Perrine Campbell", "a speaker Lisa Matthewson has worked with in Gitksan territory"),
        ("name", "Phyllis Haizimsque", "a speaker Lisa Matthewson has worked with in Gitksan territory"),
        ("name", "Herb Russell", "a speaker Lisa Matthewson has worked with in Gitksan territory"),
        ("name", "Frances Sampson", "a speaker Lisa Matthewson has worked with in Gitksan territory"),
        ("name", "Jane Smith", "a speaker Lisa Matthewson has worked with in Gitksan territory"),
        ("name", "Fern Weget", "a speaker Lisa Matthewson has worked with in Gitksan territory"),
        ("name", "Louise Wilson", "a speaker Lisa Matthewson has worked with in Gitksan territory"),
    ])
    + display(("§1.2", "T'oyaksi'y 'Nisi'm (I Thank You All), Gy'ax Hector Hill."), [
        ("§1.2", "Hector Hill", "transcription", "T'oyaksi'y 'Nisi'm", "page 2, how Hector Hill closes his introduction"),
        ("§1.2", "Hector Hill", "translation", "I Thank You All", "page 2, its English in parentheses"),
    ])
    + names("§1.3", [
        ("language", "Gitksan", "ISO 639-3 git, a continuum of Interior Tsimshianic dialects of the northwest Interior of British Columbia"),
        ("language", "Interior Tsimshianic", "the branch Gitksan and Nisga’a belong to"),
        ("language", "Tsimshianic", "the family; Hector's language has no word for it"),
        ("name", "Ts'imtsenimx", "the people on the Coast"),
    ])
    + names("§2.1", [
        ("language", "Giyaanimx", "the dialect of Git-anyaaw (Gitanyow), Vincent Gogag's"),
        ("language", "Gitsenimux", "the dialect of Gijigyukwhla (Gitsegukla), Hector Hill's and Ray Jones's"),
        ("language", "Gitxsanimx", "the dialect of Ansba'yaxw (Kispiox), Jeanne Harris's and Barbara Sennott's"),
        ("place", "Git-anyaaw", "Gitanyow"),
        ("place", "Gijigyukwhla", "Gitsegukla"),
        ("place", "Ansba'yaxw", "Kispiox"),
    ])
    + names("footnote 4", [
        ("name", "Bruce Rigsby", "the author of a Gitksan grammar, Rigsby 1986"),
        ("name", "Marie-Lucie Tarpent", "the author of a grammar of Nisga’a, Tarpent 1987"),
    ])
    + display(("§2.1", "We also want to make clear..."), [
        ("footnote 5 (i) line 1", G, "transcription", "Nee dii yee'y.",
         "page 4, footnote 5, whose example closes at the foot of page 5"),
    ])
    + [(("footnote 6", "6 The question marker aa..."), ("footnote 6", G, "cited form", one,
                                                        "page 11, footnote 6, a form of aa after a vowel"))
       for one in ("naa", "yaa", "haa")]
    + display(("(9) line 5", "‘Charlie is sick.’"), [
        ("(9A1) line 1", G, "transcription", "(Oo,) k'ap/ap siipxw 'nid aa?",
         "page 12, Adam's first answer, carries footnote 8, written here without its digit"),
        ("(9A1) line 2", G, "segmentation", between("§2.2", "A1: (Oo,)", "(Oo,) k'ap/ap siipxw 'nid=aa?"), "page 12"),
        ("(9A1) line 3", G, "gloss", A1_GLOSS[:A1_GLOSS.index(" ‘")], "page 12"),
        ("(9A1) line 4", AUTHORS, "translation", "‘Is he really sick?’", "page 12"),
        ("(9A1) line 4", AUTHORS, "citation", "(volunteered BS; accepted HH)", "the tag at the right of the translation"),
        ("(9A2) line 1", G, "transcription", "# K'ap neehl siipxwd aa?", "page 12, Adam's second answer"),
        ("(9A2) line 2", G, "segmentation", "# K'ap nee=hl siipxw-d=aa?", "page 12"),
        ("(9A2) line 3", G, "gloss", A2_GLOSS[:A2_GLOSS.index(" ‘")], "page 12"),
        ("(9A2) line 4", AUTHORS, "translation", "‘Is he really sick?’", "page 12"),
        ("(9A2) line 4", AUTHORS, "citation", "(HH)", "the tag at the right of the translation"),
        ("(9A3) line 1", G, "transcription", "#/? (Oo,) k'ap/ap nee dii siipxwd aa?", "page 12, Adam's third answer"),
        ("(9A3) line 2", G, "segmentation", "#/? (Oo,) k'ap/ap nee=dii siipxw-d=aa?", "page 12"),
        ("(9A3) line 3", G, "gloss", A3_GLOSS[:A3_GLOSS.index(" ‘")], "page 12"),
        ("(9A3) line 4", AUTHORS, "translation", "‘Is he really not sick?’", "page 12"),
        ("(9A3) line 4", AUTHORS, "citation", "(BS, HH, JH)", "the tag at the right of the translation"),
        ("(9A3) line 5", "Barbara Sennott", "speaker comment",
         between("§2.2", "Comment on (9)A3:", "Comment on (9)A3:", " (10) Another"), "page 12"),
    ])
    + [(("footnote 16", "16 This matches what Rigsby says..."), row) for row in reversed([
        ("footnote 16", "Rigsby 1986:296", "cited form", "Ee'e.", "page 26, footnote 16, ‘Yes.’"),
        ("footnote 16", "Rigsby 1986:296", "cited form", "Nee.", "page 26, footnote 16, ‘No.’"),
        ("footnote 16", "Rigsby 1986:296", "cited form", "/ne•/", "page 26, footnote 16, ‘no’, Nee. in Rigsby's phonemic writing"),
    ])]
    + ITALIC
    + [("all", ("all", AUTHORS, "symbol note", "#", "marks a sentence the speakers judged infelicitous in its context")),
       ("all", ("all", AUTHORS, "symbol note", "?", "marks a sentence the speakers found not very good; (?) one sometimes accepted, ?? one worse")),
       ("all", ("all", AUTHORS, "symbol note", "/", "between two forms different speakers used, footnote 8")),
       ("all", ("all", AUTHORS, "symbol note", "[ ]", "in a gloss, the 3.II pronoun the form elides, footnote 7")),
       ("all", ("all", AUTHORS, "notation", "(volunteered XX)",
                "the tag of an example: the initials of the speakers who gave or judged it, volunteered where they gave it themselves")),
       ]
)

WHOSE = (
    "The language is Gitksan, Interior Tsimshianic. The examples were given or judged by five fluent "
    "speakers, tagged by initials: Vincent Gogag (VG), of Git-anyaaw, who speaks Giyaanimx; Hector Hill "
    "(HH), an author, and Ray Jones (RJ), of Gijigyukwhla, who speak Gitsenimux; Jeanne Harris (JH) and "
    "Barbara Sennott (BS), of Ansba'yaxw, who speak Gitxsanimx. (2) is cited from Rigsby 1986, and so "
    "is footnote 5's (i).\n\n"
    "The who for each tier of an example is Gitksan. The who for a translation is the authors', as the "
    "English the speakers were asked about or gave a sentence for, and a cited example's is its source. "
    "A comment is the speaker's its initials name, and Lisa and Hector under (11) and (14) are the two "
    "authors. Hector Hill's own Gitksan, closing his introduction and the paper, is his. The forms the "
    "prose cites are the italic runs of the page, glossed with the English the prose gives them. The "
    "prose, the headings and the notes carry Hector Hill and Lisa Matthewson."
)

LETTERS = (
    "The forms are in the practical orthography many community members use (Hindle and Rigsby 1973), "
    "with the apostrophe for glottalization. The second line of an example breaks the sentence into "
    "its parts, = before a clitic and - before a suffix; # marks an infelicitous sentence, ? and ?? a "
    "doubtful one, / two speakers' forms, and square brackets in a gloss a pronoun the form elides."
)

PAGE_NOTES = (
    "The page prints several cross-references as (7), [(6)] and 0 where a letter or a number is "
    "missing; they are kept as printed. (9) labels Betty's line B: and Adam's three answers A1: to "
    "A3:, written here as (9A1) to (9A3). (10) sets a paragraph between its number and its context. "
    "(14b) and (14c) have no line in the orthography. Footnote * spells Jeannie Harris where page 3 "
    "and the prose have Jeanne Harris."
)
