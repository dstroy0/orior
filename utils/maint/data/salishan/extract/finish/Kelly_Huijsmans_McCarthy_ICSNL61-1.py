# Context for Kelly, Huijsmans and McCarthy, read off the 18 pages.

AUTHORS = "Rachel Kelly, Marianne Huijsmans and Mary McCarthy"
L = "ʔayʔaǰuθəm"

# Cited form candidates that are not forms of the language: kind, who, gloss.
FORMS = {
    "qaʔaχstalɛs": ("name", AUTHORS, "Dr. Elsie Paul, Tla’amin Elder, whose determiner system Reisinger, Huijsmans and Matthewson 2021 describe"),
    "K’ómoks": ("place", AUTHORS, "one of the four nations; the island dialect, with no mother tongue speakers remaining"),
    "č̓ɛč̓ɛhatanapɛšt": ("cited form", L, "the thanks that closes footnote 1 and §1.1, each time with an exclamation mark"),
    "Sḵwx̱wú7mesh": ("language", AUTHORS, "in footnote 9, after Gillon 2006. The reference list writes Skwxwú7mesh with no line below"),
    "Skwxwú7mesh": ("language", AUTHORS, "in the title of Gillon 2006 in the reference list, with no line below the k and x"),
    "derzeitgenössischen": ("note", AUTHORS, "a German word in the title of the Heim 1991 entry, set as one word where German writes derzeitigen"),
    "ʔayʔajuθəm": ("language", AUTHORS, "in the title of Huijsmans et al. 2018 as the reference list gives it, with j where the paper has ǰ"),
    "xweysás": ("note", AUTHORS, "a St’át’imcets word in the title of the 2018 volume in honour of Henry Davis"),
    "nqwal’utteníha": ("note", AUTHORS, "a St’át’imcets word in the title of the 2018 volume in honour of Henry Davis"),
    "ucwalmícwa": ("note", AUTHORS, "a St’át’imcets word in the title of the 2018 volume in honour of Henry Davis"),
    "ʔayʔaǰuθəm": ("language", AUTHORS, "the language of the paper, a Central Salish language of the Tla’amin, Homalco, Klahoose and K’ómoks First Nations"),
    "tə/šɛ": ("cited form", L, "the Previous cell of Table 4, tə or šɛ"),
    "/q̓ʷalas/": ("cited form", L, "raccoon, the phonemic form in footnote 6, between slashes"),
    "/qʷalus/": ("cited form", L, "how Molly Harry heard the researcher’s suggestion, phonemic, in footnote 6"),
    "qʷalos": ("cited form", L, "the misheard raccoon of (15), in footnote 6"),
    "q̓ʷaləs": ("cited form", L, "raccoon"),
    "tɛqɛw": ("cited form", L, "horse, which Molly Harry put in place of goat, footnote 5"),
    "kʷaʔəmnač": ("cited form", L, "roots, the target noun of Table 2. (14) writes kʷaʔəmnəč in the orthography and kʷaʔamnač in the segmentation"),
}

# Tla’amin has its own row in §1.1. replaced is English, before ‘goat’ in footnote 5.
DROP = ("Tla’amin", "replaced")

# The (vt) translations are Molly Harry's, §3.3.1 and §3.3.2.
VT_WHO = {"(7)": "Molly Harry", "(8)": "Molly Harry", "(11)": "Molly Harry", "(12)": "Molly Harry"}

TITLE = "Variation in the ʔayʔaǰuθəm determiner system"
BYLINE = "Rachel Kelly, Marianne Huijsmans and Mary McCarthy, University of Alberta"

WHOSE = (
    "The language is ʔayʔaǰuθəm, spoken by the Tla’amin, Homalco, Klahoose and K’ómoks First "
    "Nations. The speaker of the main task, and of (5) to (15), is Molly Harry, a mother-tongue "
    "speaker from Homalco, and the translations marked (vt) are her own English. (16) to (19) are "
    "the late Freddie Louie's, from Tla’amin: (16) is cited to Louie et al. 2025 and (17) to (19) "
    "carry his tag FL with a date. (i) carries the tag EP.2024/03/08. Doreen Point, the late Marion "
    "Harry, Ochele (Betty Wilson) and qaʔaχstalɛs (Dr. Elsie Paul) are thanked in footnote 1, and "
    "the paper closes that footnote and §1.1 with č̓ɛč̓ɛhatanapɛšt. (1) to (3) are cited to "
    "Reisinger et al. 2021 and (4) to Huijsmans & Reisinger 2025.\n\n"
    "The who for each tier of an example is ʔayʔaǰuθəm. The who for a translation is the work "
    "cited on its line, Molly Harry for a (vt) translation, and the three authors otherwise. The "
    "prose, the tables, the contexts and the notes carry the three authors."
)

LETTERS = (
    "The gloss labels are Unicode small capitals in the text layer, as ᴄᴅᴇ.ᴅᴇᴛ, and several person "
    "digits are U+1D7E3 and U+1D7E5, MATHEMATICAL SANS-SERIF DIGIT ONE and THREE, as 𝟣ꜱɢ. The page "
    "draws them as small capitals and plain digits, and the table keeps the text layer's code "
    "points. The orthography tier writes ᶿ, U+1DBF, for the raised theta of tᶿ, and the NAPA tier "
    "writes x̣ with U+0323 where the orthography has χ. Glottalization is U+0313 throughout."
)

PAGE_NOTES = (
    "The text layer breaks ʔayʔaǰuθəm after ʔayʔa on pages 1, 3, 14 and 16, and Sḵwx̱wú7mesh after "
    "its S in footnote 9, and the page prints both whole. Those two are page-read corrections in "
    "the residue check. (14) prints a raised θ after χaƛ̓ that the text layer also holds, and (7) "
    "sets its second segmentation above its orthography, both read at 500 dpi and recorded as "
    "notations."
)

ADD = (
    (None, ("title", AUTHORS, "title", "Variation in the ʔayʔaǰuθəm determiner system", "the paper's title, set in bold, carrying the star of footnote 1")),
    (None, ("title", AUTHORS, "name", "Rachel Kelly", "author, University of Alberta")),
    (None, ("title", AUTHORS, "name", "Marianne Huijsmans", "author, University of Alberta")),
    (None, ("title", AUTHORS, "name", "Mary McCarthy", "author, University of Alberta. Footnote 1 is marked on this name")),
    (None, ("footnote 1", AUTHORS, "name", "Molly Harry", "mother-tongue speaker from Homalco, the speaker of the main task and of (5) to (15). MH in Table 3. She volunteered the translations marked (vt)")),
    (None, ("footnote 1", AUTHORS, "name", "Freddie Louie", "late, from Tla’amin, the speaker of (16) to (19). FL in the tags of (17) to (19)")),
    (None, ("footnote 1", AUTHORS, "name", "Doreen Point", "from Homalco, DP in §3.3.5")),
    (None, ("footnote 1", AUTHORS, "name", "Marion Harry", "late. The paper does not give her examples")),
    (None, ("footnote 1", AUTHORS, "name", "Ochele (Betty Wilson)", "one of the speakers Reisinger et al. 2021 drew on")),
    (None, ("footnote 1", AUTHORS, "name", "Daniel Reisinger", "thanked in footnotes 1 and 8, for discussion and for the examples from Boas")),
    (None, ("§1.1", AUTHORS, "place", "Tla’amin", "one of the four nations, and a dialect")),
    (None, ("§1.1", AUTHORS, "place", "Homalco", "one of the four nations, and a dialect")),
    (None, ("§1.1", AUTHORS, "place", "Klahoose", "one of the four nations, and a dialect")),
    (None, ("§1.1", AUTHORS, "place", "Georgia Strait", "the traditional territories span its northern end, in British Columbia")),
    (None, ("§4", AUTHORS, "name", "Mary George", "Mrs. Mary George, whose personal narrative Watanabe 2025 gives, with 10 šɛ and 21 tə")),
    (None, ("footnote 9", AUTHORS, "language", "Sechelt", "in footnote 9, after Gillon 2006 and Beaumont 1985")),
    (None, ("all", AUTHORS, "notation", "(i)", "the example in footnote 3 carries the tag (vf | EP.2024/03/08); EP is not expanded, and the speakers named are Elsie Paul among them")),
    (None, ("all", AUTHORS, "notation", "(7)", "the page sets the segmentation tᶿ=niy-əxʷ above the orthography tᶿ niyʊxʷ on the second tier, the other way round from every other example. Read at 500 dpi")),
    (None, ("all", AUTHORS, "notation", "(13) (18)", "the translations close with U+0313 COMBINING COMMA ABOVE where the others close with ’")),
    (None, ("all", AUTHORS, "notation", "(14)", "the orthography line prints χaƛ̓ᶿ with a raised θ after the ƛ̓, where the segmentation has x̣aƛ̓ and (13) and (4) print χaƛ̓. Read at 500 dpi")),
    (None, ("all", AUTHORS, "notation", "(18)", "the gloss of səqɛtstɛxʷoɬšt ends =1ꜱɢ.ꜱʙᴊ where the clitic =št and the translation we are first person plural")),
    (None, ("all", AUTHORS, "notation", "(19)", "the tag (vf | FL.2024/06/06 has no closing parenthesis. The segmentation sets tᶿ= χaƛ̓ with a space")),
    (None, ("all", AUTHORS, "notation", "footnote 7", "cites χaχaƛ̓ɛt ‘being difficult’ in (19), and (19) does not hold it")),
    (None, ("all", AUTHORS, "notation", "footnote 2", "the gloss ‘control transitivizer has no closing quote")),
    (None, ("all", AUTHORS, "notation", "§4", "cites Huijsmans, Reisinger, & Matthewson 2020:173, and the reference list gives that chapter as pp. 65–182")),
    (None, ("all", AUTHORS, "notation", "§1.1", "cites First Nations Peoples’ Cultural Council 2018 and First Nations Peoples Cultural Council 2022, and the reference list has First Peoples’ Cultural Council")),
    (None, ("all", AUTHORS, "notation", "§4", "the prose names Boas’s work in the 1890s, and the reference list dates Boas 1890")),
    (None, ("all", AUTHORS, "notation", "§4", "cites Watanabe 2025, Heim 1991 and Bade 2021, and footnote 9 Gillon 2006 and Beaumont 1985; §1 cites Kroeber 1991 and Watanabe 2003. All are listed")),
    (None, ("all", AUTHORS, "notation", "(2)", "the gloss sets 1ꜱɢ.ᴘᴏꜱꜱ= outside with a space after the clitic boundary, as (4) sets 1ꜱɢ.ᴘᴏꜱꜱ= buy and (8) ᴄᴀᴜꜱ= 1ꜱɢ.ꜱʙᴊ")),
    (None, ("all", AUTHORS, "damage", "Since the carries more information that a", "§4 prints that where than is meant")),
    (None, ("all", AUTHORS, "symbol note", "small capitals", "the gloss labels are Unicode small capitals in the text layer, ᴄᴅᴇ.ᴅᴇᴛ, and some person digits are MATHEMATICAL SANS-SERIF DIGIT, 𝟣ꜱɢ. The table keeps both")),
    (None, ("all", AUTHORS, "symbol note", "ʔayʔaǰuθəm", "the text layer breaks the name after ʔayʔa on pages 1, 3, 14 and 16, and the page prints it whole")),
)

# Rows to change after the rest: (where, form) -> fields.
SET = {}

REPLACE = ()
