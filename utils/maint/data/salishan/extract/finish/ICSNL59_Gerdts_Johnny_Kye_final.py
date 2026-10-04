# Context for Donna Gerdts, Thomas Johnny and Ted Kye, Rhetorical Lengthening in Hul'q'umi'num'
# Story Performance. The Hul'q'umi'num' is the examples of Mrs. Jimmy Joe's six stories, each a
# line of the practical orthography over a phonemic line and a gloss, and the words the prose cites.
# The text layer ran twenty-four figure captions and five tables into the paragraphs around them;
# each caption is found on the page and parted from the prose here, and each table is a caption row
# and a row of its cells.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import PRIVATE  # noqa: E402
from workdir import WORK  # noqa: E402
import residue  # noqa: E402

STEM = "ICSNL59_Gerdts_Johnny_Kye_final"
AUTHORS = "Donna Gerdts, Thomas Johnny and Ted Kye"
HUL = "Hul’q’umi’num’"

TITLE = "Rhetorical Lengthening in Hul’q’umi’num’ Story Performance"
BYLINE = "Donna Gerdts, Thomas Johnny and Ted Kye, Simon Fraser University"
VOLUME = "59"

_repair = residue.paper_repair(STEM)
with open(os.path.join(PRIVATE, "pagetext", STEM + ".txt"), encoding="utf-8") as handle:
    PAGE = [" ".join(_repair(one).split()) for one in handle.read().split("\n")]
with open(os.path.join(WORK, STEM + ".draft.tsv"), encoding="utf-8") as handle:
    DRAFT = [one.rstrip("\n").split("\t") for one in handle][1:]

# The phonemic line holds letters the practical orthography never writes.
TIER_SCRIPT = "əšłɬθčʔ̓ᶿʷƛ̌ː"


def page_of(index):
    for back in range(index, -1, -1):
        marker = re.match(r"^===== page (\d+) =====$", PAGE[back])
        if marker:
            return int(marker.group(1))
    return 0


# Each caption as the page prints it: its line and the lines after it that open lower case, the
# rest of a caption set over two lines.
CAPTIONS = []
for _at, _line in enumerate(PAGE):
    _head = re.match(r"^(Figure|Table) (\d+): ", _line)
    if not _head:
        continue
    _text = _line
    for _next in PAGE[_at + 1:]:
        if not _next or not _next[0].islower():
            break
        _text += " " + _next
    CAPTIONS.append(("%s %s" % (_head.group(1), _head.group(2)), _text, page_of(_at)))

# Where each table's cells end and the prose after it begins.
TABLE_PROSE = {"Table 1": "Figure 3 is a box plot", "Table 2": "Figure 4 illustrates the duration",
               "Table 3": "Figure 5 is a box plot", "Table 4": "Figure 8 illustrates the maximum",
               "Table 5": "One interesting aspect about the placement"}

# Each note row cut at its captions: the prose before, the caption, a table's cells, the prose
# after. Every cut after the first is a SPLIT at the opening words of the piece it starts, the
# piece before it given its where; the last piece takes its where from the SPLIT's tail.
SPLIT = [
    ("front", "Donna Gerdts Thomas Johnny", {"where": "title", "kind": "title", "gloss": "page 1, the title, its star the acknowledgement footnote's"}, {}),
    ("front", "Abstract:", {"where": "title", "gloss": "page 1, the authors and their affiliations"}, {"gloss": "page 1, the abstract"}),
]
SET = {}
for _where, _who, _kind, _form, _gloss in DRAFT:
    if _kind != "note" or _where.startswith("Table 4 line"):
        continue
    pieces = [("prose", _form)]
    for name, caption, page in CAPTIONS:
        last_kind, last = pieces[-1]
        if caption not in last:
            continue
        before, after = last.split(caption, 1)
        pieces[-1:] = [one for one in [("prose", before.strip())] if one[1]] + [((name, "caption", page), caption)]
        after = after.strip()
        if name in TABLE_PROSE and after:
            cells, _, rest = after.partition(TABLE_PROSE[name])
            pieces.append(((name, "cells", page), cells.strip()))
            after = (TABLE_PROSE[name] + rest).strip() if rest or _ else ""
        if after:
            pieces.append(("prose", after))
    if len(pieces) == 1 and pieces[0][0] == "prose":
        continue

    def attributes(kind):
        if kind == "prose":
            return {}
        name, what, page = kind
        return {"where": name, "gloss": "page %d, %s" % (page, "the caption" if what == "caption" else "the cells in reading order")}

    # A row holding one whole caption, alone, is moved to its figure.
    if len(pieces) == 1:
        SET[(_where, pieces[0][1])] = attributes(pieces[0][0])
        continue
    for at in range(1, len(pieces)):
        SPLIT.append((_where, pieces[at][1][:60], attributes(pieces[at - 1][0]),
                      attributes(pieces[at][0]) if at == len(pieces) - 1 else {}))

# Table 4's caption runs over two lines, the first of which the engine set as a line of its own.
_T4 = [one for one in CAPTIONS if one[0] == "Table 4"][0]
REMOVE_WHERE = r"^Table 4 line"
SPLIT.append(("§3.2.3", "Position non-RL RL beginning 81.40", None, {"where": "Table 4", "gloss": "page 9, the cells in reading order"}))
SPLIT.append(("Table 4", "Figure 8 illustrates the maximum", {}, {"where": "§3.2.3", "gloss": "page 9"}))

# Each ADD goes in just after its anchor, and the later of two on one anchor lands first.
ADD = (
    (("title", "Donna Gerdts Thomas Johnny..."), ("title", AUTHORS, "name", "Ted Kye", "author, Simon Fraser University")),
    (("title", "Donna Gerdts Thomas Johnny..."), ("title", AUTHORS, "name", "Thomas Johnny", "author, Simon Fraser University")),
    (("title", "Donna Gerdts Thomas Johnny..."), ("title", AUTHORS, "name", "Donna Gerdts", "author, Simon Fraser University")),
    (("§3.2.3", "Table 4 summarizes the mean intensity..."), ("Table 4", AUTHORS, "note", _T4[1], "page 9, the caption")),
    (("§2", "Halkomelem is one of twenty-three..."), ("§2", AUTHORS, "place", "Snuneymuxw First Nation", "page 3, where Mrs. Jimmy Joe lived her adult life")),
    (("§2", "Halkomelem is one of twenty-three..."), ("§2", AUTHORS, "place", "Penelakut", "page 3, where Mrs. Jimmy Joe was from")),
    (("§2", "Halkomelem is one of twenty-three..."), ("§2", HUL, "cited form", "Tixulwut", "page 3, Mrs. Jimmy Joe's Hul’q’umi’num’ name")),
    (("§2", "Halkomelem is one of twenty-three..."), ("§2", AUTHORS, "name", "Mrs. Jimmy Joe", "page 3, the storyteller of the six stories, née Ellen Rice")),
)

FORMS = {
    HUL: ("language", AUTHORS, "Hul’q’umi’num’, the Island dialect of Halkomelem, the language of the stories"),
    "/ə/": ("notation", AUTHORS, "schwa, a phoneme of the vowel inventory"),
    "/ʌ/": ("notation", AUTHORS, "page 10, the lower quality schwa takes under rhetorical lengthening"),
    "=stəm": ("cited affix", HUL, "page 12, the passive of the causative suffix, in footnote 4"),
}
# née opens Mrs. Joe's birth name, and həyaʔstəm:4 is həyaʔstəm with its colon and footnote 4,
# a row of its own a line below.
DROP = ("née", "həyaʔstəm:4")

SET[("§4.2.1", "t̓ᶿat̓ᶿəsətəs")] = {"gloss": "page 16, ‘he’s mashing them’"}
SET[("§4.2.1", "kʷintəl")] = {"gloss": "page 17, ‘fight’"}
SET[("§4.2.2", "(a) sʔeləxʷ ‘old, elder’ with no rhetorical lengthening (b) sʔeləxʷ ‘old, elder’ with rhetorical lengthening")] = \
    {"where": "Figure 18", "gloss": "page 20, the labels of its two panels"}
# The practical orthography of (36) and (37) wraps a line the engine read as a gloss.
SET[("(36) line 2", "tthuw’ mukw’ stem thqet.")] = {"kind": "transcription"}
SET[("(37) line 3", "tun’a stseelhtun ni’ wulh ni’ ’u tthu sta’luw’.")] = {"kind": "transcription"}

WHOSE = (
    "The Hul’q’umi’num’ is Mrs. Jimmy Joe's, from six stories she told; each example names its story "
    "and line, and each line of it is given to Hul’q’umi’num’, the free translation to the authors. "
    "The words the prose cites are Hul’q’umi’num’ cited forms."
)

LETTERS = (
    "Each example sets the Hul’q’umi’num’ practical orthography over a phonemic line in the "
    "Americanist alphabet, with ʔ, ə, š, ł, θ, tᶿ, x̌ and xʷ and the glottalized consonants; "
    "rhetorical lengthening is written with repeated vowels between periods, i.i.i, and glossed <RL>."
)

PAGE_NOTES = (
    "The text layer ran twenty-four figure captions and five tables into the prose; each is a row "
    "of its own here, a table's cells kept in reading order."
)
