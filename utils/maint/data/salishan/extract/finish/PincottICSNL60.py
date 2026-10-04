# Context for Ethan Pincott, Don’t stress about schwa: The diachrony of weak roots in Secwepemctsín.
# The paradigms (4) to (7) are read off the page one form a row, a line of forms and a line of their
# translations; Tables 4 to 9 one cell a row, each with the language of its column; Tables 1 to 3,
# the root shapes, a line a row. Each footnote giving the community orthography of an example is
# parted into its words, each matched to the form of the example it writes.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import PRIVATE  # noqa: E402
from workdir import WORK  # noqa: E402
import residue  # noqa: E402

STEM = "PincottICSNL60"
AUTHORS = "Ethan Pincott"
SH = "Secwepemctsín"
LI = "St’át’imcets"
TH = "nɬeʔkepmxcín"
NIS = "Proto-Northern Interior Salish"

TITLE = "Don’t stress about schwa: The diachrony of weak roots in Secwepemctsín"
BYLINE = "Ethan Pincott"

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


def span(first, stop):
    """The nonempty page lines from the one opening on first up to the one opening on stop, each
    with its index."""
    start = at(first)
    end = at(stop, start)
    return [(index, PAGE[index]) for index in range(start, end)
            if PAGE[index] and not PAGE[index].startswith("=====") and not PAGE[index].isdigit()]


def draft_form(where, opening):
    for one in DRAFT:
        if one[0] == where and one[3].startswith(opening):
            return one[3]
    raise SystemExit("no draft row %s opens with %s" % (where, opening))


def chained(anchor, rows):
    out = []
    for row in rows:
        out.append((anchor, row))
        anchor = (row[0], row[3])
    return out


def marked(text):
    """A cell and the footnote whose digit closes it, t̓qʷ-ənt-és7 or ‘s/he sews it’8."""
    mark = re.match(r"^(.*?[^\d\s])(\d{1,2})$", text)
    if mark and not mark.group(1)[-1].isdigit():
        return mark.group(1), ", carries footnote %s, written here without its digit" % mark.group(2)
    return text, ""


HEADS = ("root-stressed", "middle", "transitive")


def paradigm(number, first, stop):
    """A paradigm set as a caption, the column heads, and a line of forms over a line of their
    translations for each lettered item."""
    lines = span(first, stop)
    page = page_of(lines[0][0])
    caption = lines[0][1]
    rows = [("(%s)" % number, AUTHORS, "note", caption[len("(%s) " % number):], "page %d, the caption" % page),
            ("(%s)" % number, AUTHORS, "note", lines[1][1], "page %d, the column heads" % page)]
    forms = []
    body = [text for index, text in lines[2:]]
    for line, translations in zip(body[0::2], body[1::2]):
        letter, cells = line.split(" ", 1)
        where = "(%s%s)" % (number, letter.rstrip("."))
        cells = [marked(one) for one in cells.split()]
        glosses = re.findall(r"‘[^’]*’\d{0,2}", translations)
        glosses = [marked(one) for one in glosses]
        for head, (form, note) in zip(HEADS, cells):
            rows.append((where + " line 1", SH, "cited form", form, "page %d, the %s form%s" % (page, head, note)))
            forms.append((where, form))
        for head, (form, _), (gloss, note) in zip(HEADS, cells, glosses):
            rows.append((where + " line 2", AUTHORS, "translation", gloss, "page %d, of %s%s" % (page, form, note)))
    return rows, forms


def table(first, stop, name, notes=()):
    """A table read off the page a line a row: the caption, the column heads, and the lines."""
    lines = span(first, stop)
    page = page_of(lines[0][0])
    notes = list(notes) + [""] * 2
    return ([(name, AUTHORS, "note", lines[0][1], "page %d, the caption%s" % (page, notes[0])),
             (name, AUTHORS, "note", lines[1][1], "page %d, the column heads%s" % (page, notes[1]))] +
            [("%s line %d" % (name, number), AUTHORS, "note", text, "page %d, a line of the table" % page)
             for number, (index, text) in enumerate(lines[2:], 1)])


def cells(name, page, number, items):
    """One line of a table set one cell a row: (form, kind, who, what) each."""
    return [("%s line %d" % (name, number), who, kind, form, "page %d, %s" % (page, what))
            for form, kind, who, what in items]


def cognates(name, pieces, th_only=()):
    """The cognate sets of Tables 6 and 7: a gloss, the Proto-NIS form, and a root for each language
    that has one. A set missing a language leaves its cell empty, and the glyph positions put
    the one cell of ‘lukewarm’, ‘burn, glare’ and ‘return’ under nɬeʔkepmxcín."""
    records = []
    for first, stop in pieces:
        for index, text in span(first, stop):
            if text.startswith("‘") or not records:
                records.append([text, page_of(index)])
            else:
                records[-1][0] += " " + text
    rows = []
    for number, (text, page) in enumerate(records, 1):
        found = re.match(r"^(‘[^’]*’) (\*\S+)\s*(.*)$", text)
        gloss, proto, rest = found.groups()
        roots = re.findall(r"√[^\s,]+(?:, [^\s√,]+)*", rest)
        whose = (SH, LI, TH) if len(roots) == 3 else (SH, TH if gloss.strip("‘’") in th_only else LI)
        items = [(gloss, "translation", AUTHORS, "the gloss of the set, rough and approximate by footnote 16"),
                 (proto, "cited form", NIS, "the reconstruction")]
        for who, cell in zip(whose, roots):
            for one in cell.split(", "):
                items.append((one, "root" if one.startswith("√") else "cited form", who,
                              "the %s cognate%s" % (who, ", beside the root before it" if not one.startswith("√") else "")))
        rows += cells(name, page, number, items)
    return rows


TABLE_1 = table("Table 1:", "3 Community orthography", "Table 1", (", carries footnote 5 on Transitive", ""))
TABLE_2 = table("Table 2:", "Unstressed schwa surfaces", "Table 2")
TABLE_3 = table("Table 3:", "Then the following sound changes", "Table 3")

PARADIGM_4, FORMS_4 = paradigm(4, "(4) Triconsonantal", "The examples in (4a)")
PARADIGM_5, FORMS_5 = paradigm(5, "(5) Exceptional", "Biconsonantal roots show the same")
PARADIGM_6, FORMS_6 = paradigm(6, "(6) Unexpected", "(7) Unexpected")
PARADIGM_7, FORMS_7 = paradigm(7, "(7) Unexpected", "These exceptional cases")

TABLE_4 = ([("Table 4", AUTHORS, "note", "Table 4: Strong and weak root alternants12", "page 7, the caption, carries footnote 12"),
            ("Table 4", AUTHORS, "note", "Root consonants /í/ grade Regular grade", "page 7, the column heads")] +
           [row for number, (root, meaning, strong, strong_gloss, weak, weak_gloss) in enumerate((
               ("√ptkʷ", "‘pierce’", "pítkʷ-ən-s", "‘s/he makes holes in it’", "pətkʷ-ənt-és", "‘s/he makes a hole in it’"),
               ("√plk̓", "‘turn over’", "pílk̓-ən-s", "‘s/he rolls it’", "pəlk̓-ənt-és", "‘s/he turns it over’"),
               ("√plqʷ", "‘break’", "pílqʷ-ən-s", "‘s/he breaks pieces off’", "pəlqʷ-ənt-és", "‘s/he breaks it off’"),
               ("√ɬʕʷ", "‘lose’", "ɬíʕʷ-ən-s", "‘s/he loses it’", "ɬʕʷ-ənt-és", "‘s/he loses them’")), 1)
            for row in cells("Table 4", 7, number, (
                (root, "root", SH, "the root"),
                (meaning, "translation", AUTHORS, "of the root"),
                (strong, "cited form", SH, "the /í/ grade, a strong root"),
                (strong_gloss, "translation", AUTHORS, "of the /í/ grade, from comments of fluent speakers"),
                (weak, "cited form", SH, "the regular grade, a weak root"),
                (weak_gloss, "translation", AUTHORS, "of the regular grade, from comments of fluent speakers")))])

TABLE_5 = ([("Table 5", AUTHORS, "note", "Table 5: Vowel grades across all paradigms14", "page 8, the caption, carries footnote 14"),
            ("Table 5", AUTHORS, "note", "√ptkʷ “pierce” Root-stressed Middle Transitive", "page 8, the column heads, the root and its gloss over the first column")] +
           [row for number, (root, grade, forms) in enumerate((
               ("√ptkʷ", "the regular grade", (("c-ptúkʷ", "‘hole’"), ("pətkʷ-úm", "‘puncture’"), ("pətkʷ-ənt-és", "‘s/he makes a hole in it’"))),
               ("√pítkʷ", "the /í/ grade", (("c-pítkʷ", "‘pierced’"), ("pət-pítkʷ-əm", "‘puncture holes’"), ("pítkʷ-ən-s", "‘s/he makes holes in it’")))), 1)
            for row in cells("Table 5", 8, number, [(root, "root", SH, grade)] + [
                one for head, (form, gloss) in zip(HEADS, forms)
                for one in ((form, "cited form", SH, "the %s form of %s" % (head, grade)),
                            (gloss, "translation", AUTHORS, "of %s" % form))])])

TABLE_6 = ([("Table 6", AUTHORS, "note", "Table 6: Cognates of root-stressed weak forms in NIS15", "page 8, the caption, carries footnote 15"),
            ("Table 6", AUTHORS, "note", " ".join(text for index, text in span("Gloss16", "‘straight")),
             "page 8, the column heads, Gloss carries footnote 16; the text layer sets St’át’imcets and nɬeʔkepmxcín with gaps inside the words")] +
           cognates("Table 6", (("‘straight", "14 Community"), ("‘tie up’", "Sets for which"))))

TABLE_7 = ([("Table 7", AUTHORS, "note", "Table 7: Additional cognate sets", "page 9, the caption"),
            ("Table 7", AUTHORS, "note", " ".join(text for index, text in span("Gloss Proto-NIS", "‘spread out’")),
             "page 9, the column heads; the text layer sets St’át’imcets and nɬeʔkepmxcín with gaps inside the words")] +
           cognates("Table 7", (("‘spread out’", "17 For ease"), ("‘roll down’", "The St’át’imcets cognates")),
                    th_only=("lukewarm", "burn, glare", "return")))

TABLE_8 = ([("Table 8, page 10", AUTHORS, "note", "Table 8: /í/ grades in Secwepemctsín and nɬeʔkepmxcín", "page 10, the caption"),
            ("Table 8, page 10", AUTHORS, "note", "Secwepemctsín19 nɬeʔkepmxcín", "page 10, the heads of the column pairs, Secwepemctsín carries footnote 19"),
            ("Table 8, page 10", AUTHORS, "note", "weak grade /í/ grade weak grade /í/ grade", "page 10, the column heads")] +
           [row for number, (page, sets) in enumerate((
               (10, (("kɬəntés", "‘s/he takes it off’"), ("kəɬkíɬəns", "‘s/he takes it apart’"),
                     ("kəɬtés", "‘s/he detaches it’"), ("kíɬes", "‘s/he detaches things’"))),
               (10, (("k̓lám", "‘s/he cuts strips’"), ("k̓éləns", "‘s/he cuts it to strips’"),
                     ("k̓lǝ̣́m", "‘s/he cuts’"), ("k̓ị́lm", "‘s/he cuts into pieces’"))),
               (10, (("cʕep", "‘torn’"), ("cíʕəns", "‘s/he tears it’"),
                     ("cʕə́p", "‘get torn, ripped’"), ("cíʕes", "‘s/he rips it in several pieces’"))),
               (11, (("t̓meq", "‘torn, ripped apart’"), ("t̓ímqəmt", "‘torn, ripped, with holes’"),
                     ("ƛ̓əm̓qetés", "‘s/he breaks rope’"), ("ƛ̓ím̓m̓q", "‘several strands break’")))), 1)
            for row in cells("Table 8, page 10", page, number, [
                one for (who, grade), (form, gloss) in zip(((SH, "weak grade"), (SH, "/í/ grade"), (TH, "weak grade"), (TH, "/í/ grade")), sets)
                for one in ((form, "cited form", who, "the %s %s" % (who, grade)),
                            (gloss, "translation", AUTHORS, "of %s%s" % (form, ", printed on page 11" if page == 10 and number == 3 else "")))])])


def stress(name, first, stop, columns, notes=()):
    """A stress derivation: the table a line a row, then each column's underlying form, the form
    the rule that applies stresses, and the surface form."""
    rows = table(first, stop, name, notes)
    for number, (under, rule, stressed, surface) in enumerate(columns, 1):
        where = "%s column %d" % (name, number)
        rows += [(where, SH, "phonemic", under, "page 12, the underlying form, in / /"),
                 (where, SH, "cited form", stressed, "page 12, stressed by %s" % rule),
                 (where, SH, "phonemic", surface, "page 12, the surface form, phonetic, in [ ]")]
    return rows


STRESS_8 = stress("Table 8, page 12", "Table 8: Revised", "In Table 8, I have omitted", (
    ("/ciq-etkʷə/", "Stress > strong", "cíqetkʷə", "[cíqkʷe]"),
    ("/peɣ-etkʷə/", "Stress > V", "peɣétkʷə", "[pəɣétkʷe]"),
    ("/peɣ-əm/", "Stress > V", "péɣəm", "[péɣəm]"),
    ("/pət-əm/", "Stress > ə", "pətə́m", "[ptém]")), (", carries footnote 22; the paper numbers two tables 8",))
STRESS_9 = stress("Table 9", "Table 9: Stress", "This analysis is extremely", (
    ("/ptəkʷ-əm/", "Stress > ə", "ptəkʷə́m", "[pətkʷúm]"),
    ("/pət-pitkʷ-əm/", "Stress > V", "pətpítkʷəm", "[pətpítkʷəm]")), (", carries footnote 23",))

SOUND_CHANGES = [("(8)", AUTHORS, "note", "Sound changes affecting stressed schwa", "page 7, the caption"),
                 ("(8a) line 1", AUTHORS, "note", "*ə́ > ú / _Cʷ", "page 7, a sound change of pre-Secwepemctsín"),
                 ("(8b) line 1", AUTHORS, "note", "*ə́ > é (listed in Kuipers 2002:5)", "page 7, a sound change of pre-Secwepemctsín")]

# The forms of the examples each community orthography footnote writes, in its order.
COMMUNITY = {
    2: ["(2a) [péɣəm]", "(2b) [pəɣétkʷe]", "(2c) [cíqəm]", "(2d) [cíqkʷe]", "(2e) [ptém]"],
    3: ["(3a) [x̌lítəmx]", "(3b) [píləmt]", "(3c) [tqeltk]", "(3d) [c̓éɬt]"],
    4: ["%s %s" % one for one in FORMS_4],
    6: ["c-ylók̓ʷ", "yəlk̓ʷ-ənt-ás"],
    8: ["%s %s" % one for one in FORMS_5],
    9: ["%s %s" % one for one in FORMS_6],
    10: ["%s %s" % one for one in FORMS_7],
    11: ["/c-k̓éʔ/", "/k̓ʔ-ém/", "/k̓-ənt-és/", "/s-cxéʔ/", "/cəxʔ-ém/", "/cx-ənt-és/"],
    12: ["Table 4 " + one for one in ("pítkʷ-ən-s", "pílk̓-ən-s", "pílqʷ-ən-s", "ɬíʕʷ-ən-s",
                                     "pətkʷ-ənt-és", "pəlk̓-ənt-és", "pəlqʷ-ənt-és", "ɬʕʷ-ənt-és")],
    13: ["/t̓uxʷt/", "/t̓uyxʷt/"],
    14: ["Table 5 " + one for one in ("c-ptúkʷ", "pətkʷ-úm", "pətkʷ-ənt-és", "c-pítkʷ", "pət-pítkʷ-əm", "pítkʷ-ən-s")],
    19: ["Table 8 " + one for one in ("kɬəntés", "kəɬkíɬəns", "k̓lám", "k̓éləns", "cʕep", "cíʕəns", "t̓meq", "t̓ímqəmt")],
    21: ["/píwkʷəns/", "/míkʷəns/", "/ɬík̓ʷəmt/"],
    22: ["Table 8 " + one for one in ("[cíqkʷe]", "[pəɣétkʷe]", "[péɣəm]", "[ptém]")],
    23: ["Table 9 " + one for one in ("[pətkʷúm]", "[pətpítkʷəm]")],
}


def footnote_of(number):
    for where, who, kind, form, gloss in DRAFT:
        if kind == "note" and "footnote" in gloss and form.startswith("%d " % number):
            return form, int(re.search(r"page (\d+)", gloss).group(1))
    raise SystemExit("no footnote %d" % number)


def community(number):
    text, page = footnote_of(number)
    listed = re.search(r"Community orthography[^:]*: (.*?)\.?$", text).group(1)
    words = [re.sub(r"^\(\w+\) ", "", one) for one in listed.split(", ")]
    targets = COMMUNITY[number]
    if len(words) != len(targets):
        raise SystemExit("footnote %d lists %d words for %d forms" % (number, len(words), len(targets)))
    rows = [("footnote %d" % number, SH, "transcription", word,
             "page %d, the community orthography of %s" % (page, target)) for word, target in zip(words, targets)]
    return chained(("footnote %d" % number, text), rows), set(words)


COMMUNITY_ADD = []
COMMUNITY_WORDS = set()
for _number in COMMUNITY:
    _rows, _words = community(_number)
    COMMUNITY_ADD += _rows
    COMMUNITY_WORDS |= _words

SOUND = "a vowel of the argument"
SHAPE = "a root shape, C a consonant, V a vowel"
FORMS = {
    "Secwepemctsín": ("language", AUTHORS, "Shuswap, the Northern Interior Salish language of the paper"),
    "Secwepemctsín/Shuswap": ("language", AUTHORS, "page 1, a keyword, the language's own name and its English one"),
    "pre-Secwepemctsín": ("language", AUTHORS, "page 6, the stage of the language before the sound changes of (8)"),
    "St’át’imcets": ("language", AUTHORS, "Lillooet, a Northern Interior Salish language"),
    "nɬeʔkepmxcín": ("language", AUTHORS, "Thompson, a Northern Interior Salish language"),
    "Spokane-Kalispel-Seliš": ("language", AUTHORS, "in the title of Black 2006"),
    "/əǝ̣/": ("cited form", "Spokane-Kalispel-Seliš", "page 13, in the title of Black 2006, schwa and retracted schwa"),
    "íy": ("cited form", TH, "page 13, in the title of Thompson 1979, Thompson Salish surface íy"),
    "Wumecwílc": ("name", SH, "page 1, the elders’ group Wumecwílc re Secwepemctsín, whose speakers the author works with"),
    "/é/": ("cited form", SH, SOUND), "/ú/": ("cited form", SH, SOUND), "/á/": ("cited form", SH, SOUND),
    "/ó/": ("cited form", SH, SOUND), "/í/": ("cited form", SH, SOUND), "/ʔ/": ("cited form", SH, "a consonant of the argument, the glottal stop"),
    "/ə/": ("cited form", TH, "page 9, the nɬeʔkepmxcín root vowel elsewhere, Thompson 1979a:210"),
    "ə": ("cited form", SH, "page 2, footnote 1, a vowel of the middle suffix, in [ ]"),
    "é": ("cited form", SH, SOUND), "ú": ("cited form", SH, SOUND), "á": ("cited form", SH, SOUND),
    "ɛ": ("phonetic", LI, "page 9, footnote 17, the pronunciation of St’át’imcets <a>, written e here"),
    "æ": ("phonetic", LI, "page 9, footnote 17, the pronunciation of St’át’imcets <a>, written e here"),
    "CCéC": ("notation", AUTHORS, SHAPE), "CəCC": ("notation", AUTHORS, SHAPE), "CCúCʷ": ("notation", AUTHORS, SHAPE),
    "Cə́CC": ("notation", AUTHORS, SHAPE), "CCV́C": ("notation", AUTHORS, SHAPE), "CVʔC": ("notation", AUTHORS, SHAPE),
    "CʔVC": ("notation", AUTHORS, SHAPE), "/*CəCəC/": ("notation", AUTHORS, SHAPE), "/*Cə́CəC/": ("notation", AUTHORS, SHAPE),
    "/Cə́CC/": ("notation", AUTHORS, SHAPE), "/*CəCə́C/": ("notation", AUTHORS, SHAPE), "/CCə́C/": ("notation", AUTHORS, SHAPE),
    "/CəCəC/": ("notation", AUTHORS, SHAPE), "/(C)CéC/": ("notation", AUTHORS, SHAPE), "/(C)CúCʷ/": ("notation", AUTHORS, SHAPE),
    "E=ə́": ("notation", AUTHORS, "page 10, E for stressed schwa, in Van Eijk 1997:32"),
    "ə-i": ("notation", AUTHORS, "page 11, the ablaut of other Salish languages, Kinkade 1981:268"),
    "⟨-ʔ-⟩": ("cited affix", SH, "page 10, the inchoative infix"),
    "<ʔ>": ("cited affix", LI, "page 12, footnote 25, the inchoative infix of roots with a full vowel"),
    "/-əp/": ("cited affix", SH, "page 12, footnote 25, the inchoative suffix of weak roots"),
    "/-etkʷə/": ("cited affix", SH, "page 2, the suffix ‘water’, of variable stress"),
    "c-ylók̓ʷ": ("cited form", SH, "page 4, footnote 6, ‘coiled’, the one retracted root the author knows"),
    "yəlk̓ʷ-ənt-ás": ("cited form", SH, "page 4, footnote 6, ‘s/he coils it’"),
}

# The candidates of the paradigms, the tables and the rules, which the rows below read again whole.
TABLE_FORMS = {
    "§3": [form for where, form in FORMS_4 + FORMS_5 + FORMS_6 + FORMS_7] + ["t̓qʷ-ənt-és7"] +
          [one.lstrip("*") for row in TABLE_1 + TABLE_2 + TABLE_3 if " line " in row[0] for one in row[3].split()],
    "§3.1": [row[3] for row in TABLE_4 if row[2] in ("root", "cited form")],
    "§4": [row[3].lstrip("*") for row in TABLE_6 + TABLE_8 if row[2] in ("root", "cited form")] +
          ["√q̓ʷí", "át", "ɬeʔkepmxcín", "√yíx̌", "√lə́x̌", "√léx̌", "√kʷéy", "√kʷél", "√q̓ʷóʕ", "√q̓ʷéʕ", "√t̓lúqʷ"],
    "§5": ["cíqetkʷə", "peɣétkʷə", "ə", "pətə́m", "cíqkʷe", "ə́", "ú", "_Cʷ", "pətpítkʷəm", "ptəkʷə́m", "pətkʷúm",
           "Secwepemctsín.24"],
}
KEEP = {("§3", "CCéC"), ("§3", "CCúCʷ"), ("§3", "CəCC")}

DROP = ()
INITIALS = {}

SPLIT = (
    ("front", "Abstract:", None, {"gloss": "page 1, the abstract"}),
    ("footnote *", "Thank you to the elders", {"kind": "transcription", "who": SH,
     "gloss": "page 1, footnote *, the author's thanks to the elders written in Secwepemctsín, in the community orthography"}, {}),
    ("§2", "However, these categories", {"where": "(1) line 1", "gloss": "page 2, the stress hierarchy of Interior Salish, after Czaykowska-Higgins 1993"}, {}),
    ("§3", "The examples in (4a)", None, {}),
    ("§3", "The root-stressed forms nearly", None, {}),
    ("§3", "Biconsonantal roots show the same", None, {}),
    ("§3", "Unstressed schwa surfaces", None, {}),
    ("§3", "These exceptional cases", None, {}),
    ("§3", "Of the 79 weak roots", {"gloss": "page 6, the caption of Figure 1, a chart the text layer does not carry"}, {}),
    ("§3", "Then the following sound changes", {}, {}),
    ("§3", "Table 3: Biconsonantal", {}, None),
    ("§3.1", "Translations are based", None, {}),
    ("§4", "Sets for which both", None, {}),
    ("§4", "In Secwepemctsín, some of the singular", None, {}),
    ("§5", "In Table 8, I have omitted", None, {}),
    ("§5", "This analysis is extremely", None, {}),
    ("references", "Black, Deirdre. 2006.", {}, {}),
    ("references", "Van Eijk, Jan. 2013.", {}, {}),
)

REMOVE = [(where, form) for where, forms in TABLE_FORMS.items() for form in forms if (where, form) not in KEEP]
REMOVE += [("§3", "Root-stressed Middle Transitive a. x̌éw-t...")]
# The rows the sound changes of (8) and the paragraph after them ran into, and every table's rows,
# read again whole.
REMOVE_WHERE = r"^\([678][ab]?\) line|^Table [4-9]"

R_RULES = draft_form("§3", "consonant. This is not the case")
RULES = " ".join(PAGE[at("These rules account"):at("3.1 The /í/ grade")]).strip()

SET = {
    ("§3", R_RULES): {"form": RULES, "gloss": "page 7"},
}
# The words a footnote cites, which the engine left in the section around it; those of a community
# orthography footnote are read again whole below.
_note = None
for _where, _who, _kind, _form, _gloss in DRAFT:
    _page = re.search(r"page (\d+)", _gloss)
    _page = int(_page.group(1)) if _page else 0
    if _kind == "note" and "footnote" in _gloss:
        _mark = re.match(r"^(\d{1,2}|\*) ", _form)
        _note = (_mark.group(1), _page) if _mark else None
        continue
    if _kind != "cited form" or not _note or _page != _note[1]:
        _note = None
        continue
    if _note[0] != "*" and int(_note[0]) in COMMUNITY:
        REMOVE.append((_where, _form))
    else:
        SET[(_where, _form)] = {"where": "footnote %s" % _note[0]}

PROSE_ROOTS = [("§4", who, "root", form, "page 9, %s, an exception to the nɬeʔkepmxcín root vowel of Thompson 1979a:210" % gloss)
               for who, form, gloss in ((TH, "√yíx̌", "‘intelligence, information’"), (LI, "√lə́x̌", "‘intelligence, information’"),
                                        (SH, "√léx̌", "‘intelligence, information’"), (TH, "√kʷéy", "‘cool, lukewarm’"),
                                        (SH, "√kʷél", "‘cool, lukewarm’"), (TH, "√q̓ʷóʕ", "‘cheap’"), (SH, "√q̓ʷéʕ", "‘cheap’"),
                                        (TH, "√ʕác", "‘tie up’"), (SH, "√ʕéc", "‘tie up’"), (TH, "√ƛ̓y̓íqʷ", "‘break’"),
                                        (SH, "√t̓lúqʷ", "‘break’"))]

ADD = tuple(
    [(None, ("title", AUTHORS, "title", TITLE, "the paper's title, carries footnote *")),
     (None, ("title", AUTHORS, "name", AUTHORS, "the author"))]
    + chained(("§3", "Although weak roots will typically..."), PARADIGM_4)
    + chained(("§3", "The examples in (4a)..."), TABLE_1)
    + chained(("§3", "There is one exception to this pattern..."), PARADIGM_5)
    + chained(("§3", "Biconsonantal roots show the same..."), TABLE_2)
    + chained(("§3", "Unstressed schwa surfaces..."), PARADIGM_6 + PARADIGM_7)
    + chained(("§3", "What can explain the observed..."), TABLE_3)
    + chained(("§3", "Then the following sound changes..."), SOUND_CHANGES)
    + chained(("§3.1", "While the distribution of..."), TABLE_4)
    + chained(("§3.1", "A strange thing about..."), TABLE_5)
    + chained(("§4", "If weak roots in Secwepemctsín go back..."), TABLE_6)
    + chained(("§4", "Sets for which both..."), PROSE_ROOTS)
    + chained(("§4", "Some additional cognate sets..."), TABLE_7)
    + chained(("§4", "A potential solution to this problem..."), TABLE_8)
    + chained(("§5", "If instead we assume..."), STRESS_8)
    + chained(("§5", "In Table 8, I have omitted..."), STRESS_9)
    + COMMUNITY_ADD
)

WHOSE = (
    "The paper's forms are Secwepemctsín unless a table's column or the prose names another language: "
    "the cognate tables give each root to Secwepemctsín, St’át’imcets or nɬeʔkepmxcín by its column and "
    "each starred form to Proto-Northern Interior Salish, and Table 8 of page 10 gives its last two "
    "columns to nɬeʔkepmxcín. Each footnote writing examples in the community orthography is parted "
    "into its words, each matched to the form of the example it writes. The acknowledgement opening "
    "footnote * is the author's own Secwepemctsín. The prose, the translations, the captions and the "
    "sound changes carry Ethan Pincott."
)

LETTERS = (
    "The examples are in Americanist letters: ʷ for rounding, ̓ U+0313 comma above for glottalization, "
    "ʔ for the glottal stop, ʕ for the pharyngeal, ɬ, ƛ̓, x̌ with U+030C caron for the uvular, and ə with "
    "an acute for stressed schwa. / / hold underlying and phonemic forms and [ ] surface ones; √ marks "
    "a root and * a reconstruction or a pre-Secwepemctsín form. The community orthography writes the "
    "glottal stop 7, ɬ ll, c ts and x̌ x."
)

PAGE_NOTES = (
    "Table 8's nɬeʔkepmxcín weak grade k̓lə̣́m carries a dot below its schwa, which the text layer drops; "
    "the table writes it as a 400 dpi render shows. The text layer spaces forms between slashes on pages 5 "
    "and 11, / k̓lám/, and runs the first two cells of Table 6's ‘transverse’ row together; the glyph "
    "positions set them as the table writes them. The paper numbers two tables 8, on pages 10 and 12, "
    "and the table names each by its page. Figure 1 is a chart the text layer does not carry, and only "
    "its caption is kept."
)
