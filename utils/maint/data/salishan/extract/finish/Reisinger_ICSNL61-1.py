# Context for Reisinger, 14 Pieces of Spontaneous Discourse in ʔayʔaǰuθəm.

AUTHORS = "D. K. E. Reisinger"
L = "ʔayʔaǰuθəm"

TITLE = ("14 Pieces of Spontaneous Discourse in ʔayʔaǰuθəm: Descriptions of Famous Artworks by "
         "Betty Wilson")
BYLINE = "D. K. E. Reisinger, University of British Columbia"

DAVIS = "in the title of Dominick et al. 2026, payɛčxʷʊt yɛχat θ qʷaytən, ‘Always Remember Your Language’"
LOUIE = ("in the title of Louie et al. 2025, niniǰe ʔəkʷ χʷɛƛ̓ay, ƛ̓aɬəm hega ɬəlkælɛ, niniǰe "
         "q̓ʷaq̓ʷθəms təsqanaməs, ‘Mountain goats, salt, and bullets’")
HENRY = ("St’át’imcets, in the title of the 2018 volume for Henry Davis, Wa7 xweysás i "
         "nqwal’utteníha i ucwalmícwa: He Loves the People’s Languages")

FORMS = {
    "ʔayʔaǰuθəm": ("language", AUTHORS, "the language of the paper, a.k.a. Comox-Sliammon, ISO 639-3 coo, Coast Salish"),
    "St’át’imcets": ("language", AUTHORS, "in the title of Davis, Mellesmoen & Huijsmans 2020"),
    "č̓ɛč̓ɛhaθɛč": ("cited form", L, "the thanks that closes the acknowledgement, footnote *"),
    "Honoré": ("name", AUTHORS, "Honoré Watanabe, who prepared Tales of Tla’amin Elders with Marion Harry"),
    "Qayχ": ("name", AUTHORS, "Mink, in the title Stories about Qayχ, told by Elsie Paul"),
    "Bartolomé": ("name", AUTHORS, "Bartolomé Esteban Murillo, painter of Children Eating a Tart"),
    "/naʔa/": ("cited form", L, "the rhetorical filler, like umm or uh, glossed FILLER"),
    "/ʔə/": ("cited form", L, "the oblique marker, often reduced or elided in natural speech"),
    "payɛčxʷʊt": ("cited form", L, DAVIS),
    "yɛχat": ("cited form", L, DAVIS),
    "θ": ("cited form", L, DAVIS),
    "qʷaytən": ("cited form", L, DAVIS),
    "niniǰe": ("cited form", L, LOUIE),
    "ʔəkʷ": ("cited form", L, LOUIE),
    "χʷɛƛ̓ay": ("cited form", L, LOUIE),
    "ƛ̓aɬəm": ("cited form", L, LOUIE),
    "ɬəlkælɛ": ("cited form", L, LOUIE),
    "q̓ʷaq̓ʷθəms": ("cited form", L, LOUIE),
    "təsqanaməs": ("cited form", L, LOUIE),
    "xweysás": ("cited form", "St’át’imcets", HENRY),
    "nqwal’utteníha": ("cited form", "St’át’imcets", HENRY),
    "ucwalmícwa": ("cited form", "St’át’imcets", HENRY),
}

# English names of the symbols in §2.3, each before the symbol in quotes, and Tla'amin, which has
# its own row in §1.
DROP = ("hyphen", "tilde", "backslash", "Tla’amin")

ADD = (
    (None, ("title", AUTHORS, "title", TITLE, "the paper's title, set on two lines, carrying the star of the acknowledgement footnote")),
    (None, ("title", AUTHORS, "name", "D. K. E. Reisinger", "author, University of British Columbia")),
    (None, ("title", AUTHORS, "name", "Betty Wilson", "the speaker of every text, of the Tla’amin dialect, recorded by Marianne Huijsmans on July 24, 2023")),
    (None, ("§1", AUTHORS, "language", "Comox-Sliammon", "another name for ʔayʔaǰuθəm")),
    (None, ("§1", AUTHORS, "language", "Tla’amin", "the dialect Betty Wilson speaks")),
    (None, ("§1", AUTHORS, "place", "Strait of Georgia", "where the language is spoken")),
    (None, ("§1", AUTHORS, "name", "Marianne Huijsmans", "who designed the experiment with the author and made the recording")),
    (None, ("§1", AUTHORS, "name", "Mary George", "the narrator of Tales of Tla’amin Elders, and of Watanabe's texts")),
    (None, ("§1", AUTHORS, "name", "Marion Harry", "who prepared Tales of Tla’amin Elders with Honoré Watanabe")),
    (None, ("§1", AUTHORS, "name", "Elsie Paul", "the teller of Stories about Qayχ, and an older speaker in footnote 8")),
    (None, ("§2.1", L, "cited form", "tam k̓ʷʊnɛtʊxʷ ʔə tə namos?", "the audio prompt of each filler item, ‘What do you see in the picture?’")),
    (None, ("§2.2", AUTHORS, "name", "Sandro Botticelli", "painter of Primavera, The Birth of Venus and Calumny of Apelles, texts 3.1 to 3.3")),
    (None, ("§2.2", AUTHORS, "name", "Caspar David Friedrich", "painter of the works of texts 3.5 to 3.7")),
    (None, ("§2.2", AUTHORS, "name", "Carl Blechen", "painter of Building the Devil’s Bridge, text 3.8")),
    (None, ("§2.2", AUTHORS, "name", "Iwan Iwanowitsch Schischkin", "painter of Morning in a Pine Forest, text 3.9")),
    (None, ("§2.2", AUTHORS, "name", "Vincent van Gogh", "painter of Sorrowing Old Man, text 3.10")),
    (None, ("§2.2", AUTHORS, "name", "Laura Muntz Lyall", "painter of Interesting Story, text 3.11")),
    (None, ("§2.2", AUTHORS, "name", "Max Liebermann", "painter of Two Riders on the Beach, text 3.12")),
    (None, ("§2.2", AUTHORS, "name", "John William Waterhouse", "painter of The Soul of the Rose and Miranda, texts 3.13 and 3.14")),
    (None, ("all", AUTHORS, "notation", "where", "each text of §3 numbers its sentences from (1), and so does §4. The where of an example carries its section, §3.2 (5) line 2")),
    (None, ("all", AUTHORS, "notation", "four lines", "orthography, then a phonemic line with morpheme breaks, then the glosses, then the English, §2.3")),
    (None, ("all", AUTHORS, "notation", "[ ]", "in the phonemic and gloss lines, elided material the author restores, [ʔə]tə=mahiy. Elided determiners are not restored")),
    (None, ("all", AUTHORS, "notation", "–", "an en dash after a false start in the orthography line, glossed FS")),
    (None, ("all", AUTHORS, "notation", "*laughs*", "Betty laughing, set between sentences")),
)

SET = {
    (None, "*laughs*"): {"gloss": "Betty Wilson laughs here, set between sentences of the text"},
    (None, "I’ve forgotten how to use the word for ‘shell’. Is it"): {"kind": "speaker comment", "who": "Betty Wilson", "gloss": "Betty's own English, between (5) and (6)"},
    (None, "Is it"): {"kind": "speaker comment", "who": "Betty Wilson", "gloss": "Betty's own English, between (6) and (7)"},
    (None, "I think it’s the elder bear there, it’s not– *laughs* I’m trying to remember…"): {"kind": "speaker comment", "who": "Betty Wilson", "gloss": "Betty's own English, between (6) and (7)"},
}
REPLACE = ()

WHOSE = (
    "Every text is Betty Wilson's, a speaker of the Tla’amin dialect, recorded by Marianne Huijsmans "
    "on July 24, 2023 while Betty described fourteen paintings. The author transcribed, glossed and "
    "translated the texts and checked the hard parts with Betty in follow-up elicitation.\n\n"
    "The who for each tier of an example is ʔayʔaǰuθəm. The who for a translation is the author, "
    "who wrote them, and the source cited on its line where one is, as for (2B) of §4.1. The English "
    "Betty spoke between sentences is a speaker comment with Betty Wilson as who. The prose, the "
    "notes and the references carry the author."
)

LETTERS = (
    "The orthography line uses the ʔayʔaǰuθəm community orthography with ɛ, ɩ and ʊ, and the "
    "phonemic line writes ə, x̌ and ƛ̓. ᶿ, U+1DBF, is the raised theta of t̓ᶿ."
)

PAGE_NOTES = (
    "The text layer sets a space inside təsqanaməs in the title of Louie et al. 2025, read off the "
    "glyph positions, and in /x̌ʷit/ on page 32. These are the page-read corrections. Betty's English "
    "between sentences and her laughter sit between the numbered sentences, and the check reads them "
    "as notes of the section."
)
