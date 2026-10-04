# Context for Grace Baleno, Jonathan Janzen and Brendon Yoder, Acoustic Correlates of Word Stress in
# Haisla. The Haisla words are the examples of the four stress rules, (1) to (4), each a word and
# its gloss, and the rules' suffixes are cited after "from". The rest is measurement: the tables and
# figure captions the text layer ran into the prose are set apart here, one row a table or caption.
STEM = "ICSNL59_Baleno_Janzen_Yoder_final"
AUTHORS = "Grace Baleno, Jonathan Janzen and Brendon Yoder"
HAISLA = "Haisla"
KWAK = "Kwak’wala"

TITLE = "Acoustic Correlates of Word Stress in Haisla"
BYLINE = ("Grace Baleno, Canada Institute of Linguistics; Jonathan Janzen, Nicola Valley Institute of Technology; "
          "and Brendon Yoder, Canada Institute of Linguistics and SIL International")
VOLUME = "59"

FORMS = {
    KWAK: ("language", AUTHORS, "Kwak’wala, North Wakashan, whose stress system Janzen (2015) analyzes"),
    "Kwak̓wala": ("language", AUTHORS, "Kwak’wala, spelled with k̓ in the title of Grubb 1974"),
    "Wùik̓ala": ("language", AUTHORS, "’Wùik̓ala, Oowekyala, North Wakashan, in footnote 5"),
    "ɛ": ("notation", AUTHORS, "page 2, [ɛ], the phonetic value of /ai/"),
    "ɔ": ("notation", AUTHORS, "page 2, [ɔ], the phonetic value of /au/"),
    "əja": ("notation", AUTHORS, "page 2, in [əja ~ əy̓a], the sequence /ia/ as two syllables"),
    "əy̓a": ("notation", AUTHORS, "page 2, in [əja ~ əy̓a], the sequence /ia/ as two syllables"),
    "əwa": ("notation", AUTHORS, "page 2, in [əwa ~ əw̓a], the sequence /ua/ as two syllables"),
    "/ɢˈaɬdəma/": ("cited form", HAISLA, "page 3, ‘pajamas’, a recording used for two data points"),
    "/ə/": ("notation", AUTHORS, "schwa, in the title of Grubb 1974"),
    "Wałda̱mas": ("cited form", KWAK, "in the title of Janzen 2015, Wałda̱mas? An exploration into the phonological-word in Kwak’wala"),
}
# The engine's candidates with a footnote digit or a bracket run on.
DROP = ("əw̓a].2", "Wùik̓ala.5")

SPLIT = [
    ("front", "Grace Baleno Jonathan Janzen", {"where": "title", "kind": "title", "gloss": "page 1, the title"}, {}),
    ("front", "Abstract:", {"where": "title", "gloss": "page 1, the authors and their affiliations, footnote 1 on Yoder's"},
     {"gloss": "page 1, the abstract"}),
    ("§2.2", "Target vowel Predictable stress", {"where": "Table 1", "gloss": "page 4, the caption"}, {}),
    ("§2.2", "For each word used", {"where": "Table 1", "gloss": "page 4, the cells in reading order: the heads, then stressed and unstressed /a/, /i/ and /u/ with their token counts for predictable and unpredictable stress"},
     {"gloss": "page 4"}),
    ("§3.1", "If intensity is a correlate", {"where": "Figure 1", "gloss": "page 5, the caption"}, {}),
    ("§3.2", "Figure 2: Pitch differences", {}, {}),
    ("§3.2", "If pitch is an acoustic", {"where": "Figure 2", "gloss": "page 5, the caption"}, {}),
    ("§3.3", "The duration of vowels varies", {"where": "Figure 3", "gloss": "page 6, the caption"}, {}),
    ("§3.4", "Figure 4: The F2", {}, {}),
    ("§3.4", "The measurements shown in Figure 4", {"where": "Figure 4", "gloss": "page 7, the caption"}, {}),
    ("appendix", "Table 3: Statistical summary", {"where": "Table 2"}, {"where": "Table 3"}),
    ("Table 2", "Correlate Stressed Unstressed", {"gloss": "page 11, the caption"},
     {"gloss": "page 11, the cells in reading order: the heads, then the mean and standard deviation of each correlate in stressed and unstressed vowels with its p value"}),
    ("Table 3", "Correlate Stressed Unstressed", {"gloss": "page 11, the caption"},
     {"gloss": "page 11, the cells in reading order: the heads, then the formant rows as the page labels them, F2 of /i/, /u/ and /a/ twice and F3 of each, each with its mean, standard deviation and p value"}),
]

ADD = (
    (("title", "Grace Baleno Jonathan Janzen..."), ("title", AUTHORS, "name", "Brendon Yoder", "author, Canada Institute of Linguistics and SIL International")),
    (("title", "Grace Baleno Jonathan Janzen..."), ("title", AUTHORS, "name", "Jonathan Janzen", "author, Nicola Valley Institute of Technology")),
    (("title", "Grace Baleno Jonathan Janzen..."), ("title", AUTHORS, "name", "Grace Baleno", "author, Canada Institute of Linguistics")),
    (("§1", "əwa"), ("§1", AUTHORS, "notation", "əw̓a", "page 2, in [əwa ~ əw̓a], the sequence /ua/ as two syllables")),
    (("§4.1", KWAK), ("§4.1", AUTHORS, "language", "’Wùik̓ala", "’Wùik̓ala, Oowekyala, whose vowel duration the authors take to mark stress as in Kwak’wala")),
    (("footnote 5", "5 More research on word stress in Oowikela..."), ("footnote 5", AUTHORS, "language", "Oowikela", "the English name of ’Wùik̓ala")),
    (("footnote 5", "5 More research on word stress in Oowikela..."), ("footnote 5", AUTHORS, "name", "John Rath", "a personal communication on ’Wùik̓ala")),
    (("footnote 5", "5 More research on word stress in Oowikela..."), ("footnote 5", AUTHORS, "name", "David Stevenson", "a personal communication on ’Wùik̓ala")),
    (None, ("all", AUTHORS, "notation", "a stressed syllable in bold with an acute accent", "the examples of (1) to (4), whose stressed syllables the page sets in bold")),
    (None, ("all", AUTHORS, "notation", "/…/", "a phoneme")),
    (None, ("all", AUTHORS, "notation", "[…]", "a phonetic value")),
)

SET = {
    ("(1) line 2", "from -ás"): {"form": "-ás", "kind": "cited affix", "gloss": "page 1, the lexically stressed suffix of p̓eɫdaudilás, after “from”"},
    ("(1) line 3", "from -áyu"): {"form": "-áyu", "kind": "cited affix", "gloss": "page 1, the lexically stressed suffix of qelqezuwáyu, after “from”"},
    ("(1) line 4", "from -má"): {"form": "-má", "kind": "cited affix", "gloss": "page 1, the lexically stressed suffix of λikumá, after “from”"},
    ("§4.2", "Wùik̓ala"): {"form": "’Wùik̓ala"},
    ("(5) line 1", "/uu/, /ii/, /aa/, /ai/, /au/"): {"kind": "notation", "gloss": "page 2, the long vowels of Haisla, exhaustively"},
}

WHOSE = (
    "The Haisla words are the authors' own, from recordings made in 2020 in Kitimat with at least six "
    "speakers of the Haisla Nation, and each word of (1) to (4) is given to Haisla with its gloss. The "
    "long vowels, the phonetic values and the phonemes are the authors' notation. Kwak’wala and "
    "’Wùik̓ala are named as the sister languages, and Wałda̱mas stands in the title of a reference."
)

LETTERS = (
    "The examples write the Haisla practical orthography, with p̓, c̓, ɫ, x̄, ḡ and xʷ; λ is the lateral "
    "affricate, as the text layer codes it, and ƛ̓ its glottalized counterpart. Long vowels are written "
    "double, and the stressed syllable carries an acute accent and is set in bold. The phonemes are in "
    "slashes and the phonetic values in square brackets."
)

PAGE_NOTES = (
    "The text layer ran the tables and the figure captions into the prose; each is a row of its own "
    "here, the numeric cells kept in reading order. It set a space inside ’Wùik̓ala on page 8, which "
    "the glyph positions and the page set whole."
)
