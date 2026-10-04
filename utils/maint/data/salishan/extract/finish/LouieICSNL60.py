# Context for Louie et al., niniǰɛ ʔəkʷ χʷɛƛ̓ay, ƛ̓aɬəm, hega ɬəlkælɛ, niniǰɛ q̓ʷaq̓ʷθəms təsqanaməs.
# One narrative, told by Freddie Louie with Elsie Paul, is printed three times: as said in
# ʔayʔaǰuθəm with English in Section 2.1, in English in Section 2.2, and glossed line by line in
# Section 2.3, each line (1) to (105) tagged F or E and closed on its time in the recording. The
# three are parsed here from the page text read by glyph rows, one turn or line to a row, in place
# of the engine's rows for them.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import PRIVATE  # noqa: E402
from workdir import WORK  # noqa: E402
import residue  # noqa: E402

STEM = "LouieICSNL60"
AUTHORS = ("Freddie Louie, Henry Davis, Laura Griffin, Marianne Huijsmans, Gloria Mellesmoen, "
           "Daniel K. E. Reisinger and Bailey Trotter")
L = "ʔayʔaǰuθəm"
SPEAKERS = {"F": "Freddie Louie", "E": "Elsie Paul"}

TITLE = ("niniǰɛ ʔəkʷ χʷɛƛ̓ay, ƛ̓aɬəm, hega ɬəlkælɛ, niniǰɛ q̓ʷaq̓ʷθəms təsqanaməs ‘Mountain goats, "
         "salt, and bullets’: A historical narrative told by late Freddie Louie")
BYLINE = ("Freddie Louie, Tla’amin Nation; Henry Davis, University of British Columbia; Laura Griffin, "
          "University of Toronto; Marianne Huijsmans, University of Alberta; Gloria Mellesmoen, "
          "University of Victoria; Daniel K. E. Reisinger, University of British Columbia; Bailey "
          "Trotter, University of British Columbia")

with open(os.path.join(WORK, STEM + ".draft.tsv"), encoding="utf-8") as handle:
    DRAFT = [one.rstrip("\n").split("\t") for one in handle][1:]

_repair = residue.paper_repair(STEM)
with open(os.path.join(PRIVATE, "pagetext", STEM + ".txt"), encoding="utf-8") as handle:
    PAGE = [" ".join(_repair(one).split()) for one in handle.read().split("\n")]

PAGE_MARK = re.compile(r"^===== page (\d+) =====$")
TIME = re.compile(r"\s*(\d\d:\d\d\.\d{3})$")
TURN = re.compile(r"^([FE]): ?(.*)$")
EXAMPLE = re.compile(r"^\((\d+)\) ([FE]): ?(.*)$")


def region(first, last):
    """(page, line) for the page lines from the one opening on first to the one opening on last,
    without page numbers, page marks and the footnotes at each page's foot. A footnote opens on its
    number, the next one due, and runs to the page mark."""
    start = next(number for number, text in enumerate(PAGE) if text.startswith(first))
    page = max(int(PAGE_MARK.match(text).group(1)) for text in PAGE[:start] if PAGE_MARK.match(text))
    kept, in_footnote = [], False
    for text in PAGE[start + 1:]:
        if text.startswith(last):
            break
        marked = PAGE_MARK.match(text)
        if marked:
            page, in_footnote = int(marked.group(1)), False
            continue
        if not text or re.fullmatch(r"\d{3}", text):
            continue
        opened = re.match(r"^(\d{1,2}) \S", text)
        if opened and int(opened.group(1)) == FOOTNOTES[0]:
            FOOTNOTES.pop(0)
            in_footnote = True
        if not in_footnote:
            kept.append((page, text))
    return kept


FOOTNOTES = list(range(1, 18))


def turns(first, last, kind, who_of, note):
    """One row per turn of a speaker, F: or E: and the lines that run on from it."""
    rows = []
    for page, text in region(first, last):
        said = TURN.match(text)
        if said:
            rows.append([page, said.group(1), said.group(2)])
        elif rows:
            rows[-1][2] += " " + text
    where = first.split()[0]
    return [("§" + where, who_of(speaker), kind, form, "page %d, %s" % (page, note % SPEAKERS[speaker]))
            for page, speaker, form in rows]


def unmarked(text, marks):
    """text without the footnote marks it carries at a word's end, gɩǰɛ.2 and ground3, each noted in
    marks. A mark after a dash, the false starts na(U+2014)6 and hiɬ-10, stays on the word, as the page
    runs the two together."""
    marks.extend(re.findall(r"(?<=[^\s\d:])(\d{1,2})(?=\s|$)", text))
    return re.sub(r"(?<=[^\s\d:—-])(\d{1,2})(?=\s|$)", "", text)


def at_time(time):
    """Where a line stands in the recording; (56) and (76) print no time."""
    return "at %s in the recording" % time if time else "no time printed"


def glossed():
    """The rows of each line (N) of Section 2.3: its words as said, their segmentation and gloss,
    three lines to a row of words, then the English and the time in the recording. A line said in
    English only is its quote and time."""
    lines = region("2.3 Glossed version", "References")
    starts = [number for number, (page, text) in enumerate(lines) if EXAMPLE.match(text)]
    out = []
    for at, start in enumerate(starts):
        chunk = lines[start:starts[at + 1] if at + 1 < len(starts) else len(lines)]
        page = chunk[0][0]
        number, speaker, first = EXAMPLE.match(chunk[0][1]).groups()
        texts = [first] + [text for _, text in chunk[1:]]
        where = "(%s)" % number
        time = ""
        # A line said in English is its quote alone; (101) prints its words as said in quotes too,
        # over a segmentation, a gloss and an English line of their own.
        english_only = first.startswith("‘") and not any(text.startswith("‘") for text in texts[1:])
        tiers, english, index = [], [], 0
        if not english_only:
            while index < len(texts) and not (index and texts[index].startswith("‘")):
                tiers.append(texts[index])
                index += 1
        english = " ".join(texts[index:])
        timed = re.search(r"(\d\d:\d\d\.\d{3})", english)
        if timed:
            time = timed.group(1)
            english = english[:timed.start()].strip()
        line = 0
        names = ("transcription", "segmentation", "gloss")
        for place, text in enumerate(tiers):
            marks = []
            form = unmarked(text, marks)
            kind = names[place % 3]
            line += 1
            gloss = "page %d, %s" % (page, "as said by " + SPEAKERS[speaker] if kind == "transcription" else kind)
            if marks:
                gloss += ", footnote %s" % " and ".join(marks)
            out.append((where, (where + " line %d" % line,
                                SPEAKERS[speaker] if kind == "transcription" else L, kind, form, gloss)))
        marks = []
        english = unmarked(english, marks)
        line += 1
        if english_only:
            gloss = "page %d, said in English by %s, %s" % (page, SPEAKERS[speaker], at_time(time))
            row = (where + " line %d" % line, SPEAKERS[speaker], "transcription", english, gloss)
        else:
            gloss = "page %d, %s" % (page, at_time(time))
            row = (where + " line %d" % line, AUTHORS, "translation", english, gloss)
        if marks:
            row = row[:4] + (row[4] + ", footnote %s" % " and ".join(marks),)
        out.append((where, row))
    return out


NARRATIVE = turns("2.1 ʔayʔaǰuθəm Text", "2.2 Direct English Translation", "transcription",
                  lambda speaker: SPEAKERS[speaker], "a turn of %s, as said")
TRANSLATION = turns("2.2 Direct English Translation", "2.3 Glossed version", "translation",
                    lambda speaker: AUTHORS, "the English of a turn of %s")
GLOSSED = glossed()

# The engine's rows for the three tellings give way to the parsed ones; the headings, the paragraph
# opening Section 2.1 and the footnotes stay.
REMOVE_WHERE = r"^\(\d+\)"
REMOVE = tuple(
    (where, form) for where, who, kind, form, gloss in DRAFT
    if where in ("§2.1", "§2.2", "§2.3") and kind != "heading" and not form.startswith("Preceding")
    and not (re.match(r"^\d{1,2} \S", form) and ("footnote" in gloss or form.startswith("6 There")))
)


def display(anchor, rows):
    """ADD entries for rows, each placed after the one before it, the first after anchor."""
    added = []
    for row in rows:
        added.append((anchor, row))
        anchor = (row[0], row[3])
    return added


def names(where, entries):
    return [(where, (where, AUTHORS, kind, form, gloss)) for kind, form, gloss in entries]


FORMS = {
    "niniǰɛ": ("cited form", L, "page 1, in the title"),
    "ʔəkʷ": ("cited form", L, "page 1, in the title"),
    "χʷɛƛ̓ay": ("cited form", L, "page 1, in the title, ‘mountain goat’"),
    "ƛ̓aɬəm": ("cited form", L, "page 1, in the title, ‘salt’"),
    "ɬəlkælɛ": ("cited form", L, "page 1, in the title, ‘bullets’"),
    "q̓ʷaq̓ʷθəms": ("cited form", L, "page 1, in the title"),
    "təsqanaməs": ("cited form", L, "page 1, in the title"),
    "Tla’amin": ("name", AUTHORS, "the Tla’amin Nation, of Freddie Louie and Elsie Paul"),
    "ʔayʔaǰuθəm": ("language", AUTHORS, "a.k.a. Comox-Sliammon, ISO 639-3 coo, Central Salish, the language of the paper"),
    "q̓ʷʊšoθɛnəm": ("cited form", L, "page 1, making people happy, often through teasing and making people laugh"),
    "gət̓θgət̓θ": ("cited form", L, "page 1, a good-humoured tease"),
    "kɛpo": ("cited form", L, "page 1, ‘coat’, Freddie's nickname for Daniel"),
    "lalyɛm": ("cited form", L, "page 1, ‘little devil’, Freddie's nickname for Gloria"),
}

# ʔayʔa- and ǰuθəm are ʔayʔaǰuθəm broken over a line on page 2.
DROP = ("ʔayʔa-", "ǰuθəm")

SPLIT = (
    ("front", "Freddie Louie Tla’amin Nation", {"kind": "title", "gloss": "page 1, the title, which carries footnote *"}, {}),
    ("front", "Abstract:", None, {}),
    ("footnote *", "Contact info:", {}, {"where": "front", "gloss": "page 1, at the foot of the first page"}),
    ("front", "In Proceedings of the International", {},
     {"gloss": "page 1, the volume, at the foot of the first page"}),
)

SET = {
    ("§2.3", "6 There is a false start here."): {"where": "footnote 6", "gloss": "page 7, footnote"},
}

ADD = tuple(
    [(None, ("title", AUTHORS, "name", name, "an author, " + place))
     for name, place in (("Freddie Louie", "Tla’amin Nation, 1936–2025, who tells the narrative"),
                         ("Henry Davis", "University of British Columbia"),
                         ("Laura Griffin", "University of Toronto"),
                         ("Marianne Huijsmans", "University of Alberta"),
                         ("Gloria Mellesmoen", "University of Victoria"),
                         ("Daniel K. E. Reisinger", "University of British Columbia"),
                         ("Bailey Trotter", "University of British Columbia"))]
    + [(("front", "ɬəlkælɛ"), ("front", L, "cited form", "hega", "page 1, in the title, ‘and’"))]
    + names("front", [
        ("language", "Comox-Sliammon", "ʔayʔaǰuθəm, ISO 639-3 coo"),
        ("language", "Central Salish", "the branch ʔayʔaǰuθəm belongs to"),
        ("place", "Strait of Georgia", "along whose northern part ʔayʔaǰuθəm is spoken"),
    ])
    + names("footnote *", [("name", "Elsie Paul", "who helped verify the translation and the title")])
    + names("§1", [
        ("name", "FirstVoices", "which hosts the ʔayʔaǰuθəm e-dictionary"),
        ("name", "Daniel", "Daniel K. E. Reisinger, whom Freddie called kɛpo"),
        ("name", "Gloria", "Gloria Mellesmoen, whom Freddie called lalyɛm"),
    ])
    + names("§2", [
        ("place", "Tla’amin", "where the narrative was recorded, May 17, 2016"),
        ("place", "Toba Inlet", "up which two men found salt and lead while hunting mountain goats"),
        ("name", "Elsie Paul", "an Elder of the Tla’amin Nation, E in the text"),
        ("name", "Freddie", "Freddie Louie, F in the text"),
        ("language", "English", "into which Freddie switches in the narrative"),
    ])
    + display(("§2.1", "Preceding the start..."), NARRATIVE)
    + display(("§2.2", "2.2 Direct English Translation"), TRANSLATION)
    + names("§2.1", [("name", "Marianne", "Marianne Huijsmans, who asks Freddie to tell the story in ʔayʔaǰuθəm")])
    + names("footnote 5", [("language", "Sechelt", "whose ʔɩš, an exclamation of disbelief, ʔišna may be like")])
    + names("footnote 12", [("place", "Toby Inlet", "Freddie's name for Toba Inlet")])
    + names("footnote 13", [
        ("name", "Sam August", "sәnpoliyan, originally from Sechelt, who married into Tla’amin"),
        ("place", "Sechelt", "where Sam August was from"),
    ])
)
ADD = ADD + tuple(
    (("§2.3", "2.3 Glossed version") if index == 0 else (GLOSSED[index - 1][1][0], GLOSSED[index - 1][1][3]), row)
    for index, (where, row) in enumerate(GLOSSED)
)

WHOSE = (
    "The language is ʔayʔaǰuθəm, a.k.a. Comox-Sliammon, Central Salish, and the paper is one narrative "
    "told by Freddie Louie of the Tla’amin Nation on May 17, 2016, with Elsie Paul, recorded, transcribed, "
    "translated and glossed by the seven authors, Freddie Louie first among them.\n\n"
    "A turn as said, in Section 2.1, and the words of a line as said, in Section 2.3, are the speaker's: "
    "Freddie Louie for F and Elsie Paul for E, with the English they switch into kept in place. A line of "
    "Section 2.3 said in English only is the speaker's too. The segmentation and gloss lines carry "
    "ʔayʔaǰuθəm, and the English of Section 2.2 and of each glossed line is the authors' translation. "
    "The cited words of Section 1, the prose, the headings and the footnotes carry the authors."
)

LETTERS = (
    "The line as said is in a practical orthography that writes what Freddie and Elsie say: ɛ, ɩ, ʊ and æ "
    "for lowered and centralized vowels, o, χ for the uvular fricative, ǰ, č, θ, ɬ, ƛ̓ and the superscript "
    "ᶿ of t̓ᶿ. The segmentation writes the underlying forms in a phonemic orthography, i, ə, u and a for "
    "the vowels and x̌ for the uvular, θɛqɛtəm over θiq-it-əm. Affixes take a hyphen, clitics =, "
    "portmanteaus +, infixes angled brackets, reduplication ∼, and an elided segment square brackets, "
    "təq-ipa[n]-t-əm=k̓ʷa."
)

PAGE_NOTES = (
    "The glossed lines set each word over its segmentation and gloss, read here by glyph rows at a "
    "0.112 em word space; the text layer, which sets each column on lines of its own, settled the "
    "tokens the rows read differently, cedar.shakes, NEG ???, <STAT>, and the ‘ of ‘And, which the "
    "glyph stream puts after the A. (101) prints Freddie's words as said in quotes. (56) and (76) print "
    "no time. A footnote mark on a word, gɩǰɛ.2, is noted in the gloss; after a dash, na—6 and hiɬ-10, "
    "it stays on the word. Footnote 13 prints sәnpoliyan with a Cyrillic ә."
)
