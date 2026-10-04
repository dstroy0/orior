# Context for Phillips, Hall, Matthewson and Reid, e sqʷincút kt (Our Speech).

AUTHORS = "Bev Phillips, Brent Hall, Lisa Matthewson and Danica Reid"
L = "nɬeʔkepmxcín"

TITLE = "e sqʷincút kt (Our Speech)"
BYLINE = ("Bev Phillips, Lytton First Nation, Brent Hall and Lisa Matthewson, University of British "
          "Columbia, and Danica Reid, Simon Fraser University")

# Every translation line is Bev's own English, 1.2: "The translation line provides the English
# translation as volunteered by Bev."
TRANSLATION_WHO = "Bev Phillips"

SECTIONS = {
    "§4": ("translation", "Bev Phillips", "the literal English translation of the story, §1, set "
           "as paragraphs. Its sentences are the translation lines of §5"),
}

T25 = "in the title of Hall & Phillips 2025, ɬ cutés us ɬ qəɬmín ɬ tmíxʷ, ‘When Old One Created the Earth’"
T24 = "in the title of Hall & Phillips 2024, xʷíʔ kʷ páq, ‘You Will Be Sorry’"

FORMS = {
    "nɬeʔkepmxcín": ("language", AUTHORS, "the language of the paper, a.k.a. Thompson River Salish, Northern Interior Salish"),
    "Nɬeʔkepmxcín": ("language", AUTHORS, "capitalized, in the title of Hannon, Stacey & Steiner 2023"),
    "nłeʔkepmxcín": ("language", AUTHORS, "in the title of Thompson & Thompson 1996, written with ł, U+0142, where the paper has ɬ"),
    "sqʷincút": ("cited form", L, "in the title, e sqʷincút kt, ‘Our Speech’"),
    "spílex̣m": ("cited form", L, "‘story’, the kind of text Bev tells, given after the word story in the abstract and §1"),
    "ƛ̓q̓əmcín": ("language", AUTHORS, "the Lytton dialect of nɬeʔkepmxcín, Bev's own"),
    "scw̕exmxcín": ("language", AUTHORS, "the Nicola Valley dialect of nɬeʔkepmxcín"),
    "nɬab": ("name", AUTHORS, "the short name of UBC's nɬeʔkepmxcín Lab"),
    "nɬeʔekpmxcín": ("damage", AUTHORS, "the paper's misprint of nɬeʔkepmxcín in §2, with e and k swapped"),
    "séytknmx": ("cited form", L, "‘Indigenous people’, which Bev uses for people in general in (2) and (20), footnote 7"),
    "m̓": ("cited affix", L, "[m̓], the glottalized form of the unstressed relational applicative, as Thompson & Thompson 1996:438 write it, footnote 8"),
    "x̣ʷóx̣ʷstm̓s": ("cited form", L, "the dictionary's form, Thompson & Thompson 1996:438, with m̓. The paper hears a plain m and writes x̣ʷóx̣ʷstms in (7)"),
    "k̓ə̣pqns": ("cited form", L, "the word of (45), written with the retracted schwa, footnote 10"),
    "/k̓ép/": ("root", L, "the root of k̓ə̣pqns that Thompson & Thompson 1996:102 propose, footnote 10"),
    "/ə̣/": ("note", AUTHORS, "the retracted schwa, after Thompson & Thompson 1992:21, footnote 10"),
    "kʷaɬtèzetkʷʔ": ("name", AUTHORS, "Bernice Garcia's name as the author list of Garcia, Hannon & Stacey 2024 gives it"),
    "Kʷəɬtəzétkʷu": ("name", AUTHORS, "Bernice Garcia's name as the title of Garcia, Hannon & Stacey 2024 gives it"),
    "ɬ": ("cited form", L, T25),
    "cutés": ("cited form", L, T25),
    "qəɬmín": ("cited form", L, T25),
    "tmíxʷ": ("cited form", L, T25),
    "xʷíʔ": ("cited form", L, T24),
    "kʷ": ("cited form", L, T24),
    "páq": ("cited form", L, T24),
}

DROP = ()

ADD = (
    (None, ("title", AUTHORS, "title", TITLE, "the paper's title, e sqʷincút kt, ‘our speech’, with the English in parentheses, carrying the star of the acknowledgement footnote")),
    (None, ("title", L, "cited form", "kt", "1PL.POSS, ‘our’, in the title e sqʷincút kt")),
    (None, ("title", AUTHORS, "name", "Bev Phillips", "author, Lytton First Nation, a first language speaker of nɬeʔkepmxcín from Lytton, who wrote and told the story")),
    (None, ("title", AUTHORS, "name", "Brent Hall", "author, University of British Columbia")),
    (None, ("title", AUTHORS, "name", "Lisa Matthewson", "author, University of British Columbia")),
    (None, ("title", AUTHORS, "name", "Danica Reid", "author, Simon Fraser University")),
    (None, ("title", AUTHORS, "place", "Lytton First Nation", "Bev Phillips's affiliation")),
    (None, ("footnote *", AUTHORS, "name", "Ella Hannon", "thanked among the members of UBC's nɬeʔkepmxcín lab")),
    (None, ("footnote *", AUTHORS, "name", "Reed Steiner", "thanked among the members of UBC's nɬeʔkepmxcín lab")),
    (None, ("§1.1", AUTHORS, "language", "Thompson River Salish", "another name for nɬeʔkepmxcín, ISO 639-3 thp")),
    (None, ("§1.1", AUTHORS, "place", "Fraser", "one of the four rivers the language is spoken along")),
    (None, ("§1.1", AUTHORS, "place", "Thompson", "one of the four rivers the language is spoken along")),
    (None, ("§1.1", AUTHORS, "place", "Nicola", "one of the four rivers the language is spoken along")),
    (None, ("§1.1", AUTHORS, "place", "Coldwater", "one of the four rivers the language is spoken along")),
    (None, ("§1.1", AUTHORS, "place", "Lytton", "where Bev Phillips is from. ƛ̓q̓əmcín is the Lytton dialect, and Bev wrote the story in the orthography used in Lytton")),
    (None, ("§1.1", AUTHORS, "place", "British Columbia", "where the language is spoken")),
    (None, ("all", AUTHORS, "notation", "[mm:ss]", "the timestamp in the audio file where a sentence begins. §3 sets one after each sentence of the story, and §5 one above each example. The where of a story line is §3 with its timestamp")),
    (None, ("all", AUTHORS, "notation", "[ ]", "in a segmentation, a sound of the underlying form that is not pronounced, qʷincút-m[in]-[t]-Ø-[e]ne, as the Appendix explains")),
    (None, ("all", AUTHORS, "notation", "§3 and §5", "the story is printed twice in nɬeʔkepmxcín: §3 as sentences with no punctuation, one to a line, and §5 as the four-line gloss. The two can differ, sk̓épqns at [06:01] and sk̓ə̣́pqns in (45)")),
)

SET = {}
REPLACE = ()

WHOSE = (
    "The story is Bev Phillips's. She wrote it in the Lytton orthography and told it to UBC's "
    "nɬeʔkepmxcín Lab over Zoom on October 23, 2025, and the other three authors transcribed the "
    "recording in the NAPA orthography of Thompson & Thompson and glossed each sentence. Bev "
    "marked where each sentence begins and ends and volunteered a translation for each one.\n\n"
    "The who for the story lines of §3 and for each tier of an example in §5 is nɬeʔkepmxcín. The "
    "who for a translation, and for the English of §4, is Bev Phillips, because she volunteered "
    "every translation. The prose, the footnotes, the references and the Appendix carry the "
    "four authors. The people the story describes are the lab's members, and the paper does not "
    "name them, footnote 4."
)

LETTERS = (
    "The orthography is the NAPA one of Thompson & Thompson 1992 and 1996. The segmentation line "
    "writes Ø for a null morpheme and puts unpronounced sounds in square brackets. Thompson & "
    "Thompson 1996 write ł in the dictionary's title where the paper writes ɬ, and scw̕exmxcín "
    "carries U+0315 COMBINING COMMA ABOVE RIGHT on its w."
)

PAGE_NOTES = (
    "The text layer sets a space inside some words: nɬeʔkepmxc ín in §1.1, n ɬeʔkepmxcín in §2 "
    "and in Koch 2011, and N ɬeʔkepmxcín in Garcia, Hannon & Stacey 2024. These are the "
    "page-read corrections. It also sets the dot below of the retracted schwa after a space, k̓ə ̣pqns, "
    "and the check puts every combining mark that follows a space back on its letter. Footnotes 5 "
    "and 6 fall between the segmentation and the gloss of (2), and footnote 4 inside the first "
    "paragraph of §4. Both are notes, and (2) and the paragraph go on over the page break."
)
