# Context for wlwlmelst (Maurice Michell), Maria Adams and Jonathan Janzen, mus te kʷúkʷpiʔs he
# sɬaʔx̣áns - The Four Food Chiefs. The story is told three times: §3.1 in nɬeʔkepmxcín as four
# paragraphs, §3.2 in English as four paragraphs, and §3.3 as (1) to (16), each in groups of four
# tiers with the English after them. The appendix's table of glosses is read off the page one
# morpheme a row.
import os
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import WORK  # noqa: E402

STEM = "MichelletalICSNL60"
AUTHORS = "wlwlmelst (Maurice Michell), Maria Adams and Jonathan Janzen"
L = "nɬeʔkepmxcín"
WLWLMELST = "wlwlmelst (Maurice Michell)"
JANZEN = "Jonathan Janzen"

TITLE = "mus te kʷúkʷpiʔs he sɬaʔx̣áns - The Four Food Chiefs"
BYLINE = "wlwlmelst (Maurice Michell), Maria Adams, Jonathan Janzen, Kanaka Bar Indian Band"

IN_TITLE = "in the title, mus te kʷúkʷpiʔs he sɬaʔx̣áns, ‘The Four Food Chiefs’"

with open(os.path.join(WORK, STEM + ".draft.tsv"), encoding="utf-8") as handle:
    DRAFT = [one.rstrip("\n").split("\t") for one in handle][1:]


def draft_form(where, opening):
    """The form of the draft row at where opening on these words."""
    for one in DRAFT:
        if one[0] == where and one[3].startswith(opening):
            return one[3]
    raise SystemExit("no draft row %s opens with %s" % (where, opening))


def cut(text, first, last=None):
    """The words of text from first up to last, or to its end."""
    start = text.index(first)
    end = text.index(last, start) if last else len(text)
    return text[start:end].strip()


def chained(anchor, rows):
    """Each row after the one before it, the first after anchor."""
    out = []
    for row in rows:
        out.append((anchor, row))
        anchor = (row[0], row[3])
    return out


# §3 says wlwlmelst "transcribed and translated this story personally", and that the interlinear
# analysis is Jonathan Janzen's alone.
WORD_GLOSS = "the English word by word under the gloss; §3 makes the interlinear analysis Jonathan Janzen's"
TRANSLATION = "wlwlmelst's English, the same sentences as §3.2"
WHO_RULES = (
    (r"^\(\d+\) line", "translation", r".", WLWLMELST, TRANSLATION),
    (r"^\(\d+\) line", "word gloss", r".", JANZEN, WORD_GLOSS),
)

SECTIONS = {
    "§3.1": ("running speech", WLWLMELST, "page 2, the story in nɬeʔkepmxcín as wlwlmelst transcribed it, one paragraph"),
    "§3.2": ("translation", WLWLMELST, "page 2, the English of the story as wlwlmelst translated it, one paragraph"),
}

FORMS = {
    "nɬeʔkepmxcín": ("language", AUTHORS, "the language of the paper, Thompson River Salish"),
    "nɬeʔképmx": ("name", AUTHORS, "the people whose language is nɬeʔkepmxcín, in nɬeʔképmx research, narratives and orthographic standard"),
    "nłeʔképmx": ("name", AUTHORS, "the people, printed once in §1 with ł U+0142 in place of ɬ"),
    "utémkt": ("language", AUTHORS, "the Southern utémkt dialect of nɬeʔkepmxcín, wlwlmelst's, spoken in all communities on the Fraser River south of Lytton, from Siska to Spuzzum, §1"),
    "kʷúkʷpiʔs": ("cited form", L, IN_TITLE + ", ‘their chief’, (8) and (9)"),
    "sɬaʔx̣áns": ("cited form", L, IN_TITLE + ", ‘their food’, (8), (9), (13) and (15)"),
    "ɬ=": ("cited form", L, "page 1, the remote proclitic, glossed ‘REM=’, which introduces the first three sections of the story"),
    "ɬ=címeɬ=us": ("cited form", L, "page 1, glossed ‘REM=first.time=3SBJV’, which begins the exposition, (1)"),
    "ɬ=nwén̓=us": ("cited form", L, "page 1, glossed ‘REM=already=3SBJV’, which begins the rising action, (6)"),
    "ɬ=ʔéx=us": ("cited form", L, "page 1, glossed ‘REM=be=3SBJV’, which begins the climax, (8)"),
    "ʔéwiʔ": ("cited form", L, "page 1, ‘because’, which begins the dénouement, (15)"),
    "ʔ": ("cited form", L, "the glottal stop, which the appendix's table starts with, after Thompson & Thompson 1996"),
    "/ə/": ("cited form", L, "the schwa phoneme, the vowel of Cə- reduplication, the appendix"),
    "-tən": ("cited affix", L, "the instrumental, in see also -tən under -m(i)n, the appendix"),
}

# English words, and the half of teʔ(e) that the text layer cut at its bracket.
DROP = ("dénouement", "reaffirmative", "preposition", "teʔ(e")

INITIALS = {}

R102 = draft_form("§3.1", "ɬ exus ne céw̓uʔ")
R107 = draft_form("§3.2", "While they were talking")
R139 = draft_form("§3.3", "then he said will I help them")
R203 = draft_form("(7) line 25", "‘All the animals")
R204 = draft_form("§3.3", "how they could help")

SPLIT = (
    ("front", "wlwlmelst (Maurice Michell), Maria Adams, Jonathan Janzen Kanaka", None, {}),
    ("front", "Abstract:", None, {"gloss": "page 1, the abstract"}),
    ("front", "Keywords:", {}, {"gloss": "page 1"}),
    # The third and fourth paragraphs of the story, which the text layer runs together.
    ("§3.1", "ʔéwiʔ ʔes cuts ɬə kʷúkʷpiʔ", {}, {}),
    ("§3.2", "The four food chiefs – deer", {"gloss": "page 2 and 3, the English of the story as wlwlmelst translated it, one paragraph"},
     {"gloss": "page 3, the English of the story as wlwlmelst translated it, one paragraph"}),
)

# The words of the story's §3.1 paragraphs, which the running speech rows hold, and the morphemes
# of the appendix, which its table rows hold.
KEEP_IN_APPENDIX = ("ʔ", "/ə/", "-tən")
REMOVE = tuple(
    [(where, form) for where, who, kind, form, gloss in DRAFT
     if kind == "cited form" and (where == "§3.1" or (where == "appendix" and form not in KEEP_IN_APPENDIX))] +
    [("§3.3", R139), ("§3.3", R204),
     ("appendix", "Morphemes Gloss T&T Gloss..."), ("appendix", "139) -m PASS IDF..."),
     ("appendix", "INDEP 1SG.SBJ first person..."), ("appendix", "See T&T (1992: 146-147) for examples."),
     ("appendix", "teʔ(e) NEG NEG...")])

SET = {
    ("(7) line 25", R203): {"form": R203 + " " + R204[:-1],
                            "gloss": "page 5, %s, carries footnote 3, written here without its digit" % TRANSLATION},
    ("(14) line 1", "xʷuy̓ cúne tekm he stuytúym̓xʷ ks x̣iyms teʔ ʔeɬ ƛ̓uʔ.”"):
        {"gloss": "page 8, the whole sentence, which the page sets on one line before its groups of tiers"},
    ("(14) line 2", "xʷuy̓ cúne tekm he stuytúym̓xʷ"): {"kind": "transcription"},
    ("(14) line 3", "xʷuy̓ cu -t -∅ -ne tekm he s- tuyt -úym̓xʷ"): {"kind": "segmentation"},
    ("(14) line 7", "ks x̣iyms teʔ ʔeɬ ƛ̓uʔ.”"):
        {"kind": "transcription", "gloss": "page 9, the line above printed a second time"},
}


def morpheme(page, form, gloss, tt_gloss, definition, tt_definition=""):
    """One row of the appendix's table: Morphemes, Gloss, T&T Gloss, Definition, T&T Definition."""
    text = "page %d, the appendix, glossed %s" % (page, gloss)
    if tt_gloss:
        text += ", %s in Thompson & Thompson 1992" % tt_gloss
    text += ": %s" % definition
    if tt_definition:
        text += ". Thompson & Thompson: %s" % tt_definition
    return ("appendix table", L, "cited affix", form, text)


APPENDIX = [
    ("appendix table", AUTHORS, "note", "Morphemes Gloss T&T Gloss Definition T&T Definition",
     "page 11, the column heads of the table of glosses, in alphabetical order from ʔ"),
    ("appendix table", AUTHORS, "notation", "-", "page 11, affix boundary, the same in the T&T gloss"),
    ("appendix table", AUTHORS, "notation", "=", "page 11, clitic boundary; N/A in the T&T gloss, where = is a lexical suffix boundary, lexical suffixes being bound morphemes with root-like meanings"),
    morpheme(11, "ʔe", "COP", "INT", "(equational) copula", "introductory predicative; ‘there is/are, it is that…’ (T&T 1992: 95); ‘and then’ when followed by nominalized predicate or auxiliary (T&T 1992: 180-181)"),
    morpheme(11, "ʔes-", "STAT", "ST", "stative; appears as a prefix on predicates to specify it as a state or condition that is already in effect as opposed to an activity or event"),
    morpheme(11, "-ʔúy", "RFM", "RFM", "reaffirmative", "‘basic, ordinary, plain, simple, real, genuine’ (T&T 1992: 129)"),
    morpheme(11, "Cə-", "IRED", "AFF", "Initial (C1) reduplication", "affective; denotes ‘special attitudes ranging from familiarity, perhaps with overtones of nostalgia, to extreme specialization.’ (T&T 1992: 115); formed by reduplicating the first consonant plus /ə/ vowel. The tiers of (1) gloss it RED"),
    morpheme(11, "CV(C)-", "AUG", "AUG", "augmentative; plural meaning, a repeated or persistent action, an intensification of activity, state or size, or a specialized meaning (see T&T 1992: 82-93); formed by reduplicating the first consonant-vowel and sometimes the final consonant, or the first consonant-consonant-vowel"),
    morpheme(11, "(h/ʔ)e=", "DET", "DIR", "determiner; determiner-complementizer; distribution is dependent on sentence structure", "direct complement marker; introduces complements that specify predicates. It can also have possessive meaning when it follows a third person pronominal"),
    morpheme(11, "-e", "IMP", "IMP", "imperative; grammatical mood that signals a command or request"),
    morpheme(11, "ʔeɬ", "ADD", "ACCM", "additive; and, or", "‘and, also, too, along with’; accomplished (T&T 1992: 139)"),
    morpheme(11, "-ey", "1PL.OBJ", "1PL.OBJ", "first person plural object; ‘us’"),
    morpheme(12, "(w)ʔex; xe", "IPFV", "ALT", "imperfective", "auxiliary particle; ‘exist, be located, reside, stay: persistent, progressive, actual’ (T&T 1992: 142)"),
    morpheme(12, "-it", "FMV", "", "general formative", "No specific meaning (T&T 1992: 125)"),
    morpheme(12, "-i(y)x", "AUT", "AUT", "autonomous", "‘refers to acts controlled by a specific agent’ (T&T 1992: 101)"),
    morpheme(12, "k=; =k", "DET", "UNR", "determiner", "unrealized; a complement marker referring to states that are as of yet unrealized, meaning they can be unknown or ‘established in the future, if at all’ (T&T 1992: 150). The tiers gloss it D/C"),
    morpheme(12, "k̓ém̓eɬ", "CTST", "", "contrastive", "‘but, on the other hand, although, though, even if’ (T&T 1992: 179)"),
    morpheme(12, "-kt", "1PL.POSS", "1PL.POSS", "first person plural possessive; ‘our’"),
    morpheme(12, "=kt", "1PL.INTR", "1PL.INTR", "first person plural intransitive; ‘we’. The tiers of (7) gloss it 1PL.SBJ"),
    morpheme(12, "ɬ", "REM", "EP", "remote determiner", "established in the past and/or established far away"),
    morpheme(12, "-ɬ-", "CONN", "LIG", "compounding connective", "ligature; a connective that joins stems to create compound words"),
    morpheme(12, "ƛ̓uʔ", "EXCL", "PER", "exclusive; but", "persistent; ‘only, just, until, up to’ (T&T 1992: 139)"),
    morpheme(12, "-m", "PASS", "IDF", "passive", "indefinite subject; i.e. ‘someone, everyone, etc.’"),
    morpheme(12, "-m", "CTR.MID", "MDL", "control middle", "middle; refer to actions or states where the subject is also the agent meaning they are acting with volition"),
    morpheme(12, "-m(i)n", "INS", "INS", "instrumental", "creates words used as instruments or implements (see also -tən), can also specify certain materials, ‘means of carrying out activities and processes’ (T&T 1992: 121)"),
    morpheme(12, "n-", "LOC", "LCL", "locative", "localizer; signals location of an action, can have abstract meaning"),
    morpheme(12, "-n", "CTR", "DRV", "control (directive)", "directive; -e DRV to see"),
    morpheme(12, "-(e)n(e)", "1SG.SBJ", "1SG.SBJ", "first person singular subject; ‘I’"),
    morpheme(12, "ncéweʔ", "1SG.SBJ.INDEP", "1SG.SBJ", "first person singular independent; ‘I’. The tiers gloss it 1SG.INDEP"),
    morpheme(12, "nmimɬ", "1PL.INDEP", "", "independent first person plural pronoun"),
    morpheme(12, "n=", "AT", "AT", "locative preposition", "‘at, to, in, into, with’, denotes a precise location or direction (T&T 1992:155)"),
    morpheme(12, "s-; s=; =s", "NMLZ", "NOM", "nominalizer", "creates a noun"),
    morpheme(12, "-(e)s", "3.SBJ", "3.SBJ", "third person subject; ‘he, she, it, they’"),
    morpheme(12, "=s", "3.POSS", "3.POSS", "third person possessive; ‘his, hers, its, theirs’"),
    morpheme(13, "-s(e)m", "1SG.OBJ", "1SG.OBJ", "first person singular object; ‘me’"),
    morpheme(13, "-t", "TR", "TR", "transitivizer", "marks predicates as transitive, meaning they take both object and subject arguments"),
    morpheme(13, "téʔe", "DEM", "EM.PART", "demonstrative", "emphatic particular; ‘about to be set forth’ (T&T 1992: 136)"),
    morpheme(13, "t(ə)=", "OBL", "OBL", "oblique particle", "introduces complements of predicates. These are usually objects but can also refer to means or instruments, specify what lexical suffixes are referring to, or definite and indefinite subject. See T&T (1992: 146-147) for examples"),
    morpheme(13, "teʔ(e)", "NEG", "NEG", "negative; ‘no, not’"),
    morpheme(13, "=uʔ", "OOC", "", "out of control", "It often indicates that an event occurs or a state of affairs develops spontaneously, without human intervention. Subjects mentioned are in patient relationship. (T&T 1992: 99)"),
    morpheme(13, "=us", "3.SBJV", "3.CJV", "third person subjunctive", "third person conjunctive; ‘he, she, it, they’"),
    morpheme(13, "xéʔe", "DEM", "", "distal demonstrative"),
    morpheme(13, "xʷuy̓", "PROSP", "FUT", "prospective", "future notion"),
    morpheme(13, "=⦰", "3.OBJ", "", "third person object; ‘him, her, them’, with ⦰ U+29B0 where the tiers write ∅"),
]

EXAMPLE_3 = [
    ("(3) line 4", JANZEN, "word gloss", cut(R139, "then he said", " he seytknmx"), "page 3, " + WORD_GLOSS),
    ("(3) line 5", L, "transcription", cut(R139, "he seytknmx.”", " he seytkn -mx"), "page 4, after the page break"),
    ("(3) line 6", L, "segmentation", cut(R139, "he seytkn -mx", " DET"), "page 4"),
    ("(3) line 7", L, "gloss", cut(R139, "DET Indigenous", " the people"), "page 4"),
    ("(3) line 8", JANZEN, "word gloss", cut(R139, "the people", " ‘Then"), "page 4, " + WORD_GLOSS),
    ("(3) line 9", WLWLMELST, "translation", cut(R139, "‘Then"), "page 4, " + TRANSLATION),
]

ADD = tuple(
    [(None, ("title", AUTHORS, "title", TITLE, "the paper's title, the story's name in nɬeʔkepmxcín and in English, carrying the star of footnote *")),
     (None, ("title", AUTHORS, "name", "wlwlmelst", "author, Maurice Michell, Kanaka Bar Indian Band, one of the few remaining speakers of the Southern utémkt dialect, who transcribed and translated the story and was recorded reading it")),
     (None, ("title", AUTHORS, "name", "Maurice Michell", "wlwlmelst's English name")),
     (None, ("title", AUTHORS, "name", "Maria Adams", "author, Kanaka Bar Indian Band")),
     (None, ("title", AUTHORS, "name", "Jonathan Janzen", "author, Kanaka Bar Indian Band, responsible for the interlinear analysis and the audio recording, §3 and footnote 1")),
     (None, ("title", AUTHORS, "place", "Kanaka Bar Indian Band", "the authors' affiliation")),
     (None, ("title", L, "cited form", "mus", IN_TITLE + ", ‘four’")),
     (None, ("front", AUTHORS, "language", "Thompson Language", "the English name of nɬeʔkepmxcín, in the keywords")),
     (None, ("footnote *", AUTHORS, "name", "FPCC", "funded the language work in part, named only by its initials")),
     (None, ("§1", AUTHORS, "place", "Fraser River", "along which the Southern utémkt dialect is spoken")),
     (None, ("§1", AUTHORS, "place", "Lytton", "the Southern utémkt dialect is spoken south of it")),
     (None, ("§1", AUTHORS, "place", "Siska", "one end of the Southern utémkt communities")),
     (None, ("§1", AUTHORS, "place", "Spuzzum", "the other end of the Southern utémkt communities")),
     (None, ("§3.1", AUTHORS, "language", "Thompson River Salish", "the English name of nɬeʔkepmxcín, in the heading of §3.1")),
     (None, ("§3.2", L, "cited form", "semecín", "‘English’, in the heading 3.2 semecín - English")),
     (None, ("all", AUTHORS, "notation", "tiers", "each example of §3.3 sets its sentence as groups of four tiers: the nɬeʔkepmxcín, its segmentation, the morpheme glosses, and English word by word, with wlwlmelst's English in quotes after the last group. (13) has no line of English word by word")),
     (None, ("all", AUTHORS, "notation", "∅", "the tiers write the null morpheme ∅ U+2205, the appendix ⦰ U+29B0")),
     ]
    + chained("(3) line 3", EXAMPLE_3)
    + chained(("appendix", "The following table defines..."), APPENDIX)
)

REPLACE = ()

WHOSE = (
    "wlwlmelst (Maurice Michell) wrote the story out from memory in nɬeʔkepmxcín, transcribed and "
    "translated it himself, and was recorded reading it. Jonathan Janzen made the interlinear analysis "
    "and edited the recording.\n\n"
    "The who for the story of §3.1 and the English of §3.2 is wlwlmelst. The who for each tier of an "
    "example in §3.3 is nɬeʔkepmxcín, for the English word by word under the gloss, Jonathan Janzen, "
    "and for the translation after the tiers, wlwlmelst, whose English of §3.2 it repeats. The prose, "
    "the footnotes, the references and the appendix carry the three authors."
)

LETTERS = (
    "The orthography follows the Thompson River Salish Dictionary, Thompson & Thompson 1996. It writes "
    "ɬ, ƛ̓, ʔ, ʕ, ə, ɣ, ʷ, x̣ with U+0323 COMBINING DOT BELOW, and U+0313 COMBINING COMMA ABOVE on "
    "glottalized letters, n̓, w̓, y̓, k̓, q̓. Stress is an acute. The tiers write ∅ for a null morpheme and "
    "the appendix ⦰."
)

PAGE_NOTES = (
    "The text layer sets a space after a glottalized letter and before the dot below, ƛ̓ uʔ for ƛ̓uʔ and "
    "x ̣iyms for x̣iyms; the page text is closed up from the glyph positions. Example (3) runs over a "
    "page break under footnote 2, and (7)'s translation over a line after which footnote 3's mark "
    "stands. (14) opens on its whole sentence set on one line and prints its second transcription "
    "line twice, and (13) has no line of English word by word; each is kept as printed."
)
