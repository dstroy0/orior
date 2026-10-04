"""The ops of 2012_Davis_H_vanEijk: Henry Davis and Jan P. van Eijk's Lillooet bird terminology, an
annotated list of every bird name recorded in Lillooet, with the beliefs, the ecology and the
problems of recording that go with them.

§2 lists the names under nine rubrics, Generic Terms to Remaining types: passerines, each entry a
headword flush left with its dialect, F or M, its gloss in quotes and the Latin name of the bird,
the entry's comment after a double pipe. The forms, Lillooet and those of the languages the
comments compare, are set upright in AboriginalSans against the serif of the prose, and
italic_runs reads that face as their italics; the true italics are the Latin names and the titles
of the references. §3 to §6 are prose, footnote 1 a web address, and the two e-mails close the paper.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Lillooet"
AUTHORS = ["Henry Davis", "Jan P. van Eijk"]
paper = gen.Paper("2012_Davis_H_vanEijk", authors=", ".join(AUTHORS), language=LANGUAGE)
NAMES = [("Davis", "Henry Davis, one of the authors, cited for the Kayám text (2001)"),
         ("Van Eijk", "Jan P. van Eijk, one of the authors, cited for Lillooet Legends and Stories (1981)"),
         ("Kuipers", "Aert H. Kuipers, the Squamish language (1967, 1969), the Shuswap language (1974, 1989) "
                     "and the Salish etymological dictionary (2002)"),
         ("Thompson and Thompson", "Laurence C. Thompson and M. Terry Thompson, the Thompson River Salish "
                                   "dictionary (1996)"),
         ("Teit", "James A. Teit, The Lillooet Indians (1906, reprinted 1975)"),
         ("Galloway", "Brent D. Galloway, the dictionary of Upriver Halkomelem (2009)"),
         ("Pete", "Tachini Pete, the Salish (Montana) translation dictionary (2010)"),
         ("Guiguet", "C. J. Guiguet, the birds of British Columbia (1970a, 1970b)"),
         ("Carl", "G. Clifford Carl, a guide to marine life of British Columbia (1971)"),
         ("Timmers", "Jan A. Timmers, a classified English-Sechelt word list (1977)"),
         ("Seaburg", "William R. Seaburg, the diffusion of a word for pigeon (1985)"),
         ("Boas", "Franz Boas (1925), as quoted in Kuipers (2002)"),
         ("Maud", "Ralph Maud, the Hill-Tout papers (1978)"),
         ("Kimball", "Geoffrey Kimball, on gathering vocabulary (1990)"),
         ("Evans", "Nicholas Evans, Dying Words (2010)"),
         ("Harrison", "K. David Harrison, When Languages Die (2007)"),
         ("Hukari", "Thomas E. Hukari, Ruby Peter and Ellen White, the Halkomelem text (1977)"),
         ("Bouchard and Kennedy", "Randy Bouchard and Dorothy I.D. Kennedy, Lillooet Stories (1977)"),
         ("Sebastian Peter", "a speaker from Fountain"),
         ("Charlie Mack", "a speaker and storyteller from Mount Currie"),
         ("Marie Abraham", "who gave Davis a copy of Charlie Mack's story"),
         ("Morgan Wells", "a speaker Davis recorded"),
         ("Carl Alexander", "a speaker raised in Sqém’qem’, the upper Bridge River valley"),
         ("Martina LaRochelle", "a storyteller of Sek’welwás (Cayoose Creek)"),
         ("Rosie Joseph", "a storyteller, whose Coyote and the Owl the paper cites"),
         ("Desmond Peters Sr.", "a speaker originally from Tsal’álh"),
         ("Billy Louie", "a speaker from Pavilion"),
         ("Julia Michell", "a speaker from Fountain"),
         ("Rose Whitley", "a speaker originally from Fountain"),
         ("Bill Edwards", "a storyteller of Pavilion"),
         ("Captain Paul", "the M speaker Charles Hill-Tout recorded the story of Kayám from"),
         ("Charles Hill-Tout", "who recorded the story of Kayám")]
LANGUAGES = [(LANGUAGE, "Northern Interior Salish, spoken in British Columbia, the language of the bird names"),
             ("St’át’imcets", "the language's own name"),
             ("Thompson", "Northern Interior Salish, the neighbor whose bird names the comments compare"),
             ("Shuswap", "Northern Interior Salish, its Western and Enderby dialects compared"),
             ("Squamish", "Central Salish, compared in the comments"),
             ("Montana Salish", "Southern Interior Salish, compared in the comments"),
             ("Upriver Halkomelem", "Central Salish, compared in the comments"),
             ("Halkomelem", "Central Salish, whose Raven story the paper cites"),
             ("Cowichan", "Central Salish, compared in the comments"),
             ("Sechelt", "Central Salish, compared in the comments"),
             ("Upper Chehalis", "Tsamosan, compared in the comments"),
             ("Kalispel", "Southern Interior Salish, among the etyma for kingfisher"),
             ("Spokane", "Southern Interior Salish, among the etyma for kingfisher"),
             ("Coeur d’Alene", "Southern Interior Salish, compared in the comments"),
             ("Proto-Salish", "PS, the reconstructions of Kuipers (2002)"),
             ("Proto-Interior-Salish", "PIS, the reconstructions of Kuipers (2002)"),
             ("Dutch", "its winterkoning, ‘wren’"),
             ("English", "the contact language whose vernacular bird names mislead")]
DIALECTS = {"M": "M, the southern (Mount Currie) dialect", "F": "F, the northern (Fountain) dialect"}
RUBRICS = ("Generic Terms", "Waterbirds", "Birds of prey (other than owls)", "Upland game birds",
           "Domesticated fowl", "Owls", "Woodpeckers", "Crows and their allies",
           "Remaining types: non-passerines", "Remaining types: passerines")
# The sans face also sets sounds and a template the prose discusses, not forms: the epenthetic a,
# ạ against a before z, the cluster Cʔ and the [q] of pqʷ-us. Two or three headwords set side by
# side are cited apart by the entry, and zúqʷˬtuʔ kʷˬs.Jizi Kri sets its J in another face.
SOUNDS = {"a", "ạ", "z", "q", "Cʔ", "zúqʷˬtuʔ kʷˬs"}
paper.form_language = lambda run: None if run in SOUNDS or ", " in run else L
# The face breaks a few runs where a letter of the form is set in the serif, the s. of the
# headword s.q̓ǝz̓ and of Squamish s.p̓áq̓ʷ-us, the x of xḷạʔ on page 8, the l of Squamish c̓čǝl,
# the a of p̓ǝ̣́ṣk̓aʔ and the parenthesis of *c̓ạl(s); Shuswap s.pǝq- míx wraps at its hyphen. Each is
# the form the page text defines, and a headword read short is left to its entry.
MENDED = {"s.pǝq￾míx": "s.pǝq- míx", "c̓čǝ": "c̓čǝl", "p̓áq̓ʷ-us": "s.p̓áq̓ʷ-us", "ḷạʔ": "xḷạʔ",
          "*c̓ạl(s": "*c̓ạl(s)", "q̓ǝz̓": None, "p̓ǝ̣́ṣk̓": None}
for number, runs in paper.italics().items():
    runs[:] = [MENDED.get(run, run) for run in runs
               if MENDED.get(run, run) and not (run == "ʔ" and "p̓ǝ̣́ṣk̓" in runs)]
    # Page 7 breaks m̓ǝ̣́ṣ:m̓ǝ̣ṣ at its colon, twice; page 6 reads it whole.
    while "m̓ǝ̣́ṣ" in runs and runs[runs.index("m̓ǝ̣́ṣ") + 1:runs.index("m̓ǝ̣́ṣ") + 2] == ["m̓ǝ̣ṣ"]:
        at = runs.index("m̓ǝ̣́ṣ")
        runs[at:at + 2] = ["m̓ǝ̣́ṣ:m̓ǝ̣ṣ"]
# Entries open at the left margin, 144 points in; their wrapped lines hang at 180.
MARGIN = 160


def english(text):
    return " ".join(text.split())


def headwords(before):
    """The headwords of an entry: the forms before its gloss or its first lettered sense, with
    the dialect, the parentheses of who recorded them and also recorded taken out."""
    cuts = [at for at in (before.find("‘"), before.find("(A)")) if at >= 0]
    head = before[:min(cuts)] if cuts else before[:before.find(": ")]
    head = re.sub(r"\((?:[^()]|\([^()]*\))*\)", " ", head).replace("also recorded", ",")
    words = [one for one in head.split() if one not in DIALECTS]
    return [one.strip(" .,:") for one in " ".join(words).split(",") if one.strip(" .,:")]


def entry(lines, rubric):
    page = paper.page(lines[0])
    pages = sorted({paper.page(one) for one in lines})
    whole = english(paper.joined(lines))
    # Two entries set the pipes against the full stop before them, respectively.|| and
    # Ptarmigan.||; the page keeps them one word, and the entry one note.
    comment = whole.find(" ||")
    before, after = (whole[:comment].strip(), whole[comment:]) if comment >= 0 else (whole, "")
    forms = headwords(before)
    dialect = next((DIALECTS[one] for one in before[:before.find("‘")].split() if one in DIALECTS), None)
    quote = before.find("‘")
    lettered = before.find("(A)")
    start = lettered if 0 <= lettered < quote else quote
    # A gloss closes on a quote no letter follows; the apostrophe of ‘Steller’s Jay’ is inside it.
    gloss = re.match(r"((?:\([A-Z]\)\s*)?‘.*?’(?!\w)(?:[;,]?\s*\([A-Z]\)\s*‘.*?’(?!\w))*)", before[start:]) \
        if quote >= 0 else None
    where = "§2, %s" % forms[0]
    for form in forms:
        note = "page %d, the headword of an entry under %s" % (page, rubric)
        if dialect:
            note += ", " + dialect
        if gloss:
            note += ", " + gloss.group(1)
        else:
            note += ", " + before[before.find(": ") + 2:]
        paper.add(where, L, "cited form", form, note)
        paper.cited_done.add(form)
    paper.add(where, A, "note", before, "page %d, the entry: its headword, gloss and remarks" % page)
    if after:
        paper.add(where, A, "note", after, "page %d, the entry's comment, after the double pipe" % page)
    # The entry is read for its forms whole: a root the comment cites alone, pǝq in s.pǝq-m̓íx, is
    # found where it stands alone.
    paper.cited(where, whole, pages)
    paper.mentions(where, whole, NAMES, "name")
    paper.mentions(where, whole, LANGUAGES, "language")


def entries(start, where):
    """§2's list: each entry from its line at the margin to the next, under its rubric."""
    end = paper.find(r"^3 +Sundry beliefs", start)
    running = paper.running_numbers_set()
    groups, rubric = [], None
    for number in range(start, end):
        text = paper.text(number).strip()
        if not text or number in running:
            continue
        if text in RUBRICS:
            groups.append((None, number))
            continue
        found = paper.word_positions(number)
        if found and found[0][0] < MARGIN or not groups or groups[-1][0] is None:
            groups.append(([number], None))
        else:
            groups[-1][0].append(number)
    for lines, heading in groups:
        if heading:
            rubric = paper.text(heading).strip()
            paper.add("§2, " + rubric, A, "heading", rubric, "page %d, a rubric of the list" % paper.page(heading))
        else:
            entry(lines, rubric)
    return end


blocks = {paper.find(r"^Generic Terms$"): entries}
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, appendix=r"^<henry\.davis@")
# The Montana Salish form of the meadowlark's song sets the J of s.Jizi in the serif face.
song = paper.find(r"zúqʷˬtuʔ kʷˬs\.Jizi Kri")
paper.add("§2, xʷǝxʷlí", L, "cited form", "zúqʷˬtuʔ kʷˬs.Jizi Kri",
          "page %d, the meadowlark's song as Lillooet Elders render it, ‘Jesus Christ has died.’" % paper.page(song))
# The references open on the author's name or, for the next work of the same author, on U+2014.
paper.rows = [row for row in paper.rows if row[0] not in ("references", "appendix")]
end, tail = paper.find(r"^References$"), paper.find(r"^<henry\.davis@")
paper.add("references", A, "heading", paper.text(end), "page %d" % paper.page(end))
# A wrapped line of a reference can open on a place or a title, Victoria, B.C. and The Hague; the
# references open on their authors' names.
opens = r"^(?:Bouchard|Carl|Davis|Evans|Galloway|Guiguet|Harrison|Hukari|Kimball|Kuipers|Maud|Pete|Seaburg|" \
        r"Teit|Thompson|Timmers|Van Eijk)[, ]|^—\."
for text, at in paper.references(end + 1, tail - 1, opens=opens):
    paper.add("references", A, "reference", text, "page %d" % at)
paper.add("end", A, "note", paper.joined([tail, tail + 1]), "page %d, the authors' e-mails" % paper.page(tail))
# The abstract is set with no heading under the second author's university, its lines one paragraph.
abstract = range(paper.find(r"^This paper contains"), paper.find(r"^1 +Introduction"))
texts = {paper.text(one) for one in abstract}
rows = []
for row in paper.rows:
    if row[0] == "front" and row[2] == "note" and row[3] in texts:
        if row[3] == paper.text(abstract[0]):
            rows.append(["front", A, "note", paper.joined(abstract), "page 1, the abstract, set with no heading"])
        continue
    rows.append(list(row))
for row in rows:
    row[4] = row[4].replace(", in italics", ", in the sans face of the forms")
paper.rows = rows
paper.write()
