# Context for Mellesmoen and Trotter, The Long and Short of Subject Clitics in ʔayʔaǰuθəm.

AUTHORS = "Gloria Mellesmoen and Bailey Trotter"
L = "ʔayʔaǰuθəm"

TITLE = "The Long and Short of Subject Clitics in ʔayʔaǰuθəm"
BYLINE = "Gloria Mellesmoen, University of Victoria, and Bailey Trotter, University of British Columbia"

FORMS = {
    "Honoré": ("name", AUTHORS, "Honoré Watanabe, author of Watanabe 2003, the source of many examples"),
    "Mascaró": ("name", AUTHORS, "Joan Mascaró, cited for allomorph selection"),
    "K’ómoks": ("place", AUTHORS, "one of the four communities that traditionally speak ʔayʔaǰuθəm"),
    "Nxaʔamxčín": ("language", AUTHORS, "Moses-Columbian, whose clitics can precede or follow their host, footnote 3"),
    "St'át'imcets": ("language", AUTHORS, "with the cognate subject marker =(ɬ)kan, after Van Eijk 1997. The apostrophes are straight"),
    "=(ɬ)kan": ("cited affix", "St'át'imcets", "the 1SG subject marker, cognate with =čan, after Van Eijk 1997"),
    "=čən": ("cited affix", "Sechelt", "the 1SG subject marker, cognate with =čan, after Beaumont 1985"),
    "Qayχ": ("name", AUTHORS, "in the title of Paul in press, Stories about Qayχ"),
    "qayχ": ("cited form", L, "in the title of Paul in press, χʷaχʷaǰɩm niniǰɛ qayχ"),
    "CVµCµ": ("note", AUTHORS, "a syllable template with its moras"),
    "CəC": ("note", AUTHORS, "a root template"),
    "CəCC": ("note", AUTHORS, "a root template"),
    "CəCµCµ": ("note", AUTHORS, "a root template with its moras"),
    "σµµµ": ("note", AUTHORS, "a trimoraic syllable"),
    "Tla’amin": ("name", AUTHORS, "one of the four communities that traditionally speak ʔayʔaǰuθəm, with Homalco, Klahoose and K’ómoks"),
}

DROP = ()

ADD = (
    (None, ("title", AUTHORS, "title", TITLE, "the paper's title, set in bold, carrying the star of the acknowledgement footnote")),
    (None, ("title", AUTHORS, "name", "Gloria Mellesmoen", "author, University of Victoria")),
    (None, ("title", AUTHORS, "name", "Bailey Trotter", "author, University of British Columbia")),
    (None, ("footnote *", AUTHORS, "name", "Elsie Paul", "thanked first among the speakers. EP in the tags of (18) and (25), and the teller of Paul in press, the source of (14)")),
    (None, ("footnote *", AUTHORS, "name", "Freddie Louie", "the late Freddie Louie, thanked in footnote *. FL in the tags of (18) and (23)")),
    (None, ("footnote *", AUTHORS, "name", "Marianne Huijsmans", "thanked with the Salish Working Group, and author of Huijsmans 2023, the source of most examples")),
    (None, ("footnote *", AUTHORS, "name", "Henry Davis", "thanked for encouraging the questions this paper asks")),
    (None, ("§1", AUTHORS, "language", "ʔayʔaǰuθəm", "a.k.a. Comox-Sliammon, Central Salish, spoken by the Tla’amin, Homalco, Klahoose and K’ómoks communities")),
    (None, ("§1", AUTHORS, "language", "Comox-Sliammon", "another name for ʔayʔaǰuθəm")),
    (None, ("§1", AUTHORS, "place", "Tla’amin", "one of the four communities")),
    (None, ("§1", AUTHORS, "place", "Homalco", "one of the four communities")),
    (None, ("§1", AUTHORS, "place", "Klahoose", "one of the four communities")),
    (None, ("§5", AUTHORS, "language", "Sechelt", "with the cognate subject marker =čən, after Beaumont 1985")),
    (None, ("all", AUTHORS, "notation", "(10) to (12), (17), (19) to (22), (31), (32)", "numbered displays of rules, derivations and trees. Their lines are rule rows")),
    (None, ("all", AUTHORS, "symbol note", "text layer", "the text layer runs the words of 106 lines together, betweenallomorphsissensitiveto:, and drops the column gaps of the examples. Each line is read from whichever of the text layer and the glyph positions keeps more of the page's spaces")),
)

SET = {
    (None, "‘Oh, I’ll beat him, I’ll beat him.’"): {"who": "Paul, in press"},
}
REPLACE = ()

WHOSE = (
    "The language is ʔayʔaǰuθəm. Most examples are cited from Huijsmans 2023 and Watanabe 2003, "
    "and (14) from Elsie Paul's stories, Paul in press. (18), (23) and (25) are volunteered forms "
    "tagged vf with EP, FL or both: Elsie Paul and the late Freddie Louie, the two speakers footnote "
    "* thanks by name.\n\n"
    "The who for each tier of an example is ʔayʔaǰuθəm. The who for a translation is the work cited "
    "on its line, and the two authors where the line carries a speaker tag. The rules, trees and "
    "derivations, the prose and the notes carry the two authors. =čən is Sechelt and =(ɬ)kan "
    "St'át'imcets."
)

LETTERS = (
    "The orthography tier writes ᶿ, U+1DBF, for the raised theta of tᶿ, and ɩ, U+0269, and ʊ, "
    "U+028A, for lax vowels. The rules write µ, U+00B5 MICRO SIGN, for the mora, σ for the "
    "syllable and ω for the prosodic word, and ⇔ between a feature bundle and its exponent."
)

PAGE_NOTES = (
    "The text layer of this paper runs the words of whole lines together and keeps the column gaps "
    "of the examples, and the glyph positions, read with pypdfium2, keep the spaces of the prose and "
    "lose the gaps of the examples. The source here takes each line from whichever of the two holds "
    "more spaces. 106 lines came from the glyph positions. Page 17 prints In press. χʷaχʷaǰɩm with a "
    "space both extractions lose, read at 400 dpi and entered as a page-read correction."
)
