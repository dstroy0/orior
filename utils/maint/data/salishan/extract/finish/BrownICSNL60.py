# Context for Brown, Clause typing and clitic linearization in Gitksan.
# The page text was read by glyph rows. The displays, the tree and bracket schemas and Table 1,
# are read off it below by the words each line opens with.
import os

from workdir import PRIVATE  # noqa: E402

AUTHORS = "Colin Brown"
L = "Gitksan"

TITLE = "Clause typing and clitic linearization in Gitksan"
BYLINE = "Colin Brown, University of British Columbia"

with open(os.path.join(PRIVATE, "pagetext",
                       "BrownICSNL60.txt"), encoding="utf-8") as handle:
    PAGE = [" ".join(line.split()) for line in handle.read().split("\n")]


def line(opening, after=0):
    """The page line opening on these words, the first after line number after."""
    for number, text in enumerate(PAGE):
        if number >= after and text.startswith(opening):
            return text
    raise SystemExit("no page line opens with %s" % opening)


def display(anchor, example, page, rows):
    """ADD entries for one display, each row placed after the one before it. rows holds
    (where suffix, kind, form, gloss) and the first row goes after anchor."""
    added = []
    for suffix, kind, form, gloss in rows:
        letter, number = suffix.split("line")
        where = "(%s%s) line%s" % (example, letter.strip(), number)
        added.append((anchor, (where, AUTHORS, kind, form, "page %d, %s" % (page, gloss))))
        anchor = (where, form)
    return added


def tree(anchor, example, page, caption, nodes):
    """A tree the page draws, its caption and then one row for each depth of its nodes, read
    left to right."""
    rows = [(" line 1", "note", caption, "the caption")]
    for depth, nodes_at in enumerate(nodes, 2):
        rows.append((" line %d" % depth, "notation", nodes_at,
                     "a tree, the nodes at depth %d read left to right" % (depth - 1)))
    return display(anchor, example, page, rows)


START_35 = PAGE.index(line("(35) Hypothetical"))
START_44 = PAGE.index(line("(44) Position of dim"))
START_47 = PAGE.index(line("(47) Structure provided"))
START_48 = PAGE.index(line("(48) Postsyntactic"))
START_50 = PAGE.index(line("(50) Structure provided"))
START_51 = PAGE.index(line("(51) Postsyntactic"))
START_52 = PAGE.index(line("(52) Yukw selects"))
STORY_59 = line("Ii hes") + " " + line("k̲’ap ’niiwin")

FORMS = {
    "k’ay": ("cited form", L, "‘still, just’, a predicative particle in Rigsby (1986:273), with hlaa ‘now, inceptive aspect’"),
    "ʔayʔajuθəm": ("language", AUTHORS, "in the title of Huijsmans 2023, (Comox-Sliammon); the text layer holds it as PayPajuθəm, the TIPA glottal stop on the code of P"),
}

# English notation, Ā for A-bar, in footnote 3 and the Davis and Brown 2011 title.
DROP = ("Ā-constructions", "Ā-dependencies")

SPLIT = (
    # The front matter runs the title, the byline and the abstract together.
    ("front", "Colin Brown University", None, {}),
    ("front", "Abstract:", None, {}),
    # Footnote * runs on into footnote 1, the gloss abbreviations, and the volume at the foot.
    ("footnote *", "1 1 = first person", {}, {"where": "footnote 1"}),
    ("footnote 1", "In Proceedings of the International", {},
     {"where": "front", "gloss": "page 1, the volume, at the foot of the first page"}),
    ("§1", "In the discussion that follows", {"gloss": "page 2, the paradox, three numbered lines"}, {}),
    ("§1", "The second process involves", None, {}),
    ("§2.1", "(9) Independent clause", {}, {}),
    ("§2.1", "There are multiple ways", None, {"gloss": "page 5"}),
    ("§2.1", "Type Examples", {}, None),
    ("§2.2", "Rigsby (1986) and Hunt (1993) show", None, {}),
    ("(35) line 1", "If (35) is correct", None, {"where": "§3.1", "gloss": "page 11"}),
    ("§3.2", "If dim’s base position", None, {}),
    ("§3.2", "I assume that the complementizer", None, {}),
    ("§3.2", "This process of dim displacement", None, {}),
    ("(59) line 1", "Ii hes", {}, None),
)

REMOVE = (
    ("§1", "(4) Two kinds of dependent marker..."),
    ("§1", "[DEP1 [DepP …]] → biclausal..."),
    ("§2.2", "(11) Two kinds of dependent marker:"),
    ("§2.2", "(21) Cyclic movement..."),
    ("§2.2", "(22) Cyclic movement..."),
    ("(44)", "(44)"),
    ("§3.2", "… AspP dim= vP /wis/ ‘rain’"),
)

# The displays the engine read as tiers, read again whole below.
REMOVE_WHERE = r"^\((5a|5b|23|24|25|28|28b|43|43b|43c|47|48|50|51|52|61)\)"

WHO_RULES = (
    # A story from the collection in preparation, told by the speaker its tag names.
    (r"^(\((42|55|59)\)|footnote 15 \(ii\)) line", "translation", ".", "Forbes et al. in prep.",
     "from a story in Forbes et al. in prep., told by the speaker the tag names"),
    (r"^\(16\) line", "translation", ".", "Hunt 1993:148",
     "the example is Hunt's, 1993:148, with its Nisga’a counterpart in Tarpent 1987:219"),
    (r"^\(36a\) line", "translation", ".", "Matthewson et al. 2022:31", "the pair (36a) and (36b) is theirs"),
    (r"^\(37a\) line", "translation", ".", "Hunt 1993:147", "the pair (37a) and (37b) is Hunt's"),
    (r"^\(63\) line", "translation", ".", "Rigsby 1986:276", "the bracketing of the segmentation is Brown's"),
)

SET = {
    ("(59) line 2", "k̲’ap ’niiwin jog̲o’y. g̲o’osun.”"): {
        "form": STORY_59,
        "gloss": "page 18, the sentence of the story, set over two lines"},
    ("(60) line 5", "Comment: Too many dims!"): {"who": "Vincent Gogag"},
}

ADD = tuple(
    [(None, ("title", AUTHORS, "title", TITLE, "the paper's title, carrying the star of the acknowledgement footnote")),
     (None, ("title", AUTHORS, "name", "Colin Brown", "the author, University of British Columbia")),
     (None, ("footnote *", AUTHORS, "name", "Barbara Sennott", "a consultant, BS in the tags")),
     (None, ("footnote *", AUTHORS, "name", "Jeanne Harris", "a consultant")),
     (None, ("footnote *", AUTHORS, "name", "Vincent Gogag", "a consultant, VG in the tags")),
     (None, ("footnote *", AUTHORS, "name", "Hector Hill", "a consultant, HH in the tags")),
     (None, ("footnote *", AUTHORS, "name", "Michael Schwan", "thanked for feedback; Michael David Schwan in the references")),
     (None, ("footnote *", AUTHORS, "name", "Clarissa Forbes", "thanked for feedback")),
     (None, ("footnote *", AUTHORS, "name", "Henry Davis", "thanked for feedback")),
     (None, ("footnote *", AUTHORS, "name", "UBC Gitksan Lab", "thanked for feedback")),
     (None, ("§1", AUTHORS, "language", "Gitksan", "the language of the paper, ISO 639-3 git, Interior Tsimshianic, British Columbia")),
     (None, ("§1", AUTHORS, "language", "Tsimshianic", "the family Gitksan belongs to")),
     (None, ("§1", AUTHORS, "place", "British Columbia", "where Gitksan is spoken")),
     (None, ("(16)", AUTHORS, "language", "Nisga’a", "whose counterpart of (16) is cited from Tarpent 1987:219")),
     (None, ("references", AUTHORS, "language", "Sm’algyax", "in the titles of Brown 2024 and of Brown and Davis 2024a and Davis and Brown 2024")),
     (None, ("references", AUTHORS, "language", "Smalgyax", "Coast Tsimshian, in the title of Brown and Davis 2024b")),
     (None, ("(42)", AUTHORS, "cited title", "Before the people die", "the story (42) comes from, told by HH, in Forbes et al. in prep.")),
     (None, ("footnote 15 (ii)", AUTHORS, "cited title", "Siipxum Hloxs", "the story (ii) comes from, told by BS, in Forbes et al. in prep.")),
     (None, ("(55)", AUTHORS, "cited title", "Raven’s Nest", "the story (55) comes from, told by VG, in Forbes et al. in prep.")),
     (None, ("(59)", AUTHORS, "cited title", "Frog Phratry", "the story (59) comes from, told by VG, in Forbes et al. in prep.")),
     (None, ("(59)", AUTHORS, "name", "’Niigyemks", "a person in the story of (59), glossed as a proper noun")),
     (None, ("all", AUTHORS, "notation", "@", "the transitive vowel and the schwa of the extraction suffix -@t are written @ in the segmentations, t’is-@-’y")),
     (None, ("all", AUTHORS, "notation", "bold", "the page sets the extraction morphology of (21) and (22) in bold, and the table does not mark it")),
     (None, ("all", AUTHORS, "notation", "gloss labels", "the page draws the gloss labels as small capitals, and the text layer holds them as ASCII capitals, PROSP. The table keeps the text layer")),
     ]
    + display(("§1", "In the discussion that follows..."), 4, 2, [
        (" line 1", "note", line("(4) Two kinds")[4:], "the caption"),
        ("a line 1", "note", line("a. Type 1 (e.g. yukw)")[3:], "the first kind"),
        ("a line 2", "notation", line("[DEP1 [DepP"), "the bracketing of the first kind"),
        ("b line 1", "note", line("b. Type 2 (e.g. wil)")[3:], "the second kind"),
        ("b line 2", "notation", line("[DepP DEP2"), "the bracketing of the second kind"),
    ])
    + display(("footnote 2", "2 E.g., head nouns..."), 5, 3, [
        ("a line 1", "note", line("(5) a. CL movement")[7:], "the first ingredient"),
        ("a line 2", "notation", line("[CP CL α"), "the bracketing"),
        ("b line 1", "note", line("b. CL movement cannot")[3:], "the second ingredient"),
        ("b line 2", "notation", line("[YP CL α"), "the bracketing, printed with one bracket unclosed"),
    ])
    + display(("§2.1", "The morphological reflexes..."), 9, 5, [
        (" line 1", "note", line("(9) Independent clause")[4:], "the caption"),
        ("a line 1", "notation", line("a. Intransitive: V [prefixes-Root-suffixes]")[3:], "the template"),
        ("b line 1", "notation", line("b. Transitive: V [prefixes")[3:], "the template"),
    ])
    + display(("(9b) line 1", line("b. Transitive: V [prefixes")[3:]), 10, 5, [
        (" line 1", "note", line("(10) Dependent clause")[5:], "the caption"),
        ("a line 1", "notation", line("a. Intransitive: V [prefixes-Root-suffixes]-Agr")[3:], "the template"),
        ("b line 1", "notation", line("b. Transitive: Agr.IA")[3:], "the template"),
    ])
    + [(("§2.1", "There are multiple ways..."), ("Table 1", AUTHORS, "note", row,
                                                   "page 5, a row of Table 1, its type and then its examples"))
       for row in reversed([line("Type Examples"), line("Clausal subordination"), line("Aspectual markers"),
                            line("Other nee"), line("Syntactically determined")])]
    + display(("§2.2", "This section examines four..."), 11, 5, [
        (" line 1", "note", line("(11) Two kinds")[5:], "the caption"),
        ("a line 1", "notation", line("a. Type 1 selects")[3:], "the first kind, with its bracketing"),
        ("b line 1", "notation", line("b. Type 2 restricted to occurring within dependent clause: [Dep")[3:],
         "the second kind, with its bracketing"),
    ])
    + display(("§2.2", "These data support the analysis..."), 21, 8, [
        (" line 1", "note", line("(21) Cyclic movement")[5:], "the caption"),
        (" line 2", "notation", line("gu =hl aamit"),
         "the sentence of (18) with the copies of gu the movement leaves in angle brackets and the extraction morphology in bold"),
    ])
    + display(("(21) line 2", line("gu =hl aamit")), 22, 8, [
        (" line 1", "note", line("(22) Cyclic movement")[5:], "the caption"),
        (" line 2", "notation", line("gu =hl yugwit"),
         "the sentence of (19) with the copies of gu the movement leaves in angle brackets and the extraction morphology in bold"),
    ])
    + display(("§3", "This section introduces a class..."), 23, 8, [
        (" line 1", "note", line("(23) CL movement")[5:], "the caption"),
        (" line 2", "notation", line("[CP CL α", PAGE.index(line("(23) CL movement"))), "the bracketing"),
    ])
    + display(("§3", "Interestingly, these clitics..."), 24, 9, [
        (" line 1", "note", line("(24) CL movement")[5:], "the caption"),
        (" line 2", "notation", line("[DEP1 [CP CL"), "the bracketing"),
    ])
    + display(("(24) line 2", line("[DEP1 [CP CL")), 25, 9, [
        (" line 1", "note", line("(25) CL movement")[5:], "the caption"),
        (" line 2", "notation", line("[CP CL DEP2"), "the bracketing"),
    ])
    + display(("§3.1", "Prior work on Gitksan shows..."), 28, 9, [
        ("a line 1", "notation", line("(28) a. dim >")[8:], "the first ordering"),
        ("b line 1", "notation", line("b. {wil, ii} > {yukw")[3:], "the second ordering"),
    ])
    + tree(("§3.1", "These data, taken in isolation..."), 35, 11, line("(35) Hypothetical")[5:],
           PAGE[START_35 + 1:START_35 + 8])
    + display(("§3.1", "The linearization paradoxes that arise..."), 43, 12, [
        ("a line 1", "notation", "dim > {wil, ii}", "the first ordering"),
        ("a line 2", "note", "See examples (29), (30)", "the examples of the first ordering"),
        ("b line 1", "notation", "{wil, ii} > {yukw, nee}", "the second ordering"),
        ("b line 2", "note", "See examples (31), (32), (33), (34)", "the examples of the second ordering"),
        ("c line 1", "notation", "{yukw, nee} > dim", "the third ordering"),
        ("c line 2", "note", "See examples (36), (37), (38), (39), (40), (41), (42)", "the examples of the third ordering"),
    ])
    + tree(("§3.2", "Matthewson et al. (2022) argues..."), 44, 13, line("(44) Position of dim")[5:],
           PAGE[START_44 + 1:START_44 + 5])
    + display(("§3.2", "I model this displacement..."), 46, 14, [
        (" line 1", "note", "Lexical entry for dim:", "the caption"),
        (" line 2", "notation", line("(46) Lexical entry")[len("(46) Lexical entry for dim: "):],
         "the vocabulary item: the feature PROSP, the arrow and the host it is proclitic to"),
        (" line 3", "notation", "/dim/", "the phonological form, set over the arrow"),
    ])
    + tree(("§3.2", "The derivation for dim linearization..."), 47, 14, line("(47) Structure")[5:],
           PAGE[START_47 + 1:START_47 + 7])
    + tree(("footnote 16", "16 While it may seem..."), 48, 15, line("(48) Postsyntactic")[5:],
           PAGE[START_48 + 1:START_48 + 6])
    + tree(("(49) line 4", "(BS)"), 50, 15, line("(50) Structure")[5:],
           PAGE[START_50 + 1:START_50 + 7])
    + tree(("(50) line 7", "‘rain’"), 51, 16, line("(51) Postsyntactic")[5:],
           PAGE[START_51 + 1:START_51 + 6])
    + tree(("(51) line 6", "‘rain’"), 52, 16, line("(52) Yukw selects")[5:],
           PAGE[START_52 + 1:START_52 + 5])
    + display(("§4", "The restricted distribution of doubling..."), 61, 18, [
        (" line 1", "note", line("(61) Dim/ji")[5:], "the caption"),
        (" line 2", "notation", line("[CP ji/dim"), "the bracketing"),
    ])
)

TITLE_ROW = TITLE

WHOSE = (
    "The language is Gitksan, Interior Tsimshianic, and the examples were elicited from four "
    "consultants, named in footnote * and tagged by initials at the right of a translation: "
    "Barbara Sennott (BS), Jeanne Harris, Vincent Gogag (VG) and Hector Hill (HH). Other examples "
    "are cited from Hunt 1993, Forbes 2018, Brown et al. 2020, Matthewson 2024, Matthewson et al. "
    "2022 and Rigsby 1986, and four come from stories in Forbes et al. in prep., each tagged with "
    "its teller and title.\n\n"
    "The who for each tier of an example is Gitksan. The who for a translation is the source its "
    "tag cites, Forbes et al. in prep. for a story, and Colin Brown for an example tagged with a "
    "consultant's initials. The comment under (60) is Vincent Gogag's. The prose, the headings, the "
    "displays and the notes carry Colin Brown."
)

LETTERS = (
    "Gitksan is written in its practical orthography, with ’ for glottalization and the underlined "
    "g̲ and k̲ of the story in (59), U+0332, beside the g̱ of (60), U+0331. The segmentations write "
    "the transitive vowel as @. The gloss labels are ASCII capitals in the text layer."
)

PAGE_NOTES = (
    "The page text was read by glyph rows, a word space over 0.133 em, and a smaller gap where a "
    "roman word meets an italic one. The trees of (35), (44), (47), (48), (50), (51) and (52) are "
    "given one row for each depth of nodes. The reference to Huijsmans 2023 prints ʔayʔajuθəm, "
    "which the text layer holds as PayPajuθəm, read at 600 dpi."
)
