# Context for Hank Nater, Origins of Velar and Pharyngeal Resonants in Interior Salish: Chains of
# Events. The sounds the paper argues over are cited one by one and given to the stage they belong
# to, Proto-Athabascan (PA), pre-Interior Salish or Interior Salish; the reconstructions (1) to (54)
# of Kuipers 2002 and (e) to (h) of Krauss & Leer 1981 are read off the page one a row, with their
# glosses and pages; the lexical copies (a) to (d) are one note each, their forms cited beside them.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import PRIVATE  # noqa: E402
from workdir import WORK  # noqa: E402
import residue  # noqa: E402

STEM = "Nater_ICSNL60_resonants"
AUTHORS = "Hank Nater"
IS = "Interior Salish"
PA = "Proto-Athabascan"
PS = "Proto-Salish"
BC = "Bella Coola"
CDA = "Coeur d’Alene"

TITLE = "Origins of Velar and Pharyngeal Resonants in Interior Salish: Chains of Events"
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


def page_of(index):
    for number in range(index, -1, -1):
        marker = re.match(r"^===== page (\d+) =====$", PAGE[number])
        if marker:
            return int(marker.group(1))
    return 0


def span(first, stop):
    """The nonempty page lines from the one opening on first up to the one opening on stop."""
    start = at(first)
    end = at(stop, start)
    return [one for one in PAGE[start:end] if one and not one.startswith("=====") and not one.isdigit()]


def draft_form(where, opening):
    for one in DRAFT:
        if one[0] == where and one[3].startswith(opening):
            return one[3]
    raise SystemExit("no draft row %s opens with %s" % (where, opening))


def cut(text, first, last=None):
    start = text.index(first)
    end = text.index(last, start) if last else len(text)
    return text[start:end].strip()


def chained(anchor, rows):
    out = []
    for row in rows:
        out.append((anchor, row))
        anchor = (row[0], row[3])
    return out


def cited(where, who, form, gloss):
    return (where, who, "cited form", form, gloss)


# A reconstruction set as a numbered or lettered line: the starred form, a remark in brackets,
# the gloss, and the pages of the source.
ENTRY = re.compile(r"^\(([0-9]+|[e-h])\) (\*\S+(?: ?/\*?\S+)*?)(?: (\([^)]*\)))? (‘.*’) \(((?:pp?\. )[^)]*)\);?"
                   r"(?: \(cf\. entry 13 above\))?$")


def entries(first, stop, who, source):
    rows = []
    start = at(first)
    end = at(stop, start)
    for index in range(start, end):
        text = PAGE[index]
        entry = ENTRY.match(text)
        if not entry:
            continue
        where = "(%s) line 1" % entry.group(1)
        page = page_of(index)
        remark = ", %s" % entry.group(3) if entry.group(3) else ""
        cf = ", cf. entry 13 above" if "cf. entry 13" in text else ""
        if entry.group(1) == "49":
            # Two reconstructions on one line, each with its gloss and page.
            rows += [(where, who, "cited form", "*səʕʷ/*səw", "page %d, a reconstruction of %s, the same as (36)" % (page, source)),
                     (where, source, "translation", "‘to flow; wetness, dew’", "page %d" % page),
                     (where, AUTHORS, "citation", "(p. 102),", "the page of %s" % source),
                     (where, who, "cited form", "*səʕʷ/*səʕ", "page %d, a reconstruction of %s" % (page, source)),
                     (where, source, "translation", "‘to drain, strain’ (a liquid)’", "page %d, printed with a closing quote inside and after the bracket" % page),
                     (where, AUTHORS, "citation", "(p. 187)", "the page of %s" % source)]
            continue
        rows += [(where, who, "cited form", entry.group(2), "page %d, a reconstruction of %s%s%s" % (page, source, remark, cf)),
                 (where, source, "translation", entry.group(4), "page %d" % page),
                 (where, AUTHORS, "citation", "(%s)" % entry.group(5), "the page of %s" % source)]
    return rows


SOUND = "a sound of the argument, "
FORMS = {
    "ɣ": ("cited form", IS, SOUND + "the velar resonant of northern Interior Salish, [ɰ]"),
    "ʕ": ("cited form", IS, SOUND + "the pharyngeal resonant of Interior Salish, [ʕ̞]"),
    "ʕʷ": ("cited form", IS, SOUND + "the rounded pharyngeal resonant of Interior Salish, [ʕ̞ʷ]"),
    "ʕw": ("cited form", IS, SOUND + "ʕʷ as Van Eijk & Nater 2020 write it, in the quotation of §4"),
    "γ": ("cited form", IS, SOUND + "ɣ as Van Eijk & Nater 2020 write it, with Greek gamma, in the quotation of §4"),
    "ɰ": ("phonetic", IS, "the pronunciation of ɣ, a velar approximant"),
    "ʕ̞": ("phonetic", IS, "the pronunciation of ʕ, a pharyngeal approximant"),
    "ʕ̞ʷ": ("phonetic", IS, "the pronunciation of ʕʷ"),
    "ɣy": ("cited form", PA, SOUND + "*ɣy, the voiced palatal continuant of PA and pre-Interior Salish, [ʝ]"),
    "ɣ̌": ("cited form", PA, SOUND + "*ɣ̌, the voiced uvular continuant of PA and pre-Interior Salish, [ʁ]"),
    "ɣ̌ʷ": ("cited form", PA, SOUND + "*ɣ̌ʷ, the rounded voiced uvular continuant of PA and pre-Interior Salish, [ʁʷ]"),
    "ʝ": ("phonetic", PA, "the pronunciation of *ɣy, a voiced palatal fricative"),
    "ḥ": ("cited form", "Columbian", SOUND + "the Columbian reflex of *ɣ̌ alongside ʕ"),
    "ḥʷ": ("cited form", "Columbian", SOUND + "the Columbian reflex of *ɣ̌ʷ alongside ʕʷ"),
    "ḥw": ("cited form", "Columbian", SOUND + "ḥʷ as Van Eijk & Nater 2020 write it"),
    "x̌": ("cited form", "Coast Salish", SOUND + "the voiceless uvular that absorbed *ɣ̌ in Coast Salish, and whose voiced allophone *ɣ̌ may have been"),
    "x̌ʷ": ("cited form", "Coast Salish", SOUND + "the rounded voiceless uvular that absorbed *ɣ̌ʷ"),
    "x̌ʷ/w": ("cited form", "Coast Salish", SOUND + "the Coast Salish reflex of *ɣ̌ʷ"),
    "/č/": ("cited form", "Salish", "page 1, the /č/ series that */ky/ became in SE Interior and most non-Bella Coola Coast Salish"),
    "z̪": ("phonetic", PA, "page 3, footnote 5, a post-PA sound, with ð"),
    "ð": ("phonetic", PA, "page 3, footnote 5, a post-PA sound, with z̪"),
    "ð̞": ("phonetic", "Lillooet", "page 3, footnote 5, the pronunciation of z in Lillooet and Thompson"),
    "š": ("cited form", "Salish", "page 6, the reflex of *xy in Interior southeast and other Coast Salish"),
    "č": ("cited form", "Cowlitz", "page 6, footnote 8, the reflex of *ky in Cowlitz, beside k"),
    "gʷ": ("cited form", "Coast Salish", "page 6, footnote 8, which replaced w in some Coast Salish"),
    "cipsx": ("cited form", BC, "page 2, footnote 2, ‘fisher’, a Common Salish word, = Coeur d’Alene cišps"),
    "cišps": ("cited form", CDA, "page 2, footnote 2, ‘fisher’, Kuipers 2002"),
    "milixʷ": ("cited form", BC, "page 2, footnote 2, ‘kinnickinnick’, whose dried leaves were smoked"),
    "mil’xʷ": ("cited form", CDA, "page 2, footnote 2, ‘tobacco’, Kuipers 2002"),
    "x̌m": ("cited form", BC, "page 2, footnote 2, ‘to bite’ = Coeur d’Alene x̌em, and ‘dead, decayed’ = Coeur d’Alene ʕem"),
    "x̌em": ("cited form", CDA, "page 2, footnote 2, ‘id.’, ‘to bite’, Kuipers 2002"),
    "t’kʷ": ("cited form", BC, "page 2, footnote 2, ‘to bleed’"),
    "t’ekʷ-s": ("cited form", CDA, "page 2, footnote 2, ‘to bleed’"),
    "ʕem": ("cited form", CDA, "‘melt, dissolve, waste away’, Reichard 1938:103; = Bella Coola x̌m ‘dead, decayed’ in footnote 2, and PA *√ɣ̌eˑn ‘melt’ in (a)"),
    "√ɣ̌eˑn": ("root", PA, "page 4, (a), *√ɣ̌eˑn ‘melt’, Krauss & Leer 1981:197"),
    "k’ʷunaʔ": ("cited form", "Lillooet", "page 4, (b), ‘salmon eggs’, Van Eijk 2013: 26"),
    "q’ʷúne": ("cited form", "Shuswap", "page 4, (b), ‘soup made of fish eggs with sceqʷm’, Kuipers 1974:249"),
    "sceqʷm": ("cited form", "Shuswap", "page 4, (b), in the gloss of q’ʷúne"),
    "ʔek’ʷn": ("cited form", "Kalispel", "page 4, (b), ‘fisheggs’, Speck 1977:175, from post-PA"),
    "ʔək’un": ("cited form", "Southern Carrier", "page 4, (b), ‘fish eggs’, the author's 1974 field notes"),
    "√k’ʷaʔ": ("root", "Carrier", "page 4, (c), ‘burp’, Story 1984:66"),
    "nu(-)q’ʷaat": ("cited form", BC, "page 4, (c), ‘to burp’, not in Nater 1994"),
    "łic’e": ("cited form", "Carrier", "page 4, (d), ‘female dog’"),
    "Ichishkíin": ("language", AUTHORS, "Yakama Sahaptin's own name, in the title of Beavert & Hargus 2009"),
    "Sínwit": ("language", AUTHORS, "in Ichishkíin Sínwit, the title of Beavert & Hargus 2009"),
}

# Broken halves and prose glued to a form, the Coeur d'Alene half of the language's name, the
# pieces of the glide reflexes the rows below give whole, and the cells of Table 2, which its
# rows hold.
DROP = ("d’Alene", "ǯ", "ǯ~ʒ", "gʷ~g", "q’uˑn", "√k’ʷàk", "łic", "general),*ɣ̌", "ɣ̌ʷ.7",
        "ɣy/*ɣ", "y/ǯ", "ɣ/y", "ʕ/ḥ", "ʕʷ/ḥʷ")

INITIALS = {}

R24 = draft_form("§1", "interior southeast:")
PART_1 = cut(R24, "For Central Salish branches", " Contact info:")
PART_2 = cut(R24, "groups began to migrate", " … The homeland")
R73 = draft_form("§2", "For decades archaeologists")
R138 = draft_form("§3", "Leer 1981:197).")
R146 = draft_form("§3", "(e) *")
R174 = draft_form("§4", "As to what prompted")

SPLIT = (
    ("front", "Hank Nater Independent Linguist", None, {}),
    ("front", "Abstract:", None, {"gloss": "page 1, the abstract"}),
    ("§1", "Figure 1: Salish divisions", None, {}),
    ("§1", "For Central Salish branches", {"gloss": "page 1, the caption of Figure 1, the tree of Salish divisions, branches and languages above it"}, {}),
    ("§1", "Contact info:", {}, {}),
    ("§1", "groups began to migrate", {"where": "front", "gloss": "page 1, the author's contact line at the foot of the page"}, {}),
    ("§1", "… The homeland thus", {}, {"who": "Kinkade 1990", "gloss": "page 2, a quotation, Kinkade 1990:10"}),
    ("§1", "Figure 2: Salish language area",
     {"kind": "citation", "gloss": "the source of the quotation above"}, {}),
    ("§1", "A striking feature",
     {"gloss": "page 2, the caption of Figure 2, a map the text layer does not carry"}, {"gloss": "page 2 and 3"}),
    ("§1", "Table 1: Proto-Athabascan", {}, None),
    ("§2", "(Seymour 2012:156–157)", {"who": "Seymour 2012", "gloss": "page 3, a quotation, Seymour 2012:156–157"}, {}),
    ("§2", "It is thus quite likely",
     {"kind": "citation", "gloss": "the source of the quotation above"}, {"gloss": "page 3 and 4"}),
    ("§2", "(a) PA *", {}, {"where": "(a) line 1", "gloss": "page 4, a lexical copy from Athabascan into Salish"}),
    ("(a) line 1", "(b) PA *", {}, {"where": "(b) line 1"}),
    ("(b) line 1", "(c) Carrier", {}, {"where": "(c) line 1"}),
    ("(c) line 1", "(d) Carrier", {}, {"where": "(d) line 1"}),
    ("(d) line 1", "For another possible", {}, {"where": "§2", "gloss": "page 4"}),
    ("§4", "While the shifts ʕ ʕw", {}, {"who": "Van Eijk & Nater 2020", "gloss": "page 6, a quotation, Van Eijk & Nater 2020:332"}),
    ("§4", "On the basis of the evidence", {"kind": "citation", "gloss": "the source of the quotation above"}, {}),
    ("§4", "I summarize my conclusions", {}, {"gloss": "page 6"}),
    ("§4", "Table 2: Reflexes", {}, {}),
    ("§4", "Reconstruction of a Common Salish", None, {"gloss": "page 6 and 7"}),
    ("references", "Kinkade, M. Dale. 1972.", {}, {}),
    ("references", "Van Eijk, Jan P. 2013.", {}, {}),
)

KRAUSS_LEER = "Krauss, Michael E. & Jeff Leer. 1981."
RESONANTS = "Athapaskan, Eyak, and Tlingit Resonants. Alaska Native Language Center Research Papers No. 5."

REMOVE = (("§1", PART_2), ("§3", R146), ("§1", "gʷ"), ("references", RESONANTS),
          ("§3", "√ǯəɣ̌ʷəƛ"), ("§3", "čʷəɣ̌ʷəs(ł)"), ("§3", "√ɣ̌ʷəǯ"), ("§3", "√ɣ̌ʷə/aˑn"))
# The reconstructions (1) to (54), read again whole off the page.
REMOVE_WHERE = r"^\(\d+\) line"

SET = {
    ("§1", PART_1): {"form": PART_1 + " " + PART_2, "gloss": "page 1 and 2, carries footnotes 1 and 2, the digits kept as printed"},
    ("§3", R138): {"form": " ".join([one[3] for one in DRAFT if one[0] in ("(54) line 2", "(54) line 3")] + [R138]),
                   "gloss": "page 5"},
    ("references", draft_form("references", "9 There does not appear")): {"where": "footnote 9", "kind": "note", "gloss": "page 7, footnote"},
    ("references", KRAUSS_LEER): {"form": KRAUSS_LEER + " " + RESONANTS},
    ("§4", "Figure 3: Traditional trade centers and networks (Walker 1997:72)"):
        {"gloss": "page 7, the caption of Figure 3, a map the text layer does not carry"},
}
# Footnote 2's forms, which the engine set in §1.
for _where, _who, _kind, _form, _gloss in DRAFT:
    if _where == "§1" and _kind == "cited form" and _form in ("cipsx", "cišps", "milixʷ", "mil’xʷ", "x̌m", "x̌em", "t’kʷ", "t’ekʷ-s", "ʕem"):
        SET[(_where, _form)] = {"where": "footnote 2"}
    if _where == "§4" and _kind == "cited form" and _form in ("č", "gʷ"):
        SET[(_where, _form)] = {"where": "footnote 8"}
    if _where == "§2" and _kind == "cited form" and _form in ("z̪", "ð", "ð̞"):
        SET[(_where, _form)] = {"where": "footnote 5"}
    if _where == "§2" and _kind == "cited form" and _form in ("√ɣ̌eˑn", "ʕem"):
        SET[(_where, _form)] = {"where": "(a) line 1"}
    if _where == "§2" and _kind == "cited form" and _form in ("k’ʷunaʔ", "q’ʷúne", "sceqʷm", "ʔek’ʷn", "ʔək’un"):
        SET[(_where, _form)] = {"where": "(b) line 1"}
    if _where == "§2" and _kind == "cited form" and _form in ("√k’ʷaʔ", "nu(-)q’ʷaat"):
        SET[(_where, _form)] = {"where": "(c) line 1"}
    if _where == "§2" and _kind == "cited form" and _form == "łic’e":
        SET[(_where, _form)] = {"where": "(d) line 1"}


def table(first, stop, name, page):
    lines = span(first, stop)
    return ([(name, AUTHORS, "note", lines[0], "page %d, the caption" % page),
             (name, AUTHORS, "note", lines[1], "page %d, the column heads" % page)] +
            [("%s line %d" % (name, number), AUTHORS, "note", one, "page %d, a line of the table" % page)
             for number, one in enumerate(lines[2:], 1)])


FIGURE_1 = [("Figure 1 line %d" % number, AUTHORS, "note", one, "page 1, a line of the tree of Figure 1")
            for number, one in enumerate(span("interior southeast:", "Figure 1: Salish"), 1)]

ADD = tuple(
    [(None, ("title", AUTHORS, "title", TITLE, "the paper's title")),
     (None, ("title", AUTHORS, "name", AUTHORS, "the author, Independent Linguist")),
     (None, ("§1", AUTHORS, "language", IS, "the branch of Salish whose resonants the paper traces")),
     (("front", "ʝ"), ("front", PA, "phonetic", "ʁ", "the pronunciation of *ɣ̌, a voiced uvular fricative")),
     (("front", "ʁ"), ("front", PA, "phonetic", "ʁʷ", "the pronunciation of *ɣ̌ʷ")),
     (("§1", "/č/"), cited("§1", PS, "*y *w", "page 1, the glides whose reflexes the four languages share")),
     (("§1", "*y *w"), cited("§1", CDA, "d gʷ", "page 1, the reflexes of *y *w, Kuipers 2002:3")),
     (("§1", "d gʷ"), cited("§1", "Comox", "ǯ g", "page 1, the reflexes of *y *w, Kuipers 2002:3")),
     (("§1", "ǯ g"), cited("§1", "Lushootseed", "ǯ~ʒ gʷ", "page 1, the reflexes of *y *w, Kuipers 2002:3")),
     (("§1", "ǯ~ʒ gʷ"), cited("§1", "Tillamook", "y gʷ~g", "page 1, the reflexes of *y *w, Kuipers 2002:3")),
     (("§2", "k’ʷunaʔ"), cited("(b) line 1", PA, "q’uˑn’", "page 4, (b), *q’uˑn’ ‘roe’, Krauss & Leer 1981:196")),
     (("§2", "√k’ʷaʔ"), ("(c) line 1", "Sarcee", "root", "√k’ʷàk’", "page 4, (c), ‘to make a choking noise’, Cook 1972:3")),
     (("§2", "łic’e"), cited("(d) line 1", "Lillooet", "łic’", "page 4, (d), ‘type of dog’, Van Eijk 2013:156")),
     (("§3", R138), ("§3", PA, "root", "√ɣ̌an", "page 5, *√ɣ̌an ‘growl’, Krauss & Leer 1981:197, which the second member of (13) resembles")),
     ]
    + chained(("§1", "First off, we should recall..."), FIGURE_1)
    + chained(("§1", "A striking feature..."), table("Table 1: Proto-Athabascan", "2 Contact with PA", "Table 1", 3))
    + chained(("§3", "Van Eijk & Nater (2020) cite..."), entries("(1) *s-mɣaw", "None of these", PS, "Kuipers 2002"))
    + chained(("§3", "The low profile of IS..."), entries("(e) *", "4 Summary", PA, "Krauss & Leer 1981"))
    + chained(("§4", "I summarize my conclusions..."), table("Table 2: Reflexes", "Reconstruction of a Common", "Table 2", 6))
)

WHOSE = (
    "The paper traces sounds across the stages of Salish and Athabascan. Each sound cited on its own "
    "goes to the stage the prose gives it: the resonants ɣ ʕ ʕʷ to Interior Salish, the continuants "
    "*ɣy *ɣ̌ *ɣ̌ʷ to Proto-Athabascan, from which the paper argues pre-Interior Salish copied them, and "
    "ḥ ḥʷ to Columbian. The reconstructions (1) to (54) are Proto-Salish and pre-Interior Salish forms "
    "of Kuipers 2002 as Van Eijk & Nater 2020 cite them, and (e) to (h) Proto-Athabascan forms of "
    "Krauss & Leer 1981; each gloss's who is its source. The words of the lexical copies (a) to (d) "
    "and of footnote 2 go to the language the prose names before each.\n\n"
    "The block quotations are Kinkade 1990, Seymour 2012 and Van Eijk & Nater 2020. The prose, the "
    "tables, the tree of Figure 1, the headings and the notes carry Hank Nater."
)

LETTERS = (
    "The forms are in Americanist letters with IPA in square brackets: ɣ, ʕ, ʕʷ, ḥ with U+0323 dot "
    "below, x̌ and ɣ̌ with U+030C caron for the uvulars, ʷ for rounding, ʔ and Ɂ for the glottal stop, "
    "’ for glottalization, and ˑ for half length. The approximants carry U+031E down tack below, ʕ̞. "
    "A star marks a reconstruction and √ a root. Van Eijk & Nater 2020, quoted in §4, write γ with "
    "Greek gamma and ʕw and ḥw with a plain w."
)

PAGE_NOTES = (
    "The page's radical before seven roots, *√ɣ̌eˑn, Carrier √k’ʷaʔ and the others, is a Symbol-font "
    "glyph the text layer holds as U+F0D6; the table writes √, read off a 300 dpi render. The text "
    "layer spaces a letter from its marks, x̌ ʷ, and the page text closes them from the glyph "
    "positions; the closing also runs together the members of four lists of sounds, ʕ̞ ʕ̞ʷ, ḥ ḥʷ, ḥ ḥw "
    "and č č’, which the table keeps apart as the glyphs space them. Figures 2 and 3 are maps, and only "
    "their captions are in the text layer; Figure 1 is a tree of text, one row a line. Footnote 9 "
    "falls among the references on page 7 and is given its own where."
)
