# Context for Lauren Schneider and Laura Griffin, On the ‘go’: Exploring auxiliary-hood in
# ʔayʔaǰuθəm in comparative perspective. The examples come from four languages, each named once at
# the right of the first line of the first example in its run: every tier of an example goes to its
# language, the name is taken off the line, and the speaker and source closing a translation or a
# gloss line are parted from it. Tables 1 and 2, Figure 2, footnote 10's example and the examples the
# text layer broke are read off the page again; so are the references, whose links the text layer
# set at the head of the entry after them.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import PRIVATE  # noqa: E402
from workdir import WORK  # noqa: E402
import residue  # noqa: E402
from finish import TRAILER  # noqa: E402

STEM = "SchneiderGriffinICSNL60"
AUTHORS = "Lauren Schneider and Laura Griffin"
AY = "ʔayʔaǰuθəm"
HUL = "Hul’q’umi’num’"
SEN = "SENĆOŦEN"
KLA = "Klallam"
PIDGIN = "Nigerian Pidgin"

TITLE = "On the ‘go’: Exploring auxiliary-hood in ʔayʔaǰuθəm in comparative perspective"
BYLINE = "Lauren Schneider, University of Arizona, and Laura Griffin, University of Toronto"

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
    return [(index, PAGE[index]) for index in range(start, end)
            if PAGE[index] and not PAGE[index].startswith("=====") and not PAGE[index].isdigit()]


def joined(first, stop):
    return " ".join(text for index, text in span(first, stop))


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


def page_in(gloss):
    return re.search(r"page (\d+)", gloss).group(1)


# The language of each example, from the name at the right of the first example of each run.
LANGUAGE = {}
for _numbers, _language in (((1, 4, 5, 9, 10, 11, 12, 13, 32, 33, 45), HUL),
                            ((14, 15, 16, 17, 34, 35, 36, 37, 38, 46, 47), SEN),
                            ((6, 7, 8), KLA), ((48,), PIDGIN)):
    for _number in _numbers:
        LANGUAGE[_number] = _language
LABELS = (HUL, AY, SEN, KLA, PIDGIN)


def number_of(where):
    found = re.match(r"^\((\d+)[a-z]?\) line", where)
    return int(found.group(1)) if found else None


SET = {}
SPLIT = []


def set_row(at_where, at_form, **change):
    SET.setdefault((at_where, at_form), {}).update(change)


for _where, _who, _kind, _form, _gloss in DRAFT:
    _number = number_of(_where)
    if _number is None:
        continue
    _language = LANGUAGE.get(_number, AY)
    if _kind in ("transcription", "segmentation", "gloss"):
        set_row(_where, _form, who=_language)
    # The language printed at the right of an example's first line, and a footnote digit before it.
    for _label in LABELS:
        if _kind in ("transcription", "segmentation") and _form.endswith(" " + _label):
            _bare = _form[:-len(_label) - 1]
            _note = ""
            _mark = re.match(r"^(.*[.!?’”])(\d{1,2})$", _bare)
            if _mark:
                _bare, _note = _mark.group(1), ", carries footnote %s, written here without its digit" % _mark.group(2)
            set_row(_where, _form, form=_bare,
                    gloss="page %s%s, %s printed at the right of the line" % (page_in(_gloss), _note, _label))
    # The speaker and the source closing a translation or a gloss line: (DL) (Schneider 2024b:4).
    if _kind in ("translation", "gloss") and not (_kind == "translation" and TRAILER.match(_form)):
        _tail = re.search(r" (\((?:[^()]|\([^()]*\))*\)(?: \((?:[^()]|\([^()]*\))*\))*)$", _form)
        if _tail and re.search(r"\d|[A-Z]{2}|&", _tail.group(1)):
            if _kind == "gloss":
                set_row(_where, _form[:_tail.start()].strip(), who=_language)
            SPLIT.append((_where, _tail.group(1), {}, {"kind": "citation", "who": AUTHORS,
                          "gloss": "page %s, the speaker and the source at the right of the line" % page_in(_gloss)}))

# (2) and (23) are said in the story in quotes, and the engine took the quoted line for a
# translation; (28) opens without its number's line of words.
R2 = draft_form("(2) line 1", "“nɛ")
R2_SEG = draft_form("(2) line 1", "niʔ=č")
set_row("(2) line 1", R2, kind="transcription", who=AY, gloss="page 5, the line as spoken in the story, in quotes, ʔayʔaǰuθəm printed at the right")
set_row("(2) line 1", R2_SEG, where="(2) line 2", kind="segmentation")
set_row("(2) line 2", draft_form("(2) line 2", "be.there"), where="(2) line 3")
set_row("(2) line 3", "“And I’ll be way up there.”", where="(2) line 4")
set_row("(2) line 3", "(Mink and Eagle:line 8)", where="(2) line 4", kind="citation", gloss="page 5, the story and its line")
set_row("(2) line 3", "(Paul to appear:88)", where="(2) line 4", kind="citation", gloss="page 5, the source")
R23 = draft_form("(23) line 1", "“hotᶿəm")
set_row("(23) line 1", R23, kind="transcription", who=AY, gloss="page 12, the lines as spoken in the story, in quotes")
set_row("(23) line 1", draft_form("(23) line 1", "hu=tᶿ"), where="(23) line 2", kind="segmentation")
set_row("(23) line 2", draft_form("(23) line 2", "go=1SG"), where="(23) line 3")
set_row("(23) line 2", draft_form("(23) line 2", "yaɬ-at"), where="(23) line 4", kind="segmentation")
set_row("(23) line 3", draft_form("(23) line 3", "call-CTR"), where="(23) line 5")
set_row("(23) line 4", draft_form("(23) line 4", "“I’m going"), where="(23) line 6")
set_row("(23) line 5", "(Mink and Greybird:line 21)", where="(23) line 6", kind="citation", who=AUTHORS, gloss="page 12, the story and its line")
set_row("(23) line 5", "(Paul to appear:13)", where="(23) line 6")
R28 = draft_form("(28) line 1", "Ɂa~Ɂaqʷ")
set_row("(28) line 1", R28, form=R28[:-len(AY) - 1], kind="segmentation", who=AY, gloss="page 14, ʔayʔaǰuθəm printed at the right of the line")
set_row("(28) line 2", draft_form("(28) line 2", "PROG~go"), kind="gloss", who=AY, gloss="page 14")
set_row("(28) line 3", "(Watanabe 2003:94)", kind="citation", gloss="page 14, the source")
set_row("(16) line 4", "ʔə tθə ʔəšés", kind="segmentation")
set_row("(37) line 4", "ʔə tθə θáʔtx̣", kind="segmentation")
set_row("(43) line 1", draft_form("(43) line 1", "kʷihit x̌ax̌aɫ"), form="kʷihit x̌ax̌aɫ Tony hu Gloria", kind="segmentation",
        gloss="page 17, ʔayʔaǰuθəm printed at the right of the line")

# Footnote 2 runs to page 2's foot; the engine took its last lines for comment rows of (1).
FOOTNOTE_2 = joined("2 Abbreviations used", "===== page 3")
set_row("footnote 2", draft_form("§1", "2 Abbreviations"), form=FOOTNOTE_2)
R4 = draft_form("(4c) line 4", "In (4), the NP subject")
R4_END = "but cannot follow the auxiliary ni’ ‘there/then’."
set_row("§3", R4_END, form=R4 + " " + R4_END)
FOOTNOTE_10 = "10 Note that hu can take causative morphology when it behaves as a main verb, see below:"
set_row("footnote 10", draft_form("§4", "10 Note that"), form=FOOTNOTE_10)

SPLIT += [
    ("front", "Abstract:", None, {"gloss": "page 1, the abstract"}),
    ("§2", "Halkomelem is a Central Salish", {"gloss": "page 4, the caption of Figure 1, a map the text layer does not carry"}, {}),
    ("§3", "As demonstrated by Table 1", None, {}),
    ("§4", "As with the other Central Salish", None, {}),
    ("§5.1", "In this example, θo", None, {}),
    ("§5.2", "The SENĆOŦEN examples that mirror", None, {}),
]

# The candidates the rebuilt examples and tables hold.
REMOVE = [("(4c) line 4", R4),
          ("(2) line 1", AY), ("(42)", "(42)"), ("§5.1", "(42) Ɂaǰ-am-a-t-as θu Ɂə=kʷ=naɁa nənqəm."),
          ("§4", "(i) hɛhɛw tihmot šɛ k̓ʷaxʷa θohosxʷasoɬ."),
          ("§5.2", draft_form("§5.2", "more tall Tony")),
          ("§4", "Hul’q’umi’num’ nem’ no yes SENĆOŦEN YÁ¸ yes! yes!"), ("§4", "ʔayʔaǰuθəm ho/θo yes no!"),
          ("§4", "!Based on observations from texts; negative data needed."), ("§4", "SENĆOŦEN"), ("§4", "YÁ¸")]
REMOVE += [(one[0], one[3]) for one in DRAFT if one[0] in ("(1) line 5", "(1) line 6", "(1) line 7",
                                                           "(8c) line 3", "(8c) line 4", "(8c) line 5")]
REMOVE += [("§3", one) for one in ("ʔa", "həyeʔ=č", "həyeʔ-stǝxʷ", "tθǝ", "sqʷəmey̓", "nem̓", "yéʔ")]
REMOVE += [("§4", one) for one in ("ʔaw̓θ", "ʔut", "ʔuwk̓ʷ", "čaʔat", "hu/θu", "ǰaqaʔ", "kʷən", "ƛ̓iʔ", "namaɬ", "niš",
                                   "payaʔ", "qəǰi", "qʷəl̓", "χʷit", "χʷuχʷ", "hɛhɛw", "šɛ", "k̓ʷaxʷa", "θohosxʷasoɬ",
                                   "šə=k̓ʷaxʷa", "θu~hu-sxʷ-as-uɬ")]
REMOVE += [("§5.1", one) for one in ("Ɂaǰ-am-a-t-as", "θu", "Ɂə=kʷ=naɁa", "nənqəm")]
# The references, read again whole below.
REMOVE += [(one[0], one[3]) for one in DRAFT if one[0] == "references" and one[2] == "note"]
REMOVE_WHERE = r"^\(13\) line|^Table 2 line"
KIND_RULES = (("^references$", "^note$", ".", "reference", AUTHORS, "read again below"),)

EXAMPLE_13 = [("(13) line 1", HUL, "transcription", "?’aa, huye’ ch huye’stuhw tthu sqwumey’.",
               "page 8, the question mark marks it awkward, Hul’q’umi’num’ printed at the right of the line"),
              ("(13) line 2", HUL, "segmentation", "ʔa: həyeʔ=č həyeʔ-stǝxʷ tθǝ sqʷəmey̓", "page 8"),
              ("(13) line 3", HUL, "gloss", "Ah leave=2SG.SUBJ leave-CS DET dog", "page 8"),
              ("(13) line 4", AUTHORS, "note", "Intended:", "page 8, the label before the translation"),
              ("(13) line 4", "Schneider 2024b", "translation", "‘Ah, you take the dog away.’", "page 8, the intended meaning"),
              ("(13) line 4", AUTHORS, "citation", "(DL) (Schneider 2024b:74)", "page 8, the speaker and the source at the right of the line")]
EXAMPLE_42 = [("(42) line 1", AY, "segmentation", "Ɂaǰ-am-a-t-as θu Ɂə=kʷ=naɁa nənqəm.", "page 17"),
              ("(42) line 2", AY, "gloss", "change-MD-LV-CTR-3ERG go OBL=DET=FILL.PRT blackfish", "page 17"),
              ("(42) line 3", "Watanabe 2022a", "translation", "‘They would change him into a blackfish.’", "page 17"),
              ("(42) line 3", AUTHORS, "citation", "(MG016) (Watanabe 2022a:319)", "page 17, the text and the source at the right of the line")]
EXAMPLE_43 = [("(43) line 2", AY, "gloss", "more tall Tony go Gloria", "page 17"),
              ("(43) line 3", "Davis & Mellesmoen 2019", "translation", "‘Tony is taller than Gloria.’", "page 18, after footnote 14"),
              ("(43) line 3", AUTHORS, "citation", "(Davis & Mellesmoen 2019:32)", "page 18, the source at the right of the line")]
EXAMPLE_48 = [("(48) line 1", PIDGIN, "transcription", "Nyam swit pas rays.", "page 18, Nigerian Pidgin printed at the right of the line"),
              ("(48) line 2", PIDGIN, "gloss", "yam be.tasty pass rice", "page 18"),
              ("(48) line 3", "Faraclas 1996", "translation", "‘Yam is more delicious than rice.’", "page 18"),
              ("(48) line 3", AUTHORS, "citation", "(Faraclas 1996:11)", "page 18, the source at the right of the line")]
FOOTNOTE_10_I = [("footnote 10 (i) line 1", AY, "transcription", "hɛhɛw tihmot šɛ k̓ʷaxʷa θohosxʷasoɬ.", "page 12"),
                 ("footnote 10 (i) line 2", AY, "segmentation", "hihiw tih-mut šə=k̓ʷaxʷa θu~hu-sxʷ-as-uɬ.", "page 12"),
                 ("footnote 10 (i) line 3", AY, "gloss", "really big-INT DET=box PROG~go-CAUS-3ERG-PST", "page 12"),
                 ("footnote 10 (i) line 4", "Huijsmans 2023", "translation", "‘He was bringing a really big box.’", "page 12"),
                 ("footnote 10 (i) line 4", AUTHORS, "citation", "(vf | EP 2021/09/04, from Huijsmans 2023: 17)",
                  "page 12, volunteered by the speaker, EP, on the date, and the source")]
PROSE_8 = ("§3", AUTHORS, "note", joined("These examples show that", "6 For this paper"), "page 7")

FIGURE_2 = [("Figure 2", AUTHORS, "note", "Auxiliary8 *precede NP argument bleaching Main predicate",
             "page 10, the labels along the continuum, Auxiliary carries footnote 8"),
            ("Figure 2", AUTHORS, "note", joined("|---", "nem̓ (hur)"), "page 10, the continuum drawn as a rule in three parts"),
            ("Figure 2", AUTHORS, "note", "nem̓ (hur) yéʔ (str) həyeʔ (hur)", "page 10, the verbs placed along the continuum"),
            ("Figure 2", HUL, "cited form", "nem̓", "page 10, (hur) Halkomelem, at the auxiliary end"),
            ("Figure 2", SEN, "cited form", "yéʔ", "page 10, (str) Northern Straits, between bleaching and the main predicate"),
            ("Figure 2", HUL, "cited form", "həyeʔ", "page 10, (hur) Halkomelem, at the main predicate end"),
            ("Figure 2", AUTHORS, "note", "Figure 2: Representation of the lexical verb-auxiliary continuum", "page 10, the caption")]

TABLE_1 = [("Table 1", AUTHORS, "note", "Table 1: Auxiliaries in ʔayʔaǰuθəm (Watanabe 2003:90)", "page 11, the caption"),
           ("Table 1", AUTHORS, "note", "ʔayʔaǰuθəm English gloss", "page 11, the column heads")]
for _number, (_index, _text) in enumerate(span("ʔaw̓θ suddenly", "As with the other Central"), 1):
    _form, _gloss = _text.split(" ", 1)
    _mark = re.match(r"^(.*?)(\d{1,2})$", _gloss)
    _note = ", carries footnote %s, written here without its digit" % _mark.group(2) if _mark else ""
    TABLE_1 += [("Table 1 line %d" % _number, AY, "cited form", _form, "page 11, an auxiliary of Watanabe 2003:90"),
                ("Table 1 line %d" % _number, "Watanabe 2003", "translation", _mark.group(1) if _mark else _gloss,
                 "page 11, the English gloss%s" % _note)]

TABLE_2 = [("Table 2", AUTHORS, "note", "Table 2: Comparing pre-predicate ‘go’ verbs in three Central Salish languages", "page 14, the caption"),
           ("Table 2", AUTHORS, "note", "‘go’ verb *precede NP argument bleaching", "page 14, the column heads")]
for _number, (_language, _verb, _precede, _bleach) in enumerate(((HUL, "nem’", "no", "yes"), (SEN, "YÁ¸", "yes!", "yes!"),
                                                                (AY, "ho/θo", "yes", "no!")), 1):
    TABLE_2 += [("Table 2 line %d" % _number, AUTHORS, "note", " ".join((_language, _verb, _precede, _bleach)), "page 14, a line of the table"),
                ("Table 2 line %d" % _number, _language, "cited form", _verb,
                 "page 14, the %s ‘go’ verb: precedes an NP argument %s, bleaching %s" % (_language, _precede, _bleach))]
TABLE_2.append(("Table 2", AUTHORS, "note", "!Based on observations from texts; negative data needed.", "page 14, the note under the table"))


def references():
    """One reference an entry, a line opening on a name and a comma that carries the entry's year
    starting each."""
    entries = []
    for index in range(at("Aikhenvald, Alexandra Y. 2011."), len(PAGE)):
        text = PAGE[index]
        if not text or text.startswith("=====") or text.isdigit():
            continue
        if re.match(r"^[A-Z][\w’'\-]+(?: [A-Z][\w’'\-]+)?, [A-Z]", text) and \
                re.search(r"\b(?:1[89]|20)\d\d[a-z]?(?:/\d{4})?\.|\bto appear\.", text):
            entries.append([text, page_of(index)])
        else:
            entries[-1][0] += " " + text
    return [("references", AUTHORS, "reference", text, "page %d" % page) for text, page in entries]


REFERENCES = references()

ADD = tuple(
    [(None, ("title", AUTHORS, "title", TITLE, "the paper's title")),
     (None, ("title", AUTHORS, "name", "Lauren Schneider", "an author, University of Arizona")),
     (None, ("title", AUTHORS, "name", "Laura Griffin", "an author, University of Toronto")),
     (("(8c) line 2", "(Montler 2003:115)"), PROSE_8)]
    + chained(("§3", "This type of construction resembles..."), EXAMPLE_13)
    + chained(("§3", "While no cross-linguistic tests exist..."), FIGURE_2)
    + chained(("§4", "Watanabe (2003:§12.2) details..."), TABLE_1)
    + chained(("footnote 10", "10 Note that..."), FOOTNOTE_10_I)
    + chained(("§4", "The presence of a ‘going to go’..."), TABLE_2)
    + chained(("§5.1", "There is evidence of bleaching..."), EXAMPLE_42)
    + chained(("(43) line 1", "kʷihit x̌ax̌aɫ..."), EXAMPLE_43)
    + chained(("§5.2", "The construction in (47) is of interest..."), EXAMPLE_48)
    + chained(("references", "References"), REFERENCES)
)

FORMS = {
    AY: ("language", AUTHORS, "Comox-Sliammon, ISO coo, the Central Salish language of the paper"),
    "ʔayʔaǰuθəm-speaking": ("language", AUTHORS, "page 19, of the people who speak ʔayʔaǰuθəm"),
    HUL: ("language", AUTHORS, "the Island dialect of Halkomelem, Cowichan and Nanaimo"),
    "Hul’q’umi’num": ("language", AUTHORS, "Hul’q’umi’num’, printed here without its last apostrophe"),
    "Hulʼqʼumiʼnumʼ": ("language", AUTHORS, "Hul’q’umi’num’ in the title of Schneider 2021, with U+02BC"),
    SEN: ("language", AUTHORS, "Saanich, a dialect of Northern Straits Salish"),
    "hən̓q̓əmin̓əm̓": ("language", AUTHORS, "page 4, the Downriver dialect of Halkomelem, Musqueam"),
    "Halq’eméylem": ("language", AUTHORS, "page 4, the Upriver dialect of Halkomelem, Chilliwack"),
    "Lək̓ʷəŋín̓əŋ": ("language", AUTHORS, "page 4, Lekwungen or Songhees, a dialect of Northern Straits"),
    "T’Sou-ke": ("language", AUTHORS, "page 4, Sooke, a dialect of Northern Straits"),
    "Xwlemi’chosen": ("language", AUTHORS, "page 4, Lummi, the dialect of Northern Straits in Washington State"),
    "W̱SÁNEĆ": ("name", SEN, "page 4, the W̱SÁNEĆ School Board, which offers SENĆOŦEN immersion schooling"),
    "Tla’amin": ("name", AY, "one of the four communities where ʔayʔaǰuθəm is spoken"),
    "K’ómoks": ("name", AY, "one of the four communities where ʔayʔaǰuθəm is spoken"),
    "Qayx̣": ("name", AY, "page 3, Mink, of Elsie Paul's stories"),
    "t’itsum": ("cited form", HUL, "page 3, ‘swim’, in (1)"),
    "kwunut": ("cited form", HUL, "page 3, ‘take it’, in (1)"),
    "tsun": ("cited form", HUL, "page 3, ‘I’, the second-position clitic of (1)"),
    "shun’tsu": ("cited form", HUL, "page 3, in thunu shun’tsu ‘my catch’ of (1)"),
    "Ɂi": ("cited form", HUL, "page 6, ‘here (and now)’, an auxiliary, Gerdts 1988:22"),
    "niɁ": ("cited form", HUL, "page 6, ‘there (and then)’, an auxiliary, Gerdts 1988:22"),
    "m’i": ("cited form", HUL, "page 6, ‘come’, an auxiliary, Gerdts 1988:22"),
    "huye’stuhw": ("cited form", HUL, "page 8, ‘go take it away’, in nem’ huye’stuhw"),
    "/həyeʔ/": ("cited form", HUL, "page 9, ‘leave, depart’, the cognate of SENĆOŦEN /yéʔ/"),
    "həyeʔ": ("cited form", HUL, "page 9, ‘leave’"),
    "(hə)yeʔ": ("cited form", "Central Salish", "page 10, ‘depart’, Hul’q’umi’num’ həyeʔ and SENĆOŦEN yéʔ at once"),
    "ts’tem": ("cited form", HUL, "page 15, ‘crawl’"),
    "/-nəs/": ("cited affix", HUL, "page 15, footnote 12, the directional applicative"),
    "/c̓tem-nəs/": ("cited form", HUL, "page 15, footnote 12, ‘crawl to’"),
    "qəl̓et": ("cited form", HUL, "page 18, ‘again’"),
    "YÁ¸": ("cited form", SEN, "‘go, depart’"),
    "/yéʔ/": ("cited form", SEN, "page 9, ‘go, depart’"),
    "hiyáʔ": ("cited form", KLA, "page 7, ‘go’, an auxiliary, Montler 2003:114"),
    "ʔənʔá": ("cited form", KLA, "page 7, ‘come’, an auxiliary, Montler 2003:114"),
    "ƛ̓áy": ("cited form", KLA, "page 7, ‘again’, an auxiliary, Montler 2003:114"),
    "húy": ("cited form", KLA, "page 7, ‘finish’, an auxiliary, Montler 2003:114"),
    "/hiyáʔ/": ("cited form", KLA, "page 9, footnote 7, ‘go away’, Campbell 2023:62"),
}

DROP = ("SENĆOŦEN.7", "Bätscher", "Zúñiga", "Kittilä", "Honoré")
INITIALS = {}

WHOSE = (
    "Each example's tiers go to the language named at the right of the first example of its run: "
    "Hul’q’umi’num’ for (1), (4) and (5), (9) to (13), (32), (33) and (45); SENĆOŦEN for (14) to (17), "
    "(34) to (38), (46) and (47); Klallam for (6) to (8); Nigerian Pidgin for (48); ʔayʔaǰuθəm for the "
    "rest and for footnote 10's example. Each translation goes to the work it is cited from, and the "
    "speaker's initials and the source after it make a citation row. The words the prose cites go to "
    "the language the prose names before them. The prose, the tables, Figure 2 and the notes carry the "
    "authors."
)

LETTERS = (
    "ʔayʔaǰuθəm is written two ways: the practical line of Paul's stories, with ɛ and ɩ, and the "
    "Americanist line beneath it, with ǰ, x̌, χ and ʔ or Ɂ. Hul’q’umi’num’ examples set the community "
    "orthography, with ’ and tth, over an Americanist line with ̓ U+0313 and tθ; SENĆOŦEN examples set "
    "the capitals of its alphabet, with ¸ and W̱, over an Americanist line. Square brackets mark words "
    "unpronounced in fast speech, * an ungrammatical sentence and ? an awkward one."
)

PAGE_NOTES = (
    "Closing the space after a mark below runs three pairs of words together that the page sets apart, "
    "SW̱ YÁ¸ in (36) and (38), qayx̣ kʷum in (24) and Qayx̣ (Mink) on page 3; a 400 dpi render and the "
    "glyph positions part them. Figure 1 is a map, and only its caption is in the text layer. The text "
    "layer sets each reference's link at the head of the entry after it; the references are read off "
    "the page again one entry a row."
)
