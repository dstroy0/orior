# Context for Schneider & Gerdts, Polyptotonic repetition in Hul'q'umi'num' narratives.

AUTHORS = "Lauren Schneider and Donna B. Gerdts"
L = "Hul’q’umi’num’"

TITLE = "Polyptotonic repetition in Hul’q’umi’num’ narratives"
BYLINE = "Lauren Schneider & Donna B. Gerdts, Simon Fraser University"

# The practical orthography over the phonemic line: a row with one of these letters is the
# phonemic line.
TIER_SCRIPT = "əʔʷᶿθłɬščƛ̓̌ː"

MOHAWK = ("Kanien’kéha, in the names of Tehota’kerá:tonh & Owennatékha 2018 and the title of their "
          "book, Onkwawén:na Kentyókhwa")

FORMS = {
    "Gómez": ("name", AUTHORS, "Gómez de García, of Axelrod & Gómez de García 2007"),
    "García": ("name", AUTHORS, "Gómez de García, of Axelrod & Gómez de García 2007"),
    "CV(ʔ)-": ("cited affix", L, "the reduplicant of the durative, CV(ʔ)- reduplication plus a vowel shift and resonant glottalization"),
    "ʔayʔajuθəm": ("language", AUTHORS, "the one Salish language without a nominalizing prefix s-, Kroeber 1999:11"),
    "Tehota’kerá:tonh": ("name", AUTHORS, MOHAWK),
    "Owennatékha": ("name", AUTHORS, MOHAWK),
    "Onkwawén:na": ("cited form", "Kanien’kéha", MOHAWK),
    "Kentyókhwa": ("cited form", "Kanien’kéha", MOHAWK),
    "Β": ("symbol note", AUTHORS, "a Greek capital beta in the reference list where the page prints B"),
    "yə=": ("cited affix", L, "the dynamic clitic, in the title of Schneider & Gerdts 2024"),
    "Hul’q’umi’num’": ("language", AUTHORS, "the Vancouver Island dialect of Halkomelem, the language of the paper"),
    "Sti’tum’at": ("name", AUTHORS, "Sti’tum’at, Dr. Ruby Peter, who told the Basket Ogress story in 2016"),
    "Tth’uwxe’le’ts": ("name", L, "the Basket Ogress of the story of (27) to (30)"),
    "tth’uwxe’le’ts": ("name", L, "the Basket Ogress, in the story's title tth’uwxe’le’ts | Basket Ogress"),
    "Qwul’ilh": ("name", L, "Pitchy log man of (31) and (32)"),
    "Qwul’ilh’s": ("name", L, "Pitchy log man's, of (32)"),
    "qwul’ilh": ("name", L, "in the story's title chumux qwul’ilh | Pitchy log man"),
}

# Words the glossed-form rule offers that are English.
DROP = ("either", "plus", "translated")
# The second line of the translation of (32f) runs into the paragraph below it; it becomes a row
# of (32f) of its own.
REPLACE = (("“Come ashore, Qwul’ilh, the sun is high now.” He came ashore.’ (MJJ 34-36)28 Across",
            "Across"),)

STORY = {
    "WSa": "Wilfred Sampson", "EW": "Ellen White", "MJJ": "Mrs. Jimmy Joe", "SM": "Sophie Misheal",
    "TR": "Theresa Rice", "EM": "Elwood Modeste",
}

ADD = (
    (("§3", "Next, it was the eldest that called out to him in the morning."), ("(32f) line 6", AUTHORS, "translation", "“Come ashore, Qwul’ilh, the sun is high now.” He came ashore.’", "page 23, the second line of the translation of (32f)")),
    ("(32f) line 6", ("(32f) line 6", AUTHORS, "citation", "(MJJ 34-36)28", "page 23, the source of (32f), carrying footnote 28")),
    (None, ("title", AUTHORS, "title", TITLE, "the paper's title, carrying the star of the funding footnote")),
    (None, ("title", AUTHORS, "name", "Lauren Schneider", "author, Simon Fraser University")),
    (None, ("title", AUTHORS, "name", "Donna B. Gerdts", "author, Simon Fraser University, curator of the story collection")),
    (None, ("front", AUTHORS, "language", "Halkomelem", "the Salish language of which Hul’q’umi’num’ is the Vancouver Island dialect")),
    (None, ("§1", AUTHORS, "place", "Vancouver Island", "where Hul’q’umi’num’ is spoken")),
    (None, ("§1", AUTHORS, "place", "Salish Sea", "along which the territory of the Hul’q’umi’num’ people extends")),
    (None, ("§1", AUTHORS, "place", "Nanoose", "one end of the territory")),
    (None, ("§1", AUTHORS, "place", "Malahat", "the other end of the territory")),
    (None, ("§1", AUTHORS, "place", "British Columbia", "")),
    (None, ("§1", "English", "cited example", "With eager feeding food doth choke the feeder.", "(3), an English polyptoton from Shakespeare's Richard II")),
    (None, ("§1", AUTHORS, "name", "Shakespeare", "(3) is from his play Richard II")),
    (None, ("footnote 1", AUTHORS, "name", "Wayne Suttles", "whose recordings the story collection is based on, with Thomas Hukari and Donna B. Gerdts, 1962 to 2000")),
    (None, ("footnote 1", AUTHORS, "name", "Thomas Hukari", "whose recordings the story collection is based on")),
    (None, ("footnote 1", AUTHORS, "name", "Basil Alphonse", "a speaker whose story is featured")),
    (None, ("footnote 1", AUTHORS, "name", "Cecelia Leo Alphonse", "a speaker whose story is featured")),
    (None, ("footnote 1", AUTHORS, "name", "Mrs. Jimmy Joe", "a speaker whose story is featured, MJJ in the citations, Pitchy log man")),
    (None, ("footnote 1", AUTHORS, "name", "Sophie Misheal", "a speaker whose story is featured, SM in the citations, Crane steals the river")),
    (None, ("footnote 1", AUTHORS, "name", "Peter Mitchell", "a speaker whose story is featured")),
    (None, ("footnote 1", AUTHORS, "name", "Elwood Modeste", "a speaker whose story is featured, EM in the citations")),
    (None, ("footnote 1", AUTHORS, "name", "Ruby Peter", "a speaker whose story is featured, the speaker of (19)")),
    (None, ("footnote 1", AUTHORS, "name", "Theresa Rice", "a speaker whose story is featured, TR in the citations")),
    (None, ("footnote 1", AUTHORS, "name", "Wilfred Sampson", "a speaker whose story is featured, WSa in the citations, Elder and the sea lion and The young man that turned into a seal")),
    (None, ("footnote 1", AUTHORS, "name", "Ellen White", "a speaker whose story is featured, EW in the citations, the ethnobiology account of (26)")),
    (None, ("footnote 1", AUTHORS, "name", "Elena Barreiro", "of the research team that edited the stories")),
    (None, ("footnote 1", AUTHORS, "name", "Samara Channell", "of the research team")),
    (None, ("footnote 1", AUTHORS, "name", "Zachary Gilkison", "of the research team")),
    (None, ("footnote 1", AUTHORS, "name", "Sarah Kell", "of the research team")),
    (None, ("footnote 1", AUTHORS, "name", "Kaoru Kiyosawa", "of the research team")),
    (None, ("footnote 1", AUTHORS, "name", "Janet Leonard", "of the research team")),
    (None, ("footnote 1", AUTHORS, "name", "Zoey Peterson", "of the research team")),
    (None, ("footnote 1", AUTHORS, "name", "Helen Zhang", "of the research team")),
    (None, ("footnote 2", AUTHORS, "notation", "< >", "non-concatenative morphology, in the gloss line, walk<IPFV>")),
    (None, ("footnote 4", AUTHORS, "notation", "a.a.a", "vowels extended with periods, rhetorical lengthening in the orthography, glossed RL")),
    (None, ("footnote 4", AUTHORS, "notation", "…", "in the middle of a line, a pause")),
    (None, ("footnote 24", AUTHORS, "name", "Christopher Alphonse", "of the poetry version of Basket Ogress")),
    (None, ("footnote 24", AUTHORS, "name", "Roseanna George", "of the poetry version of Basket Ogress")),
    (None, ("footnote 24", AUTHORS, "name", "Martina Joe", "of the poetry version of Basket Ogress")),
    (None, ("footnote 24", AUTHORS, "name", "Thomas Johnny", "of the poetry version of Basket Ogress")),
    (None, ("footnote 24", AUTHORS, "name", "Donna Modeste", "of the poetry version of Basket Ogress")),
    (None, ("footnote 24", AUTHORS, "name", "A.V. Sharon Seymour", "of the poetry version of Basket Ogress")),
    (None, ("footnote 24", AUTHORS, "name", "Helen Yu Zhang", "of the poetry version of Basket Ogress")),
    (None, ("footnote 24", AUTHORS, "name", "John Lyon", "who taught the 2021 field methods class")),
    (None, ("footnote 26", AUTHORS, "notation", "indentation", "in the oral paragraphs, the intonation: the left edge is the higher starting pitch, and each indent a pitch reset, Alphonse et al. 2021:3")),
    (None, ("§3", AUTHORS, "notation", "¶", "opens each oral paragraph of the poetry versions")),
    (None, ("all", AUTHORS, "notation", "(WSa.GE.357)", "the storyteller and line a sentence comes from: WSa Wilfred Sampson, EW Ellen White, MJJ Mrs. Jimmy Joe, SM Sophie Misheal, TR Theresa Rice, EM Elwood Modeste")),
    (None, ("all", AUTHORS, "notation", "tiers", "the practical orthography, then the phonemic line with morpheme breaks, then the glosses, then the English. Oral paragraphs of more than three lines have no interlinear glosses, footnote 3")),
    (None, ("all", AUTHORS, "notation", "√", "marks a root, √kwun ‘take’")),
)

COME_ASHORE = "“Come ashore, Qwul’ilh, the sun is high now.” He came ashore.’ (MJJ 34-36)28 "

SET = {
    ("(17) line 4", "But everyone was mean to him, did not like him, I guess because of his ugly wrinkles.’ (TR"):
        {"kind": "translation", "who": AUTHORS, "gloss": "page 7, the translation of (17), which the page opens without its ‘, with the start of its source (TR 31)"},
    ("(17) line 5", "31)17"): {"kind": "citation", "who": AUTHORS, "gloss": "page 7, the end of the source (TR 31) on its own line, carrying footnote 17"},
    ("(32b) line 7", "lheel lheel qwul’ilh lheel."): {"kind": "transcription", "who": L, "gloss": "page 22, the third line of the song"},
    ("§3", "Next, it was the eldest that called out to him in the morning."):
        {"where": "(32f) line 5", "kind": "translation", "gloss": "page 23, the first line of the translation of (32f), which the page opens without its ‘"},
}

WHOSE = (
    "The stories are the Elders': Wilfred Sampson, Ellen White, Mrs. Jimmy Joe, Sophie Misheal, "
    "Theresa Rice, Elwood Modeste and Ruby Peter, from recordings by Wayne Suttles, Thomas Hukari and "
    "Donna B. Gerdts between 1962 and 2000, curated by Gerdts. The code after a translation names "
    "the storyteller and line, as the notation row lists them.\n\n"
    "The who for each line of an example and each cell of a table is Hul’q’umi’num’. The English "
    "translations were made by the research team that edited the stories, and carry the authors. "
    "The prose, the notes and the references carry the authors."
)

LETTERS = (
    "The first line of an example is the Hul’q’umi’num’ practical orthography, plain letters with ’ "
    "for glottalization and the glottal stop, and periods inside a word for rhetorical lengthening, "
    "swa.a.aw’lus. The second is the phonemic line, with ə, ʔ, ł, š, č, ƛ̓, x̌, the raised theta "
    "of tᶿ (U+1DBF), ʷ and ̓. √ marks a root."
)

PAGE_NOTES = (
    "The engine reads the practical orthography as English, since it has no letter outside English, "
    "and a line with glottal apostrophes and a tier or translation below it is read as a first line. "
    "The tables are set one cell to a line, and each row is rebuilt under its line label."
)
