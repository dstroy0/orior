# Context for Pincott, Philology of Secwepemctsín: The Phonology of Studies on Shuswap.

AUTHORS = "Ethan Pincott"
L = "Secwepemctsín"

TITLE = "Philology of Secwepemctsín: The Phonology of Studies on Shuswap"
BYLINE = "Ethan Pincott, Simon Fraser University"

# The language labels of the comparative tables, after Kuipers 2002, §4.
WHO = {
    "Sh": "Secwepemctsín",
    "Th": "nɬeʔkepmxcín",
    "Li": "St’át’imcets",
    "Ok": "nsyilxcən",
    "Cb": "Moses-Columbian",
    "Ka": "Kalispel",
    "Sq": "Sḵwx̱wú7mesh",
    "PS": "Proto-Salish",
}

FORMS = {
    "Secwepemctsín": ("language", AUTHORS, "the language of the paper, a.k.a. Shuswap, Interior Salish. Sh"),
    "Secwépemc": ("name", AUTHORS, "the people whose language is Secwepemctsín"),
    "nɬeʔkepmxcín": ("language", AUTHORS, "Thompson, the closest relative of Secwepemctsín. Th in the tables"),
    "nɬeʔképmx": ("name", AUTHORS, "the Thompson people, Michel's and those of the Nicola Valley reserves"),
    "St’át’imcets": ("language", AUTHORS, "Lillooet. Li"),
    "nsyilxcən": ("language", AUTHORS, "Okanagan. Ok"),
    "nsəlxcin": ("language", AUTHORS, "in the name of the nsəlxcin dictionary, Relational Lexicography 2023"),
    "ʔayʔaǰuθəm": ("language", AUTHORS, "the language of Reisinger and Griffin 2022"),
    "ʔayʔajuθəm": ("language", AUTHORS, "one of Le Jeune's eight languages, written with j in §2.2"),
    "Halq’eméylem": ("language", AUTHORS, "one of Le Jeune's eight languages"),
    "Sḵwx̱wú7mesh": ("language", AUTHORS, "Squamish, one of Le Jeune's eight languages. Sq"),
    "sháshíshalh-em": ("language", AUTHORS, "Sechelt, one of Le Jeune's eight languages"),
    "Secwepemctsín-specific": ("note", AUTHORS, "an English compound on the language name, footnote 8"),
    "Secwepemctsín-speaking": ("note", AUTHORS, "an English compound on the language name, §5"),
    "Wumecwílc": ("name", AUTHORS, "the Wumecwílc re Secwepemctsín group, whose Elders footnote * thanks"),
    "skukwstsétselp": ("cited form", L, "in Xyum re skukwstsétselp!, the thanks that closes footnote *"),
    "St̓uxwtéws": ("place", AUTHORS, "Bonaparte, a Secwépemc reserve Le Jeune visited"),
    "Jean-Marie-Raphaël": ("name", AUTHORS, "Jean-Marie-Raphaël Le Jeune, the missionary who wrote Studies on Shuswap, 1925"),
    "Émile": ("name", AUTHORS, "Émile Duployé, whose French shorthand the Chinook Shorthand was developed from"),
    "Duployé": ("name", AUTHORS, "Émile Duployé, whose French shorthand the Chinook Shorthand was developed from"),
    "Tqeltkúkwpi7": ("cited form", L, "/tqəltkúkʷpiʔ/ ‘Creator, God’, written {TK} in the Shorthand"),
    "/tqəltkúkʷpiʔ/": ("cited form", L, "Tqeltkúkwpi7 ‘Creator, God’"),
    "⟨sèben⟩": ("cited form", L, "Le Jeune's spelling of sépen ‘brother-in-law’, the only use of ⟨b⟩, footnote 3"),
    "sépen": ("cited form", L, "‘brother-in-law’, footnote 3"),
    "/sépən/": ("cited form", L, "sépen ‘brother-in-law’, footnote 3"),
    "C₁əC₂": ("note", AUTHORS, "Kuipers's notation for total reduplication, one form for obstruent and resonant C₂"),
    "/kʷəC/": ("note", AUTHORS, "a pattern, C any consonant, one reading of Le Jeune's ⟨kwC⟩"),
    "/kʷuC/": ("note", AUTHORS, "a pattern, C any consonant, one reading of Le Jeune's ⟨kwC⟩"),
    "/sk̓ék̓ʔit/": ("cited form", "nɬeʔkepmxcín", "‘spider’, the cognate of skék̓i7, footnote 23"),
    "/péɬuskʷu/": ("cited form", "nɬeʔkepmxcín", "‘lake’, footnote 17"),
    "/ptínusəm/": ("cited form", "nɬeʔkepmxcín", "‘thought’, footnote 18"),
    "-axʷ": ("cited affix", "Proto-Salish", "*-axʷ ‘2SG.SUB’, after Newman 1979:216, footnote 6"),
    "nəw-": ("cited affix", "Proto-Salish", "*nəw-, the reconstruction of the locative prefix the author favors, footnote 7"),
    "nəxʷ-": ("cited affix", "Proto-Salish", "*nəxʷ-, a step from *nəw- to Secwepemctsín x-, footnote 7"),
    "xʷ-": ("cited affix", "Proto-Salish", "*xʷ-, a step from *nəw- to Secwepemctsín x-, footnote 7"),
    "yukʷaʔ": ("cited form", "Proto-Interior Salish", "*yukʷaʔ ‘wart’, Kuipers 2002:199, footnote 24"),
    "l̩": ("cited form", L, "[l̩], the article le as the southern Western dialect says it"),
}

DROP = ()

ADD = (
    (None, ("title", AUTHORS, "title", TITLE, "the paper's title, carrying the star of the acknowledgement footnote")),
    (None, ("title", AUTHORS, "name", "Ethan Pincott", "author, Simon Fraser University")),
    (None, ("footnote *", L, "cited form", "Xyum re skukwstsétselp!", "the thanks that closes the acknowledgement")),
    (None, ("footnote *", AUTHORS, "name", "David Robertson", "thanked for help reading the Chinook Shorthand, and author of Robertson 2025, whose romanization the paper uses")),
    (None, ("§2.1", AUTHORS, "language", "Shuswap", "the English name of Secwepemctsín")),
    (None, ("§2.1", AUTHORS, "language", "Thompson", "the English name of nɬeʔkepmxcín")),
    (None, ("§2.1", AUTHORS, "language", "Lillooet", "the English name of St’át’imcets")),
    (None, ("§2.1", AUTHORS, "place", "Fraser River", "the western border of the language")),
    (None, ("§2.1", AUTHORS, "place", "Pavillion", "on the western border")),
    (None, ("§2.1", AUTHORS, "place", "Soda Creek", "on the western border")),
    (None, ("§2.1", AUTHORS, "place", "Cariboo Plateau", "on the northern border")),
    (None, ("§2.1", AUTHORS, "place", "Rocky Mountains", "the eastern border")),
    (None, ("§2.1", AUTHORS, "place", "Jasper", "where the northern border meets the Rocky Mountains")),
    (None, ("§2.1", AUTHORS, "place", "Invermere", "the southern end of the eastern border")),
    (None, ("§2.1", AUTHORS, "place", "Splatsin", "within the southern border")),
    (None, ("§2.1", AUTHORS, "place", "Shuswap Lakes", "within the southern border")),
    (None, ("§2.1", AUTHORS, "place", "Kamloops", "on the southern border, near the line between the Western and Eastern dialects, and Le Jeune's home from 1882")),
    (None, ("§2.1", AUTHORS, "place", "Cache Creek", "on the southern border")),
    (None, ("§2.1", AUTHORS, "place", "Chase", "near the line between the Western and Eastern dialects")),
    (None, ("§2.1", AUTHORS, "place", "Simpcw", "a Western community whose speech differs from the rest, and Chu Chua in §2.2")),
    (None, ("§2.2", AUTHORS, "name", "Durieu", "Bishop Durieu, who gave Le Jeune a manual of the Chinook Jargon")),
    (None, ("§2.2", AUTHORS, "name", "Michel", "an nɬeʔképmx convert at Yale who taught Le Jeune the language")),
    (None, ("§2.2", AUTHORS, "name", "Mary Ta-hwi-nak", "an elder at Spuzzum who taught Le Jeune words and prayers")),
    (None, ("§2.2", AUTHORS, "language", "Chinook Jargon", "the trade language Le Jeune learned first and wrote the Kamloops Wawa in")),
    (None, ("§2.2", AUTHORS, "language", "nsyilxcn", "one of Le Jeune's eight languages, written without ə in §2.2")),
    (None, ("§2.2", AUTHORS, "place", "Skeetchestn", "Deadman's Creek, a Secwépemc reserve Le Jeune visited")),
    (None, ("§2.2", AUTHORS, "place", "Nicola Valley", "where the nɬeʔképmx reserves Le Jeune visited are")),
    (None, ("§4", AUTHORS, "language", "Moses-Columbian", "Cb")),
    (None, ("§4", AUTHORS, "language", "Kalispel", "Ka")),
    (None, ("all", AUTHORS, "notation", "⟨ ⟩", "Le Jeune's Latin orthography, as Studies on Shuswap prints it, §4")),
    (None, ("all", AUTHORS, "notation", "{ }", "Le Jeune's Chinook Shorthand, romanized after Robertson 2025, §3.2.2. The Shorthand itself is not reproduced")),
    (None, ("all", AUTHORS, "notation", "[* ]", "the pronunciation the author reconstructs from Le Jeune's spellings, where it differs from modern Secwepemctsín")),
    (None, ("all", AUTHORS, "notation", "/ /", "phonemic transcription, of Secwepemctsín unless a label or a column names another language. A form after * inside the slashes is a reconstruction")),
    (None, ("all", AUTHORS, "notation", "[ ]", "phonetic transcription")),
    (None, ("all", AUTHORS, "notation", "italics", "modern Secwepemctsín in the community orthography. The text layer keeps no italics, and those forms are the transcription rows with no brackets")),
    (None, ("all", AUTHORS, "notation", "(1) to (19), Table 1, Table 2", "comparative tables. Each line is split into one row per cell, and the who of a cell is the language of its column or of the label before it")),
)

SET = {
    ("(13) line 3", "ɣ : x"): {"kind": "note", "who": AUTHORS, "gloss": "the correspondence, Secwepemctsín ɣ where the other languages have x"},
    ("(13) line 6", "l : ɬ"): {"kind": "note", "who": AUTHORS, "gloss": "the correspondence, Secwepemctsín l where the other languages have ɬ"},
}
REPLACE = ()

WHOSE = (
    "The language is Secwepemctsín. The oldest forms are Le Jeune's, from Studies on Shuswap, 1925: "
    "his Latin spelling in ⟨ ⟩ and his Chinook Shorthand, romanized in { }. The author reconstructs "
    "their pronunciation in [* ] and gives the modern word in the community orthography with its "
    "phonemic and phonetic forms, from Kuipers 1974 and 1989.\n\n"
    "Every one of those is a Secwepemctsín row, whatever its notation. The cognate columns and labels "
    "of (1), (4), (8) and (13) give nɬeʔkepmxcín, St’át’imcets, nsyilxcən, Moses-Columbian, "
    "Kalispel, Sḵwx̱wú7mesh and Proto-Salish forms, from the sources §4 names, and each carries its "
    "language as who. The glosses, the reconstructions of sound laws, the prose and the notes carry "
    "the author."
)

LETTERS = (
    "The community orthography writes 7 for the glottal stop, ll for ɬ and c for x, as Table 1 "
    "sets out, and the phonemic forms use ƛ̓ and x̌. Le Jeune's own spellings are ASCII with grave "
    "accents. C₁ and C₂ carry U+2081 and U+2082 SUBSCRIPT ONE and TWO, and l̩ U+0329 COMBINING "
    "VERTICAL LINE BELOW."
)

PAGE_NOTES = (
    "The text layer sets a space before a letter from the IPA font, Secwepemcts ín for Secwepemctsín "
    "nineteen times, St’ át’imcets, n ɬeʔkepmxcín, nsyilxc ən, / əy, əw/ and /xt éwméɬxʷ/, and a "
    "space inside the closing bracket of ⟨s, j, sh, z ⟩. The page prints each whole, and each is a "
    "page-read correction. The check also puts back the space the inserted-space repair closes "
    "before an opening slash, t̓ /t̓/, where an even count of slashes before it on the line shows "
    "the slash opens a form."
)
