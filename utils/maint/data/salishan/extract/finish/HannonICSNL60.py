# Context for Hannon, Prospective aspect in nɬeʔkepmxcín. Every example is nɬeʔkepmxcín, elicited
# from the three speakers footnote * names and tagged (XX | YY | ZZ): the speaker's initials, the
# dialect and whether the form was suggested (SF) or volunteered (VF). The page text was read by
# glyph rows. The rows below are keyed on the draft's own forms, read off DRAFT.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import WORK  # noqa: E402
import finish  # noqa: E402

STEM = "HannonICSNL60"
AUTHORS = "Ella Hannon"
N = "nɬeʔkepmxcín"

TITLE = "Prospective aspect in nɬeʔkepmxcín"
BYLINE = "Ella Hannon, University of British Columbia"

with open(os.path.join(WORK, STEM + ".draft.tsv"), encoding="utf-8") as handle:
    DRAFT = [one.rstrip("\n").split("\t") for one in handle][1:]


def draft_form(where, opening):
    """The form of the draft row at where opening on these words."""
    for one in DRAFT:
        if one[0] == where and one[3].startswith(opening):
            return one[3]
    raise SystemExit("no draft row %s opens with %s" % (where, opening))


SPEAKERS = {"BP": "Bev Phillips", "CMA": "Marty Aspinall", "KBG": "Bernice Garcia"}


def speakers_of(tag):
    """The speakers a tag names, (BP, KBG | Ly, NV | SF) for Bev Phillips and Bernice Garcia."""
    names = [SPEAKERS[one.strip()] for one in tag.strip("() ").split("|")[0].split(",")]
    return names[0] if len(names) == 1 else ", ".join(names[:-1]) + " and " + names[-1]


# The tag of each example, from its translation or the citation row under a comment.
TAGS = {}
for _where, _who, _kind, _form, _gloss in DRAFT:
    _example = re.match(r"^(\(\w+\))", _where)
    _tag = re.search(r"\((?:BP|CMA|KBG)[^()]*\)$", _form)
    if _example and _tag and _kind in ("translation", "citation"):
        TAGS[_example.group(1)] = _tag.group(0)

# A translation the speakers accepted is theirs, and the English of a sentence they rejected,
# Intended: ‘I will enjoy it.’, is the author's. A speaker's comment is the tagged speaker's.
SET = {}
for _where, _who, _kind, _form, _gloss in DRAFT:
    _example = re.match(r"^(\(\w+\))", _where)
    if not _example or _example.group(1) not in TAGS:
        continue
    if _kind == "translation" and not _form.startswith("Intended:"):
        SET[(_where, finish.TRAILER.match(_form).group(1))] = {"who": speakers_of(TAGS[_example.group(1)])}
    if _kind == "speaker comment":
        SET[(_where, _form)] = {"who": speakers_of(TAGS[_example.group(1)])}

INTRODUCTION = draft_form("§1.1", "This paper focuses")
INTRODUCTION = INTRODUCTION[INTRODUCTION.index("ʔes ʔúmәcms"):INTRODUCTION.index(" ‘My traditional")]
OVERVIEW = draft_form("§1.1", "This paper focuses")
OVERVIEW = OVERVIEW[:OVERVIEW.index("* ném")].strip()
OVERVIEW_END = draft_form("§1.1", "main clauses, if the context")

FORMS = {
    N: ("language", AUTHORS, "a.k.a. Thompson River Salish, ISO 639-3 thp, northern Interior Salish, the language of the paper"),
    "nłeʔkepmxcín": ("language", AUTHORS, "page 2, footnote 1, nɬeʔkepmxcín as the page spells it there, with ł for ɬ"),
    "St’at’imcets": ("language", AUTHORS, "St’át’imcets, a.k.a. Lillooet, northern Interior Salish, spelled here without its accent"),
    "St’át’imcets": ("language", AUTHORS, "a.k.a. Lillooet, northern Interior Salish, in the title of Glougie 2007"),
    "scw̓éxmx": ("language", AUTHORS, "the Nicola Valley dialect of nɬeʔkepmxcín, NV in the tags, footnote *"),
    "ƛ̓q̓mcín": ("language", AUTHORS, "the Lytton dialect of nɬeʔkepmxcín, Ly in the tags, footnote *"),
    "nlaka’pamux": ("name", "Bernice Garcia", "the Nlaka’pamux people, whose lands the English of Bernice Garcia's introduction names, footnote *"),
    "kʷaɬtèzetkʷuʔ": ("name", AUTHORS, "the nɬeʔkepmxcín name of Bernice Garcia, KBG, footnote *"),
    "c̓úʔsinek": ("name", AUTHORS, "the nɬeʔkepmxcín name of Marty Aspinall, CMA, footnote *"),
    "ném": ("cited form", N, "‘very’, in ném kʷukʷstéyp, the thanks of footnote *"),
    "kʷukʷstéyp": ("cited form", N, "‘thank you’, the thanks of footnote *"),
    "Todoróvić": ("name", AUTHORS, "Neda Todoróvić, an author of Matthewson et al. 2022"),
    "Jóhannsdóttir": ("name", AUTHORS, "K. Jóhannsdóttir, an editor of the volume Glougie 2007 appears in"),
    "Cécile": ("name", AUTHORS, "Cécile Meier, an editor in the entry of Hacquard 2014"),
    "x̣ә́pe": ("cited form", N, "page 14, footnote 6, ‘wonder’, the predicate of ʔe stx̣ә́pe neʔ in Thompson & Thompson 1996:423"),
}

# The pieces of the formulas of (49), rows of their own below, and an em-dash the text layer runs
# onto xʷúy̓.
DROP = ("λQ", "λt", "λe", "λw.∀wʹ", "wʹ", "Q(t)(e)(wʹ", "—xʷúy̓", "néʔ(e)", "téʔ(e)", "xéʔ(e)")

SPLIT = (
    # The front matter runs the title, the byline and the abstract together.
    ("front", "Ella Hannon University", None, {}),
    ("front", "Abstract:", None, {}),
    # Footnote * sits inside the paragraph the first page breaks, and runs on into the contact
    # line and the volume at the foot of the page. Bernice Garcia introduces herself in it.
    ("§1.1", "* ném kʷukʷstéyp", {}, {"where": "footnote *", "gloss": "page 1, footnote *"}),
    ("footnote *", "ʔes ʔúmәcms", {}, {"who": "Bernice Garcia", "kind": "transcription",
                                      "gloss": "page 1, footnote *, how Bernice Garcia introduces herself in nɬeʔkepmxcín"}),
    ("footnote *", "‘My traditional name", {}, {"kind": "translation",
                                               "gloss": "page 1, footnote *, the English of her introduction"}),
    ("footnote *", "Alongside each example", {}, {"who": AUTHORS, "kind": "note", "gloss": "page 1, footnote *"}),
    ("footnote *", "Contact info:", {}, {"where": "front", "gloss": "page 1, at the foot of the first page"}),
    ("front", "In Proceedings of the International", {},
     {"gloss": "page 1, the volume, at the foot of the first page"}),
    # A comment printed under an example's translation opens the paragraph after it.
    ("§4.2.1", "Epistemic modals marked with nke", {
        "where": "(30) line 6", "who": "Bev Phillips", "kind": "speaker comment",
        "gloss": "page 13, printed under the example as the first line of the paragraph after it"}, {}),
    ("§4.2.2", "Consider also the example in (39)", {
        "where": "(38) line 6", "who": "Bernice Garcia", "kind": "speaker comment",
        "gloss": "page 16, printed under the example as the first line of the paragraph after it"}, {}),
    ("§4.2.3", "To summarize, in this section", {
        "where": "(44) line 6", "who": "Bev Phillips", "kind": "speaker comment",
        "gloss": "page 18, printed under the example as the first line of the paragraph after it"}, {}),
    # The denotation of the null modal, its source and the prose after it, run together.
    ("§4.3", "Matthewson et al. (2022):21", {
        "where": "(49) line 2", "kind": "note",
        "gloss": "page 19, the denotation of the null modal MOD, after Matthewson et al. 2022"}, {}),
    ("§4.3", "Reference to future event", {
        "where": "(49) line 3", "kind": "citation", "gloss": "the source of the denotation"}, {}),
)

REMOVE = (
    ("§1.1", OVERVIEW_END),
    ("§2.1", "and it’s getting dark.’"),
    ("§4.3", "and h is a stereotypical ordering source."),
) + tuple(("§1.1", word) for word in INTRODUCTION.replace(",", "").replace(".", "").split()
          if word not in ("kʷaɬtèzetkʷuʔ", "scw̓éxmx"))

SET.update({
    ("§1.1", OVERVIEW): {"form": OVERVIEW + " " + OVERVIEW_END,
                         "gloss": "page 1, the paragraph runs on to page 2 over footnote *"},
    ("footnote *", INTRODUCTION): {"form": INTRODUCTION.rstrip(",")},
    ("(11) line 6", draft_form("(11) line 6", "Consultant comment")): {
        "who": "Marty Aspinall",
        "form": draft_form("(11) line 6", "Consultant comment") + " and it’s getting dark.’",
        "gloss": "page 6, engine english, the comment runs on to a second line"},
    ("(22) line 1", draft_form("(22) line 1", "⟦xʷúy̓⟧")): {
        "who": AUTHORS, "kind": "note",
        "gloss": "page 10, the denotation of xʷúy̓, Matthewson et al.'s of the Gitksan prospective aspect dim"},
    ("(22) line 2", "Matthewson et al. (2022:19)"): {"who": AUTHORS, "kind": "citation",
                                                    "gloss": "the source of the denotation"},
    ("(49) line 1", draft_form("(49) line 1", "⟦MOD⟧")): {
        "kind": "note",
        "form": draft_form("(49) line 1", "⟦MOD⟧") + " and h is a stereotypical ordering source.",
        "gloss": "page 19, the condition the null modal MOD is defined under, after Matthewson et al. 2022"},
})


def cited(where, who, form, gloss):
    """An ADD entry for a form the draft does not hold, after the last row at where."""
    return (where, (where, who, "cited form", form, gloss))


ADD = (
    (None, ("title", AUTHORS, "title", TITLE, "the paper's title, which carries footnote *")),
    (None, ("title", AUTHORS, "name", "Ella Hannon", "the author, University of British Columbia")),
    ("footnote *", ("footnote *", AUTHORS, "name", "Bernice Garcia",
                    "kʷaɬtèzetkʷuʔ, a speaker of the Nicola Valley dialect, KBG, a Kamloops Indian Residential "
                    "School survivor relearning her language")),
    ("footnote *", ("footnote *", AUTHORS, "name", "Marty Aspinall", "c̓úʔsinek, a speaker of the Nicola Valley dialect, CMA")),
    ("footnote *", ("footnote *", AUTHORS, "name", "Bev Phillips", "a speaker of the Lytton dialect, BP")),
    ("footnote *", ("footnote *", AUTHORS, "name", "Lisa Matthewson", "P.I. of the ISI grant that funded the work, and thanked for comments")),
    ("footnote *", ("footnote *", AUTHORS, "name", "Sander Nederveen", "thanked for comments")),
    ("footnote *", ("footnote *", AUTHORS, "name", "Brent Hall", "thanked for comments")),
    ("footnote *", ("footnote *", AUTHORS, "name", "Kamloops Indian Residential School", "KIRS, of which Bernice Garcia is a survivor")),
    ("footnote *", ("footnote *", "Bernice Garcia", "place", "Coldwater", "her home, in her introduction, of ‘Nicola’")),
    ("footnote *", ("footnote *", AUTHORS, "place", "Nicola Valley", "where the NV dialect, scw̓éxmx, is spoken")),
    ("footnote *", ("footnote *", AUTHORS, "place", "Lytton", "where the Ly dialect, ƛ̓q̓mcín, is spoken")),
    ("§1.1", ("§1.1", AUTHORS, "language", "Interior Salish", "the branch of Salish nɬeʔkepmxcín belongs to, northern Interior Salish, with an estimated 100 speakers")),
    ("§3", ("§3", AUTHORS, "language", "Javanese", "for which Chen et al. 2021 propose a null non-restricted tense")),
    ("§4.2", ("§4.2", AUTHORS, "name", "Cayla Smith", "Smith, p.c., on the sensory evidential nukʷ; an author of Hannon and Smith 2023")),
    ("references", ("references", AUTHORS, "language", "tlingit", "Tlingit, in the title of Cable 2017, printed in lower case")),
    cited("front", "Gitksan", "dim", "the Gitksan prospective aspect, after Matthewson 2013 and Matthewson et al. 2022"),
    cited("§1.1", "Gitksan", "dim", "page 2, the Gitksan prospective aspect"),
    cited("§2.1", "Gitksan", "dim", "page 6, the Gitksan prospective aspect"),
    cited("§3", "Gitksan", "dim", "page 10, the Gitksan prospective aspect"),
    cited("§4", "Gitksan", "dim", "page 11, the Gitksan prospective aspect"),
    cited("§4.1", "Gitksan", "dim", "page 13, the Gitksan prospective aspect"),
    ("§2.1", ("§2.1", AUTHORS, "language", "Gitksan", "Interior Tsimshianic, the language of dim")),
    cited("§2.1", "St’át’imcets", "cuz’", "page 5, the St’át’imcets prospective aspect of Glougie 2007"),
    cited("footnote 3", "St’át’imcets", "cuz’", "page 5, footnote 3"),
    cited("footnote 3", "St’át’imcets", "kelh", "page 5, footnote 3, the St’át’imcets future modal"),
    cited("§5", "St’át’imcets", "cuz’", "page 19"),
    cited("§2.1", N, "ʔe stx̣ә́pe neʔ", "page 5, ‘(it) might’, the complex epistemic possibility modal"),
    cited("§4.2.2", N, "ʔe stx̣ә́pe néʔ(e)", "page 14, ‘it might’, with the demonstrative néʔ(e)"),
    cited("footnote 6", N, "néʔ(e)", "page 14, footnote 6, the demonstrative the author segments out of ʔe stx̣ә́pe neʔ"),
    cited("footnote 6", N, "téʔ(e)", "page 14, footnote 6, a demonstrative also felicitous after ʔe stx̣ә́pe"),
    cited("footnote 6", N, "xéʔ(e)", "page 14, footnote 6, a demonstrative also felicitous after ʔe stx̣ә́pe"),
    ("all", ("all", AUTHORS, "symbol note", "⟦ ⟧",
             "the denotation brackets of (22) and (49), typed J and K in the text layer")),
    ("all", ("all", AUTHORS, "symbol note", "#", "marks a sentence the speaker judged infelicitous in its context")),
    ("all", ("all", AUTHORS, "symbol note", "∅",
             "the null third-person subject and object, typed 0/ in the text layer; (20), (21), (23), (24), (29) "
             "and (30) print the letter Ø")),
    ("all", ("all", AUTHORS, "symbol note", "[ ]", "in a segmentation, the segments of a morpheme the surface form does not pronounce, nés-[n]-[t]-si-n")),
    ("all", ("all", AUTHORS, "notation", "(XX | YY | ZZ)",
             "the tag of an example: the speaker's initials, KBG, CMA or BP, the dialect, NV or Ly, and SF for a "
             "suggested form or VF for a volunteered one")),
)

WHOSE = (
    "The language is nɬeʔkepmxcín, northern Interior Salish, and the examples were elicited by the "
    "author from three speakers footnote * names: Bernice Garcia, kʷaɬtèzetkʷuʔ (KBG), and Marty "
    "Aspinall, c̓úʔsinek (CMA), of the Nicola Valley dialect, and Bev Phillips (BP), of the Lytton "
    "dialect. Each example's tag gives the speaker, the dialect and whether the form was suggested "
    "(SF) or volunteered (VF). Examples (1) and (2) come from recorded conversation between KBG and "
    "CMA.\n\n"
    "The who for each tier of an example is nɬeʔkepmxcín. The who for a translation is the speaker "
    "or speakers its tag names, and the English of a sentence a speaker rejected, marked Intended:, "
    "is Ella Hannon's. A comment under an example is the tagged speaker's, and the comments printed "
    "as the first line of the paragraph after (30), (38) and (44) name their speaker. Bernice Garcia's "
    "introduction in footnote * and its English are hers. The prose, the headings, the denotations "
    "and the notes carry Ella Hannon."
)

LETTERS = (
    "The forms are in the orthography of Thompson and Thompson (1992; 1996). The segmentation marks "
    "a clitic boundary with =, an affix with -, reduplication with ∼ and an infix with angle "
    "brackets, cú<ʔi>et; square brackets hold the segments of a morpheme the surface form does not "
    "pronounce, and ∅ is the null third person. # marks an infelicitous sentence. The glosses "
    "follow the Leipzig conventions and footnote 2 lists the others; from Section 3 on xʷúy̓ is "
    "glossed PROSP."
)

PAGE_NOTES = (
    "The page text was read by glyph rows, with the word spaces pdfium sets on tight justified lines "
    "kept (of the, Section 2 presents). The text layer types the null sign as 0/, repaired to ∅, and "
    "the denotation brackets of (22) and (49) as J and K, repaired to ⟦ and ⟧, read at 300 dpi. The "
    "superscript g and the subscript type <l,st> of a denotation are set on the line. (20), (21), "
    "(23), (24), (29) and (30) "
    "print the letter Ø for the null sign, kept as printed. The page prints the(in)felicity and "
    "anlysis in footnote 4 and theroetically in footnote 5, kept. Example (28) sets its context with "
    "no Context: label."
)
