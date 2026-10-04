# Context for the schedule of ICSNL 57, the Nicola Valley Institute of Technology, August 11 and 12,
# 2022. It is a schedule of talks, not a paper. Each printed line is a row under its day, and the
# language names and words a line holds follow it.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import PRIVATE  # noqa: E402
import residue  # noqa: E402

STEM = "ICSNL57_program"
AUTHORS = "ICSNL 57 organizers"

TITLE = "57th International Conference on Salish and Neighbouring Languages"
BYLINE = "the ICSNL 57 organizers, the Nicola Valley Institute of Technology"
VOLUME = "57"

_repair = residue.paper_repair(STEM)
with open(os.path.join(PRIVATE, "pagetext", STEM + ".txt"), encoding="utf-8") as handle:
    PAGE = [" ".join(_repair(one).split()) for one in handle.read().split("\n")]

# Every row is read off the page again.
DRAFT_UNTIL = 0

# What each word and name is, read off the schedule.
KNOWN = {
    "Proto-Salish": ("language", AUTHORS, "in the title of Gloria Mellesmoen's talk"),
    "Upper Chehalis": ("language", AUTHORS, "the language of Gloria Mellesmoen's talk"),
    "ʔayʔaǰuθəm": ("language", AUTHORS, "the language of Daniel Reisinger's first talk"),
    "Secwepemctsín": ("language", AUTHORS, "the language of Julia Schillo's and Sander Nederveen's talks"),
    "nsyilxcən": ("language", AUTHORS, "the language of Craig Carpenter's talk"),
    "Haisla": ("language", AUTHORS, "the language of Jonathan Janzen's talk"),
    "Westcoast": ("language", AUTHORS, "Westcoast, South Wakashan, the language of Adam Werle's talk"),
    "Kwak̓wala": ("language", AUTHORS, "the language of Peter Jacobs's talk"),
    "Ktunaxa": ("language", AUTHORS, "the language of Kate (Yangshuying) Zhou's talk"),
    "Hul’q’umi’num’": ("language", AUTHORS, "the language of Lauren Schneider's talk"),
    "Nsyilxcn": ("language", AUTHORS, "nsyilxcən, in the name of the Bachelor of Nsyilxcn Language Fluency program"),
    "Nuuchahnulth": ("language", AUTHORS, "Barkley Sound Nuuchahnulth, in the title of Henry Kammler's talk"),
    "Ahts": ("name", AUTHORS, "the Ahts, in the title of the catechism Henry Kammler's talk retrieves"),
    "Comox-Sliammon": ("language", AUTHORS, "the language of Daniel Reisinger and Laura Griffin's talk"),
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
        day = re.match(r"^\w+day, August (1[12])th:$", text)
        if day:
            where = "August %s" % day.group(1)
        rows.append((where, AUTHORS, "heading" if day else "note", text, "page %d" % page))
        for form, (kind, who, gloss) in KNOWN.items():
            if held(text, form):
                # A name is glossed once, where the schedule first prints it.
                rows.append((where, who, kind, form, "page %d, %s%s" % (page, gloss, ", again" if form in seen else "")))
                seen.add(form)
    return rows


FORMS = {}
DROP = ()
ADD = tuple([(None, ("title", AUTHORS, "title", TITLE, "the schedule's heading line")),
             (None, ("title", AUTHORS, "notation", "the schedule of a conference, not a paper",
                     "its talks' papers stand in the corpus under ICSNL57"))]
            + [(None, one) for one in program()])

WHOSE = (
    "This is the schedule of ICSNL 57, a list of talks, and none of its words is an example. The "
    "language in it is the names of the languages the talks are about, given to the organizers with "
    "the lines of the schedule."
)

LETTERS = (
    "The text layer sets ʔayʔaǰuθəm, nsyilxcən and Kwak̓wala apart letter by letter and drops the fi "
    "ligature of Infixing and Griffin; residue corrections close them up and put it back."
)

PAGE_NOTES = ""
