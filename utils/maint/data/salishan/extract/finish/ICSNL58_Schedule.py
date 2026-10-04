# Context for the schedule of ICSNL 58, the `Snuneymuxw Learning Centre`, July 27 to 29, 2023.
# It is a schedule of talks, not a paper. The engine kept only its candidates and two lines; here
# each printed line is a row under its day, and the words and language names a line holds follow it.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import PRIVATE  # noqa: E402
import residue  # noqa: E402

STEM = "ICSNL58_Schedule"
AUTHORS = "ICSNL 58 organizers"
HULQ = "Hul’q’umi’num’"

TITLE = "ICSNL58 Snuneymuxw Learning Centre July 27-29, 2023"
BYLINE = "the ICSNL 58 organizers, Donna Gerdts and Lauren Schneider, the Snuneymuxw Learning Centre"
VOLUME = "58"

_repair = residue.paper_repair(STEM)
with open(os.path.join(PRIVATE, "pagetext", STEM + ".txt"), encoding="utf-8") as handle:
    PAGE = [" ".join(_repair(one).split()) for one in handle.read().split("\n")]

# Every row is read off the page again.
DRAFT_UNTIL = 0

# What each word and name is, read off the schedule.
KNOWN = {
    "Snuneymuxw": ("name", AUTHORS, "the Snuneymuxw First Nation, host of the conference"),
    "xu’athun": ("cited form", HULQ, "in the title of Luke Marston's talk"),
    "stl’q’een’": ("cited form", HULQ, "in the title of Luke Marston's talk"),
    "hwulmuhw": ("cited form", HULQ, "in the title of Luke Marston's talk, hwulmuhw perspectives on art and language"),
    "hwsxwi'xwi'em'": ("cited form", HULQ, "the title of the Hul’q’umi’num’ storytellers workshop"),
    "Hwulmuhwqun Hwst'ilum": ("cited form", HULQ, "the session led by Gina Salazar"),
    "Hul'q'umi'num'": ("language", AUTHORS, "Hul’q’umi’num’, with straight apostrophes"),
    "Hul’q’umi’num’": ("language", AUTHORS, "the language of the storytellers workshop and the plurals session"),
    "pentl’ach": ("language", AUTHORS, "Pentlatch, in the title of Andreatta, Recalma and Urbanczyk's talk"),
    "Lushootseed": ("language", AUTHORS, "the language of Ted K. Kye's talk"),
    "Secwepemctsín": ("language", AUTHORS, "the language of Julia Schillo's talk"),
    "Nsyilxcn": ("language", AUTHORS, "nsyilxcən, the language of three talks"),
    "St’át’imcets": ("language", AUTHORS, "the language of two talks by Henry Davis"),
    "Haisla": ("language", AUTHORS, "the language of Janzen's and Murphey's talks"),
    "Haislakala": ("language", AUTHORS, "Haisla, the language of Grace Baleno's talk"),
    "Nɬeʔkepmxcín": ("language", AUTHORS, "the language of five talks"),
    "ʔayʔaǰuθəm": ("language", AUTHORS, "the language of Marianne Huijsmans's talk"),
    "ʔayʔajuθəm": ("language", AUTHORS, "ʔayʔaǰuθəm, spelled with j, in the title of Reisinger and Huijsmans's talk"),
    "Kwak'wala": ("language", AUTHORS, "the language of Katie Sardinha's talk"),
}
_EDGE_BEFORE = " (‘“–-"
_EDGE_AFTER = " ,.;:?!)’”':"


def held(text, form):
    for found in re.finditer(re.escape(form), text):
        before = text[found.start() - 1] if found.start() else " "
        after = text[found.end()] if found.end() < len(text) else " "
        if before in _EDGE_BEFORE and after in _EDGE_AFTER:
            return True
    return False


def program():
    rows, where, page, seen = [], "front", 0, set()
    for text in PAGE:
        marker = re.match(r"^===== page (\d+) =====$", text)
        if marker:
            page = int(marker.group(1))
            continue
        if not text:
            continue
        day = re.match(r"^\w+day (2[7-9])th July 2023", text)
        if day:
            where = "July %s" % day.group(1)
        elif text == "Organizers:":
            where = "organizers"
        rows.append((where, AUTHORS, "heading" if day else "note", text, "page %d" % page))
        for form, (kind, who, gloss) in KNOWN.items():
            if held(text, form):
                # A name is glossed once, where the schedule first prints it.
                rows.append((where, who, kind, form, "page %d, %s%s" % (page, gloss, ", again" if form in seen else "")))
                seen.add(form)
    return rows


FORMS = {}
DROP = ()
ADD = tuple([(None, ("title", AUTHORS, "title", TITLE, "the schedule's heading line, its dashes left out")),
             (None, ("title", AUTHORS, "notation", "the schedule of a conference, not a paper",
                     "its talks' papers stand in the corpus under ICSNL58"))]
            + [(None, one) for one in program()])

WHOSE = (
    "This is the schedule of ICSNL 58, a list of talks, and none of its words is an example. The "
    "language in it is the Hul’q’umi’num’ words of the opening day's titles and the names of the "
    "languages the talks are about. Each word is given to Hul’q’umi’num’, and the lines of the "
    "schedule to its organizers."
)

LETTERS = (
    "The Hul’q’umi’num’ titles write its practical orthography, twice with straight apostrophes, "
    "hwsxwi'xwi'em' and Hwst'ilum."
)

PAGE_NOTES = ""
