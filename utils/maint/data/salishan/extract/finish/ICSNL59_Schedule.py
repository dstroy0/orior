# Context for the program of ICSNL 59, UBC Okanagan and the `En'owkin Centre`, July 24 to 27, 2024.
# It is a schedule of talks, not a paper. The engine ran each page into one note; here each printed
# line is a row under its day. The language in it is the names speakers carry in their languages,
# the words of talk titles and the names of the languages, and the engine's candidates follow the
# line that holds them.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import PRIVATE  # noqa: E402
from workdir import WORK  # noqa: E402
import residue  # noqa: E402

STEM = "ICSNL59_Schedule"
AUTHORS = "ICSNL 59 organizers"
NSYILXCEN = "nsyilxcən"
NLAKA = "Nɬeʔkepmxcín"
SECWEP = "Secwepemctsín"
HULQ = "Hul’q’umi’num’"

TITLE = "Program, 59th Annual International Conference on Salish and Neighbouring Languages (ICSNL)"
BYLINE = "the ICSNL 59 organizers, UBC Okanagan and the En’owkin Centre"
VOLUME = "59"

_repair = residue.paper_repair(STEM)
with open(os.path.join(PRIVATE, "pagetext", STEM + ".txt"), encoding="utf-8") as handle:
    PAGE = [" ".join(_repair(one).split()) for one in handle.read().split("\n")]

with open(os.path.join(WORK, STEM + ".draft.tsv"), encoding="utf-8") as handle:
    DRAFT = [one.rstrip("\n").split("\t") for one in handle][1:]

# Every row is read off the page again.
DRAFT_UNTIL = 0

# What each of the engine's candidates is, read off the program and the talks' own papers.
KNOWN = {
    "En’owkin": ("place", AUTHORS, "the En’owkin Centre, Penticton"),
    "k̓ɬk̓əmpíc̓aʔ": ("cited form", NSYILXCEN, "the name of Rose Caldwell (WFN), who opened and closed the conference"),
    "lax̌lax̌tkʷ": ("cited form", NSYILXCEN, "the name of Jeannette Armstrong"),
    "sxʷəxʷəlík̓ʷm": ("cited form", NSYILXCEN, "the name of Ashley Gregoire"),
    "sumaxatkʷ": ("cited form", NSYILXCEN, "the name of Tracey Kim Bonneau"),
    "captikʷɬ": ("cited form", NSYILXCEN, "in the title of Bonneau's talk, captikʷɬ forums with syilx community"),
    "nłeʔkepmxcin": ("language", AUTHORS, "Nɬeʔkepmxcín, spelled with ł, in the title of Janzen and LaFontaine's talk"),
    "Nɬeʔkepmx": ("language", AUTHORS, "the Nɬeʔkepmx speaker of the Interior Salish fluent speaker panel"),
    "Hul’q’umi’num’": ("language", AUTHORS, "the language of Gerdts, Johnny and Kye's talk and the flashtalks"),
    "stk̓másq̓ət": ("cited form", NSYILXCEN, "the name of Skye Fay"),
    "iʔ skcahaháms iʔ sqʷəlqʷílt": ("cited form", NSYILXCEN, "the title of Alexis and Fay's talk, ‘The Nsyilxcn Textbook Project’"),
    "Nɬeʔkepmxcín": ("language", AUTHORS, "the language of Reid's and Smith's talks"),
    "Secwepemctsín": ("language", AUTHORS, "the language of Oliver's talk, and of Nederveen and Oliver's"),
    "–St’át’imcets": ("language", AUTHORS, "St’át’imcets, the language of Davis, Matthewson and Oliver's talk, with the dash the page sets against it"),
    "yə=": ("cited affix", HULQ, "the aspectoid proclitic of Schneider and Gerdts's talk"),
    "xʷíʔ kʷ páq": ("cited form", NLAKA, "the title of Brent Hall's talk, (You Will Be Sorry), a story in Nɬeʔkepmxcín"),
    "ʔe meɬ nes": ("cited form", NLAKA, "the construction of Ella Hannon's talk, ʔé məɬ nés in her paper"),
}
# Words the engine read as English, which the program sets as the language: tsut, the verb of
# Oliver's talk, and Stsptekwll, Secwepemc stories, in Ignace and Gottfriedson's.
PLAIN = {
    "tsut": ("cited form", SECWEP, "the verb of Oliver's talk, thinking and saying"),
    "Stsptekwll": ("cited form", SECWEP, "in the title of Ignace and Gottfriedson's talk, Secwepemc Ornithology and Stsptekwll"),
    "Nsyilxcn": ("language", AUTHORS, "nsyilxcən, in the title of Alexis and Fay's talk"),
    "Secwepemc": ("language", AUTHORS, "Secwepemc, of the panel and of Ignace and Gottfriedson's talk"),
    "Syilx": ("language", AUTHORS, "the Syilx speaker of the panel"),
    "Kwak'wala": ("language", AUTHORS, "the language of Sardinha's talk"),
    "Ktunaxa": ("language", AUTHORS, "the language of two talks"),
    "Lushootseed": ("language", AUTHORS, "the language of Mellesmoen and Urbanczyk's talk"),
    "Pentlatch": ("language", AUTHORS, "the language of Andreatta, Recalma and Urbanczyk's project"),
}
_EDGE_BEFORE = " (‘“–-"
_EDGE_AFTER = " ,.;:?!)’”'"


def held(text, form):
    for found in re.finditer(re.escape(form), text):
        before = text[found.start() - 1] if found.start() else " "
        after = text[found.end()] if found.end() < len(text) else " "
        if before in _EDGE_BEFORE and (after in _EDGE_AFTER or form.endswith("=")):
            return True
    return False


def program():
    rows, where, page, seen = [], "front", 0, set()
    for text in PAGE:
        marker = re.match(r"^===== page (\d+) =====$", text)
        if marker:
            page = int(marker.group(1))
            continue
        # The date at the head of every page is printed once as the program's own date.
        if not text or (text == "2024-07-28" and page > 1):
            continue
        day = re.match(r"^July (2[4-7]), 2024", text)
        if day:
            where = "July %s" % day.group(1)
        rows.append((where, AUTHORS, "heading" if day else "note", text, "page %d" % page))
        for form, (kind, who, gloss) in list(KNOWN.items()) + list(PLAIN.items()):
            if held(text, form):
                # A name is glossed once, where the program first prints it.
                rows.append((where, who, kind, form, "page %d, %s%s" % (page, gloss, ", again" if form in seen else "")))
                seen.add(form)
    return rows


FORMS = {}
DROP = ()
ADD = tuple([(None, ("title", AUTHORS, "title", TITLE, "the program's heading, set over four lines")),
             (None, ("title", AUTHORS, "notation", "the program of a conference, not a paper",
                     "its talks' papers stand in the corpus under ICSNL59"))]
            + [(None, one) for one in program()])

WHOSE = (
    "This is the program of ICSNL 59, a schedule of talks, and none of its words is an example. The "
    "language in it is the names speakers carry in nsyilxcən, the words of talk titles in nsyilxcən, "
    "Secwepemctsín, Nɬeʔkepmxcín and Hul’q’umi’num’, and the names of the languages. Each name and "
    "word is given to the language the program or the talk's own paper names, and the lines of the "
    "program to its organizers."
)

LETTERS = (
    "The names and titles write the practical orthographies of nsyilxcən and Nɬeʔkepmxcín, with k̓, "
    "c̓, q̓, x̌, ɬ and ʔ, and the program once spells Nɬeʔkepmxcín with ł."
)

PAGE_NOTES = (
    "The text layer sets a space after each glottalized or marked letter of a name, k̓ ɬk̓ əmpíc̓ aʔ and "
    "lax̌ lax̌ tkʷ, which the repair closes; the glyph positions set each name with no gap inside it."
)
