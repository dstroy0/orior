# Context for wlwlmelst (Maurice Michell) and Jonathan Janzen, ɬe meʔmʔéw̓s te nk̓y̓ep eɬ x̣aʔx̣ʔéyqʷ
# nqəmmín - Mr. and Mrs. Coyote and their Magic Kettle.

AUTHORS = "wlwlmelst (Maurice Michell) and Jonathan Janzen"
L = "nɬeʔkepmxcín"
WLWLMELST = "wlwlmelst (Maurice Michell)"
JANZEN = "Jonathan Janzen"
STEWART = "Stewart 1941"

TITLE = "ɬe meʔmʔéw̓s te nk̓y̓ep eɬ x̣aʔx̣ʔéyqʷ nqəmmín - Mr. and Mrs. Coyote and their Magic Kettle"
BYLINE = "wlwlmelst (Maurice Michell) and Jonathan Janzen, Kanaka Bar Indian Band"

IN_TITLE = "in the title, ɬe meʔmʔéw̓s te nk̓y̓ep eɬ x̣aʔx̣ʔéyqʷ nqəmmín, ‘Mr. and Mrs. Coyote and their Magic Kettle’"

# The translation of each example is the English of Stewart 1941, "again with the original
# translation in English below it" (§3). The line of English under each gloss is Janzen's, who
# is responsible for the interlinear analysis.
WHO_RULES = (
    (r"^\(\d+\) line", "translation", r".", STEWART,
     "the English of Stewart 1941, Meet Mr. Coyote, which §3 sets under the gloss"),
    (r"^\(\d+\) line", "word gloss", r".", JANZEN,
     "the English word by word under the gloss; §3 makes the interlinear analysis Jonathan Janzen's"),
)

SECTIONS = {
    "§3.2": ("translation", STEWART, "page 3, the English of the story as Stewart 1941 gives it, one paragraph"),
}

FORMS = {
    "nɬeʔkepmxcín": ("language", AUTHORS, "the language of the paper, Thompson River Salish"),
    "nɬeʔkepmx": ("cited form", L, "in nɬeʔkepmx research, the abstract; the people's name, as nɬeʔképmx elsewhere"),
    "nɬeʔképmx": ("name", AUTHORS, "the people whose language is nɬeʔkepmxcín, in nɬeʔképmx stories, story-telling, language and orthographic standard"),
    "ƛ̓q̓əmcín": ("language", AUTHORS, "the Lytton, B.C. area where the legends originate, §1"),
    "utémkt": ("language", AUTHORS, "the Southern utémkt dialect of nɬeʔkepmxcín, wlwlmelst's, spoken in all communities on the Fraser River south of Skuppah, from Siska to Spuzzum, §1"),
    "ɬe": ("cited form", L, IN_TITLE),
    "meʔmʔéw̓s": ("cited form", L, IN_TITLE + ", ‘Mr. and Mrs.’, the spouses"),
    "eɬ": ("cited form", L, IN_TITLE + ", ‘and’"),
    "x̣aʔx̣ʔéyqʷ": ("cited form", L, IN_TITLE + ", ‘magic’"),
    "nqəmmín": ("cited form", L, IN_TITLE + ", ‘kettle’"),
    "nk̓y̓ep": ("cited form", L, "‘coyote’, " + IN_TITLE + ". It is also Ernie Michell's name, §1"),
    "ʔeyɬ": ("cited form", L, "‘now’, with which c̓- renders ‘right now’ or ‘today’, the appendix"),
    "nexcín": ("cited form", L, "in Dear Human (2), nexcín e ʔestekʷ ‘give you shelter’, the appendix on (h/ʔ)e="),
    "ʔestekʷ": ("cited form", L, "‘give you shelter’ with nexcín e, from Dear Human (2), the appendix on (h/ʔ)e="),
    "-tən": ("cited affix", L, "the instrumental, in see also -tən under -m(i)n, the appendix"),
    "ʔ": ("cited form", L, "the glottal stop, which the appendix's table starts with, after Thompson & Thompson 1996"),
}

DROP = ("dénouement", "emphasizes", "nɬab”3")

STORY = (
    ("ɬe meʔmʔéw̓s te nk̓y̓ep eɬ x̣aʔx̣ʔéyqʷ nqəmmín", "the story's title"),
    ("ɬ nwén̓us ʔex xeʔ he spzuʔ seytknmx ne tmixʷ, ƛ̓uʔ newm wə xeʔ ntiʔmetáqs, newm xeʔ "
     "nxʷəntíyxs he meʔmʔéw̓s te nk̓y̓ep tes c̓e xeʔ he skiʔéw̓ɬs. cənkʷúst newm xeʔ yəx̣yíx̣, ƛ̓uʔe xin̓ te "
     "k̓ʷínex te szenxʷ ʔeyɬ wʔázixus he seytknmx ne tmixʷ, c̓e wiʔ xeʔ he spzuʔ seytknmx he "
     "cəcun̓ékstms ks cuwéɬxʷs te q̓az̓mín tuʔ wə xəmn̓úsn̓s te snew̓t eɬ stekɬ. then̓ he xʷúy̓us p̓emsm "
     "eks qʷʔécixs eɬ cutíyxs he cumíns túwe sxen̓x.", "paragraph 1, (2) to (7)"),
    ("ʔiɬ qʷcíyxʷus tuɬ sƛ̓iqt tmixʷ ɬ meʔmʔéw̓s te nk̓y̓ep, xeʔɬkʷúkʷpiʔ n̓tes ɬ smʔems ɬ nk̓y̓ep te "
     "qʷámqʷəmt te ʔesk̓ʷúxʷ te nqəmmíntn. nqʷuʔtn xéʔe eɬ ncucíntn xéʔe eɬ ƛ̓uʔ. ƛ̓uʔ newm xeʔ "
     "ʔesx̣zúms he x̣aʔx̣ʔéyqʷ eɬ ʔex n̓tes te qʷən̓qʷen̓tmíns he ʔeskíyxʷ eɬ ʔeszumíns tuʔ wə k̓ist.",
     "paragraph 2, (8) to (10)"),
    ("swet xeʔ xʷuy̓ ʔesx̣ʷelíks tes newms xeʔ ʔeszumíns xeʔ ɬ smʔems nk̓y̓ep he qʷámqʷəmt "
     "te sm̓əns tes weʔxstés eɬ ƛ̓uʔ xeʔ he y̓e te steʔ. nhen̓ teʔ he x̣án̓iws ɬ smʔems ɬ nk̓y̓ep zaʕzaʕzʕə́p "
     "he x̣ʷəst məlmlámns he sp̓aq̓m eɬ he y̓e te sm̓ens ne sɣep ʔesmlámes he sx̣án̓is. ɬ péyeʔus səxsə́xt ɬ "
     "qə́m̓tus ɬ smʔems ɬ nk̓y̓ep ne sq̓ʷmax̣ns eɬ ʔespíʔ ɬ nzuʔzuʔtəns, néʔe ɬ x̣íymus te ʔeszíyts he ʕiƛ̓ts "
     "scuw̓úʔxʷ ne kʔew te nk̓menk, ʔespúts he n̓tes te nyəx̣yix̣étkʷu ɬe seytknmx he nx̣aw̓mn cíʔe.",
     "paragraph 3, (11) to (15)"),
    ("ƛ̓uʔ newm xeʔ ʔesxʷyəps ɬ spzuʔ seytknmx tes ʔexs xeʔ te muɬʔúpnekst te x̣əcpq̓íqn̓ wə "
     "cənkʷúst ɬ smʔems ɬ nk̓y̓ep, ʔescúts ɬ sptínusms ks xʷuy̓s ɬk̓íwix nɬ x̣aʔx̣ʔéyqʷ x̣nuxʷ tes c̓e xeʔ ɬ "
     "sntékɬus wə tmixʷ, eɬ xʷuy̓ p̓en̓t wuɬ xeʔɬkʷúkʷpiʔ eɬ p̓én̓tim̓s ɬ qʷámqʷəmt x̣aʔx̣ʔéyqʷ c̓y̓e.",
     "paragraph 4, (16) to (18)"),
)

ENGLISH_TITLE = "Mr. and Mrs. Coyote and their Magic Kettle"
ENGLISH_1 = ("Long, long ago when the animal people lived in British Columbia, they were all very "
             "friendly together and looked loyally to Mr. and Mrs. Coyote as their leader. They were "
             "clever folk, so that when after many years mankind first appeared in their world, it was "
             "the animal people who taught these men how to build shelters against wind and rain. How "
             "to make fire for warmth and how to make stone tools.")

REMOVE = (
    ("§3.1", "ɬe meʔmʔéw̓s te nk̓y̓ep eɬ x̣aʔx̣ʔéyqʷ nqəmmín ɬ nwén̓us..."),
    ("§3.1", "ʔiɬ qʷcíyxʷus tuɬ..."),
    ("§3.2", "Mr. and Mrs. Coyote and their Magic Kettle Long, long ago..."),
)


def chained(anchor, rows):
    """Each row after the one before it, the first after anchor."""
    out = []
    for row in rows:
        out.append((anchor, row))
        anchor = (row[0], row[3])
    return out


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
     "page 14, the column heads of the table of glosses, in alphabetical order from ʔ"),
    ("appendix table", AUTHORS, "notation", "-", "page 14, affix boundary, the same in the T&T gloss"),
    ("appendix table", AUTHORS, "notation", "=", "page 14, clitic boundary; N/A in the T&T gloss, where = is a lexical suffix boundary"),
    morpheme(14, "<ʔ>", "INCH", "INC", "inchoative, an aspect marker for the beginning of an action, a developing action or a changing state"),
    morpheme(14, "ʔe", "COP", "INT", "(equational) copula", "introductory predicative, ‘there is/are, it is that…’ (T&T1992: 95); ‘and then’ when followed by a nominalized predicate or auxiliary (T&T1992: 180-181)"),
    morpheme(14, "ʔes-", "STAT", "ST", "stative, a prefix on predicates for a state or condition already in effect"),
    morpheme(14, "c̓-", "EMPH", "EMPH", "emphatic, a rare prefix often with idiomatic meaning; with ʔeyɬ ‘now’ it renders ‘right now’ or ‘today’"),
    morpheme(14, "cənkʷúst", "3PL.IND", "", "third person plural independant pronoun, as printed"),
    morpheme(14, "CV(C)-", "AUG", "AUG", "augmentative reduplication: plural, repeated or persistent action, intensification, or a specialized meaning (T&T1992: 82-93)"),
    morpheme(15, "<[V](C)>", "DIM", "DIM", "diminutive, ‘smaller size or amount or reduced force’, or a specialized meaning (T&T1992:89)"),
    morpheme(15, "(h/ʔ)e=", "DET", "DIR", "determiner; determiner-complementizer, its distribution dependent on sentence structure", "direct complement marker, introducing complements that specify predicates; possessive after a third person pronominal"),
    morpheme(15, "-e", "RES", "RSL", "resultative, ‘the recent, often sudden, completion of an activity or change of state’ (T&T 1992:96)"),
    morpheme(15, "ʔeɬ", "ADD", "ACCM", "additive; and, or", "‘and, also, too, along with’; accomplished"),
    morpheme(15, "-e, -n", "FMV", "FMV", "formative"),
    morpheme(15, "-im̓", "IT", "", "iterative; repitition, frequent, repeatedly, as printed"),
    morpheme(15, "-i(y)x", "AUT", "AUT", "autonomous, ‘refers to acts controlled by a specific agent’ (T&T1992: 101)"),
    morpheme(15, "-iyxs", "3PL.SBJ", "", "third person plural subject"),
    morpheme(15, "k=; =k", "D/C", "UNR", "determiner-complementizer", "unrealized, a complement marker for states as yet unrealized, ‘established in the future, if at all’ (T&T1992: 150)"),
    morpheme(15, "ɬ", "REM", "EP", "remote determiner"),
    morpheme(15, "-ɬ-", "CONN", "LIG", "compounding connective", "ligature, a connective that joins stems into compound words"),
    morpheme(15, "ƛ̓uʔ", "EXCL", "PER", "exclusive; but", "persistent, ‘only, just, until, up to’ (T&T1992: 139)"),
    morpheme(15, "-m", "CTR.MID", "MDL", "control middle", "middle, for actions or states where the subject is also the agent, acting with volition"),
    morpheme(16, "-m(i)n", "RLT", "INS", "instrumental, words used as instruments or implements (see also -tən), ‘means of carrying out activities and processes’ (T&T1992: 121)"),
    morpheme(16, "n-", "LOC", "LCL", "locative"),
    morpheme(16, "n=", "AT", "", "preposition, ‘at, to, in, on, with, etc’"),
    morpheme(16, "-(ə)p", "INCH", "INC", "inchoative, an aspect marker for the beginning of an action, a developing action or a changing state"),
    morpheme(16, "s-; s=; =s", "NMLZ", "NOM", "nominalizer; creates a noun"),
    morpheme(16, "-(e)s", "3.SBJ", "3.SBJ", "third person subject, ‘he, she, it, they’"),
    morpheme(16, "=s", "3.POSS", "3.POSS", "third person possessive, ‘his, hers, its, theirs’"),
    morpheme(16, "-s", "CAUS", "CAU", "causative; the subject causes the action to happen"),
    morpheme(16, "-t", "TR", "TR", "transitivizer; the predicate takes both object and subject arguments"),
    morpheme(16, "-t", "INTR", "IM", "intermediate, “states and actions which have just gone into effect” (T&T1992:92)"),
    morpheme(16, "t-", "QT", "", "qualitative. The tiers of (7) gloss it QLT"),
    morpheme(16, "t(ə)=", "OBL", "OBL", "oblique particle, introducing complements of predicates, usually objects (T&T1992: 146-147)"),
    morpheme(16, "-t(ə)n; -min", "INS", "INS", "instrumental, words used as instruments or implements (see also -min)"),
    morpheme(16, "=us", "3SBJV", "3.CJV", "third person subjunctive"),
    morpheme(16, "w=; u=", "TO", "", "preposition"),
    morpheme(16, "wiʔ", "EMPH", "", "emphasizer"),
    morpheme(16, "xéʔe", "DEM", "", "distal demonstrative"),
    morpheme(16, "xʷuy̓, xʷiʔ", "PROSP", "FUT", "prospective"),
    morpheme(16, "=⦰", "3SBJ", "", "third person subject, with ⦰ U+29B0 where the tiers write ∅"),
    morpheme(16, "=⦰", "3.OBJ", "", "third person object"),
]

ADD = tuple(
    [(None, ("title", AUTHORS, "title", TITLE, "the paper's title, the story's name in nɬeʔkepmxcín and in English")),
     (None, ("title", AUTHORS, "name", "wlwlmelst", "author, Maurice Michell, Kanaka Bar Indian Band, one of the few remaining speakers of the Southern utémkt dialect, who translated the story back into nɬeʔkepmxcín, transcribed it and read it aloud")),
     (None, ("title", AUTHORS, "name", "Maurice Michell", "wlwlmelst's English name")),
     (None, ("title", AUTHORS, "name", "Jonathan Janzen", "author, Kanaka Bar Indian Band, responsible for the interlinear analysis and the audio recording, footnote 1")),
     (None, ("title", AUTHORS, "place", "Kanaka Bar Indian Band", "the authors' affiliation")),
     (None, ("footnote 1", AUTHORS, "name", "Kanaka Bar First Voices", "the page where this story and others can be found, https://www.firstvoices.com/kanakabar/")),
     (None, ("footnote 1", AUTHORS, "name", "FPCC", "funded the language work in part, named only by its initials")),
     (None, ("§1", AUTHORS, "name", "Ernie Michell", "nk̓y̓ep, wlwlmelst's younger brother, who gives the English narration of the recording")),
     (None, ("§1", AUTHORS, "name", "Noel Stewart", "whose students at St. George's Indian Residential School illustrated Meet Mr. Coyote")),
     (None, ("§1", AUTHORS, "place", "St. George's Indian Residential School", "in Lytton, B.C.")),
     (None, ("§1", AUTHORS, "place", "Lytton, B.C.", "where the legends originate; ƛ̓q̓əmcín")),
     (None, ("§1", AUTHORS, "name", "British Columbia Indian Arts and Welfare Society", "published Meet Mr. Coyote in 1941")),
     (None, ("§1", AUTHORS, "name", "James Teit", "to whom the stories are attributed; their storyteller and recorder are unknown")),
     (None, ("§1", AUTHORS, "place", "Fraser River", "along which the Southern utémkt dialect is spoken")),
     (None, ("§1", AUTHORS, "place", "Skuppah", "the Southern utémkt dialect is spoken south of it")),
     (None, ("§1", AUTHORS, "place", "Siska", "one end of the Southern utémkt communities")),
     (None, ("§1", AUTHORS, "place", "Spuzzum", "the other end of the Southern utémkt communities")),
     (None, ("§1", AUTHORS, "language", "Thompson River Salish", "the English name of nɬeʔkepmxcín, in the keywords, §1 and the heading of §3.1")),
     ("§3.1", ("§3.1", AUTHORS, "notation", "§3.1", "the story in nɬeʔkepmxcín, a title line and four paragraphs, as wlwlmelst translated it back and transcribed it. §3.3 sets the same sentences as (1) to (18)")),
     (None, ("§3.2", L, "cited form", "semecín", "‘English’, in the heading 3.2 semecín - English")),
     (None, ("footnote 3", AUTHORS, "name", "Lisa Matthewson", "thanked with the UBC researchers for their work on this language")),
     (None, ("appendix", AUTHORS, "name", "nɬab", "UBC's lab, whose glossing conventions the Gloss column follows")),
     (None, ("appendix", AUTHORS, "notation", "Dear Human (2)", "another story, cited for nexcín e ʔestekʷ ‘give you shelter’ under (h/ʔ)e=")),
     (None, ("all", AUTHORS, "notation", "tiers", "each example of §3.3 sets its sentence as groups of four tiers: the nɬeʔkepmxcín, its segmentation, the morpheme glosses, and English word by word, with Stewart's English in straight quotes after the last group")),
     (None, ("all", AUTHORS, "notation", "∅", "the tiers write the null morpheme ∅ U+2205, the appendix ⦰ U+29B0")),
     (None, ("all", AUTHORS, "notation", "comma below", "§3.1 prints a small comma under the line after ƛ̓uʔ in paragraphs 2 and 4, after ʔeszumíns and under ʔexs. The text layer holds no character there, and the same words in the tiers carry none")),
     (None, ("all", AUTHORS, "symbol note", "text layer", "the font names U+0313 COMBINING COMMA ABOVE as the letter ƛ, and as w in the bold title, the acute as ə and the dot below as x, and maps í to i. The page text here is read from the glyph positions, each mark set on the letter it sits over, and each í from the slanted stroke the page prints in place of the dot")),
     ]
    + chained(("§3.1", "3.1 nɬeʔkepmxcín – Thompson River Salish"),
              [("§3.1", WLWLMELST, "running speech", form, "page 2, " + gloss) for form, gloss in STORY])
    + chained(("§3.2", "3.2 semecín - English"),
              [("§3.2", STEWART, "translation", ENGLISH_TITLE, "page 3, the story's title in Stewart 1941"),
               ("§3.2", STEWART, "translation", ENGLISH_1, "page 3, the English of the story as Stewart 1941 gives it, one paragraph")])
    + [(None, row) for row in APPENDIX]
)

SET = {}
REPLACE = ()

WHOSE = (
    "The story is a back-translation. Stewart 1941 printed it in English, attributed to James Teit, "
    "and wlwlmelst (Maurice Michell) translated it back into nɬeʔkepmxcín, transcribed it, and read "
    "it aloud for a recording with his brother nk̓y̓ep (Ernie Michell), who gives the English. Jonathan "
    "Janzen made the interlinear analysis.\n\n"
    "The who for the story of §3.1 is wlwlmelst. The who for each tier of an example in §3.3 is "
    "nɬeʔkepmxcín, and for the English word by word under the gloss, Jonathan Janzen. The who for a "
    "translation, and for the English of §3.2, is Stewart 1941. The prose, the footnotes, the "
    "references and the appendix carry the two authors."
)

LETTERS = (
    "The orthography follows Thompson & Thompson 1996. It writes ɬ, ƛ̓, ʔ, ʕ, ə, ɣ, ʷ, x̣ with U+0323 "
    "COMBINING DOT BELOW, and U+0313 COMBINING COMMA ABOVE on glottalized letters, n̓, w̓, y̓, k̓, q̓. "
    "Stress is an acute. The tiers write ∅ for a null morpheme and the appendix ⦰."
)

PAGE_NOTES = (
    "The text layer of this paper is not the page. Its font names the comma above as the letter ƛ "
    "(w in the bold title), the acute as ə and the dot below as x, and gives í as i; the text layer "
    "reads ƛ ƛ qƛ əmcin for ƛ̓q̓əmcín. The source here is the page read from the glyph positions with "
    "pypdfium2: each misread mark is set on the letter it sits over or under, each row of an example "
    "is rebuilt by the glyphs' height, and an i is read as í where the page prints a slanted stroke "
    "over it, 12 to 16 pixels wide at 600 dpi against the dot's 8 to 10. Every í of §3.1 was checked "
    "against a render of page 2 at 400 dpi."
)
