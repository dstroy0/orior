# Context for Davis and Nederveen, Intransitive -t in Salish.
# The paper compares four Interior Salish languages, and each example's heading names its language.
# Table 1, the displays of Section 3.2 and the lines of (21), (23) to (25) and (30) are read off the
# page text below by the words each line opens with. The page text is read the way residue.py
# reads it, with the corrections of residue.CORRECTIONS applied.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import PRIVATE  # noqa: E402
from workdir import WORK  # noqa: E402
import residue  # noqa: E402

STEM = "Davis-NederveenICSNL60"
AUTHORS = "Henry Davis and Sander Nederveen"
N = "nɬeʔkepmxcín"
S = "Secwepemctsín"
L = "St’át’imcets"
X = "nxaʔamxcín"

TITLE = "Intransitive -t in Salish"
BYLINE = "Henry Davis and Sander Nederveen, University of British Columbia"

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


def display(anchor, rows):
    """ADD entries for rows (where, who, kind, form, gloss), each placed after the one before it,
    the first after anchor."""
    added = []
    for row in rows:
        added.append((anchor, row))
        anchor = (row[0], row[3])
    return added


def table_1():
    """The cells of Table 1 on page 4, one cited form a cell under the language that heads its row.

    A row opens with the language's English name, one or more capitalized words and (River), and
    then has one cell for each of ‘long/tall’, ‘wide’ and ‘thick’. A cell can hold two forms joined
    by / or a comma. A ? is a cell with no data.
    """
    heads = ("‘long/tall’", "‘wide’", "‘thick’")
    start = PAGE.index(line("Language ‘long/tall’ ‘wide’ ‘thick’"))
    rows = [("Table 1", AUTHORS, "note", line("Table 1: Cross-Salishan"), "page 4, the caption"),
            ("Table 1", AUTHORS, "note", PAGE[start], "page 4, the column heads")]
    for number, text in enumerate(PAGE[start + 1:start + 22], 1):
        tokens = text.split()
        name = []
        while tokens and (re.fullmatch(r"[A-Z][A-Za-z’-]*", tokens[0]) or tokens[0] == "(River)"):
            name.append(tokens.pop(0))
        cells = []
        for token in tokens:
            if cells and (token == "/" or cells[-1].endswith((" /", ","))):
                cells[-1] += " " + token
            else:
                cells.append(token)
        if len(cells) != 3:
            raise SystemExit("Table 1 row %d has %d cells: %s" % (number, len(cells), text))
        language = " ".join(name)
        rows.append(("Table 1 line %d" % number, AUTHORS, "note", language,
                     "page 4, the language of the row, its traditional English name (footnote 2)"))
        for head, cell in zip(heads, cells):
            if cell == "?":
                continue
            gloss = "page 4, Table 1, %s in %s" % (head, language)
            if cell.startswith("("):
                gloss += ", in parentheses, not cognate with the common -t forms (footnote 2)"
            rows.append(("Table 1 line %d" % number, language, "cited form", cell, gloss))
    return rows


FORMS = {
    N: ("language", AUTHORS, "a.k.a. Thompson River Salish, ISO 639-3 thp, Northern Interior Salish, one of the four languages compared"),
    S: ("language", AUTHORS, "a.k.a. Shuswap, ISO 639-3 shs, Northern Interior Salish, one of the four languages compared"),
    L: ("language", AUTHORS, "a.k.a. Lillooet, ISO 639-3 lil, Northern Interior Salish, one of the four languages compared"),
    X: ("language", AUTHORS, "a.k.a. Moses-Columbia Salish, ISO 639-3 col, Southern Interior Salish, one of the four languages compared"),
    "Secwepmctsín": ("language", AUTHORS, "Secwepemctsín, printed without its second e on page 5"),
    "St’at’imcets": ("language", AUTHORS, "St’át’imcets, printed without its accent"),
    "St’á’timcets": ("language", AUTHORS, "St’át’imcets, printed with its accent and apostrophe swapped on page 12"),
    "St’átimcets": ("language", AUTHORS, "St’át’imcets, printed without its second apostrophe"),
    "St’át'imcets": ("language", AUTHORS, "St’át’imcets, printed with a straight second apostrophe on page 24"),
    "nɬɬeʔkepmxcín": ("language", AUTHORS, "nɬeʔkepmxcín, printed with its ɬ doubled in the grant title of footnote *"),
    "nxa’amxcín/Moses": ("language", AUTHORS, "nxa’amxcín/Moses Columbia, as footnote 1 names the language of Willet 2003"),
    "Nxa’amxcín": ("language", AUTHORS, "in the title of Willet 2003"),
    "Seliš/Montana": ("language", AUTHORS, "Seliš/Montana Salish, a Southern Interior language"),
    "Seliš": ("language", AUTHORS, "in the title of Pete 2011"),
    "D’Alene": ("language", AUTHORS, "Coeur D’Alene, a Southern Interior language"),
    "SENĆOTEN": ("language", AUTHORS, "Northern Straits Salish, the language of Kiyota 2008"),
    "Nlaka’pamux": ("name", AUTHORS, "the people whose lands Bernice Garcia’s home is in, footnote *"),
    "Qwa7yán’ak": ("name", AUTHORS, "the St’át’imcets name of Carl Alexander, footnote *"),
    "kʷaɬɬtèzetkʷuʔ": ("name", AUTHORS, "the nɬeʔkepmxcín name of Bernice Garcia, footnote *"),
    "c̓úʔsinek": ("name", AUTHORS, "the nɬeʔkepmxcín name of Marty Aspinall, footnote *"),
    "Calderón-Corona": ("name", AUTHORS, "Mariana Calderón-Corona, an editor in Nederveen 2024"),
    "Sqwéqwel": ("cited form", L, "‘stories’, Sqwéqwel’ in the title of Davis 2022 and Mitchell 2022"),
    "St’át’imc": ("name", AUTHORS, "the Upper St’át’imc Language, Culture and Education Society, a publisher in Davis 2022"),
    "múta7": ("cited form", L, "‘and’, in the title of Edwards et al. in preparation"),
    "Skelkela7llkálha": ("cited form", L, "‘Legends and Stories of our Ancestors’, in the title of Edwards et al. in preparation"),
    "Sqwéqwel’s": ("cited form", L, "in the title of Edwards et al. 2017"),
    "Skelkekla7lhkálha": ("cited form", L, "‘Tales of our Elders’, in the title of Edwards et al. 2017"),
    "<ʔ>": ("cited affix", AUTHORS, "the inchoative infix of strong roots"),
    "√CoS": ("notation", AUTHORS, "a change-of-state root, in the formulas of (23) and (25)"),
    "ʔac-": ("cited affix", AUTHORS, "the stative prefix, reconstructed *ʔac-, found across the family"),
    "-ɬ": ("cited affix", AUTHORS, "the suffix that replaced adjectival -t on dimensional adjectives in Tsamosan"),
    "-wíl̓x": ("cited affix", AUTHORS, "the ‘developmental’ suffix, reconstructed *-wíl̓x"),
    "-út": ("cited affix", AUTHORS, "a stressed variant of -t, footnote 13"),
    "-ét": ("cited affix", AUTHORS, "a stressed variant of -t, footnote 13"),
    "-míx": ("cited affix", X, "the imperfective suffix, -míx ~ -mx"),
    "Nyoʔnuntn": ("cited form", "Montana Salish", "in the title of Pete 2011, Seliš Nyoʔnuntn (Medicine for the Salish Language)"),
}

# The language names with a footnote mark or a bracket left on them, each already a row of its
# section, and the words the formulas and the tables put in the candidates.
DROP = ("glossed", "St’át’imcets7", "nxaʔamxcín.10", "St’át’imcets).14", "Secwepemctsín).18",
        "ʔac-marked", "λxλe[m↑m(x)(init(e))(x)(fin(e", "λg", "λxλe", "∃", "([[√CoS]])")

SPLIT = (
    ("front", "Henry Davis and Sander Nederveen University", None, {}),
    ("front", "Abstract:", None, {}),
    ("§2.1", "Table 1: Cross-Salishan", {}, None),
    ("§2.1", "The most striking thing about Table 1", None, {}),
    ("(12) line 1", "15 It is important", {},
     {"where": "footnote 15", "gloss": "page 13, footnote, set between the heading of (12) and its tiers"}),
    ("(17) line 5", "It is also worth pointing out", {"who": "Carl Alexander"},
     {"where": "§3.1", "who": AUTHORS, "kind": "note", "gloss": "page 14"}),
    ("§3.2.2", "(21) Difference functions", {"where": "footnote 16", "gloss": "page 16, the end of footnote 16, carried over from page 15"}, None),
    ("§3.2.2", "The definition in (21) states", None, {}),
    ("§3.2.2", "In other words, a COS root", None, {}),
    ("§3.2.2", "In other words, intransitive -t takes", None, {}),
    ("§3.2.2", "COS roots suffixed with intransitive -t thus", None, {}),
    ("§3.3", "St’át’imcets inchoative marking does not entail", {},
     {"where": "(26) heading", "gloss": "page 19, the heading over (26) to (28)"}),
    ("§3.3", "Interpretive Economy maximizes", None, {}),
)

INTRODUCTION = ("ʔes ʔúməcms kʷəɬtèzétkʷuʔ təw ɬe c̓əɬétkʷu wéʔe ncitxʷ. ƛ̓uʔ wéʔec ʔex netíyxs "
                "scwew̓ xmx, ƛ̓uʔ tékm xéʔe ne nɬeʔképmx e tmixʷs.")
TRANSLATION = "My traditional name is kʷəɬtèzetkʷuʔ, my home is in Coldwater of ‘Nicola’ of Nlaka’pamux lands."

# The table's cells and the words of Bernice Garcia's introduction, each a row of its own below.
REMOVE = tuple(
    [("§2.1", form) for where, who, kind, form, gloss in DRAFT
     if where == "§2.1" and kind == "cited form" and gloss.startswith("candidate, page 4") and form != "-ɬ"] +
    [("§1", form) for where, who, kind, form, gloss in DRAFT
     if where == "§1" and kind == "cited form" and gloss.startswith("candidate, page 1") and
     re.sub(r"[.,]", "", form) in (INTRODUCTION + " " + TRANSLATION).replace(".", "").replace(",", "").split()] +
    [("footnote 2", "2 Here and in the rest..."),
     ("§2.1", "and Tillamook are omitted..."),
     ("(32) line 2", "(w)ʔex"),
     # The engine reads (23) to (25) as displays; the rows below split their lines and their sources.
     ("(23) line 1", "[[√CoS]]..."),
     ("(24) line 1", "[[ -t ]]..."),
     ("(25) line 1", "[[ -t ]]..."),
     ("(33) line 2", "(w)ʔex")])

# The consultant of the St'át'imcets examples is Carl Alexander, footnote *.
INITIALS = {"Consultant": "Carl Alexander"}

# The heading of (13) defines the language nɬeʔkempxcín.
WHO = {"nɬeʔkempxcín": N, "St’át’imcets; Davis 2024:310": "Davis 2024:310"}

ST_EXAMPLES = r"^\((3|6|7|8|9|18|19|26|27|28|35|36|37|38)[a-z]?\) line"
WHO_RULES = tuple(
    (ST_EXAMPLES, kind, ".", L, "a St’át’imcets example") for kind in ("segmentation", "gloss", "transcription")
) + tuple(
    (r"^footnote 20 \((i|ii)\) line", kind, ".", N, "a nɬeʔkepmxcín example, footnote 20")
    for kind in ("segmentation", "gloss", "transcription")) + (
    # The forms the prose cites, by the language the sentence names.
    (r"^§2\.1$", "cited form", r"^ʕə́n̓<ʕən̓>ət$", L, "‘short-tempered’, a St’át’imcets adjective"),
    (r"^§2\.1$", "root", r"^√ʕn̓$", L, "the root of ʕə́n̓<ʕən̓>ət, St’át’imcets"),
    (r"^§2\.2$", "cited form", r"^(zík-in̓|ɬáp-an̓|ƛ̓íqʷ-in̓|qam̓t|qam̓t-s)$", L, "St’át’imcets, footnote 7"),
    (r"^§2\.2$", "cited form", r"^(ʔac-ʔitx|xa<q̓>q̓|ʔac-pə<n̓>n̓|ʔac-t̕uc|ʔac-wiʔ|ʔac-yaʕ̓)$", X,
     "a stative form of nxaʔamxcín, from Willet 2003 or Kinkade 1989"),
    (r"^§2\.3$", "cited form", r"^(c̓níqʷ-ən|cu-n|máy-s-ən)$", L, "a transitive St’át’imcets form"),
    (r"^§3\.3$", "cited form", r"^(tákem|sáq̓ʷuɬ|saq̓ʷuɬ)$", L, "a St’át’imcets quantifier, footnote 23"),
)

KIND_RULES = (
    (r"^\(20[ab]\) line 1$", ".", ".", "notation", AUTHORS, "the denotation of a St’át’imcets root, Bar-el et al. (2005), which Davis (2024) adopts"),
    (r"^footnote 16 \(i\) line 1$", ".", ".", "notation", AUTHORS, "Kiyota (2008:80)’s formula for bare root COS verbs in SENĆOTEN"),
    (r"^\(22\) line 1$", ".", ".", "notation", AUTHORS, "a difference function for two objects at a single point in time"),
    (r"^\(29\) line [23]$", ".", ".", "notation", AUTHORS, "the denotation of the St’át’imcets inchoative"),
    (r"^\(31\) line 1$", ".", ".", "notation", AUTHORS, "the denotation of the imperfective, after Kratzer (1998)"),
)

SET = {
    ("(3) line 1", "√zaw̓ ‘annoyed, irritated’"): {"kind": "root", "form": "√zaw̓", "gloss": "page 6, ‘annoyed, irritated’, the root of (3a) and (3b)"},
    ("(29) line 1", "St’át’imcets inchoative"): {"kind": "note", "who": AUTHORS, "gloss": "page 20, the heading"},
    ("§3.1", "# ‘The woman went out (like a light).’ (≠ The woman extinguished something (like a light).’)"):
        {"where": "(18b) line 3", "kind": "translation", "gloss": "page 15"},
    ("(6) line 7", "‘Only his grandmother was there, she had been left behind as well, because she couldn’t walk that well anymore.’"):
        {"who": "Edwards et al. 2017:118", "gloss": "page 12, cited Edwards et al. 2017:118"},
    ("(26) line 7", "'The church got on fire, but the firefighters put it out so the church didn’t burn.’ (consultant’s translation)"):
        {"who": "Carl Alexander", "gloss": "page 19, the consultant’s translation, as the page says; the page opens it with a straight quote"},
    ("(3a) line 7", "‘The hockey coach gets really annoyed when they can’t score.’5"):
        {"form": "‘The hockey coach gets really annoyed when they can’t score.’",
         "gloss": "page 6, carries footnote 5, written here without its digit"},
    ("§2.2", "xa<q̓>q̓"): {"form": "ʔac-xa<q̓>q̓", "gloss": "page 9, ‘get paid’, broken after ʔac- at the line end, a stative form of nxaʔamxcín from Willet 2003"},
    ("§2.2", "ʔac-ʔitx"): {"gloss": "page 9, ‘sleep’, a stative form of nxaʔamxcín from Willet 2003"},
    ("§2.2", "ʔac-pə<n̓>n̓"): {"gloss": "page 9, ‘it bends’, a stative form of nxaʔamxcín from Willet 2003"},
    ("§2.2", "ʔac-t̕uc"): {"gloss": "page 9, ‘it’s lying down’, a stative form of nxaʔamxcín from Kinkade 1989"},
    ("§2.2", "ʔac-wiʔ"): {"gloss": "page 9, ‘it’s finished’, a stative form of nxaʔamxcín from Kinkade 1989"},
    ("§2.2", "ʔac-yaʕ̓"): {"gloss": "page 9, ‘they’re gathered, bunched up’, a stative form of nxaʔamxcín from Kinkade 1989"},
    ("§2.2", "ƛ̓íqʷ-in̓"): {"gloss": "page 8, ‘crack something (e.g., a whip)’, St’át’imcets, footnote 7"},
    ("§3.3", "St’át’imcets: imperfective with punctual COS verbs"):
        {"where": "(35) heading", "gloss": "page 22, the heading over (35) and (36)"},
    ("(32) line 1", "Secwepemctsín: combination of t-marked COS verbs with the progressive predicate"):
        {"form": "Secwepemctsín: combination of t-marked COS verbs with the progressive predicate (w)ʔex"},
    ("(33) line 1", "nɬeʔkepmxcín: combination of t-marked COS verbs with the imperfective auxiliary"):
        {"form": "nɬeʔkepmxcín: combination of t-marked COS verbs with the imperfective auxiliary (w)ʔex"},
    ("(13) line 1", "nɬeʔkempxcín: t-marked COS verbs are unaccusative"):
        {"gloss": "page 14, the heading, which spells the language nɬeʔkempxcín"},
    ("(4d) line 1", "St’át’imcets7"): {"form": "St’át’imcets", "gloss": "page 8, the language of the words below, carries footnote 7, written here without its digit"},
    ("(5a) line 1", "nxaʔamxcín12"): {"form": "nxaʔamxcín", "gloss": "page 10, the language of the words below, carries footnote 12, written here without its digit"},
}

WHOSE = (
    "The paper compares four Interior Salish languages: nɬeʔkepmxcín, Secwepemctsín and "
    "St’át’imcets of the Northern Interior, and nxaʔamxcín of the Southern Interior. Each example's "
    "heading or list label names its language, and the who of each tier and each listed word is that "
    "language. The unattributed examples come from fieldwork with the speakers footnote * thanks: "
    "Carl Alexander, Qwa7yán’ak, for St’át’imcets, whose are the Consultant's comments of (17) and "
    "(18a) and the consultant's translation of (26); Bridget Dan, Julie Antoine and Garlene Dodson "
    "for Secwepemctsín; Bernice Garcia, Marty Aspinall, Gene Moses and Bev Phillips for "
    "nɬeʔkepmxcín. Bernice Garcia's introduction of herself in footnote * is hers. The word lists of "
    "(1), (4) and (5) cite Willet 2003, Kuipers 1974, Thompson and Thompson 1992 and Kinkade 1989 "
    "where their labels say so.\n\n"
    "Table 1 gives one reflex of each of three adjectives across the family; each cell's who is the "
    "language that heads its row, by the traditional English name the table uses. The who for a "
    "translation is the source its tag cites, and the authors for an untagged one. The formulas of "
    "Section 3.2 and 3.3 are the authors', and the definition (21), the paraphrase under (23) and "
    "the principle (30) are Kennedy and Levin's and Kennedy's. The prose, the headings and the "
    "notes carry Henry Davis and Sander Nederveen."
)

LETTERS = (
    "The forms are in the Salish version of the North American Phonetic Alphabet, footnote 2, with "
    "the glottalization of a resonant as a comma above, U+0313, and a null subject as Ø or ∅. The "
    "gloss labels are ASCII capitals in the text layer."
)

PAGE_NOTES = (
    "The text layer drops the dot below in nine words, most often leaving a space where it was, "
    "sə̣́n<sə̣n>-t and ʔi=pə̣tạ́k=a among them, and sets the existential ∃ of the formulas as the "
    "katakana ﾖ or the Latin Ǝ; each was read off the page at 600 dpi. The ɬɬ of kʷaɬɬtèzetkʷuʔ, "
    "nɬɬeʔkepmxcín, peɬɬt and e=n-ɬɬeɬɬúxʷ, the space in scwew̓ xmx and the k̓̓ of footnote 20 are "
    "printed so and kept."
)

ADD = tuple(
    [(None, ("title", AUTHORS, "title", TITLE, "the paper's title, which carries footnote *")),
     (None, ("title", AUTHORS, "name", "Henry Davis", "an author, University of British Columbia")),
     (None, ("title", AUTHORS, "name", "Sander Nederveen", "an author, University of British Columbia")),
     (None, ("footnote *", AUTHORS, "name", "Carl Alexander", "Qwa7yán’ak, a speaker of St’át’imcets, the consultant of the St’át’imcets examples")),
     (None, ("footnote *", AUTHORS, "name", "Bridget Dan", "a speaker of Secwepemctsín")),
     (None, ("footnote *", AUTHORS, "name", "Julie Antoine", "a speaker of Secwepemctsín")),
     (None, ("footnote *", AUTHORS, "name", "Garlene Dodson", "a speaker of Secwepemctsín")),
     (None, ("footnote *", AUTHORS, "name", "Bernice Garcia", "kʷaɬɬtèzetkʷuʔ, a speaker of nɬeʔkepmxcín who is re-learning her language")),
     (None, ("footnote *", AUTHORS, "name", "Marty Aspinall", "c̓úʔsinek, a speaker of nɬeʔkepmxcín")),
     (None, ("footnote *", AUTHORS, "name", "Gene Moses", "a speaker of nɬeʔkepmxcín")),
     (None, ("footnote *", AUTHORS, "name", "Bev Phillips", "a speaker of nɬeʔkepmxcín")),
     ] +
    display(("footnote *", "* We owe a great debt..."), [
        ("footnote *", "Bernice Garcia", "transcription", INTRODUCTION, "page 1, footnote *, how Bernice Garcia introduces herself in nɬeʔkepmxcín"),
        ("footnote *", "Bernice Garcia", "translation", TRANSLATION, "page 1, footnote *, the English of her introduction"),
    ]) +
    [(("§2.1", "While widespread and quite common..."), row) for row in reversed(table_1())] +
    [(("§2.1", "While widespread and quite common..."),
      ("footnote 2", AUTHORS, "note",
       joined("2 Here and in the rest", "here: in many cases") + " " +
       joined("and Tillamook are omitted", "are unreliable, so our"),
       "page 3, footnote, carried over to the foot of page 4"))] +
    display(("footnote 16", "This version differs in two respects..."), [
        ("(21) line 1", AUTHORS, "note", "Difference functions", "page 17, the heading of the definition"),
        ("(21) line 1", AUTHORS, "citation", "(Kennedy & Levin 2008: 17)", "the source of the definition"),
        ("(21) line 2", "Kennedy & Levin 2008: 17", "notation",
         joined("For any measure function m", "any d ∈ S, m↑ is"), "page 17, the definition, quoted"),
        ("(21) line 3", "Kennedy & Levin 2008: 17", "notation", line("a) its range is"), "page 17"),
        ("(21) line 4", "Kennedy & Levin 2008: 17", "notation", line("b) for any x, t in the domain"), "page 17"),
    ]) +
    display(("§3.2.2", "For COS roots, a measure of change function applies..."), [
        ("(23) line 1", AUTHORS, "notation", line("(23) [[√CoS]]")[5:], "page 17, the denotation of a COS root, Nederveen (in prep.)"),
        ("(23) line 2", "Kennedy & Levin 2008:18", "note",
         "COS roots yield the degree of difference between the degree of x at the beginning and the degree measured by m at the end of e.",
         "page 17, what (23) says, quoted"),
        ("(23) line 2", AUTHORS, "citation", "(Kennedy & Levin 2008:18)", "the source of the paraphrase"),
        ("§3.2.2", AUTHORS, "notation", "√CoS", "a change-of-state root, in the formulas of (23) and (25)"),
    ]) +
    display(("§3.2.2", "In other words, a COS root..."), [
        ("(24) line 1", AUTHORS, "notation", line("(24) [[ -t ]]")[5:], "page 18, the denotation of intransitive -t"),
    ]) +
    display(("§3.2.2", "Compositionally, intransitive -t derives..."), [
        ("(25) line 1", AUTHORS, "notation", line("(25) [[ -t ]]([[√CoS]])")[5:], "page 18, intransitive -t applied to a COS root"),
        ("(25) line 2", AUTHORS, "notation", line("(λxλe[m↑m(x)(init(e))(x)(fin(e))])"), "page 18"),
        ("(25) line 3", AUTHORS, "notation", line("= λxλe ∃ d [m↑m(x)"), "page 18"),
    ]) +
    display(("§3.3", "Here, the initial point of the event..."), [
        ("(30) line 1", AUTHORS, "note", "Interpretive Economy", "page 20, the heading of the principle"),
        ("(30) line 2", "Kennedy 2007:36", "note",
         joined("Maximize the contribution", "the computations of tis truth").replace(" (Kennedy 2007:36)", ""),
         "page 20, the principle, quoted, printed with tis for its"),
        ("(30) line 2", AUTHORS, "citation", "(Kennedy 2007:36)", "the source of the principle"),
    ])
)
