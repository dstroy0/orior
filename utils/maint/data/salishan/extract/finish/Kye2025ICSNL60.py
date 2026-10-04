# Context for Kye, The realization of /a/ in ay in Southern Lushootseed storytelling. The paper has
# no numbered examples: its Lushootseed is cited in the prose, set in italics, and in two tables,
# Table 1 of minimal pairs and Table 2 of the words holding /ay/, read off the page text below one
# row to a line. Snyder's 1957 transcriptions are his, and the phonetics of writer and wife in
# footnote 2 and Section 5 is English.
import os
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import PRIVATE  # noqa: E402
from workdir import WORK  # noqa: E402
import residue  # noqa: E402

STEM = "Kye2025ICSNL60"
AUTHORS = "Ted Kye"
L = "Lushootseed"
SNYDER = "Snyder 1957"

TITLE = "The realization of /a/ in ay in Southern Lushootseed storytelling"
BYLINE = "Ted Kye, University of Washington"

with open(os.path.join(WORK, STEM + ".draft.tsv"), encoding="utf-8") as handle:
    DRAFT = [one.rstrip("\n").split("\t") for one in handle][1:]

_repair = residue.paper_repair(STEM)
with open(os.path.join(PRIVATE, "pagetext", STEM + ".txt"), encoding="utf-8") as handle:
    PAGE = [" ".join(_repair(one).split()) for one in handle.read().split("\n")]


def draft_form(where, opening):
    """The form of the draft row at where opening on these words."""
    for one in DRAFT:
        if one[0] == where and one[3].startswith(opening):
            return one[3]
    raise SystemExit("no draft row %s opens with %s" % (where, opening))


def rows_between(first, last):
    """The page lines after the one opening on first, up to the one opening on last."""
    start = next(number for number, text in enumerate(PAGE) if text.startswith(first))
    end = next(number for number, text in enumerate(PAGE) if number > start and text.startswith(last))
    return [text for text in PAGE[start + 1:end] if text]


def display(anchor, rows):
    """ADD entries for rows (where, who, kind, form, gloss), each placed after the one before it,
    the first after anchor."""
    added = []
    for row in rows:
        added.append((anchor, row))
        anchor = (row[0], row[3])
    return added


def names(where, entries):
    """ADD entries for the names at where, each (kind, form, gloss)."""
    return [(where, (where, AUTHORS, kind, form, gloss)) for kind, form, gloss in entries]


TABLE_1 = rows_between("Table 1 List of", "One argument that Snyder")
TABLE_2 = rows_between("or [ɐy].", "When examining [ay]")

FORMS = {
    "suq̓ʷabš": ("name", L, "page 1, the Suquamish, whose Southern Lushootseed Snyder 1957 describes"),
    "dɐy̓": ("cited form", L, "page 1, day̓ ‘only’ as heard with [ɐ]"),
    "/jˀ/": ("cited form", L, "page 1, the IPA of /y̓/"),
    "ʌ": ("cited form", SNYDER, "Snyder's transcription of a variant of /a/, among [ə ʌ a]"),
    "ʌy": ("cited form", SNYDER, "page 2, Snyder's narrow transcription of /ay/"),
    "yóq̓ʷʌyʔ": ("cited form", SNYDER, "page 2, Snyder's narrow transcription of yuq̓ʷay̓ ‘rotten stick’"),
    "sɬadʌy": ("cited form", SNYDER, "page 2, Snyder's narrow transcription of sɬadayʔ ‘woman’"),
    "sqʷubʌ́yʔ": ("cited form", SNYDER, "page 2, Snyder's narrow transcription of sqʷəbayʔ ‘dog’"),
    "uáytxʷčid": ("cited form", SNYDER, "page 2, Snyder's transcription of ʔuʔaydxʷ čəd ‘I found it’, with ay"),
    "xʷ": ("cited form", L, "page 2, the labiovelar fricative, before which an unstressed schwa rounds"),
    "ʉ": ("cited form", L, "page 2, the rounded schwa before a labiovelar consonant, Kye 2023b"),
    "sləx̌il": ("cited form", L, "page 3, with the inchoative suffix -il"),
    "sləx̌i": ("cited form", L, "page 3, sləx̌il as realized in the Southern dialect, the l of -il not pronounced"),
    "=əlgʷə": ("cited affix", L, "page 3, ‘they/them’, a vowel-initial clitic"),
    "=əxʷ": ("cited affix", L, "page 3, ‘now’, a vowel-initial clitic"),
    "ʔə": ("cited form", L, "page 3, an unstressed particle opening on a glottal stop"),
    "ʔal": ("cited form", L, "page 3, an unstressed particle opening on a glottal stop"),
    "tə́yil": ("cited form", L, "page 3, təyil ‘go upstream’ with the Southern stress, on the schwa"),
    "təyíl": ("cited form", L, "page 3, təyil ‘go upstream’ with the Northern stress, on /i/"),
    "kaykay": ("cited form", L, "page 5, ‘Steller Jay’"),
    "ɾ": ("cited form", "English", "page 11, footnote 2, the flapped /t/ of English"),
    "wɹɐɪɾə˞": ("cited form", "English", "page 11, footnote 2, English writer, with Canadian Raising before a flap"),
}

# The pieces the text layer cuts from their brackets, /ə of /ə i a u/ and [ɐ] carrying footnote 1,
# ɜ of the IPA list in footnote 1, and the words of the phrases written whole below.
DROP = ("/ə", "[ɐ]1", "ɜ", "ʔuʔaydxʷ", "čəd", "ɬəčil", "stubš", "ɬəči")

SPLIT = (
    # The front matter runs the title, the byline and the abstract together.
    ("front", "Ted Kye University", None, {}),
    ("front", "Abstract:", None, {}),
    # Footnote * runs on into the contact line.
    ("footnote *", "Contact info:", {}, {"where": "front", "gloss": "page 1, at the foot of the first page"}),
    # Table 1 and Table 2 run their cells into the prose after them; the cells are read again one
    # row to a line below.
    ("§1", "/a/ /ə/ bak̓ʷ", {"where": "Table 1", "gloss": "page 2, the caption"}, {"where": "Table 1"}),
    ("Table 1", "One argument that Snyder made", None, {"where": "§1", "gloss": "page 2"}),
    ("§4.1", "When examining [ay] and [ɐy]", None, {"gloss": "page 6"}),
    # A caption runs into the prose after it, and the panel labels (a) (b) of Figure 3 into the
    # caption of Figure 4.
    ("§2", "There are some phonological features", {"gloss": "page 3, the caption of Figure 1"}, {}),
    ("§4.2", "Figure 4 is a formant chart", {"gloss": "page 7, the caption of Figure 3"}, {"gloss": "page 7"}),
    ("§4.2", "Figure 4 Formant chart", None, {"gloss": "page 7, the caption of Figure 4"}),
    ("§5", "Figure 7 Rhetorical lengthening", {}, {"gloss": "page 12, the caption of Figure 7"}),
)

SET = {
    ("§4.1", draft_form("§4.1", "Table 2 List of")): {"where": "Table 2", "gloss": "page 6, the caption"},
    ("§4.1", draft_form("§4.1", "Figure 2 Example")): {"gloss": "page 6, the caption of Figure 2"},
    ("§4.3", draft_form("§4.3", "Figure 5 Average")): {"gloss": "page 9, the caption of Figure 5"},
    ("§4.3", draft_form("§4.3", "Figure 6 Scatter")): {"gloss": "page 10, the caption of Figure 6"},
    ("§1", "day̓"): {"gloss": "page 1, ‘only’, the adverbial auxiliary, heard as [dɐy̓] or [day̓]"},
    ("§4.1", "ʔay̓gʷəs"): {"gloss": "page 5, ‘exchange’"},
    ("§5", "ʔay̓gʷəs"): {"gloss": "page 12, ‘exchange’"},
    ("§5", "šay̓"): {"gloss": "page 12, ‘show, reveal’"},
}

ADD = tuple(
    [(None, ("title", AUTHORS, "title", TITLE, "the paper's title, which carries footnote *")),
     (None, ("title", AUTHORS, "name", "Ted Kye", "the author, University of Washington")),
     ]
    + names("footnote *", [
        ("name", "Annie Jack", "the elder speaker of the study, a storyteller of Southern Lushootseed"),
        ("name", "Denise Bill", "a great-granddaughter of Annie Jack"),
        ("name", "Willard Bill, Jr.", "a great-grandson of Annie Jack"),
        ("name", "Elise Bill-Gerrish", "a great-great-granddaughter of Annie Jack, daughter of Denise Bill"),
        ("name", "Justice Bill", "a great-great-grandson of Annie Jack, son of Willard Bill, Jr."),
        ("name", "Burke Museum", "which made the recordings available"),
        ("name", "Laurel Sercombe", "who digitized the recordings"),
        ("name", "Leon Metcalf", "who spent about six years recording elder speakers of Lushootseed in the 1950’s"),
    ])
    + names("§1", [
        ("language", "Lushootseed", "ISO 639-3 lut, Coast Salish, the language of the paper"),
        ("language", "Southern Lushootseed", "the dialect of the study"),
        ("language", "Southern Puget Salish", "the name of Southern Lushootseed in Snyder 1957"),
        ("name", "Warren Snyder", "the anthropologist whose 1957 grammar describes the Suquamish variety"),
    ])
    + display(("§1", "Lushootseed is a Coast Salish language..."), [
        ("§1", L, "cited form", "/ə i a u/", "page 1, the four contrastive vowels of Lushootseed"),
    ])
    + display(("Table 1", "Table 1 List of (near) minimal pairs..."),
              [("Table 1", AUTHORS, "note", text,
                "page 2, the heads of Table 1" if number == 0 else
                "page 2, a row of Table 1: an /a/ word and an /ə/ word, each with its gloss")
               for number, text in enumerate(TABLE_1)])
    + display(("§1", "uáytxʷčid"), [
        ("§1", L, "cited form", "ʔuʔaydxʷ čəd", "page 2, ‘I found it’, which Snyder writes uáytxʷčid"),
        ("§1", SNYDER, "cited form", "ay", "page 2, Snyder's transcription of /ay/ in uáytxʷčid"),
    ])
    + names("§2", [
        ("language", "Northern Lushootseed", "the other dialect of Lushootseed"),
        ("language", "Coast Salish", "the branch Lushootseed belongs to"),
        ("place", "Puget Sound", "the region of the Pacific Northwest where Lushootseed is spoken"),
        ("place", "Skagit Valley", "past which the Lushootseed area extends north"),
        ("place", "Kitsap Peninsula", "whose east parts are in the Lushootseed area"),
        ("place", "Cascades", "whose western parts are in the Lushootseed area"),
    ])
    + display(("§2", "sləx̌i"), [
        ("§2", L, "cited form", "ɬəčil ti stubš", "page 3, ‘the man arrived’"),
        ("§2", L, "cited form", "ɬəči ti stubš", "page 3, ɬəčil ti stubš as realized in the Southern dialect"),
        ("§2", L, "cited affix", "-il", "page 3, the inchoative suffix, whose l the Southern dialect drops"),
    ])
    + display(("§2", "=əlgʷə"), [
        ("§2", L, "cited affix", "-s", "page 3, the 3rd-person possessive"),
        ("§2", L, "cited affix", "-cut", "page 3, the reflexive"),
        ("§2", L, "cited affix", "-bi-", "page 3, the relational applicative"),
    ])
    + names("§3.2", [
        ("place", "Green River", "near which Annie Jack was born in the 1870’s"),
        ("place", "Muckleshoot Tribal Reservation", "where Annie Jack lived her entire life"),
        ("language", "Duwamish", "a variety of Southern Lushootseed Annie Jack spoke"),
        ("language", "Green River", "a variety of Southern Lushootseed Annie Jack spoke"),
        ("language", "White River", "a variety of Southern Lushootseed Annie Jack spoke"),
    ])
    + display(("§4.1", "ʔay̓gʷəs"), [("§4.1", L, "cited form", "hay", "page 5, ‘know’, realized only as [ay]")])
    + display(("§4.1", "sqʷəbay̓"), [("§4.1", L, "cited form", "tay", "page 5, ‘to raid’, realized only as [ay]")])
    + display(("§4.1", "Table 2 List of words containing..."),
              [("Table 2", AUTHORS, "note", text,
                "page 6, the heads of Table 2" if number == 0 else
                "page 6, a row of Table 2: words with /ay/, each with its gloss and count, "
                "the one realized as [ay] first")
               for number, text in enumerate(TABLE_2)])
    + names("§5", [
        ("language", "English", "whose Canadian Raising the paper compares"),
    ])
    + display(("§5", "A similar phenomenon has been observed..."), [
        ("§5", "English", "cited form", "/aɪ/", "page 11, the English diphthong Canadian Raising raises"),
        ("§5", "English", "cited form", "[wɐɪf]", "page 11, English wife, with Canadian Raising before [f]"),
        ("§5", "English", "cited form", "wife", "page 11"),
    ])
    + display(("§5", "wɹɐɪɾə˞"), [
        ("§5", "English", "cited form", "writer", "page 11, footnote 2"),
        ("§5", "English", "cited form", "rider", "page 11, footnote 2, against writer, the /d/ lengthening the diphthong"),
    ])
    + display(("§5", "ʔay̓gʷəs"), [("§5", L, "cited form", "hay", "page 12, ‘know’")])
    + display(("§5", "sqʷəbay̓"), [("§5", L, "cited form", "tay", "page 12, ‘to raid’")])
    + [("all", ("all", AUTHORS, "symbol note", "[ɐ]",
                "a range of pronunciations in the open-mid and mid central range, [ə ɜ ʌ ɐ] of the IPA, footnote 1")),
       ("all", ("all", AUTHORS, "symbol note", "/ / [ ]", "a phoneme between slashes, a pronunciation between brackets")),
       ]
)

WHOSE = (
    "The language is Lushootseed, Coast Salish, and the data are six recordings of the Southern "
    "Lushootseed storyteller Annie Jack made by Leon Metcalf between 1951 and 1954. The paper cites "
    "its words in the prose, from Bates et al. 1994 in Table 1 and from Annie Jack's recordings in "
    "Table 2.\n\n"
    "The who of a cited Lushootseed word is Lushootseed. Snyder's 1957 transcriptions, ʌy for /ay/, "
    "are Snyder's, and writer, rider and wife in footnote 2 and Section 5 are English. The table "
    "rows, the prose, the headings and the notes carry Ted Kye."
)

LETTERS = (
    "The forms are in the Americanist orthography of Bates, Hess and Hilbert 1994: y for the palatal "
    "glide, y̓ for its glottalized form, ə for schwa, x̌ for the uvular fricative, ƛ̕ for the "
    "glottalized lateral affricate, ʷ for labialization. Phonemes stand between slashes and "
    "pronunciations between square brackets, with IPA [ɐ] for a range of mid central vowels."
)

PAGE_NOTES = (
    "The text layer sets a space after a letter with a mark, k̓ ʷaɬ in Table 1, closed from the glyph "
    "positions. Table 2 prints ‘grandmother without its closing quote, and Steller Jay and Steller jay "
    "in two rows; both kept as printed. The prose cites Hess (1977) and Chambers 2006, which the "
    "references do not list; the list holds Hess 1967 and Chambers 1973. The figures, a map, "
    "spectrograms and plots, have no text layer beyond their captions."
)
