# Context for Hank Nater, Jargon and European Origins of some Bella Coola Lexicon. A comparative
# paper: each cited form is given to the language the prose names before it, Bella Coola (BeCo),
# Coeur d'Alene, Chinook Jargon, Russian, Spanish and a dozen others. Tables 1 to 5 are read off
# the page, Table 3 one cell a row and Table 4 one form a row.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import PRIVATE  # noqa: E402
from workdir import WORK  # noqa: E402
import residue  # noqa: E402

STEM = "NaterICSNL60_BellaCoola"
AUTHORS = "Hank Nater"
BC = "Bella Coola"
CDA = "Coeur d’Alene"
CJ = "Chinook Jargon"
PS = "Proto-Salish"

TITLE = "Jargon and European Origins of some Bella Coola Lexicon"
BYLINE = "Hank Nater, Independent Linguist"

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


def span(first, stop):
    """The nonempty page lines from the one opening on first up to the one opening on stop."""
    start = at(first)
    end = at(stop, start)
    return [one for one in PAGE[start:end] if one and not one.startswith("=====") and not one.isdigit()]


def draft_form(where, opening):
    """The form of the draft row at where opening on these words."""
    for one in DRAFT:
        if one[0] == where and one[3].startswith(opening):
            return one[3]
    raise SystemExit("no draft row %s opens with %s" % (where, opening))


def chained(anchor, rows):
    """Each row after the one before it, the first after anchor."""
    out = []
    for row in rows:
        out.append((anchor, row))
        anchor = (row[0], row[3])
    return out


def cited(where, who, form, gloss):
    return (where, who, "cited form", form, gloss)


FORMS = {
    "Nuχalk": ("name", AUTHORS, "the Nuχalk community of Bella Coola, British Columbia, to which the language belongs"),
    "/č/": ("cited form", "Coast Salish", "page 2, the /č/ series that */k/ became in SE Interior and non-BeCo Coast Salish"),
    "cipsx": ("cited form", BC, "page 3, ‘fisher’, a Common Salish word, = Coeur d’Alene cišps"),
    "cišps": ("cited form", CDA, "page 3, ‘fisher’, = BeCo cipsx, Kuipers 2002"),
    "milixʷ": ("cited form", BC, "page 3, ‘kinnickinnick’, whose dried leaves were smoked, = Coeur d’Alene mil’xʷ"),
    "mil’xʷ": ("cited form", CDA, "page 3, ‘tobacco’, = BeCo milixʷ, Kuipers 2002"),
    "√smiw": ("root", BC, "page 3, *‘medium-sized mammal’, = Coeur d’Alene smiyíw ‘coyote’; the √ is a Symbol-font glyph the text layer holds as U+F0D6"),
    "smiyíw": ("cited form", CDA, "page 3, ‘coyote’, = BeCo smiw, with /y/ from */γ/, Kuipers 2002"),
    "/γ/": ("cited form", PS, "page 3, */γ/, the source of Coeur d’Alene /y/ in smiyíw"),
    "/ǯ/": ("cited form", CDA, "page 3, the reflex smiyíw does not show"),
    "t’kʷ": ("cited form", BC, "page 3, ‘to bleed’, = Coeur d’Alene t’əkʷs"),
    "t’əkʷs": ("cited form", CDA, "page 3, ‘to bleed’, = BeCo t’kʷ, Kuipers 2002"),
    "χm": ("cited form", BC, "page 3, ‘to bite’, = Coeur d’Alene χem"),
    "χem": ("cited form", CDA, "page 3, ‘id. (of animal)’, ‘to bite’, = BeCo χm, Kuipers 2002"),
    "/-muxʷ/": ("cited affix", "Alsea", "page 3, footnote 1, ‘RECIP’, = BeCo /-maxʷ/"),
    "/-maxʷ/": ("cited affix", BC, "page 3, footnote 1, the reciprocal, = Alsea /-muxʷ/"),
    "/-n-waxʷ/": ("cited affix", BC, "page 3, footnote 1, */-n-waxʷ/, a possible source of /-maxʷ/"),
    "/-awalxʷ/": ("cited affix", PS, "page 3, footnote 1, */-awalxʷ/, the reciprocal as Kinkade reconstructs it"),
    "ik’awan": ("cited form", "Chinook Proper", "page 3, footnote 1, ‘type of salmon’, = BeCo k’awn"),
    "k’awn": ("cited form", BC, "page 4, footnote 1, = Chinook Proper ik’awan ‘type of salmon’"),
    "bołkʷ": ("cited form", "Quileute", "page 4, footnote 1, ‘hair’, /b/ from */m/, = BeCo mnłkʷa"),
    "mnłkʷa": ("cited form", BC, "page 4, footnote 1, ‘hair’, from *məłkʷ-ən, = Quileute bołkʷ"),
    "məłkʷ-ən": ("cited form", BC, "page 4, footnote 1, *məłkʷ-ən, the source of mnłkʷa"),
    "√t’uχ": ("root", "South Wakashan", "page 4, footnote 1, ‘head’, = BeCo t’nχʷ"),
    "√t’uḥʷ": ("root", "South Wakashan", "page 4, footnote 1, ‘head’, = BeCo t’nχʷ"),
    "t’nχʷ": ("cited form", BC, "page 4, footnote 1, ‘head’, = South Wakashan t’uχ, t’uḥʷ"),
    "c’a·bap": ("cited form", "Makah", "page 4, footnote 2, cf. C’aamas, from c’a"),
    "c’a·maqak": ("cited form", "Nootka", "page 4, footnote 2, ‘sound, large body of water’, No in the text"),
    "√c’a": ("root", "Nootka", "page 4, footnote 2, the root of c’a·bap and c’a·maqak"),
    "tulumic": ("cited form", BC, "page 5, ‘I defeated him’, from tulu, tuulu"),
    "tulwamkcut": ("cited form", BC, "page 5, ‘to exceed one’s expectations about self, successfully complete a task’, from tulu, tuulu"),
    "tuulu": ("cited form", BC, "page 5, ‘to win, succeed’, from Chinook Jargon tulu"),
    "nusaplinta": ("cited form", BC, "page 5, ‘flour (saplin) sack’"),
    "kʷułtaala": ("cited form", BC, "page 5, ‘rich’, from taala ‘money’"),
    "taala": ("cited form", BC, "page 5, ‘money’, from Chinook Jargon dala, tala"),
    "musmus": ("cited form", CJ, "page 5, ‘cow’, of “obscure origin” in Zenk et al 2010"),
    "√músmuski": ("root", "Upper Chehalis", "page 5, ‘cow, cattle’, compared in Zenk et al 2010"),
    "mášmaš": ("cited form", "Tahltan", "page 6, ‘cow’, the author's field notes"),
    "wasóos": ("cited form", "Tlingit", "page 6, ‘cow’, Edwards 2009"),
    "masmúus": ("cited form", "Haida", "page 6, ‘cow’, Lachler 2010"),
    "vache": ("cited form", "French", "page 6, ‘cow’"),
    "mašmús": ("cited form", CJ, "page 6, *mašmús, a regional form the northern words may continue, a hybrid of French vache and Cree mōswa"),
    "mōswa": ("cited form", "Cree", "page 6, ‘moose’, Aubin 1975"),
    "баня": ("cited form", "Russian", "page 6, ‘sauna’, in Cyrillic, [baɲə]"),
    "baɲə": ("cited form", "Russian", "page 6, [baɲə], the pronunciation of баня ‘sauna’"),
    "q’ʷup": ("cited form", BC, "page 6, ‘to (expose to) smoke’, transitive"),
    "nusq’ʷupalsta": ("cited form", BC, "page 6, *nusq’ʷupalsta, a smokehouse as BeCo might have named it without panya"),
    "nusuq’ʷpalsta": ("cited form", BC, "page 6, *nusuq’ʷpalsta, a smokehouse as BeCo might have named it without panya"),
    "elnaβo": ("cited form", "Spanish", "page 6, [elnaβo], el nabo ‘the turnip’"),
    "enˑaβo": ("cited form", "Spanish", "page 6, [enˑaβo], el nabo ‘the turnip’"),
    "ə": ("cited form", PS, "page 8, *ə, whose elimination BeCo shows"),
    "q’psttχ": ("cited form", BC, "page 8, ‘taste it!’, a voiceless word"),
}

# English and French words, broken halves of forms the rows below give whole, a name, and the
# letters of the phoneme chart, which the rows of Table 2 hold.
DROP = ("vis-à-vis", "d’Alene", "gʷ/", "gʷ~g/", "/ǯ~ʒ", "/ǯ", "/-kš", "-ikš", "-ukš/", "C’aamas2", "aplsuł",
        "Métis", "nabo", "k’xłłcxʷ", "słχʷtłłc")

INITIALS = {}

R29 = draft_form("§1.1", "The position of BeCo")
R31 = draft_form("§1.1", "1 E.g., Alsea")
R70 = draft_form("§4", "salmon’ = BeCo k’awn")

SPLIT = (
    ("front", "Hank Nater Independent Linguist", None, {}),
    ("front", "Abstract:", None, {"gloss": "page 1, the abstract"}),
    ("§1", "Contact info:", {"gloss": "page 1, the caption of Figure 1, a map of the seven once thriving regions the text layer does not carry"}, {}),
    ("§1", "In regards to physical", {"where": "front", "gloss": "page 1, the author's contact line at the foot of the page"},
     {"gloss": "page 2"}),
    ("§1.1", "Table 1: The position", {}, {}),
    ("§1.1", "For Central Salish branches", None, {}),
    ("§1.1", "Note also that Coeur", {}, {"gloss": "page 2 and 3"}),
    ("§1.1", "Figure 2: Salish language area", {}, {"gloss": "page 3, the caption of Figure 2, a map the text layer does not carry"}),
    ("§1.1", "In regards to a Salish homeland", {}, {"gloss": "page 3"}),
    ("§1.1", "Of course, this implies",
     {"who": AUTHORS, "kind": "citation", "gloss": "the source of the quotation above"}, {}),
    ("§2", "Doubled phonemes", None, {}),
    ("§4.1", "In regards to Chinook Jargon musmus", None, {"gloss": "page 5 and 6"}),
    ("§4.3", "(For Haida, see", None, {"where": "Table 4", "gloss": "page 7, the sources of the forms of the table"}),
    ("§5", "Yet, Alsea is located",
     {"who": AUTHORS, "kind": "citation", "gloss": "the source of the quotation above"}, {}),
    ("§5", "Amounts and percentages",
     {"gloss": "page 7, the caption of Figure 3, a map the text layer does not carry"}, {"gloss": "page 8"}),
    ("§5", "These percentages confirm", None, {}),
)

# The cited forms the tables' rows hold: the phoneme chart's letters, Table 3's cells and
# Table 4's forms; and the first line of footnote 1's second half, joined to its first.
REMOVE = tuple(
    [(where, form) for where, who, kind, form, gloss in DRAFT
     if kind == "cited form" and (where == "§2" or (where == "§4.3" and form not in ("elnaβo", "enˑaβo")))] +
    [(where, form) for where, who, kind, form, gloss in DRAFT[DRAFT.index(
        next(one for one in DRAFT if one[3] == "prêtre")):] if kind == "cited form" and where == "§4.1"
     and form in ("prêtre", "ən", "n̩", "paałac", "pałach", "t’sikt’sik", "smukʷaws", "église", "samən-ulali", "solt", "haws")] +
    [("§4", R70)])
REMOVE_WHERE = r"^Table [34] line"

SET = {
    ("footnote 1", R31): {"form": R31 + " " + R70, "gloss": "page 3 and 4, footnote"},
    ("§1.1", draft_form("§1.1", "… The homeland")): {"who": "Kinkade 1990", "gloss": "page 3, a quotation, Kinkade 1990:10"},
    ("§5", draft_form("§5", "If Alsea has borrowed")): {"who": "Kinkade 2005", "gloss": "page 7, a quotation, Kinkade 2005: 66-67"},
}
# The forms of footnote 1, which the engine set in the section each half of it fell on.
for _where, _who, _kind, _form, _gloss in DRAFT:
    if _kind == "cited form" and _form in ("/-muxʷ/", "/-maxʷ/", "/-n-waxʷ/", "/-awalxʷ/", "ik’awan", "k’awn", "bołkʷ",
                                           "mnłkʷa", "məłkʷ-ən", "√t’uχ", "√t’uḥʷ", "t’nχʷ"):
        SET[(_where, _form)] = {"where": "footnote 1"}
    if _kind == "cited form" and _form in ("c’a·bap", "c’a·maqak", "√c’a"):
        SET[(_where, _form)] = {"where": "footnote 2"}

# Table 3: Gloss, BeCo, CTGR (2011) and Origin. The BeCo cell is the first form and each one a
# comma joins to it; the CTGR cell runs to the word that opens the Origin cell.
ORIGIN = re.compile(r"^(‘[^’]*’)(\d?) (.+?) (French|English|unknown|Unknown|Kalapuya|Nootkan|Chinook Proper)\b(.*)$")


def table_3():
    rows = []
    start = at("Table 3: Chinook Jargon")
    caption = PAGE[start]
    heads = PAGE[start + 2] if not PAGE[start + 1] else PAGE[start + 1]
    rows.append(("Table 3", AUTHORS, "note", caption, "page 5, the caption, carrying footnote 3's mark"))
    rows.append(("Table 3", AUTHORS, "note", heads, "page 5, the column heads; CTGR is the Confederated Tribes of the Grand Ronde, footnote 3"))
    number = 0
    for text in PAGE[at(heads, start) + 1:]:
        cells = ORIGIN.match(text)
        if not cells:
            break
        number += 1
        where = "Table 3 line %d" % number
        words = cells.group(3).split()
        take = 1
        while words[take - 1].endswith(","):
            take += 1
        beco, ctgr = " ".join(words[:take]), " ".join(words[take:])
        mark = ", carries footnote %s, written here without its digit" % cells.group(2) if cells.group(2) else ""
        rows.append((where, AUTHORS, "translation", cells.group(1), "page 5, the Gloss column%s" % mark))
        rows.append((where, BC, "transcription", beco, "page 5, the BeCo column, the author's 1972 field notes"))
        if ctgr == "n/a":
            rows.append((where, AUTHORS, "note", ctgr, "page 5, the CTGR (2011) column, which has no entry"))
        else:
            rows.append((where, CJ, "transcription", ctgr, "page 5, the CTGR (2011) column, the Chinook Jargon form"))
        rows.append((where, AUTHORS, "note", (cells.group(4) + cells.group(5)).strip(), "page 5, the Origin column"))
    return rows


# Table 4: the Spanish word, its pronunciation, the reconstructed stages and the forms of each
# language, one to a row.
TABLE_4 = [
    ("Table 4", AUTHORS, "note", PAGE[at("Table 4: Spanish origin")], "page 6, the caption"),
    ("Table 4 line 1", "Spanish", "phonemic", "/elˬnabo/", "page 6, el nabo ‘the turnip’"),
    ("Table 4 line 1", "Spanish", "transcription", "[elnaβo]", "page 6, its pronunciation"),
    ("Table 4 line 1", CJ, "cited form", "*l̩nawo", "page 6, the reconstructed Jargon stage"),
    ("Table 4 line 1", CJ, "transcription", "lenawo ~ lenamo", "page 6, Chinook Jargon, Shaw 1909 and CTGR 2011"),
    ("Table 4 line 2", "Spanish", "transcription", "[enˑaβo]", "page 7, the second pronunciation"),
    ("Table 4 line 2", AUTHORS, "cited form", "*’enawu", "page 7, the reconstructed stage of the northern forms"),
    ("Table 4 line 2", "Heiltsuk", "transcription", "’ynawú", "page 7, Rath 2010"),
    ("Table 4 line 2", "Haida", "transcription", "inúˑ", "page 7, Lachler 2010"),
    ("Table 4 line 2", "Nishga", "transcription", "iinuu", "page 7, Tarpent 1987"),
    ("Table 4 line 3", AUTHORS, "cited form", "*’enahu", "page 7, the reconstructed stage of the forms below"),
    ("Table 4 line 3", "Tlingit", "transcription", "anahuˑ", "page 7, Edwards 2009"),
    ("Table 4 line 3", "Oowekyala", "transcription", "’yanahu", "page 7, First Voices"),
    ("Table 4 line 3", BC, "transcription", "’yanahu", "page 7, ‘turnip’"),
]


def table_notes(name, first, stop, page, heads_gloss):
    lines = span(first, stop)
    rows = [(name, AUTHORS, "note", lines[0], "page %d, the caption" % page)]
    if heads_gloss:
        rows.append((name, AUTHORS, "note", lines[1], "page %d, %s" % (page, heads_gloss)))
        lines = lines[1:]
    rows += [("%s line %d" % (name, number), AUTHORS, "note", one, "page %d, a line of the table" % page)
             for number, one in enumerate(lines[1:], 1)]
    return rows


ADD = tuple(
    [(None, ("title", AUTHORS, "title", TITLE, "the paper's title")),
     (None, ("title", AUTHORS, "name", AUTHORS, "the author, Independent Linguist")),
     (None, ("§1", AUTHORS, "language", BC, "the Salish language the paper is about, BeCo, now defunct")),
     (("§1.1", "Note also that Coeur..."), cited("§1.1", PS, "/*y *w/", "page 2, the glides whose reflexes the four languages share")),
     (("§1.1", "/*y *w/"), cited("§1.1", CDA, "/d gʷ/", "page 2, the reflexes of */*y *w/, Kuipers 2002")),
     (("§1.1", "/d gʷ/"), cited("§1.1", "Tillamook", "/y gʷ~g/", "page 2, the reflexes of */*y *w/, Kuipers 2002")),
     (("§1.1", "/y gʷ~g/"), cited("§1.1", "Lushootseed", "/ǯ~ʒ gʷ/", "page 2, the reflexes of */*y *w/, Kuipers 2002")),
     (("§1.1", "/ǯ~ʒ gʷ/"), cited("§1.1", "Comox", "/ǯ g/", "page 2, the reflexes of */*y *w/, Kuipers 2002")),
     (("footnote 1", "/-maxʷ/"), cited("footnote 1", "Chinook Proper", "/-kš, -ikš, -ukš/", "page 4, footnote 1, ‘ANIM PL’, = BeCo /-uks/")),
     (("footnote 1", "/-kš, -ikš, -ukš/"), cited("footnote 1", BC, "/-uks/", "page 4, footnote 1, ‘ANIM/MASS PL’")),
     (("§2", "The BeCo phonemes can be tabulated as follows:"), cited("§2", BC, "/m̩, n̩, l̩/", "page 4, the syllabic sonorants, whose syllabicity is written only where it is not predictable")),
     (("§4", "Below, I consider BeCo..."), cited("§4", BC, "C’aamas", "page 4, Victoria on Vancouver Island, where BeCo traders did business, carries footnote 2, written here without its digit")),
     (("§4.1", "tuulu"), cited("§4.1", BC, "tulu", "page 5, ‘to win, succeed’, from Chinook Jargon tulu")),
     (("§4.1", "nusaplinta"), cited("§4.1", BC, "saplin", "page 5, ‘flour’, in nusaplinta")),
     (("§4.1", "kʷułtaala"), cited("§4.1", BC, "nutaalaata", "page 5, ‘wallet’")),
     (("§4.1", "taala"), cited("§4.1", BC, "’aplsuł", "page 5, ‘apple (’apls) juice’")),
     (("§4.1", "’aplsuł"), cited("§4.1", BC, "’apls", "page 5, ‘apple’")),
     (("§4.1", "músmuski"), cited("§4.1", "Klamath", "mosmas~mosmos", "page 5, compared with Chinook Jargon musmus in Zenk et al 2010")),
     (("§4.1", "mosmas~mosmos"), cited("§4.1", "Molala", "musims", "page 6, ‘black-tailed deer’")),
     (("§4.1", "mōswa"), cited("§4.1", CJ, "saplel", "page 6, ‘flour, bread’, an old origin (1579) in Lyon 2016")),
     (("§4.2", "4.2 A Russian word in Bella Coola"), cited("§4.2", BC, "panya", "page 6, transitive-intransitive, ‘to smoke fish’, perhaps from Russian баня")),
     (("§4.2", "nusuq’ʷpalsta"), cited("§4.2", BC, "nuspanyaasta", "page 6, ‘smokehouse’")),
     (("§4.3", "4.3 A Spanish connection"), cited("§4.3", BC, "’yanahu", "page 6, ‘turnip’, a copy of Oowekyala ’yanahu")),
     (("§4.3", "’yanahu"), cited("§4.3", "Oowekyala", "’yanahu", "page 6, ‘turnip’")),
     (("§4.3", "elnaβo"), cited("§4.3", "Spanish", "el nabo", "page 6, ‘the turnip’")),
     (("§4.3", "enˑaβo"), cited("§4.3", CJ, "lenawo", "page 6, ‘turnip’, Shaw 1909")),
     (("§4.3", "lenawo"), cited("§4.3", CJ, "lamooow", "page 6, ‘turnip’, Shaw 1909, marked [sic]")),
     (("§4.3", "lamooow"), cited("§4.3", CJ, "lenamo", "page 6, ‘turnip’, CTGR 2011")),
     (("§5", "q’psttχ"), cited("§5", BC, "k’xłłcxʷ słχʷtłłc", "page 8, ‘you had seen me go through the passage’, a voiceless sentence")),
     ]
    + chained(("§1.1", "The position of BeCo..."),
              table_notes("Table 1", "Table 1: The position", "For Central Salish", 2, ""))
    + chained(("§2", "The BeCo phonemes can be tabulated as follows:"),
              table_notes("Table 2", "Table 2: BeCo phoneme", "Doubled phonemes", 4, "the column heads, set over four lines, the letters of OBSTRUENT and SONORANT spaced apart"))
    + chained(("§4.1", "A few Chinook Jargon words..."), table_3())
    + chained(("§4.3", "BeCo ’yanahu ‘turnip’..."), TABLE_4)
    + chained(("§5", "Amounts and percentages..."),
              table_notes("Table 5", "Table 5: Etymological", "These percentages", 8, "the column heads"))
)

WHOSE = (
    "The paper compares words across languages, and each cited form's who is the language the "
    "prose names before it: BeCo (Bella Coola) from the author's dictionary, Nater 1990, and his "
    "1972 field notes; Coeur d’Alene, Tillamook, Lushootseed and Comox from Kuipers 2002; Chinook "
    "Jargon from CTGR 2011, Zenk et al 2010, Shaw 1909 and Gibbs 1863; and Alsea, Chinook Proper, "
    "Quileute, South Wakashan, Makah, Nootka, Upper Chehalis, Klamath, Molala, Tahltan, Tlingit, "
    "Haida, Heiltsuk, Nishga, Oowekyala, Cree, French, Russian and Spanish as the prose cites them. "
    "A starred reconstruction goes to the language it is the ancestor of, or to Proto-Salish.\n\n"
    "The two block quotations are Kinkade 1990 and Kinkade 2005. Table 3 sets each word in four "
    "cells, its gloss, BeCo, the Chinook Jargon form and its origin. The prose, the tables' "
    "captions, the headings and the notes carry Hank Nater."
)

LETTERS = (
    "The BeCo forms are in the author's practical Americanist orthography, Nater 1990: ’ for the "
    "glottal stop and for glottalization, written after the letter, t’ and k’ʷ, ł, ƛ, χ, ʷ for "
    "rounding, and a syllabic sonorant with U+0329 where it is not predictable. Other languages keep "
    "their sources' letters: ǯ and ʒ, γ, β, ɲ, ḥ, ˑ for half length, ˬ in /elˬnabo/, · in c’a·bap, "
    "and Russian in Cyrillic, баня."
)

PAGE_NOTES = (
    "Figures 1 to 3 are maps, and only their captions are in the text layer. Table 2's heads space "
    "the letters of OBSTRUENT and SONORANT apart and are kept as printed. Footnote 1 runs from page 3 "
    "onto page 4 and is joined. Table 3's last row carries footnote 4's mark on its gloss, and its "
    "caption footnote 3's. The page's radical, which marks a root in √smiw, √t’uχ, √t’uḥʷ, √c’a and "
    "√músmuski, and the bullets of §4.2 and §5 are Symbol-font "
    "glyphs the text layer holds in the private use area, U+F0D6 and U+F0A8; the table writes them √ "
    "and ♦, read off a 300 dpi render. The ⬧ bullets of §1.1 are in the text layer as printed."
)
