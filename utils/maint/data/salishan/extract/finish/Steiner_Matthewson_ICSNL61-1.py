# Context for Steiner and Matthewson, That moon has risen: The semantics of nɬeʔkepmxcín nominal
# demonstratives.

AUTHORS = "Reed Steiner and Lisa Matthewson"
L = "nɬeʔkepmxcín"

TITLE = "That moon has risen: The semantics of nɬeʔkepmxcín nominal demonstratives"
BYLINE = "Reed Steiner and Lisa Matthewson, University of British Columbia"

BEV = "Bev Phillips"
MARTY = "c̓úʔsinek (Marty Aspinall)"
BERNICE = "kʷaɬtèzetkʷ (Bernice Garcia)"

# The speaker tags of §1.2, BP, KBG and CMA, and RS, Reed Steiner, in the discussion under (21c).
INITIALS = {"BP": BEV, "KBG": BERNICE, "CMA": MARTY, "RS": "Reed Steiner"}

# The engine takes the tag at the right of a translation, (SF | BP 22 May 2025), as its who. SF is a
# sentence the authors supplied and VF one the speaker volunteered; the translation is the authors'.
# (11) is Carl Alexander's St'át'imcets sentence from Alexander et al. 2025.
WHO_MATCH = (
    (r"^St’át’imcets; Carl Alexander", r"^translation$", "Alexander et al. 2025",
     "Carl Alexander's St’át’imcets sentence as Alexander et al. 2025:55 give it"),
    (r"^(SF|VF)\b", r".", AUTHORS, "the authors' translation of the speaker's sentence"),
)

WHO_RULES = (
    (r"^\(11\) line [2-7]$", "segmentation", r".", "St’át’imcets", "Carl Alexander's sentence, after Alexander et al. 2025:55"),
    (r"^\(11\) line [2-7]$", "gloss", r".", "St’át’imcets", "Carl Alexander's sentence, after Alexander et al. 2025:55"),
    (r"^footnote 13 \(i\) line [2-4]$", "transcription", r".", "ʔayʔaǰuθəm", "reported by Marianne Huijsmans (p.c.), footnote 13"),
    (r"^footnote 13 \(i\) line [2-4]$", "segmentation", r".", "ʔayʔaǰuθəm", "reported by Marianne Huijsmans (p.c.), footnote 13"),
    (r"^footnote 13 \(i\) line [2-4]$", "gloss", r".", "ʔayʔaǰuθəm", "reported by Marianne Huijsmans (p.c.), footnote 13"),
    (r"^footnote 13 \(i\) line 5$", "translation", r".", "Marianne Huijsmans (p.c.)", "the ʔayʔaǰuθəm example of footnote 13"),
    (r"^footnote 13 \(i\) line 6$", "speaker comment", r".", "a ʔayʔaǰuθəm speaker consulted by Marianne Huijsmans",
     "reported in footnote 13"),
)

# The discussion of (21c) between the consultant and the researcher, set as prose.
KIND_RULES = (
    (r"^§3\.4$", r"^note$", r"^KBG: ", "speaker comment", BERNICE, "KBG in the discussion of (21c)"),
    (r"^§3\.4$", r"^note$", r"^RS: \[repeats", "speaker comment", BERNICE, "RS repeats (c) and KBG answers, in the discussion of (21c)"),
    (r"^§3\.4$", r"^note$", r"^RS: ", "note", "Reed Steiner", "RS, the researcher, in the discussion of (21c)"),
)

NAME = "name"
FORMS = {
    "nɬeʔkepmxcín": ("language", AUTHORS, "the language of this paper, a.k.a. Thompson River Salish, Northern Interior Salish, ISO 639-3 thp"),
    "nłeʔkepmxcín": ("language", AUTHORS, "nɬeʔkepmxcín written with ł"),
    "Nɬeʔkepmxcin": ("language", AUTHORS, "nɬeʔkepmxcín in a reference title"),
    "Nłeʔkepmxcin": ("language", AUTHORS, "nɬeʔkepmxcín in a reference title, written with ł"),
    "St’át’imcets": ("language", AUTHORS, "Lillooet, Northern Interior Salish. (11) is St’át’imcets"),
    "St'át'imcets": ("language", AUTHORS, "St’át’imcets with straight apostrophes, in a reference entry"),
    "St’át’imc": ("language", AUTHORS, "the St’át’imc, in a reference entry"),
    "ʔayʔaǰuθəm": ("language", AUTHORS, "Comox-Sliammon, Central Salish, the language of footnote 13's (i)"),
    "Skwxwú7mesh": ("language", AUTHORS, "Squamish, in a reference entry"),
    "ƛ̓q̓mcín": ("language", AUTHORS, "the Lytton dialect, Bev Phillips's"),
    "nc̕eɬétkʷu": ("language", AUTHORS, "the Coldwater dialect, kʷaɬtèzetkʷ's"),
    "scw̓exmxcín": ("language", AUTHORS, "the Nicola Valley dialect, c̓úʔsinek's"),
    "nlaka’pamux": ("language", AUTHORS, "in the translation of kʷaɬtèzetkʷ's self-introduction, footnote 2"),
    "c̓úʔsinek": (NAME, AUTHORS, "Marty Aspinall's name, CMA in the tags, from Coldwater, recorded in conversation"),
    "c̕úʔsinek": (NAME, AUTHORS, "Marty Aspinall's name, written with c̕ where §1.2 first writes c̓"),
    "kʷaɬtèzetkʷ": (NAME, AUTHORS, "Bernice Garcia's traditional name, KBG in the tags"),
    "kʷałtèzetkʷ": (NAME, AUTHORS, "Bernice Garcia's traditional name, KBG in the tags, written with ł"),
    "kʷałtèzetkʷ’s": (NAME, AUTHORS, "Bernice Garcia's traditional name in the possessive"),
    "kʷałtèzetkʷ's": (NAME, AUTHORS, "Bernice Garcia's traditional name in the possessive, straight apostrophe"),
    "kʷukʷstéyp": ("cited form", L, "from the thanks nem kʷukʷstéyp in the acknowledgement, left untranslated"),
    "Na’zinek": (NAME, AUTHORS, "Mandy Na’zinek Jimmie, thanked in the acknowledgement"),
    "nɬab": (NAME, AUTHORS, "the UBC nɬab, the nɬeʔkepmxcín lab, thanked in the acknowledgement"),
    "Šimík": (NAME, AUTHORS, "Radek Šimík, cited on demonstratives"),
    "Löbner": (NAME, AUTHORS, "Sebastian Löbner, cited on definiteness"),
    "Gärdenfors": (NAME, AUTHORS, "in a reference entry"),
    "Yağmur": (NAME, AUTHORS, "in a reference entry"),
    "Stefánsdóttir": (NAME, AUTHORS, "in a reference entry"),
    "Schöller": (NAME, AUTHORS, "in a reference entry"),
    "Jürgen": (NAME, AUTHORS, "in a reference entry"),
    "Universität": ("place", AUTHORS, "in a reference entry"),
    "München": ("place", AUTHORS, "in a reference entry"),
    "Qwa7yán’ak": ("cited form", "Skwxwú7mesh", "a word in the title of a reference entry"),
    "múta7": ("cited form", "Skwxwú7mesh", "a word in the title of a reference entry"),
    "t̓əgəm": ("cited form", "ʔayʔaǰuθəm", "‘moon’, in the consultant's comment of footnote 13"),
}

# An English possessive, a footnote mark on a word, and the half of a name the text layer broke.
DROP = ("Y’all’s", "xéʔe.7", "kʷ", "aɬtèzetkʷ")

REMOVE_WHERE = r"^Table 1 line (?!1$)\d+$"


def cell(head, form, english):
    where = "Table 1 %s" % head
    return [(where, L, "transcription", form, "page 7, the cell of %s" % head),
            (where, AUTHORS, "translation", english, "page 7")]


TABLE_1 = [("Table 1 heads", AUTHORS, "note", "Evidence Neutral | Previous Direct Evidence | Non-visible",
            "page 7, the column heads. The rows are Proximal and Distal")] + \
    cell("Proximal, Evidence Neutral", "xʔé", "‘this, these’") + \
    cell("Distal, Evidence Neutral", "xéʔe", "‘that, those’") + \
    cell("Previous Direct Evidence", "ɬə́n̓e", "‘this/that (no longer present)’") + \
    cell("Non-visible", "kʷukʷ", "‘this/that (not visible)’")


def chained(anchor, rows):
    """Each row after the one before it, the first after anchor."""
    out = []
    for row in rows:
        out.append((anchor, row))
        anchor = (row[0], row[3])
    return out


SELF = ("ʔes ʔúməcms kʷaɬtèzetkʷ təw ɬe c̓əɬétkʷu wéʔe ncítxʷ ƛ̓uʔ wéʔec ʔex netíyxs scwéw̓xmx ƛ̓uʔ "
        "tékm xéʔe ne nɬeʔképmx e tmíxʷs")

ADD = tuple(
    [(None, ("title", AUTHORS, "title", TITLE, "the paper's title, carrying the star of the acknowledgement footnote")),
     (None, ("title", AUTHORS, "name", "Reed Steiner", "author, University of British Columbia. RS in the discussion of (21c)")),
     (None, ("title", AUTHORS, "name", "Lisa Matthewson", "author, University of British Columbia")),
     (None, ("footnote *", AUTHORS, "name", BEV, "consultant, BP in the tags, from Lytton, speaker of ƛ̓q̓mcín. The primary consultant")),
     (None, ("footnote *", AUTHORS, "name", "Bernice Garcia", "consultant, KBG in the tags, kʷaɬtèzetkʷ, from Coldwater, speaker of nc̕eɬétkʷu")),
     (None, ("footnote *", AUTHORS, "name", "Marty Aspinall", "CMA, c̓úʔsinek, from Coldwater, speaker of scw̓exmxcín, recorded in conversation with kʷaɬtèzetkʷ and not consulted directly")),
     (None, ("footnote *", AUTHORS, "name", "Henry Davis", "thanked for feedback and discussion")),
     (None, ("footnote *", AUTHORS, "name", "Mandy Na’zinek Jimmie", "thanked for feedback and discussion. Jimmie 1994 is cited for the orthography")),
     (None, ("footnote *", AUTHORS, "name", "Ella Hannon", "of the Salish Working Group, thanked for detailed comments on a draft")),
     (None, ("footnote *", AUTHORS, "name", "Marianne Huijsmans", "of the Salish Working Group, thanked. Footnote 13 reports her ʔayʔaǰuθəm data")),
     (None, ("footnote *", AUTHORS, "name", "Bruce Oliver", "of the Salish Working Group, thanked")),
     (None, ("footnote *", AUTHORS, "name", "Salish Working Group", "thanked with the UBC nɬab")),
     (None, ("footnote *", AUTHORS, "name", "Social Sciences and Research Council of Canada", "a funder, as the page names it")),
     (None, ("footnote *", AUTHORS, "name", "Jacobs Research Funds", "a funder")),
     (None, ("footnote *", AUTHORS, "name", "UBC’s Indigenous Strategic Initiatives Fund", "a funder")),
     ("§1.2", ("§1.2", BERNICE, "running speech", SELF, "page 2, footnote 2, kʷaɬtèzetkʷ introducing herself")),
     (("§1.2", SELF), ("§1.2", AUTHORS, "translation", "‘My traditional name is kʷaɬtèzetkʷ, my home is in Coldwater of ‘Nicola’ of nlaka’pamux lands.’", "page 2, footnote 2, the translation printed with it")),
     (None, ("all", AUTHORS, "notation", "SF", "a sentence the authors supplied and the speaker judged")),
     (None, ("all", AUTHORS, "notation", "VF", "a sentence the speaker volunteered")),
     (None, ("all", AUTHORS, "notation", "#", "judged infelicitous in the given context")),
     (None, ("all", AUTHORS, "notation", "*", "judged ill-formed")),
     (None, ("all", AUTHORS, "notation", "?", "an uncertain judgement")),
     (None, ("all", AUTHORS, "notation", "≠", "a reading the sentence does not have, set over the one it has in footnote 4's (i), (41b) and (46)")),
     ("(44)", ("(44)", AUTHORS, "symbol note", "(44)", "page 41, a tree drawn as a picture with no text layer: DP of type e over a null choice function ∅ of type ⟨⟨e,t⟩,e⟩ and a DP of type ⟨e,t⟩, which is D e= DET of type ⟨e,t⟩ and NP smúɬec ‘woman’ of type ⟨e,t⟩")),
     ("(50)", ("(50)", AUTHORS, "symbol note", "(50)", "page 44, a tree drawn as a picture with no text layer: DP of type e over DemP xéʔe DIST of type ⟨⟨e,t⟩,e⟩ and a DP of type ⟨e,t⟩, which is D e= DET and NP smúɬec ‘woman’, each of type ⟨e,t⟩")),
     (None, ("all", AUTHORS, "symbol note", "text layer", "the text layer puts a space at every change of font, n ɬeʔkepmxcín demonstrative s. 256 lines were closed up where the glyph positions show no gap, keeping the column gaps of the examples")),
     ]
    + chained("Table 1 line 1", TABLE_1)
)

INTRO = "a word of kʷaɬtèzetkʷ's self-introduction, footnote 2, page 2"
SET = dict(
    [(("§1.2", one), {"kind": "cited form", "who": L, "gloss": INTRO})
     for one in ("ʔes", "ʔúməcms", "təw", "ɬe", "wéʔe", "ncítxʷ", "ƛ̓uʔ", "wéʔec", "ʔex", "netíyxs",
                 "tékm", "xéʔe", "nɬeʔképmx", "tmíxʷs")]
    + [(("§1.2", "c̓əɬétkʷu"), {"kind": "place", "who": L, "gloss": "Coldwater, in kʷaɬtèzetkʷ's self-introduction, footnote 2"}),
       (("§1.2", "scwéw̓xmx"), {"kind": "place", "who": L, "gloss": "Nicola, in kʷaɬtèzetkʷ's self-introduction, footnote 2"})]
    # The heading of a sub-example carries a footnote mark.
    + [(("(15a) line 1", "Plain DP: 11"), {"form": "Plain DP:", "gloss": "page 12, the heading of (15a), carries footnote 11, written here without its digit"}),
       (("(31a) line 1", "Plain DP:18"), {"form": "Plain DP:", "gloss": "page 26, the heading of (31a), carries footnote 18, written here without its digit"})]
)

WHOSE = (
    "The language is nɬeʔkepmxcín. The examples come from Bev Phillips (BP) and kʷaɬtèzetkʷ Bernice "
    "Garcia (KBG), each tagged SF for a sentence the authors supplied or VF for one the speaker "
    "volunteered, with the date. c̓úʔsinek Marty Aspinall (CMA) is heard in recorded conversation. "
    "(11) is St’át’imcets, Carl Alexander's sentence from Alexander et al. 2025, and footnote 13's "
    "(i) is ʔayʔaǰuθəm, reported by Marianne Huijsmans.\n\n"
    "The who for each tier of an example is the language. The who for a translation is the authors "
    "where the tag carries SF or VF, and the work cited where it names one. A consultant's comment "
    "is that consultant's, and the discussion under (21c) is kʷaɬtèzetkʷ's and the researcher's. The "
    "trees, the table heads and the prose carry the authors."
)

LETTERS = (
    "The paper writes ɬ and sometimes ł for the lateral fricative, kʷaɬtèzetkʷ beside kʷałtèzetkʷ, "
    "and c̓ beside c̕ for the glottalized c, c̓úʔsinek beside c̕úʔsinek. Glottalized resonants take "
    "U+0313 COMBINING COMMA ABOVE, n̓ and w̓."
)

PAGE_NOTES = (
    "The text layer of this paper puts a space at every change of font, inside words: n ɬeʔkepmxcín, "
    "demonstrative s, c̓ úʔsinek, ƛ̓ q̓ mcín. The source here closes each such gap where the glyph "
    "positions, read with pypdfium2, show none, and keeps the widths of the column gaps between the "
    "cells of an example. The trees of (44) and (50) are drawn as pictures and have no text layer; "
    "their content is written out in a symbol note each, read at 110 and 150 dpi."
)
