# Table 16 is a row of its own in the draft, and Tables 1 and 4 lines of their own.
REMOVE = [(one[0], one[3]) for one in DRAFT if one[2] == "note" and one[3].startswith("Table 16: ")]
REMOVE_WHERE = r"^Table [14] line"
# The table cells the engine offered as candidates that no prose sentence prints; the table rows
# read again hold them.
REMOVE += [("§3.2.1", form) for form in ("=Ø", "-əs")]
REMOVE += [("§3.5.1", form) for form in ("-alikʷ", "-áls", "-im̓", "-ʔəm", "-il̓", "-íl", "-əl", "-iʔ", "-agʷil", "-ŋ")]
REMOVE += [("§3.5.2", form) for form in ("-(ə)t", "-dxʷ", "-nəxʷ", "-ləxʷ", "-naxʷ", "-(n)xʷ", "-taxʷ", "-stxʷ",
                                         "-sxʷ", "-ləs", "-əxy", "-əs", "iš", "aš")]
REMOVE += [("§3.5.3", form) for form in ("-ši-t", "-mə-t", "-ŋi-t")]
REMOVE += [("§3.7.3", "hiɬ"), ("§4", "ƛ̓u/ƛ̓a")]

SPLIT = [
    ("front", "Henry Davis University", {"where": "title", "kind": "title", "gloss": "page 1, the title, its star the acknowledgement footnote's"}, {}),
    ("front", "Abstract:", {"where": "title", "gloss": "page 1, the author and his affiliation"}, {"gloss": "page 1, the abstract"}),
    ("§2", "The division in Figure 1", None, {}),
    ("§3.1", "There are four patterns.", None, {}),
    ("§3.2.1", "Four types of clause", None, {}),
    ("§3.2.2", "Here NK is part of a core", None, {}),
    ("§3.3.1", "As can be seen in the table, the distribution", None, {}),
    ("§3.3.2", "Table 6: The general", {}, {}),
    ("§3.3.2", "As the table shows, loss", None, {}),
    ("§3.4.1", "As the table shows, subject-initial", None, {}),
    ("§3.4.1", "Table 8 shows the distribution", {"kind": "heading", "gloss": "page 13, the heading, numbered 3.4.1 on the page"}, {}),
    ("§3.4.1", "Table 8: Distribution", {}, {}),
    ("§3.4.1", "There are three attested patterns", None, {}),
    ("§3.5.1", "All of the intransitive markers", None, {}),
    ("§3.5.2", "Of the five transitivizers", None, {}),
    ("§3.5.3", "Table 11: Applicatives", {}, {}),
    ("§3.5.3", "Looking first at the redirectives", None, {}),
    ("§3.6", "The distribution of Type B is disjoint", None, {}),
    ("§3.7.1", "The pattern shown here", None, {}),
    ("§3.7.2", "As the table shows, this development", None, {}),
    ("§3.7.3", "Table 15: Form of", {}, {}),
    ("§3.7.3", "In the form of the clefting predicate", None, {}),
]

PS = "Proto-Salish"
IS = "Interior Salish"
CS = "Central Salish"
FORMS = {
    "lə=": ("cited affix", LU, "the progressive marker, a proclitic by Beck (2018)"),
    "ʔay": ("cited form", NK, "the imperfective auxiliary"),
    "ʔe:y": ("cited form", UH, "the intransitive aspectual verb Galloway glosses in UH, the source he gives for ʔay"),
    "ʔux̌ʷ": ("cited form", NK, "the prospective auxiliary, from the verb ‘go’, borrowed from LU"),
    "ƛ̓=": ("cited affix", NK, "the oblique proper noun determiner of NK, HL and SQ"),
    "tɬ=": ("cited affix", "Northern Straits Salish", "the oblique determiner, tɬ= ~ ƛ̓="),
    "(ʔ)ə=": ("cited affix", CS, "the general oblique marker of most of CS"),
    "ʔə=": ("cited affix", NK, "oblique ʔə=, of which the NK corpus has no case"),
    "-əls": ("cited affix", NK, "the active intransitive, -als ~ -əls in NK, HL and NSS"),
    "-iyiš": ("cited affix", "Comox", "the autonomous suffix, in footnote 20"),
    "-(ə)m": ("cited affix", CS, "the middle suffix, whose glottalized version the northern languages use for the active intransitive"),
    "ikʷ": ("cited form", LU, "the increment [ikʷ] of the LU active intransitive"),
    "-ílx": ("cited affix", IS, "the autonomous, -ílx ~ -ləx, with a lexical reflexive meaning"),
    "-ləx": ("cited affix", IS, "the autonomous, -ílx ~ -ləx"),
    "-wíl̓x": ("cited affix", IS, "the developmental, a lexically stressed suffix with an inchoative meaning"),
    "gʷ": ("notation", AUTHORS, "the initial [w] ~ [gʷ] of the autonomous suffix in NK and LU"),
    "š": ("notation", AUTHORS, "the segment [š]"),
    "-txʷ": ("cited affix", NK, "the causative"),
    "-stəxʷ": ("cited affix", PS, "the causative *-stəxʷ, whose [s] NK deletes"),
    "-nəs": ("cited affix", PS, "the relic transitivizer *-nəs of the South Georgia languages"),
    "-aš": ("cited affix", PS, "the purposive transitivizer *-aš ~ -iš"),
    "-iš": ("cited affix", PS, "the purposive transitivizer *-aš ~ -iš"),
    "-n(ə)s": ("cited affix", NK, "the relic transitivizer of (20)"),
    "-(a)š": ("cited affix", NK, "the purposive as a less UH-influenced NK would pronounce it, in footnote 23"),
    "-əɬc-t": ("cited affix", HL, "the redirective, an innovation"),
    "-əs-t": ("cited affix", HL, "the redirective, an innovation"),
    "-ʔəm-t": ("cited affix", "Sechelt and Comox", "the redirective of the North Georgia region"),
    "-min̓-t": ("cited affix", SQ, "the relational, marginal in SQ"),
    "min̓-t": ("cited affix", SQ, "three stems with min̓-t in Kuipers (1967:78)"),
    "diɬ": ("cited form", LU, "the clefting predicate"),
    "ƛ̓u": ("cited form", NK, "the clefting predicate and third-person independent pronoun"),
    "ƛ̓a": ("cited form", HL, "the clefting predicate, ƛ̓u after the u → a shift"),
    "niɬ": ("cited form", CS, "the clefting particle of the languages with a reflex of PS *niɬ"),
    "cədiɬ": ("cited form", LU, "the third-person independent pronoun"),
    "cəniɬ": ("cited form", PS, "the third-person independent pronoun, reconstructed by Newman (1977)"),
    "cəníɬ": ("cited form", PS, "the third-person independent pronoun"),
    "wə-": ("cited affix", NK, "the “perfective” prefix of Louisa George's grammar, cognate with LU u-"),
    "-nwéɬən": ("cited affix", TH, "the non-control intransitive morpheme"),
    "-waɬən": ("cited affix", NK, "the first-person plural object marker"),
    "-nwaɬən": ("cited affix", PS, "*-nwaɬən, which became an object marker in NK"),
    "ƛ̓uʔ": ("cited form", TH, "the exclusive particle, ‘so, just, but, only’"),
    "cukʷ": ("cited form", TH, "‘finish’"),
    "/k̓y/": ("notation", AUTHORS, "a palatalized ejective velar of mainland HL"),
    "/č/": ("notation", AUTHORS, "the palato-alveolar affricate"),
    "/č̓/": ("notation", AUTHORS, "the ejective palato-alveolar affricate"),
    "/š/": ("notation", AUTHORS, "the palato-alveolar fricative"),
    "ʔiɬ": ("cited form", NK, "the auxiliary"),
    "ʔeyɬ": ("cited form", TH, "the adverb ‘now, next, then’"),
    "ʔayɬ": ("cited form", LI, "the adverb ‘now, next, then’"),
    "ʔi": ("cited form", UH, "the common auxiliary Galloway derives ʔiɬ from"),
    "uɬ": ("cited affix", UH, "the past tense enclitic =uɬ"),
    "Sḵwxwúmesh": ("language", AUTHORS, "Squamish, in the title of Bar-el 2005"),
    "Xweysás": ("cited form", LI, "in the title of the volume Davis 2012 stands in, Wa7 Xweysás i Nqwal’úttensa i Ucwalmícwa"),
    "Nqwal’úttensa": ("cited form", LI, "in the title Wa7 Xweysás i Nqwal’úttensa i Ucwalmícwa"),
    "Ucwalmícwa": ("cited form", LI, "in the title Wa7 Xweysás i Nqwal’úttensa i Ucwalmícwa"),
    "St’át’imc": ("language", AUTHORS, "in a reference title"),
    "Nqwal’uttenlhkálha": ("cited form", LI, "the title of the Upper St’át’imcets dictionary in preparation"),
    "St’át’imcets": ("language", AUTHORS, "St’át’imcets, Lillooet"),
    "ʔayʔaǰuθəm": ("language", AUTHORS, "Comox-Sliammon, in the title of Davis and Huijsmans 2017"),
    "Nɬeʔkepmxcín": ("language", AUTHORS, "Thompson River Salish, in the title of Koch and Zimmermann 2009"),
    "nɬeʔkepmxcín": ("language", AUTHORS, "Thompson River Salish"),
    "SENĆOŦEN": ("language", AUTHORS, "Saanich, in the title of Montler 2018"),
    "Honoré": ("name", AUTHORS, "Honoré Watanabe, in a reference entry"),
}
# The engine's candidates with a parenthesis or a footnote digit run on, or a slash left over from
# the phonemes of footnote 33, each held again by a row of its own.
DROP = ("(ʔay).6", "ʔay).7", "/ʔeyɬ", ">*čshift", "k̓/")

SET = {
    ("front", "Halq’eméylem/Upriver"): {"form": "Halq’eméylem", "kind": "language", "who": AUTHORS, "gloss": "page 1, Upriver Halkomelem"},
    ("§1", "Halq’eméylem/Upriver"): {"form": "Halq’eméylem", "kind": "language", "who": AUTHORS, "gloss": "page 2, Upriver Halkomelem"},
    ("front", "nɬeʔkepmxcín/Thompson"): {"form": "nɬeʔkepmxcín", "kind": "language", "who": AUTHORS, "gloss": "page 1, Thompson River Salish"},
    ("§5", "nɬeʔkepmxcín/Thompson"): {"form": "nɬeʔkepmxcín", "kind": "language", "who": AUTHORS, "gloss": "page 22, Thompson River Salish"},
    ("§3", "ʔayʔaǰuθəm/Comox-Sliammon"): {"form": "ʔayʔaǰuθəm", "kind": "language", "who": AUTHORS, "gloss": "page 4, Comox-Sliammon, in footnote 4"},
    ("§3.3.1", "SENĆOTEN/Saanich"): {"form": "SENĆOTEN", "kind": "language", "who": AUTHORS, "gloss": "page 11, Saanich"},
    ("§3.3.2", "St’át’imcets/Lillooet"): {"form": "St’át’imcets", "kind": "language", "who": AUTHORS, "gloss": "page 11, Lillooet, in footnote 17"},
    ("§3.5.2", "-nəs"): {"form": "*-nəs"},
    ("§3.5.2", "-stəxʷ"): {"form": "*-stəxʷ"},
}

# The examples of other languages, named with their sources on the page.
WHO = {"TH: Koch & Zimmermann 2009:242": TH, "TH: Thompson & Thompson 1996:7": TH}
WHO_RULES = []
for _example, _language, _source in (("(4)", UH, "Galloway's UH, which the text names"),
                                     ("(8a)", UH, "UH, from Galloway 1993a:187"), ("(8b)", UH, "UH, from Galloway 1993a:187"),
                                     ("(9a)", SQ, "SQ, from Kuipers 1967:89"), ("(9b)", SQ, "SQ, from Kuipers 1967:89"),
                                     ("(29)", TH, "TH, from Koch & Zimmermann 2009:242"),
                                     ("(30)", LI, "LI, from the consultant CA"), ("(33)", TH, "TH, from Thompson & Thompson 1996:7"),
                                     ("(34)", LI, "LI, from Davis et al. in prep.")):
    for _kind in ("transcription", "segmentation", "gloss"):
        WHO_RULES.append((r"^%s line" % re.escape(_example), _kind, r".", _language, _source))

ADD = tuple(
    [(("title", "Henry Davis University..."), ("title", AUTHORS, "name", "Henry Davis", "author, University of British Columbia"))]
    + chained(("§2", "In Salish historical linguistics..."), FIGURE)
    + chained(("§3.1", "C1 reduplication..."), TABLE_1)
    + chained(("§3.2.1", "Table 2 gives the distribution..."), TABLE_2)
    + chained(("§3.2.2", "The distribution of the *3 > 2 ban..."), TABLE_3)
    + chained(("§3.2.3", "The distribution of promotional and non-promotional passive..."), TABLE_4)
    + chained(("§3.3.1", "Table 5 shows the distribution..."), TABLE_5)
    + chained(("§3.3.2", "The form and distribution of the general oblique marker..."), TABLE_6)
    + chained(("§3.4.1", "The distribution of pre-predicative subjects..."), TABLE_7)
    + chained(("§3.4.1", "Table 8 shows the distribution..."), TABLE_8)
    + chained(("§3.5.1", "Table 9 compares intransitive markers..."), TABLE_9)
    + chained(("§3.5.2", "Table 10 shows the basic..."), TABLE_10)
    + chained(("§3.5.3", "There are three main applicative..."), TABLE_11)
    + chained(("§3.6", "The distribution of these patterns..."), TABLE_12)
    + chained(("§3.7.1", "The distribution of these systems..."), TABLE_13)
    + chained(("§3.7.2", "The distribution of this pattern..."), TABLE_14)
    + chained(("§3.7.3", "Table 15 shows the form..."), TABLE_15)
    + chained(("§4", "Table 16 summarizes..."), TABLE_16)
    + [(None, ("all", AUTHORS, "notation", "*", "a reconstructed form, or with a person sequence an ungrammatical one"))]
)

