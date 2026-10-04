# Context for Brian Diep, Chenxi Xu, Molly Babel and M. Ryan Bochnak, Prosody in Ktunaxa
# Interrogatives: An Initial Examination of Acoustics and Perception. The Ktunaxa is four examples,
# (1) to (4), set word over word: each word of the orthography line stands over its segmentation
# and its gloss, and the page text gives them a line each. Each example is read off the page again
# a tier a row. The eight tables and three figure captions the text layer ran into the prose are
# rows of their own, a printed line of a table a row.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import PRIVATE  # noqa: E402
from workdir import WORK  # noqa: E402
import residue  # noqa: E402

STEM = "ICSNL59_Diep_Xu_Babel_Bochnak_final"
AUTHORS = "Brian Diep, Chenxi Xu, Molly Babel and M. Ryan Bochnak"
KTUNAXA = "Ktunaxa"

TITLE = "Prosody in Ktunaxa Interrogatives: An Initial Examination of Acoustics and Perception"
BYLINE = "Brian Diep, Chenxi Xu, Molly Babel and M. Ryan Bochnak, University of British Columbia"
VOLUME = "59"

_repair = residue.paper_repair(STEM)
with open(os.path.join(PRIVATE, "pagetext", STEM + ".txt"), encoding="utf-8") as handle:
    PAGE = [" ".join(_repair(one).split()) for one in handle.read().split("\n")]
with open(os.path.join(WORK, STEM + ".draft.tsv"), encoding="utf-8") as handle:
    DRAFT = [one.rstrip("\n").split("\t") for one in handle][1:]


def page_of(index):
    """The page a page-text line stands on."""
    for back in range(index, -1, -1):
        marker = re.match(r"^===== page (\d+) =====$", PAGE[back])
        if marker:
            return int(marker.group(1))
    return 0


def chained(anchor, rows):
    """Each row after the one before it, the first after anchor."""
    out = []
    for row in rows:
        out.append((anchor, row))
        anchor = (row[0], row[3])
    return out


def example(number):
    """Example (number): its words in threes, orthography, segmentation and gloss, to the line
    opening on a quote, the free translation. A tier a row."""
    start = next(at for at, one in enumerate(PAGE) if one.startswith("(%d) " % number))
    lines = [PAGE[start][len("(%d) " % number):]]
    for one in PAGE[start + 1:]:
        if one.startswith("‘"):
            translation = one
            break
        lines.append(one)
    page = page_of(start)
    where = "(%d)" % number
    tiers = (("transcription", KTUNAXA, "the orthography"), ("segmentation", KTUNAXA, "the segmentation"),
             ("gloss", KTUNAXA, "the gloss"))
    out = []
    for offset, (kind, who, what) in enumerate(tiers):
        out.append(("%s line %d" % (where, offset + 1), who, kind, " ".join(lines[offset::3]),
                    "page %d, %s, a word over each column" % (page, what)))
    out.append(("%s line 4" % where, AUTHORS, "translation", translation, "page %d" % page))
    return out


def table(caption, end, where=None):
    """The printed lines of a table from its caption to the line before end, a line a row."""
    start = next(at for at, one in enumerate(PAGE) if one.startswith(caption))
    stop = next(at for at in range(start + 1, len(PAGE)) if PAGE[at].startswith(end))
    page = page_of(start)
    where = where or re.match(r"^(Table \d+|Figure \d+)", caption).group(1)
    out = [(where, AUTHORS, "note", PAGE[start], "page %d, the caption" % page)]
    out += [(where, AUTHORS, "note", PAGE[at], "page %d, a line of the table in reading order" % page_of(at))
            for at in range(start + 1, stop) if PAGE[at] and not PAGE[at].isdigit() and not PAGE[at].startswith("=====")]
    return out


EXAMPLES = [example(number) for number in (1, 2, 3, 4)]
TABLE_1 = table("Table 1: Number of tokens", "cues segmental identity")
TABLE_2 = table("Table 2: Summary of each trial", "In most experiments")
TABLE_3 = table("Table 3: GAM parametric", "Table 4: GAM smooth")
TABLE_4 = table("Table 4: GAM smooth", "5.2 Perception study results")
TABLE_5 = table("Table 5: Consultant", "Table 6: Learner 1")
TABLE_6 = table("Table 6: Learner 1", "Table 7: Learner 2")
TABLE_7 = table("Table 7: Learner 2", "In trial 1.1")
TABLE_8 = table("Table 8: Learner 3", "Figure 3: Participant")
FIGURE_1 = [("Figure 1", AUTHORS, "note", "Figure 1: Mean time-normalized f0 contours across Ktunaxa utterance types", "page 9, the caption")]
FIGURE_2 = [("Figure 2", AUTHORS, "note", "Figure 2: GAM model of time-normalized f0 contours across Ktunaxa utterance types", "page 10, the caption")]
FIGURE_3 = [("Figure 3", AUTHORS, "note", "Figure 3: Participant Classification Accuracy for All Question Types vs. Declaratives", "page 12, the caption")]


def _draft(where, opening):
    return [one for one in DRAFT if one[0] == where and one[3].startswith(opening)][0]


def _cut(text, start, end):
    """text with the span from start to the text just before end taken out."""
    head, rest = text.split(start, 1)
    return head.rstrip() + " " + end + rest.split(end, 1)[1]


# The draft rows holding a table in their prose, taken out and set again: the prose around the
# table a row, then the table.
_TOKENS = _draft("§4.2", "The tokens were left unaltered")
_GAM = _draft("§5.1", "Figure 1: Mean")
_TRIAL = _draft("§5.2", "Table 5: Consultant")
_THREE = _draft("§5.2", "In trial 3.1 and 3.2")
_T2 = _draft("§4.4", "Table 2: Summary")
_T3 = _draft("§5.1", "Table 3: GAM")
_T4 = _draft("§5.1", "polar question 1943")
REMOVE = [(one[0], one[3]) for one in (_TOKENS, _GAM, _TRIAL, _THREE, _T2, _T3, _T4)]
REMOVE += [(one[0], one[3]) for one in DRAFT if re.match(r"^\([1-4]\) line", one[0])]

_GAM_PROSE = _GAM[3][len(FIGURE_1[0][3]):].strip()
_GAM_PROSE, _GAM_TAIL = _GAM_PROSE.split(" " + FIGURE_2[0][3] + " ", 1) if FIGURE_2[0][3] in _GAM_PROSE else (_GAM_PROSE, "")
_TRIAL_PROSE = "In trial 1.1" + _TRIAL[3].split("In trial 1.1", 1)[1]
_THREE_HEAD, _THREE_REST = _THREE[3].split(" Table 8: ", 1)
_THREE_TAIL = "In the aforementioned" + _THREE_REST.split("In the aforementioned", 1)[1]

ADD = tuple(
    [(("title", "Brian Diep University..."), ("title", AUTHORS, "name", name, "author, University of British Columbia"))
     for name in ("M. Ryan Bochnak", "Molly Babel", "Chenxi Xu", "Brian Diep")]
    + [(("footnote *", "* Many thanks..."), ("footnote *", AUTHORS, "name", name, "a Ktunaxa consultant, thanked"))
       for name in ("Dorothy Alpine", "Violet Birdstone")]
    + chained(("§1", "ȼ"), EXAMPLES[0] + EXAMPLES[1])
    + chained(("§2.4", "Questions in Ktunaxa are marked..."), EXAMPLES[2])
    + chained(("§2.4", "Like tag questions in English..."), EXAMPLES[3])
    + chained(("§4.2", "Each of the collected utterances..."),
              [("§4.2", AUTHORS, "note", _cut(_TOKENS[3], "Table 1: Number", "cues segmental identity"), "pages 5 and 6")] + TABLE_1)
    + chained(("§4.4", "Each trial varied in exact length..."),
              TABLE_2 + [("§4.4", AUTHORS, "note", "In most experiments" + _T2[3].split("In most experiments", 1)[1], "page 8")])
    + chained(("§5.1", "5.1 Acoustic analysis results"),
              FIGURE_1 + [("§5.1", AUTHORS, "note", _GAM_PROSE + (" " + _GAM_TAIL if _GAM_TAIL else ""), "pages 9 and 10")]
              + FIGURE_2 + TABLE_3 + TABLE_4)
    + chained(("§5.2", "For the perceptual study, we present..."),
              TABLE_5 + TABLE_6 + TABLE_7 + [("§5.2", AUTHORS, "note", _TRIAL_PROSE, "page 11")])
    + chained(("§5.2", "In trial 2.1..."),
              [("§5.2", AUTHORS, "note", _THREE_HEAD, "page 11")] + TABLE_8 + FIGURE_3
              + [("§5.2", AUTHORS, "note", _THREE_TAIL, "page 12")])
    + [(("§2.4", "q̓aksa"), ("§2.4", KTUNAXA, "cited form", "qa(s)", "page 4, ‘how/where/when/why’, a wh-word")),
       (("§2.4", "q̓aksa"), ("§2.4", KTUNAXA, "cited form", "qapsin(s)", "page 4, ‘what/why’, a wh-word")),
       (("§2.4", "q̓aksa"), ("§2.4", KTUNAXA, "cited affix", "k", "pages 1 and 4, the question morpheme, also the subordinate complementizer")),
       (("§2.4", "qaqa"), ("§2.4", KTUNAXA, "cited form", "kqaqa", "page 4, the question tag, “Is that so?”"))]
    + [(("references", "ʔA∙kⱡukaqwum"), ("references", AUTHORS, "language", "Ksanka", "the Kootenai people and language, in the title of the Kootenai Dictionary"))]
    + [(None, ("all", AUTHORS, "notation", "V·", "a long vowel, in the Ktunaxa orthography"))]
)

SPLIT = [
    ("front", "Brian Diep University", {"where": "title", "kind": "title", "gloss": "page 1, the title, its star the acknowledgement footnote's"}, {}),
    ("front", "Abstract:", {"where": "title", "gloss": "page 1, the authors and their affiliations"}, {"gloss": "page 1, the abstract"}),
    ("references", "Li, C. N., and S. A. Thompson", {}, {}),
]

FORMS = {
    "ⱡ": ("notation", AUTHORS, "the Ktunaxa letter for [ɬ], in footnote 1"),
    "ȼ": ("notation", AUTHORS, "the Ktunaxa letter for [t͡s], in footnote 1"),
    "SENĆOŦEN": ("language", AUTHORS, "a Salish language, whose interrogatives lack prosodic correlates by Caldecott (2016)"),
    "Secwepemctsín": ("language", AUTHORS, "a Salish language"),
    "qaʔas": ("cited form", KTUNAXA, "page 4, ‘where/when’, a wh-word"),
    "q̓aksa": ("cited form", KTUNAXA, "page 4, ‘how many/how much’, a wh-word"),
    "qaqa": ("cited form", KTUNAXA, "page 4, ‘to be so’"),
    "St’át’imcets": ("language", AUTHORS, "in the title of Caldecott 2016"),
    "Gösta": ("name", AUTHORS, "in a reference entry"),
    "ʔA∙kⱡukaqwum": ("cited form", KTUNAXA, "in the title of the Kootenai Dictionary, Ksanka ʔA∙kⱡukaqwum"),
    "Laboratório": ("cited form", "Portuguese", "in a reference entry"),
}
DROP = ("ì",)
SET = {("§2.4", "qaⱡa(s)"): {"form": "qaⱡa(s)", "gloss": "page 4, ‘who/whom’, a wh-word"}}

WHOSE = (
    "The Ktunaxa examples were recorded from the authors' consultants, L1 speakers, and each word, "
    "segmentation and gloss is given to Ktunaxa; the translations are the authors'. The wh-words "
    "and the tag of §2.4 are Ktunaxa cited forms, and the Salish languages named for comparison are "
    "language rows."
)

LETTERS = (
    "The examples write the standardized Ktunaxa orthography of the Kootenai Culture Committee (1999), "
    "with ⱡ for [ɬ], ȼ for [t͡s], ʔ, t̓ and q̓, and a raised dot after a long vowel."
)

PAGE_NOTES = (
    "The examples are set word over word, and the page text gives each word, segmentation and gloss "
    "a line; they are read a tier a row. The text layer ran the eight tables and three figure captions "
    "into the prose, and codes the [ɬ] of footnote 1 as ì and drops the tie bar of [t͡s]; the footnote "
    "is read off the page at 400 dpi."
)
