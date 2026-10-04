# Context for Lyon, Zero Prospectives, Zero Modals, and Modo-Temporal Interactions in nsyilxcn.
# Pages 1 and 4 read against the text layer; the rest reconstructed by the engine.

AUTHORS = "John Lyon"
L = "nsyilxcn"

TITLE = "Zero Prospectives, Zero Modals, and Modo-Temporal Interactions in nsyilxcn (Okanagan Salish)"
BYLINE = "John Lyon, University of British Columbia - Okanagan"

# The initials the speaker comments open with, expanded as the paper's tags write the names.
INITIALS = {
    "DD": "Delphine Derickson-Armstrong",
    "DM": "Dave Michele",
    "SM": "Sarah McLeod",
    "JL": "John Lyon",
    "Dave Michel": "Dave Michele",
}

FORMS = {
    "nɬeʔkepmxcín": ("language", AUTHORS, "Thompson River Salish, cited from Hannon 2025"),
    "ɬeʔkepmxcín": ("language", AUTHORS, "Thompson River Salish, as the text layer breaks it"),
    "nɬeʔkepmxc": ("language", AUTHORS, "Thompson River Salish, broken by the text layer before ín"),
    "St’át’imcets": ("language", AUTHORS, "cited from Rullmann & Matthewson 2018"),
    "Skwxwú7mesh": ("language", AUTHORS, "in the reference list"),
    "nxaʔamxčín": ("language", AUTHORS, "Moses-Columbian, spoken to the south of nsyilxcn, with a past tense enclitic =ay̓"),
    "Séna7": ("note", AUTHORS, "a St’át’imcets word in the title of a reference, Precedings of the 51st ICSNL"),
    "Spax̌mən": ("place", AUTHORS, "the chief of Spax̌mən, in an example translation"),
    "spáx̌mən": ("place", L, "in an example, with k̓l ‘to’"),
    "St’at’imcets": ("language", AUTHORS, "cited from Rullmann & Matthewson 2018, printed here without its acute"),
    "En’owkin": ("name", AUTHORS, "the En’owkin Centre in Penticton, BC, a supporter of the work"),
}

# English: innermost, before ‘inclusion aspect’.
DROP = ("innermost",)

ADD = (
    # The place of (18b), in its tiers and in its translation.
    ("(18b) line 6", ("(18b) line 6", L, "place", "spáx̌mən", "page 10, in the tiers of (18b), with k̓l ‘to’, glossed Douglas.Lake")),
    ("(18b) line 6", ("(18b) line 6", AUTHORS, "place", "Spax̌mən", "page 10, the chief of Spax̌mən, in the translation of (18b)")),
    (None, ("title", AUTHORS, "title", TITLE, "the paper's title, set in bold, carrying the star of the acknowledgement footnote")),
    (None, ("title", AUTHORS, "name", "John Lyon", "the author, University of British Columbia - Okanagan. JL in (ii)")),
    (None, ("footnote *", AUTHORS, "name", "Delphine Derickson-Armstrong", "of Westbank reserve, thanked first. DD in the speaker comments")),
    (None, ("footnote *", AUTHORS, "name", "Dave Michele", "of Westbank reserve. DM in the speaker comments, and Dave Michel in the comment of (3a)")),
    (None, ("footnote *", AUTHORS, "name", "Lottie Lindley", "Upper Nicola Elder, twi-Lottie Lindley in footnote *")),
    (None, ("footnote *", AUTHORS, "name", "Sarah McLeod", "Upper Nicola Elder. SM in §3")),
    (None, ("footnote *", L, "cited form", "Limlmt", "thanks, opening the last sentence of footnote *")),
    (None, ("footnote *", AUTHORS, "name", "twi-Lottie Lindley", "as footnote * writes her name, with twi- in italics")),
    (None, ("footnote *", AUTHORS, "place", "Westbank reserve", "home of Delphine Derickson-Armstrong and Dave Michele")),
    (None, ("footnote *", AUTHORS, "place", "Upper Nicola", "home of Lottie Lindley and Sarah McLeod")),
    (None, ("footnote *", AUTHORS, "name", "En’owkin Centre", "in Penticton, BC, a supporter of the work")),
    (None, ("§1", AUTHORS, "language", "nsyilxcn", "the language of the paper, a.k.a. Okanagan, Southern Interior Salish. The paper writes it without a schwa")),
    (None, ("§1", AUTHORS, "language", "Okanagan", "another name for nsyilxcn")),
    (None, ("abstract", AUTHORS, "language", "Gitksan", "cited from Matthewson 2013 for a zero circumstantial modal")),
    (None, ("all", AUTHORS, "notation", "gloss labels", "the page draws the gloss labels as small capitals, and the text layer holds them as ASCII capitals, N.PROS-PFV. The table keeps the text layer")),
    (None, ("all", AUTHORS, "notation", "bold", "the page sets the words most important to the discussion in bold, as footnote 2 says, and the table does not mark it")),
    (None, ("all", AUTHORS, "notation", "(3a)", "the comment is signed Dave Michel, and the tag on the same example and footnote * write Dave Michele")),
    (None, ("all", AUTHORS, "notation", "(ii)", "the example is numbered (ii) and set between (102d) and (102e) on page 44")),
    (None, ("all", AUTHORS, "symbol note", "∅", "the null morpheme is U+2205 EMPTY SET in the text layer, drawn as a slashed zero on the page")),
)

# (18a) wraps its segmentation and gloss: k̓l Vancouver. goes on the segmentation, to Vancouver on
# the gloss.
SET = {
    ("(18a) line 4", "k̓l Vancouver."): {"kind": "segmentation"},
    ("(18a) line 5", "to Vancouver"): {"kind": "gloss"},
}
REPLACE = ()

TITLE_ROW = TITLE

WHOSE = (
    "The language is nsyilxcn, Okanagan, and the examples were elicited from four speakers, each "
    "named in the tag at the right of the translation: Delphine Derickson-Armstrong and Dave "
    "Michele of Westbank reserve, and the Upper Nicola Elders Lottie Lindley and Sarah McLeod, whom "
    "footnote * thanks with the word Limlmt. VF in a tag marks a form the speaker volunteered, CF a "
    "form the author constructed, and VG a gloss or translation the speaker volunteered. The speaker "
    "comments after an example are theirs, signed with their initials DD, DM and SM, and JL is the "
    "author.\n\n"
    "The who for each tier of an example is nsyilxcn. The who for a translation tagged VG is the "
    "speaker named in the tag, and John Lyon otherwise. The who for a speaker comment is the speaker "
    "who made it, or the speaker named in the example's tag where the comment is unsigned. The "
    "prose, the contexts and the notes carry John Lyon."
)

LETTERS = (
    "The null morpheme ∅ is U+2205. Glottalization is U+0313 on the letter, and the caron of x̌ is "
    "U+030C. The raised w of kʷ is U+02B7. The gloss labels are ASCII capitals in the text layer."
)

PAGE_NOTES = (
    "Pages 1 and 4 were read against the text layer and agree with it letter for letter. The rest of "
    "the paper was reconstructed by the engine and checked by the residue check alone."
)
