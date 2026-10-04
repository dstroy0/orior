# Context for Reed Steiner, nɬeʔkepmxcín Somatic Suffixes. The examples come from the author's three
# consultants, each tagged sf or vf with the date, and from Thompson and Thompson 1992 and 1996. The
# denotations of §3 and §4 and their paraphrases are read off the page again, one row a line of
# formula and one a paraphrase, since the text layer ran them into the prose; so are (1), which it
# set as one note, (30), whose readings it ran into the prose, and the references. The literal
# translations the engine left in the sections go back to their examples.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import PRIVATE  # noqa: E402
from workdir import WORK  # noqa: E402
import residue  # noqa: E402
from finish import TRAILER  # noqa: E402

STEM = "SteinerICSNL60"
AUTHORS = "Reed Steiner"
L = "nɬeʔkepmxcín"
LI = "St̕át̕imcets"
HALKOMELEM = "Halkomelem"

TITLE = "nɬeʔkepmxcín Somatic Suffixes"
BYLINE = "Reed Steiner, University of British Columbia"

BEV = "Bev Phillips"
MARTY = "c̓úʔsinek (Marty Aspinall)"
BERNICE = "kʷaɬtèzetkʷuʔ (Bernice Garcia)"

# The speaker tags of footnote 4, and RS for the author asking in (33b).
INITIALS = {"BP": BEV, "CMA": MARTY, "KBG": BERNICE, "RS": AUTHORS}

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


def span(first, stop, after=0):
    """The nonempty page lines from the one opening on first up to the one opening on stop."""
    start = at(first, after)
    end = at(stop, start + 1)
    return [(index, PAGE[index]) for index in range(start, end)
            if PAGE[index] and not PAGE[index].startswith("=====") and not PAGE[index].isdigit()]


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


# A translation closing on a straight quote, 'My dog warmed herself.' (vf | BP 27 Feb 2025), which
# the engine's pattern for the tag does not part.
STRAIGHT = re.compile(r"^(.*')\s*(\([^()]*\))$")


def translated(where, who, text, gloss):
    """A translation and the tag or source at its right, as the engine parts them."""
    split = TRAILER.match(text) or STRAIGHT.match(text)
    if not split:
        return [(where, who, "translation", text, gloss)]
    return [(where, who, "translation", split.group(1), gloss),
            (where, AUTHORS, "citation", split.group(2), "the tag or source at the right of the translation")]


ROMAN = ("i", "ii", "iii")
CONTINUED = re.compile(r"^(?:∧|Agent\(|InalPoss\()")


def formulas(number, first, stop, after=0):
    """A display of denotations read off the page: its caption a note, each line of formula a rule
    and each paraphrase the translation of the formula above it. A letter a. to c. or a step (i) to
    (iii) opens each part."""
    pieces = []
    label = ""
    seen = set()
    for index, text in span(first, stop, after):
        page = page_of(index)
        opening = "(%s)" % number
        if text.startswith(opening):
            text = text[len(opening):].strip()
        mark = re.match(r"^(?:([a-c]) ?\.|\((i{1,3})\))\s+(.*)$", text)
        if mark:
            label = mark.group(1) or mark.group(2)
            printed = ""
            # (48) prints its third step (ii) again; the prose names it (iii).
            if label in seen and label in ROMAN:
                printed = label
                label = ROMAN[ROMAN.index(label) + 1]
            seen.add(label)
            text = mark.group(3)
            pieces.append([label, None, "", page, printed])
        last = pieces[-1] if pieces else None
        if re.match(r"^[‘']", text):
            kind = "translation"
        elif re.search(r"[⟦λ]", text) or CONTINUED.match(text):
            kind = "rule"
        else:
            kind = "note"
        if last and last[0] == label and (last[1] is None or (
                last[1] == kind and (kind != "rule" or CONTINUED.match(text))) or (
                last[1] in ("translation", "note") and kind == "note")):
            if last[1] is None:
                last[1] = kind
            last[2] = (last[2] + " " + text).strip()
            continue
        pieces.append([label, kind, text, page, ""])
    rows = []
    lines = {}
    for label, kind, text, page, printed in pieces:
        lines[label] = lines.get(label, 0) + 1
        where = "(%s%s) line %d" % (number, label, lines[label])
        gloss = {"note": "page %d, the caption" % page,
                 "rule": "page %d, the author's denotation, read off the page" % page,
                 "translation": "page %d, the paraphrase of the denotation above it" % page}[kind]
        if printed:
            gloss += ", printed (%s) again where the prose names step (%s)" % (printed, label)
        rows.append((where, AUTHORS, kind, text, gloss))
    return rows


FORMULA_34 = formulas("34", "(34) ⟦-ekst⟧", "The challenge with a denotation")
FORMULA_36 = formulas("36", "(36) Patient modifier", "However, Restrict cannot generalize")
FORMULA_37 = formulas("37", "(37) “Restrict2”", "While a function like Restrict2")
FORMULA_47 = formulas("47", "(47) Medio-reflexive", "Encoding inalienable possession")
FORMULA_48 = formulas("48", "(48) (for illustrative", "Medio-reflexive predicates without")
FORMULA_49 = formulas("49", "(49) Medio-reflexive", "Although this approach does predict")
FORMULA_50 = formulas("50", "(50) ⟦-ekst⟧", "Because somatic suffixes are of type")
FORMULA_51 = formulas("51", "(51) a.", "Non-somatic suffixes won't compose")
FORMULA_55 = formulas("55", "(55) a.", "Independent support for this approach")
FORMULA_62 = formulas("62", "(62)", "In the c̓eɬétkʷu dialect, however")
FORMULA_63 = formulas("63", "(63) a.", "Under this approach, all medio-")
FORMULA_64 = formulas("64", "(64) Somatic", "(65) Unmarked")
FORMULA_65 = formulas("65", "(65) Unmarked", "(66) Autonomous")
FORMULA_66 = formulas("66", "(66) Autonomous", "Not only does this approach")
FORMULA_67 = formulas("67", "(67) Somatic Control", "The somatic relational transitive is formalized")
FORMULA_68 = formulas("68", "(68) Somatic Relational", "These denotations are necessary")
FORMULA_69 = formulas("69", "(69) A somatic", "Although the semantics work")
FORMULA_74 = formulas("74", "(74) Somatic unaccusatives", "If this hypothesis is correct")


def interlinear(number, first, stop):
    """Lettered examples of four lines each, the transcription, its segmentation, the gloss and the
    translation."""
    rows = []
    label, line = "", 0
    for index, text in span(first, stop):
        mark = re.match(r"^([a-c])\. (.*)$", text)
        if mark:
            label, line, text = mark.group(1), 0, mark.group(2)
        line += 1
        where = "(%s%s) line %d" % (number, label, line)
        page = page_of(index)
        if line == 4:
            rows += translated(where, AUTHORS, text, "page %d" % page)
        else:
            rows.append((where, L, ("transcription", "segmentation", "gloss")[line - 1], text,
                         "page %d, read off the page" % page))
    return rows


EXAMPLE_1 = [("(1) line 1", AUTHORS, "note", "Change-of-state predicate with non-somatic suffix", "page 3, the caption")] + \
    interlinear("1", "a. ʔéx nukʷ", "(2) Immediate-marked")

T_177 = "T&T1996:177"
EXAMPLE_30 = [("(30a) line 2", T_177, "translation", "Lit. ‘shake a head’", "page 14, the literal translation")] + \
    [("(30a) line 2", AUTHORS, "citation", "(T&T1996:177)", "the tag or source at the right of the translation"),
     ("(30b) line 1", T_177, "translation", "‘[of a horse] shake [its own] head’", "page 14, a somatic reading"),
     ("(30b) line 1", AUTHORS, "citation", "(ibid:177)", "the tag or source at the right of the translation"),
     ("(30c) line 1", T_177, "translation", "‘[of a person] brush [one’s own] hair’", "page 14, a somatic reading"),
     ("(30c) line 1", AUTHORS, "citation", "(ibid:177)", "the tag or source at the right of the translation")]


def references():
    """One reference an entry, a line opening on a name and a comma that carries the entry's year
    starting each."""
    entries = []
    for index in range(at("Baker, Mark C."), len(PAGE)):
        text = PAGE[index]
        if not text or text.startswith("=====") or text.isdigit():
            continue
        if re.match(r"^(?:van )?[A-Z][\w’'\-]+(?: [A-Z][\w’'\-]+)?, [A-Z]", text) and \
                re.search(r"(?<= )(?:1[89]|20)\d\d[a-z]?\.", text):
            entries.append([text, page_of(index)])
        else:
            # A page range the line broke joins without a space, 303-312.
            broken = entries[-1][0].endswith("-") and text[:1].isdigit() and \
                not entries[-1][0].split()[-1].startswith("http")
            entries[-1][0] += ("" if broken else " ") + text
    return [("references", AUTHORS, "reference", text, "page %d" % page) for text, page in entries]


REFERENCES = references()

# The literal translations the engine left in the sections: the tier under a translation opening on
# Lit., with its tag or source when it has one.
KIND_RULES = (
    (r"^§", r"^note$", r"^Lit\. ‘.*’(?: \([^()]*\))?$", "translation", AUTHORS, "the literal translation"),
    (r"^references$", r"^note$", r".", "reference", AUTHORS, "read again below"),
)

# (74) on page 33 is Nahuatl, after Mithun 1984:860.
WHO_RULES = (
    (r"^\(74\) line 2$", "segmentation", r"ikši", "Nahuatl", "after Mithun 1984:860"),
)

REPLACE = (("However, - nwéɬn", "However, -nwéɬn"),)

DROP = ()

FORMS = {
    L: ("language", AUTHORS, "the language of this paper, a.k.a. nlaka’pamux or Thompson River Salish, Northern Interior Salish, ISO 639-3 thp"),
    "nɬeʔkpemxcín": ("language", AUTHORS, "nɬeʔkepmxcín, as page 17 prints it"),
    "nlaka’pamux": ("language", AUTHORS, "another name for nɬeʔkepmxcín"),
    "ƛ̓q̕əmcín": ("language", AUTHORS, "the Lytton dialect, Bev Phillips's"),
    "ƛ̓q̓əmcín": ("language", AUTHORS, "the Lytton dialect, written with q̓ where §1.1 writes q̕"),
    "ƛ̓əq̕mcín": ("language", AUTHORS, "the Lytton dialect, as page 24 spells it"),
    "scw̕exmxcín": ("language", AUTHORS, "the Nicola Valley dialect, c̓úʔsinek's, in the spelling of Thompson and Thompson 1996:45"),
    "scew̕exmxcín": ("language", AUTHORS, "the Nicola Valley dialect as c̓úʔsinek spells it, footnote 1"),
    "scwexmxcín": ("language", AUTHORS, "the Nicola Valley dialect as others spell it, footnote 1"),
    "c̓eɬétkʷu": ("language", AUTHORS, "the Coldwater dialect, kʷaɬtèzetkʷuʔ's"),
    "Stó꞉lō": ("language", AUTHORS, "the Stó꞉lō dialect of Halkomelem, Coast Salish, on c̓úʔsinek's father's side"),
    "Secwepemctsín": ("language", AUTHORS, "Shuswap, Northern Interior Salish, ISO 639-3 shs"),
    "St̕át̕imcets": ("language", AUTHORS, "Lillooet, Northern Interior Salish"),
    "St̕at̕imcets": ("language", AUTHORS, "Lillooet, Northern Interior Salish, written without its accent"),
    "St'át'imcets": ("language", AUTHORS, "Lillooet, in the title of Davis 1997"),
    "ʔayʔaǰuθəm": ("language", AUTHORS, "Comox-Sliammon, in the title of Huijsmans 2023"),
    "c̓úʔsinek": ("name", AUTHORS, "Marty Aspinall's name, CMA in the tags, one of the three consultants"),
    "kʷaɬtèzetkʷuʔ": ("name", AUTHORS, "Bernice Garcia's traditional name, KBG in the tags, one of the three consultants"),
    "nɬab": ("name", AUTHORS, "the nɬeʔkepmxcín Lab, thanked in footnote *"),
    "Lucía": ("name", AUTHORS, "Lucía A. Golluscio, in a reference entry"),
    "País": ("name", AUTHORS, "the Universidad del País Vasco, in a reference entry"),
    "Tromsø": ("place", AUTHORS, "the University of Tromsø, in a reference entry"),
    "ném": ("cited form", L, "from the thanks ném kʷukʷstéyp nsnuk̓ʷnúk̓ʷeʔ closing footnote *, left untranslated"),
    "kʷukʷstéyp": ("cited form", L, "from the thanks ném kʷukʷstéyp nsnuk̓ʷnúk̓ʷeʔ closing footnote *, left untranslated"),
    "nsnuk̓ʷnúk̓ʷeʔ": ("cited form", L, "from the thanks ném kʷukʷstéyp nsnuk̓ʷnúk̓ʷeʔ closing footnote *, left untranslated"),
    "c̓əɬétkʷu": ("place", L, "Coldwater, in kʷaɬtèzetkʷuʔ's self-introduction, footnote *"),
    "scwew̓xmx": ("place", L, "Nicola, in kʷaɬtèzetkʷuʔ's self-introduction, footnote *"),
    "=eɬp": ("cited affix", L, "page 2, footnote 2, -eɬp ‘plant’ with the double hyphen the literature writes"),
    "<ʔ>": ("cited affix", L, "page 3, the inchoative infix of strong roots"),
    "-ɬ-": ("cited affix", L, "page 12, footnote 12, the compound connective CONN"),
    "-ala+kaʔ": ("cited affix", LI, "page 12, footnote 12, ‘tool’ from CONN+hand, after Henry Davis (p.c.)"),
    "-θət": ("cited affix", HALKOMELEM, "page 19, the reflexive suffix, after Gerdts 2003:355"),
    "-əm": ("cited affix", HALKOMELEM, "page 18, the middle suffix, after Gerdts and Hukari 1998:173"),
    "-∅+m": ("cited affix", "Proto Northern Interior Salish", "page 33, *-∅+m -body+MID, an unmarked medio-reflexive"),
    "-ilx+∅": ("cited affix", "Proto Northern Interior Salish", "page 33, *-ilx+∅ -body+MID, an autonomous medio-reflexive"),
    "nwéɬn": ("cited affix", L, "page 6, footnote 8, the limited control middle -nwéɬn, its hyphen closing the line above"),
    "níɬm̓": ("cited form", L, "page 9, footnote 11, an interjection of surprise and recognition, after T&T 1992:219"),
}
INTRO = "a word of kʷaɬtèzetkʷuʔ's self-introduction, footnote *"
for _word in ("ʔes", "ʔúməcms", "təw", "ɬe", "wéʔe", "ncitxʷ", "ƛ̓uʔ", "wéʔec", "ʔex", "netíyxs",
              "tékm", "xéʔe", "nɬeʔképmx", "tmixʷs"):
    FORMS[_word] = ("cited form", L, INTRO)

# The words and affixes the prose cites in plain letters, which the engine read as English.
AFFIXES = [
    ("front", L, "-inek", "page 1, the abstract, ‘star’"),
    ("front", L, "-aqs", "page 1, the abstract, ‘nose’"),
    ("§1.2", L, "-us", "page 2, ‘face’"),
    ("§1.2", L, "-xn", "page 2, ‘foot’"),
    ("§2", L, "-m", "page 3, the middle suffix of unergative predicates"),
    ("§2", L, "-n-t-", "page 3, the control transitive"),
    ("§2", L, "-min-t-", "page 3, the relational transitive"),
    ("§2.1", L, "-t", "page 3, the immediate IMM suffix, after T&T 1992:92"),
    ("§2.1", L, "-p", "page 3, the inchoative INCH suffix of weak roots, after T&T 1992:97"),
    ("footnote 8", "Secwepemctsín", "-nwelln", "page 6, footnote 8, the cognate of the limited control middle -nwéɬn, after Nederveen 2022"),
    ("footnote 9", L, "-me", "page 7, footnote 9, the control middle after a posttonic open syllable, after T&T 1992:102"),
    ("§2.3", L, "-t", "page 8, the transitivizing TR suffix, after T&T 1992:61"),
    ("§2.3", L, "-n", "page 8, the control CTR suffix, after T&T 1992:62, 65"),
    ("§2.3", L, "-s", "page 8, the causative CAUS suffix, after T&T 1992:70"),
    ("§2.3", L, "-xi", "page 8, the redirective RDR suffix, after T&T 1992:71"),
    ("§2.3", L, "-min", "page 8, the relational RLT suffix, after T&T 1992:73"),
    ("§2.4", L, "-ekst", "page 12, ‘hand’"),
    ("footnote 12", LI, "-al+us", "page 12, footnote 12, 'eye' from CONN+face, after Henry Davis (p.c.)"),
    ("§3.2", "Proto-Salish", "-sut", "page 18, *-sut REFL, after Kroeber 1999:32"),
    ("§4.1", L, "-iyx", "page 23, the autonomous suffix AUT, after T&T 1992:101"),
    ("§4.1", LI, "-ilx", "page 23, the autonomous suffix, after Davis 1997:66"),
    ("§5.1", "Proto Northern Interior Salish", "-us-m", "page 33, *-us-m -face-MID, a somatic medio-reflexive"),
]

# The paraphrases of the denotations, which the engine set in the sections, some with the prose
# after them: the prose is kept and the paraphrase read again with its formula.
PROSE = {
    "The challenge with a denotation like (34)": {}, "However, Restrict cannot generalize": {},
    "While a function like Restrict2": {}, "Encoding inalienable possession": {},
    "Medio-reflexive predicates without somatic suffixes": {"where": "§3.2"},
    "Although this approach does predict": {}, "Because somatic suffixes are of type": {},
    "Non-somatic suffixes won't compose": {}, "Independent support for this approach": {},
    "In the c̓eɬétkʷu dialect, however": {}, "Under this approach, all medio-": {},
    "Not only does this approach": {}, "The somatic relational transitive is formalized": {},
    "These denotations are necessary": {},
}
PARAPHRASE = re.compile(r"^(?:[‘']+(?:Take|A ripping)|exists an individual)")

SPLIT = [
    ("front", "Abstract:", None, {"gloss": "page 1, the abstract"}),
    ("front", "Keywords:", {}, {"gloss": "page 1, the keywords"}),
    ("(9) line 1", "7 Brent Hall", {}, {"where": "footnote 7", "gloss": "page 5, footnote"}),
    ("§2.4", "12 The suffix -éleʔ", {}, {"where": "footnote 12", "gloss": "page 12, footnote"}),
    ("§2.4", "Typically, when an ambiguous", None, {}),
]
# A literal translation the text layer ran into the prose after it, and the tag closing it.
for _where, _marker, _example, _who, _tag, _page in (
        ("§2.2", "Crucially, the medio-reflexive", "(13c) line 5", AUTHORS, "(sf | KBG 13 Nov 2024)", 7),
        ("§2.3", "The readings in (19)", "(20b) line 7", AUTHORS, "(vf | KBG 29 Sep 2023)", 10),
        ("§2.3", "Across unaccusative, unergative", "(26) line 5", "T&T1996:95", None, 12),
        ("§2.4", "All somatic suffixes can access", "(29b) line 5", "T&T 1992:22", "(T&T 1992:22)", 13)):
    SPLIT.append((_where, _marker, {"where": _example, "kind": "translation", "who": _who,
                                    "gloss": "page %d, the literal translation" % _page}, {}))
    if _tag:
        SPLIT.append((_example, _tag, {}, {"kind": "citation", "who": AUTHORS,
                                           "gloss": "the tag or source at the right of the translation"}))

for _where, _who, _kind, _form, _gloss in DRAFT:
    if _kind == "translation" and not TRAILER.match(_form) and STRAIGHT.match(_form):
        SPLIT.append((_where, STRAIGHT.match(_form).group(2), {}, {
            "kind": "citation", "who": AUTHORS, "gloss": "the tag or source at the right of the translation"}))

REMOVE = [("§2.1", "(1) Change-of-state predicate with non-somatic suffix..."),
          ("§2.4", "undergoes metaphorical extension..."),
          ("(74) line 1", "Somatic unaccusatives using Restrict2 (for illustrative purposes only)")]
# The words of (1), which the engine cited from the note it made of the example.
REMOVE += [("§2.1", one) for one in ("ʔéx", "nukʷ", "x̣ʷúsəs", "heʔpíyə", "nɬə", "típəl", "ʔéx=∅=nukʷ",
                                      "x̣ʷús~əs", "[e]=eʔ-píyə", "n=ɬə=típəl", "x̣ʷúsəsetkʷuʔ", "x̣ʷús~əs-etkʷu")]
for _where, _who, _kind, _form, _gloss in DRAFT:
    # The engine's reference entries, which are read again whole below.
    if _where == "references" and _kind == "note":
        REMOVE.append((_where, _form))
    elif _kind == "cited form" and re.search(r"[λ⟦]", _form):
        REMOVE.append((_where, _form))
    elif _kind == "note" and PARAPHRASE.match(_form):
        _prose = [one for one in PROSE if one in _form]
        if _prose:
            SPLIT.append((_where, _prose[0], None, PROSE[_prose[0]]))
        else:
            REMOVE.append((_where, _form))

REMOVE_WHERE = (r"^\((?:34|36|37|37a|47|48|49|50|51|55a|62|63a|64|65|66|67|67a|68|68a|69|69a|69b|73c|74a)\) line"
                r"|^\((?:i|ii|iii)\) line|^footnote 17 \(")

SELF = draft_form("§1.1", "* I extend").split("thus: ")[1].split(", ‘My traditional")[0]
FOOTNOTE_12_HEAD = draft_form("§2.4", "Non-somatic body-part suffixes differ").split(" 12 The suffix")[0]

ADD = tuple(
    [(None, ("title", AUTHORS, "title", TITLE, "the paper's title, carrying the star of the acknowledgement footnote")),
     (None, ("title", AUTHORS, "name", AUTHORS, "author, University of British Columbia")),
     (("footnote *", "* I extend..."), ("footnote *", BERNICE, "running speech", SELF, "page 1, kʷaɬtèzetkʷuʔ introducing herself")),
     (("footnote *", SELF), ("footnote *", AUTHORS, "translation", "‘My traditional name is kʷaɬtèzetkʷuʔ, my home is in Coldwater of ‘Nicola’ of nlaka’pamux lands.’", "page 1, the translation printed with it")),
     (None, ("footnote *", AUTHORS, "name", BEV, "consultant, BP in the tags, speaker of the ƛ̓q̕əmcín (Lytton) dialect")),
     (None, ("footnote *", AUTHORS, "name", "Marty Aspinall", "consultant, CMA in the tags, c̓úʔsinek, speaker of the scw̕exmxcín (Nicola Valley) dialect")),
     (None, ("footnote *", AUTHORS, "name", "Bernice Garcia", "consultant, KBG in the tags, kʷaɬtèzetkʷuʔ, speaker of the c̓eɬétkʷu (Coldwater) dialect")),
     (None, ("footnote *", AUTHORS, "name", "Henry Davis", "thanked for comments and feedback")),
     (None, ("footnote *", AUTHORS, "name", "Lisa Matthewson", "thanked for introducing the author to the language")),
     (None, ("footnote *", AUTHORS, "name", "Marcin Morzycki", "thanked for reading the denotations")),
     (None, ("footnote *", AUTHORS, "name", "Ella Hannon", "thanked")),
     (None, ("footnote *", AUTHORS, "name", "Brent Hall", "thanked. Footnote 7 cites him")),
     (None, ("all", AUTHORS, "notation", "sf", "supplied form, footnote 4")),
     (None, ("all", AUTHORS, "notation", "vf", "volunteered form, footnote 4")),
     (None, ("all", AUTHORS, "notation", "*", "judged ill-formed")),
     (None, ("all", AUTHORS, "notation", "#", "judged infelicitous in the given context")),
     (None, ("all", AUTHORS, "notation", "?", "an uncertain judgement")),
     (None, ("all", AUTHORS, "notation", "?/*", "judged uncertain to ill-formed")),
     (None, ("all", AUTHORS, "notation", "Restrict", "Predicate Restriction of Chung and Ladusaw 2004")),
     (None, ("all", AUTHORS, "notation", "Restrict2", "the hypothetical mode of composition of (37a), for functions of type <e,et>")),
     (("(30a) line 1", "‘thresh wheat’"), EXAMPLE_30[0])]
    + [(where, (where, who, "cited affix", form, gloss)) for where, who, form, gloss in AFFIXES]
    + chained(("§2.1", "Unaccusative predicates have one internal argument..."), EXAMPLE_1)
    + chained(("(30a) line 2", EXAMPLE_30[0][3]), EXAMPLE_30[1:])
    + chained(("§3.1", "In her treatment of Halkomelem..."), FORMULA_34)
    + chained(("§3.1", "The semantics of (35) can be formalized..."), FORMULA_36)
    + chained(("§3.1", "However, Restrict cannot generalize..."), FORMULA_37)
    + chained(("§3.2", "Instead, the medio-reflexive middle needs..."), FORMULA_47)
    + chained(("§3.2", "Encoding inalienable possession..."), FORMULA_48)
    + chained(("§3.2", "Medio-reflexive predicates without..."), FORMULA_49)
    + chained(("§4", "This section combines the two approaches..."), FORMULA_50)
    + chained(("§4.1", "In unergative predicates, it is the medio-reflexive..."), FORMULA_51)
    + chained(("§4.1", "The denotation in (51) can be extended..."), FORMULA_55)
    + chained(("§4.1", "This difference between the ƛ̓əq̕mcín..."), FORMULA_62)
    + chained(("§4.1", "In the c̓eɬétkʷu dialect, however..."), FORMULA_63)
    + chained(("§4.1", "Under this approach, all medio-..."), FORMULA_64 + FORMULA_65 + FORMULA_66)
    + chained(("§4.2", "To extend the proposal in 4.1..."), FORMULA_67)
    + chained(("§4.2", "The somatic relational transitive is formalized..."), FORMULA_68)
    + chained(("§4.3", "There is no barrier to this in the semantics..."), FORMULA_69)
    + chained(("§4.3", "One hypothesis stipulates..."), FORMULA_74)
    + chained(("references", "References"), REFERENCES)
)

SET = {
    ("§2.4", FOOTNOTE_12_HEAD): {"form": FOOTNOTE_12_HEAD + " " + draft_form("§2.4", "undergoes metaphorical extension"),
                                 "gloss": "pages 12 and 13, the paragraph footnote 12 broke"},
    ("§2.3", "níɬm̓"): {"where": "footnote 11"},
    ("(30a) line 1", "‘thresh wheat’"): {"who": T_177, "gloss": "page 14, the non-somatic reading"},
}
# The literal translations the engine left in the sections, and the tags closing them.
for _where, _form, _example, _who, _tag in (
        ("§2.1", "Lit. ‘Your feet are free (from being bound).’", "(6) line 6", AUTHORS, "(sf | KBG 5 March 2025)"),
        ("§2.1", "Lit. ‘Her feet are removed.’", "(8) line 6", "T&T1992:83", "(T&T1992:83)"),
        ("§2.2", "Lit. ‘I should be making (things).’", "(11a) line 5", "T&T1992:141", "(T&T1992:141)"),
        ("§2.3", "Lit. ‘I always close that room's mouth.’", "(19c) line 7", AUTHORS, None),
        ("§2.3", "Lit. ‘Don't release your hand from the rope.’", "(21a) line 7", AUTHORS, "(sf | BP 29 May 2025)"),
        ("§4.3", "Lit. ‘Her feet are removed.’", "(71) line 5", "T&T1992:83", "(T&T1992:83)")):
    SET[(_where, _form)] = {"where": _example, "who": _who}
    if _tag:
        SET[(_where, _tag)] = {"where": _example}
# (59b) and the two examples of footnote 16 (ii), which the engine numbered on from the example
# before them, and the letter the text layer left at the head of each.
for _where, _who, _kind, _form, _gloss in DRAFT:
    _line = re.match(r"^(\(59a\)|footnote 16 \(ii\)) line (\d+)$", _where)
    if not _line or (_line.group(1) == "(59a)" and int(_line.group(2)) < 5) or \
            (_line.group(1) != "(59a)" and int(_line.group(2)) < 2):
        continue
    _number = int(_line.group(2))
    if _line.group(1) == "(59a)":
        _new = "(59b) line %d" % (_number - 4)
    else:
        _new = "footnote 16 (ii%s) line %d" % ("a" if _number <= 7 else "b", _number - 1 if _number <= 7 else _number - 7)
    _change = {"where": _new}
    _letter = re.match(r"^[ab] ?\. (.*)$", _form)
    if _letter:
        _change["form"] = _letter.group(1)
        _change["gloss"] = _gloss + ", the text layer set the letter of the example before it"
    _pieces = translated(_where, _who, _form, _gloss) if _kind == "translation" else [(_where, _who, _kind, _form)]
    for _piece in _pieces:
        SET[(_where, _piece[3])] = _change if _piece[3] == _form else {"where": _new}
# The words footnote 12 cites, which the engine left in the section above it.
for _form in ("-éleʔ", "-ɬ-", "-éleʔ+xn", "St̕at̕imcets", "-ala+kaʔ", "ch-éleʔ-xn-me", "siʔh-éleʔ-xn"):
    SET[("§2.4", _form)] = {"where": "footnote 12"}

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
    "The language is nɬeʔkepmxcín. The examples come from the author's three consultants, Bev "
    "Phillips (BP), c̓úʔsinek Marty Aspinall (CMA) and kʷaɬtèzetkʷuʔ Bernice Garcia (KBG), each "
    "tagged sf for a supplied form or vf for a volunteered form, with the date, and from Thompson "
    "and Thompson 1992 and 1996. (74) on page 33 is Nahuatl, after Mithun 1984.\n\n"
    "The who for each tier of an example is the language. The who for a translation is the work "
    "cited on its line, and the author's where the line carries a consultant's tag. A consultant's "
    "comment is theirs, and RS in (33b) is the author. The denotations of §3 and §4 and their "
    "paraphrases, the captions and the prose carry the author."
)

LETTERS = (
    "The paper writes the glottalized stops with a comma above, c̓ and q̓, and in places with a "
    "reversed comma, q̕ and w̕, both in the same dialect name, ƛ̓q̕əmcín beside ƛ̓q̓əmcín. ∅ marks "
    "a null morpheme and [ ] a segment the segmentation restores. The denotations write ⟦ ⟧ for the "
    "interpretation brackets, λ for abstraction and ∧ for conjunction, with the types as subscripts "
    "the text layer sets on the line, λxe for λxₑ."
)

PAGE_NOTES = (
    "The text layer runs each denotation and its paraphrase into the prose around it; the displays "
    "of §3 and §4 are read off the page again here, one row a line of formula and one a "
    "paraphrase. The paper numbers two examples (74), the Restrict2 display on page 30 and the "
    "Nahuatl example on page 33, and prints the third step of (48) as (ii) again. The text layer "
    "sets a space after the accented schwa of sɣə́p, cʕə́p and c̓k̓ʷə́m, which the glyph positions "
    "close; those three are corrected before the page is read."
)
