# Context for Steiner, Locative Demonstratives in nɬeʔkepmxcín.

AUTHORS = "Reed Steiner"
L = "nɬeʔkepmxcín"

TITLE = "Locative Demonstratives in nɬeʔkepmxcín"
BYLINE = "Reed Steiner, University of British Columbia"

BEV = "Bev Phillips"
MARTY = "c̓úʔsinek (Marty Aspinall)"
BERNICE = "kʷaɬtèzetkʷuʔ (Bernice Garcia)"

# The speaker tags of §2.1: BP, CMA and KBG.
INITIALS = {"BP": BEV, "CMA": MARTY, "KBG": BERNICE}

# The engine takes the tag at the right of a translation, (SF | BP 22 May 2025), as its who. SF is a
# sentence the author supplied and VF one the speaker volunteered. VT is a translation the speaker
# volunteered, and both of those are Bev Phillips's. Every other translation is the author's.
WHO_MATCH = (
    (r"VT \| BP", r"^translation$", BEV, "volunteered by the speaker, VT"),
    (r"\| BP in Hall and Phillips 2024", r".", "Hall and Phillips 2024",
     "Bev Phillips's sentence as Hall and Phillips 2024:137 give it"),
    (r"^(SF|VF)\b", r".", AUTHORS, "the author's translation of the speaker's sentence"),
)

# The formulas of §5 and §6.2 are the author's denotations: J and K are the text layer's ⟦ and ⟧.
FORMULA = r"^\((6[23]|6[7-9]|7[013-6]|8[89]|9[08])[a-g]?\) line"
KIND_RULES = (
    (FORMULA, r"^(transcription|segmentation|gloss)$", r".", "rule", AUTHORS,
     "a line of the author's formula"),
)

# (96) is St'át'imcets and (97) ʔayʔaǰuθəm, both after Davis and Mellesmoen 2019.
WHO_RULES = (
    (r"^\(96\) line [2-7]$", "segmentation", r".", "St’át’imcets", "after Davis and Mellesmoen 2019:30"),
    (r"^\(96\) line [2-7]$", "gloss", r".", "St’át’imcets", "after Davis and Mellesmoen 2019:30"),
)

NAME = "name"
FORMS = {
    "nɬeʔkepmxcín": ("language", AUTHORS, "the language of this paper, a.k.a. Nlaka’pamux or Thompson River Salish, Northern Interior Salish, ISO 639-3 thp"),
    "nɬeʔkpemxcín": ("language", AUTHORS, "nɬeʔkepmxcín, as the paper also prints it"),
    "nɬeʔkepmxcin": ("language", AUTHORS, "nɬeʔkepmxcín without its accent, in a reference title"),
    "Nlaka’pamux": ("language", AUTHORS, "another name for nɬeʔkepmxcín"),
    "St’át’imcets": ("language", AUTHORS, "Lillooet, Northern Interior Salish. van Eijk 1997 named its demonstrative pronouns, and (96) is St’át’imcets"),
    "ʔayʔaǰuθəm": ("language", AUTHORS, "Comox-Sliammon, Central Salish, the language of (97) and of ɬɛn̓"),
    "ƛ̓q̓əmcín": ("language", AUTHORS, "the Lytton dialect, Bev Phillips's"),
    "scw̕exmxcín": ("language", AUTHORS, "the Nicola Valley dialect, c̓úʔsinek's, in the spelling of Thompson and Thompson 1996:45"),
    "scew̕exmxcín": ("language", AUTHORS, "the Nicola Valley dialect as c̓úʔsinek spells it, footnote 1"),
    "Stó:lō": ("language", AUTHORS, "the Stó:lō dialect of Halkomelem, Coast Salish, ISO 639-3 hur, on c̓úʔsinek's father's side"),
    "c̓eɬétkʷu": ("language", AUTHORS, "the Coldwater dialect, kʷaɬtèzetkʷuʔ's"),
    "c̓úʔsinek": (NAME, AUTHORS, "Marty Aspinall's name, CMA in the tags, one of the three consultants"),
    "c̕úʔsinek": (NAME, AUTHORS, "Marty Aspinall's name, CMA in the tags, written with c̕ where §2.1 writes c̓"),
    "c̕úʔsinek’s": (NAME, AUTHORS, "Marty Aspinall's name, CMA in the tags, in the possessive"),
    "kʷaɬtèzetkʷuʔ": (NAME, AUTHORS, "Bernice Garcia's traditional name, KBG in the tags, one of the three consultants"),
    "kʷałtèzetkʷuʔ": (NAME, AUTHORS, "Bernice Garcia's traditional name, KBG in the tags, written with ł"),
    "kʷałtèzetkʷ": (NAME, AUTHORS, "Bernice Garcia's traditional name, KBG in the tags, short and written with ł"),
    "kʷałtèzetkʷ’s": (NAME, AUTHORS, "Bernice Garcia's traditional name in the possessive"),
    "kʷəɬtəzétkʷu": (NAME, AUTHORS, "Bernice Garcia's traditional name as a reference entry spells it"),
    "Gärdenfors": (NAME, AUTHORS, "Peter Gärdenfors, cited on conceptual spaces"),
    "O’Keefe": (NAME, AUTHORS, "John O’Keefe, cited on vector representations of space"),
    "Růžička": (NAME, AUTHORS, "in a reference entry"),
    "Jürgen": (NAME, AUTHORS, "in a reference entry"),
    "ném": ("cited form", L, "from the thanks ném kʷukʷstéyp! in the acknowledgement, left untranslated"),
    "kʷukʷstéyp": ("cited form", L, "from the thanks ném kʷukʷstéyp! in the acknowledgement, left untranslated"),
    "hu∼θu": ("cited form", "ʔayʔaǰuθəm", "‘go’, the motion verb Central Salish uses for a source, after Davis and Mellesmoen 2019"),
    "ʔə=": ("cited affix", "ʔayʔaǰuθəm", "the oblique, the only preposition ʔayʔaǰuθəm has, after Davis and Mellesmoen 2019"),
    "ɬəl=": ("cited affix", "St’át’imcets", "‘from’, after Davis and Mellesmoen 2019"),
    "ɬɛn̓": ("cited form", "ʔayʔaǰuθəm", "the determiner of previous direct evidence, after Reisinger and Huijsmans 2021"),
    "σ": ("notation", AUTHORS, "the spatial trace function of Link 1998 and Krifka 1989, 1998"),
    "α": ("notation", AUTHORS, "a variable over individuals, locations and situations"),
    "⃗v": ("notation", AUTHORS, "a vector"),
    "⃗w": ("notation", AUTHORS, "a vector"),
    "⃗U": ("notation", AUTHORS, "the characteristic function of a set of vectors"),
    "⃗V": ("notation", AUTHORS, "the characteristic function of a set of vectors"),
    "⃗W": ("notation", AUTHORS, "the characteristic function of a set of vectors"),
    "Ǽ": ("notation", AUTHORS, "a pictogram in (98) standing for an iconographic variable, printed as a glyph of the paper's picture font"),
    "í": ("notation", AUTHORS, "a pictogram in (98) standing for an iconographic variable, printed as a glyph of the paper's picture font"),
}

# Two words the line broke, the glued math of §5.1.2 and a footnote mark on a word.
DROP = ("nɬeʔkep-", "mxcín", "nɬeʔkepmxcín.2", "aren’t.16", "arm’s-length", "⟨α",
        "diacritic:⃗V", "(shaded).⃗V", "spaces⃗U", "set⃗V")

# The engine sets a table one cell to a line, and Table 5 inside the paragraph above it. Each is
# read off its page again here. (73a) and (73b) run their paraphrase over lines 3 to 5, and (97)
# sets its two lines as cells.
REMOVE_WHERE = r"^Table [1-4] line (?!1$)\d+$|^\(73[ab]\) line [45]$|^\(97\) line ([2-9]|1[01])$"
REMOVE = (
    ("§4.3", "Distal n=éʔe at=DIST ‘(right) there’ t=éʔe by=DIST ‘about there’ w=éʔe to=DIST ‘that way’"),
) + tuple(("§4.4", one) for one in ("tu(xʷ)-initial", "n=ʔé", "t=ʔé", "w=ʔé", "tw=ʔé",
                                     "n=éʔe", "t=éʔe", "w=éʔe", "tw=éʔe"))
REPLACE = (
    ("Table 5: Segmented Locative Demonstratives in nɬeʔkepmxcín (expanded) Point n-initial Area "
     "t-initial Goal w-initial Source tu(xʷ)-initial Proximal n=ʔé at=PROX ‘(right) here’ t=ʔé "
     "by=PROX ‘about here’ w=ʔé to=PROX ‘this way’ tw=ʔé from=PROX ‘from here’ Distal n=éʔe "
     "at=DIST ‘(right) there’ t=éʔe by=DIST ‘about there’ w=éʔe to=DIST ‘that way’ tw=éʔe "
     "from=DIST ‘from there’ There are two", "There are two"),
)


def chained(anchor, rows):
    """Each row after the one before it, the first after anchor."""
    out = []
    for row in rows:
        out.append((anchor, row))
        anchor = (row[0], row[3])
    return out


def cell(table, head, form, english, page, kind="transcription", gloss=None):
    where = "%s %s" % (table, head)
    rows = [(where, L, kind, form, "page %d, the cell of %s" % (page, head))]
    if gloss:
        rows.append((where, L, "gloss", gloss, "page %d" % page))
    rows.append((where, AUTHORS, "translation", english, "page %d" % page))
    return rows


TABLE_1 = [("Table 1 heads", AUTHORS, "note", "Evidence Neutral | Previous Direct Evidence | Not Visible",
            "page 5, the column heads. The rows are Proximal and Distal")] + \
    cell("Table 1", "Proximal, Evidence Neutral", "xʔé", "‘this, these’", 5) + \
    cell("Table 1", "Distal, Evidence Neutral", "xéʔe", "‘that, those’", 5) + \
    cell("Table 1", "Previous Direct Evidence", "ɬǝ́n̕e", "‘this/that (no longer present)’", 5) + \
    cell("Table 1", "Not Visible", "kʷukʷ", "‘this/that (not visible)’", 5)
TABLE_1[-4] = TABLE_1[-4][:4] + ("page 5, one cell over the proximal and distal rows",)
TABLE_1[-2] = TABLE_1[-2][:4] + ("page 5, one cell over the proximal and distal rows",)

POINTS = (("Point", "n", "‘(right) here’", "‘(right) there’", "at"),
          ("Area", "t", "‘about here’", "‘about there’", "by"),
          ("Goal", "w", "‘this way’", "‘that way’", "to"))


def paradigm(table, page, heads, segmented=False, source=False):
    rows = [("%s heads" % table, AUTHORS, "note", heads,
             "page %d, the column heads. The rows are Proximal and Distal" % page)]
    columns = POINTS + ((("Source", "tw", "‘from here’", "‘from there’", "from"),) if source else ())
    for side, deictic, label in (("Proximal", "ʔé", "PROX"), ("Distal", "éʔe", "DIST")):
        for head, prefix, here, there, preposition in columns:
            english = here if side == "Proximal" else there
            if segmented:
                rows += cell(table, "%s, %s" % (side, head), "%s=%s" % (prefix, deictic), english, page,
                             kind="segmentation", gloss="%s=%s" % (preposition, label))
            else:
                rows += cell(table, "%s, %s" % (side, head), prefix + deictic, english, page)
    return rows


TABLE_2 = paradigm("Table 2", 12, "Point | Area | Goal")
TABLE_3 = [("Table 3 heads", AUTHORS, "note", "Point n-initial | Area t-initial | Goal w-initial",
            "page 18, the column heads. The rows are Preposition, Proximal and Distal")] + \
    cell("Table 3", "Preposition, Point", "n=", "‘at, on, in’", 18, kind="cited affix") + \
    cell("Table 3", "Preposition, Area", "t=", "‘by, about’", 18, kind="cited affix") + \
    cell("Table 3", "Preposition, Goal", "w=", "‘to, toward’", 18, kind="cited affix") + \
    paradigm("Table 3", 18, "")[1:]
TABLE_4 = paradigm("Table 4", 28, "Point n-initial | Area t-initial | Goal w-initial", segmented=True)
TABLE_5 = [("Table 5 line 1", AUTHORS, "note", "Segmented Locative Demonstratives in nɬeʔkepmxcín (expanded)",
            "page 28, the caption")] + \
    paradigm("Table 5", 28, "Point n-initial | Area t-initial | Goal w-initial | Source tu(xʷ)-initial",
             segmented=True, source=True)

SELF = "ʔes ʔúməcms kʷaɬtèzetkʷʔ tuɬe c̓əɬétkʷu wéʔe ncitxʷ ƛ̓uʔ wéʔec ʔex netíyxs scwew̓xmx ƛ̓uʔ tékm xéʔe ne nɬeʔképmx e tmixʷs"

ADD = tuple(
    [(None, ("title", AUTHORS, "title", TITLE, "the paper's title, carrying the star of the acknowledgement footnote")),
     (None, ("title", AUTHORS, "name", "Reed Steiner", "author, University of British Columbia")),
     (None, ("footnote *", AUTHORS, "name", BEV, "consultant, BP in the tags, from Lytton, speaker of ƛ̓q̓əmcín. The author's primary consultant")),
     (None, ("footnote *", AUTHORS, "name", "Marty Aspinall", "consultant, CMA in the tags, c̓úʔsinek, from Coldwater, speaker of scw̕exmxcín")),
     (None, ("footnote *", AUTHORS, "name", "Bernice Garcia", "consultant, KBG in the tags, kʷaɬtèzetkʷuʔ, from Coldwater, speaker of c̓eɬétkʷu")),
     (None, ("footnote *", AUTHORS, "name", "Marcin Morzycki", "reader, thanked")),
     (None, ("footnote *", AUTHORS, "name", "Lisa Matthewson", "reader, thanked. Matthewson 2023 is cited in footnote 2")),
     (None, ("footnote *", AUTHORS, "name", "Henry Davis", "thanked with the nɬeʔkepmxcín lab")),
     (None, ("footnote *", AUTHORS, "name", "Brent Hall", "of the nɬeʔkepmxcín lab, thanked. Hall and Phillips 2024 is the source of (18) and (92b)")),
     (None, ("footnote *", AUTHORS, "name", "Ella Hannon", "of the nɬeʔkepmxcín lab, thanked. Footnote 31: she elicited the LocDem examples of §6.2")),
     (None, ("footnote *", AUTHORS, "name", "Bruce Oliver", "of the nɬeʔkepmxcín lab, thanked")),
     (None, ("footnote *", AUTHORS, "name", "Jacobs Research Funds", "a funder of the research")),
     (None, ("footnote *", AUTHORS, "name", "Kinkade Language and Culture Foundation", "a funder of the research")),
     (None, ("§2.1", AUTHORS, "name", "Mandy Jimmie", "cited by personal communication in §3.1 and §5.2.4")),
     ("§2.1", ("§2.1", BERNICE, "running speech", SELF, "page 3, kʷaɬtèzetkʷuʔ introducing herself, set in italics in the prose")),
     (("§2.1", SELF), ("§2.1", AUTHORS, "translation", "‘My traditional name is kʷaɬtèzetkʷuʔ, my home is in Coldwater of ‘Nicola’ of nlaka’pamux lands.’", "page 3, the translation printed with it")),
     (None, ("all", AUTHORS, "notation", "SF", "a sentence the author supplied and the speaker judged, §2.1")),
     (None, ("all", AUTHORS, "notation", "VF", "a sentence the speaker volunteered, §2.1")),
     (None, ("all", AUTHORS, "notation", "VT", "a translation the speaker volunteered, in (91a) and (91b)")),
     (None, ("all", AUTHORS, "notation", "*", "judged ill-formed by the speaker")),
     (None, ("all", AUTHORS, "notation", "#", "judged infelicitous in the given context")),
     (None, ("all", AUTHORS, "notation", "?", "an uncertain judgement")),
     (None, ("all", AUTHORS, "notation", "Located", "the locative type-shift of (67), taking a set of vectors and an individual or eventuality")),
     (None, ("all", AUTHORS, "notation", "Follows", "the path type-shift of (74), directing an event along a path")),
     (None, ("all", AUTHORS, "symbol note", "J K", "the text layer's J and K are the denotation brackets ⟦ and ⟧ of the page")),
     (None, ("all", AUTHORS, "symbol note", "h", "the text layer's h before the paraphrases of (73a) and (73b) is a frowning face ☹ on the page, marking a reading the paper rejects")),
     (None, ("all", AUTHORS, "symbol note", "σ(Ǭ) σ(ʴ) σ(͋) σ(ƀ) σ(P) σ(ί) σ(̨)", "pictograms of the paper's picture font, standing for iconographic variables: the basket of (63a), the door of (63b), the table of (69), the tree of (71a), the house of (71b) and (73b), the mountain of (73a) and nɬeʔképmx land of (68)")),
     (None, ("all", AUTHORS, "symbol note", "text layer", "the text layer runs the words of 540 lines together, Thispaperreexaminesthedemonstrativeparadigm. Each line is read from whichever of the text layer and the glyph positions keeps more of the page's spaces")),
     ]
    + chained("Table 1 line 1", TABLE_1)
    + chained("Table 2 line 1", TABLE_2)
    + chained("Table 3 line 1", TABLE_3)
    + chained("Table 4 line 1", TABLE_4)
    + chained(("§4.4", "A similar analysis..."), TABLE_5)
    + chained("(97) line 1", [
        ("(97) line 2", "ʔayʔaǰuθəm", "transcription", "kʷihit x̌ax̌aɬ Tony hu Gloria", "page 52, read off the page, hu set in bold"),
        ("(97) line 3", "ʔayʔaǰuθəm", "gloss", "more tall Tony go Gloria", "page 52, read off the page"),
    ])
)

# The words of kʷaɬtèzetkʷuʔ's self-introduction, the greeting of footnote 2 and a reference title.
INTRO = "a word of kʷaɬtèzetkʷuʔ's self-introduction, page 3"
SET = dict(
    [(("§2.1", one), {"kind": "cited form", "who": L, "gloss": INTRO})
     for one in ("ʔes", "ʔúməcms", "tuɬe", "wéʔe", "ncitxʷ", "ƛ̓uʔ", "wéʔec", "ʔex", "netíyxs",
                 "tékm", "xéʔe", "nɬeʔképmx", "tmixʷs")]
    + [(("§2.1", "kʷaɬtèzetkʷʔ"), {"kind": NAME, "who": L, "gloss": "her traditional name in her self-introduction, page 3"}),
       (("§2.1", "c̓əɬétkʷu"), {"kind": "place", "who": L, "gloss": "Coldwater, in kʷaɬtèzetkʷuʔ's self-introduction, page 3"}),
       (("§2.1", "scwew̓xmx"), {"kind": "place", "who": L, "gloss": "Nicola, in kʷaɬtèzetkʷuʔ's self-introduction, page 3"}),
       (("§2.1", "nlaka’pamux"), {"kind": "language", "who": AUTHORS, "gloss": "in the translation of kʷaɬtèzetkʷuʔ's self-introduction, page 3"})]
    + [(("footnote 2", one), {"gloss": "a word of the scw̕exmxcín greeting hén̕ ɬeʔ kʷ, footnote 2"})
       for one in ("hén̕", "ɬeʔ", "kʷ")]
    + [(("references", one), {"gloss": "a word of the title of Hall and Phillips 2024, xʷíʔ kʷ páq (you will be sorry)"})
       for one in ("xʷíʔ", "kʷ", "páq")]
    + [(("(73) line 1", "Locative type-shift applied to path PPs (for illustrative purposes only)"),
        {"kind": "note", "who": AUTHORS}),
       (("(73a) line 3", "h ‘Take an object α from the domain of individuals or events. Return true iff all points"),
        {"kind": "translation", "who": AUTHORS, "gloss": "page 40, the paraphrase of (73a) over three lines. h is the frowning face ☹ of the page",
         "form": "h ‘Take an object α from the domain of individuals or events. Return true iff all points in the spatial trace of α are the endpoint of a vector whose endpoint is in the eigenspace of the mountain.’"}),
       (("(73b) line 3", "h ‘Take a set of vectors characterized by ⃗W and an object α from the domain of"),
        {"kind": "translation", "who": AUTHORS, "gloss": "page 40, the paraphrase of (73b) over three lines. h is the frowning face ☹ of the page",
         "form": "h ‘Take a set of vectors characterized by ⃗W and an object α from the domain of individuals or events. Return true iff all points in the spatial trace of α are the endpoint of a vector which starts in the eigenspace of my house.’"}),
       (("(96) line 1", "St’át’imcets"), {"kind": "note", "who": AUTHORS, "gloss": "page 52, the language of the example"}),
       (("(97) line 1", "ʔayʔaǰuθəm"), {"kind": "note", "who": AUTHORS, "gloss": "page 52, the language of the example"}),
       (("(100) line 2", "You say, hén̕ ɬeʔ kʷ [greeting]:"), {"kind": "note", "who": AUTHORS, "gloss": "page 53, the context goes on with the greeting"})]
)

WHOSE = (
    "The language is nɬeʔkepmxcín. The examples come from the author's three consultants, Bev "
    "Phillips (BP), c̓úʔsinek Marty Aspinall (CMA) and kʷaɬtèzetkʷuʔ Bernice Garcia (KBG), each "
    "tagged SF for a sentence the author supplied or VF for one the speaker volunteered, with the "
    "date. A few come from Thompson and Thompson 1996, Koch 2008b, Hall and Phillips 2024 and "
    "kʷałtèzetkʷ, Hannon and Stacey 2024. (96) is St’át’imcets and (97) ʔayʔaǰuθəm, both after Davis "
    "and Mellesmoen 2019.\n\n"
    "The who for each tier of an example is the language. The who for a translation is the work "
    "cited on its line, Bev Phillips where the tag carries VT, and the author where it carries SF or "
    "VF. A consultant's comment is theirs. The formulas of §5 and §6.2 and their paraphrases, the "
    "trees, the tables' heads and the prose carry the author."
)

LETTERS = (
    "The paper writes ɬ and sometimes ł for the lateral fricative, kʷaɬtèzetkʷuʔ beside "
    "kʷałtèzetkʷuʔ, and c̓ beside c̕ for the glottalized c, c̓úʔsinek beside c̕úʔsinek. The formulas "
    "write ⃗v for a vector, U+20D7 COMBINING RIGHT ARROW ABOVE, and σ for the spatial trace."
)

PAGE_NOTES = (
    "The text layer of this paper runs the words of 540 lines together. The source here takes each "
    "line from whichever of the text layer and the glyph positions keeps more of the page's spaces. "
    "The denotation brackets ⟦ ⟧ come out of the text layer as J and K, the frowning face of (73) as "
    "h, and the pictograms of the iconographic variables as single glyphs of a picture font, "
    "σ(Ǭ) for the basket. Pages 2, 3, 4 and 40 were read at 200 to 300 dpi for the spaces the page "
    "prints and the text layer drops."
)
