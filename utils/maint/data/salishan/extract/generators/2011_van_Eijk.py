"""The ops of 2011_van_Eijk: Jan P. van Eijk's Look out for number one, kid, the Lillooet root pálaʔ
‘one’ and its 36 derivations, by affixation, reduplication and compounding.

§1 sets out Lillooet morphology and the marks the paper writes it with: a period after a prefix in a
full word, a high dot before an infix, a hyphen before a suffix, the colon of CVC and CV
reduplication, angle brackets round the copy of consonant reduplication, = before a VC copy, + round
the connective of a compound and the underloop of a clitic. §2 lists pálaʔ and its derivations as
dictionary entries: the form flush left, its gloss in quotes, sentences with their translations, and
after a double pipe the entry's comment; a secondary derivation opens on an m-dash under the form it
comes from. The forms are set upright in AboriginalSans against the AboriginalSerif of the prose,
and italic_runs reads that face as their italics.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Lillooet"
AUTHORS = ["Jan P. van Eijk"]
paper = gen.Paper("2011_van_Eijk", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Van Eijk", "Jan P. van Eijk, the author, cited for his grammar of Lillooet (1997) and his papers "
                      "on reduplication (1990, 1993, 1998a, 1998b) and morphology (2004)"),
         ("Hinkson", "Mercedes Q. Hinkson, Salishan lexical suffixes (1999)"),
         ("Kuipers", "Aert H. Kuipers, the Shuswap language (1974) and the Salish etymological dictionary (2002)"),
         ("Bill Edwards", "a storyteller, whose story holds pálʔ-ac-min̓"),
         ("Rosie Joseph", "a storyteller, whose story holds palʔ+aɬ+cítxʷ"),
         ("Marantz", "Alec P. Marantz, Re Reduplication (1982)"),
         ("Yu", "Alan C. L. Yu, a natural history of infixation (2007)"),
         ("Broselow", "Ellen Broselow, Salish double reduplications")]
LANGUAGES = [(LANGUAGE, "Northern Interior Salish, spoken in British Columbia"),
             ("St’át’imcets", "the language's own name, given with Lillooet"),
             ("Comox", "one of the languages that attest the numeral pálaʔ"),
             ("Twana", "one of the languages that attest the numeral pálaʔ"),
             ("Quinault", "one of the languages that attest the numeral pálaʔ"),
             ("Lower Chehalis", "one of the languages that attest the numeral pálaʔ"),
             ("Siletz", "one of the languages that attest the numeral pálaʔ"),
             ("Tillamook", "one of the languages that attest the numeral pálaʔ"),
             ("Thompson", "a language that borrowed pálaʔ, as Lillooet did"),
             ("Shuswap", "a language that derives ‘eight’ from ‘one’"),
             ("French", "its vert and vère, reduplication against apophony")]
DIALECTS = {"M": "M, the southern (Mount Currie) dialect", "F": "F, the northern (Fountain) dialect"}
# The sans face of the forms also sets the sounds and templates the prose discusses, not forms of
# Lillooet: Ci, a, i, ə, s, z and the w̓ of a copy. The enclitic a is added by hand after ti. Two
# headwords set side by side are cited apart by the entry.
SOUNDS = {"Ci", "a", "i", "ə", "ə́", "s", "z", "w̓", "id", "pálʔ-alc-əm, pálʔ-alc-ən"}
paper.form_language = lambda run: None if run in SOUNDS else L
# Entries open at the left margin, 144 points in; their wrapped lines hang at 180.
MARGIN = 160


def english(text):
    """Text of the entries with its lines joined by a space. A form wrapped at a morpheme's hyphen
    keeps the space as the corpus does, s.wáz̓- am-s."""
    return " ".join(text.split())


lillooet = english


def entry(lines):
    """The rows of one dictionary entry from its lines."""
    page = paper.page(lines[0])
    whole = "\n".join(paper.text(one) for one in lines)
    comment = re.search(r"\s*(\|\s?\|.*)$", whole, re.S)
    before = whole[:comment.start()] if comment else whole
    derived = before.startswith("— ")
    quote = before.find("‘")
    head = before[2 if derived else 0:quote if quote >= 0 else None].strip()
    # A gloss that closes on a colon has sentences after it, each Lillooet then its translation.
    colon = before.rfind(":’")
    intro, sentences = (before[:colon + 2], before[colon + 2:]) if colon >= 0 else (before, "")
    words = head.split()
    dialect = next((DIALECTS[one] for one in words if one in DIALECTS), None)
    forms = [one for one in re.split(r",\s*", " ".join(one for one in words if one not in DIALECTS and
                                                        not re.fullmatch(r"\([A-Z]\)", one)))]
    if quote < 0:
        forms = [words[0]]
    first = forms[0]
    where = "§2, %s" % first
    parent = entry.parent if derived else None
    if not derived:
        entry.parent = first
    # The gloss, over two lines some of them, and the second where a form has two senses,
    # (A) ‘eight (objects);’ (B) ‘eight animals.’
    lettered = before.find("(A)")
    gloss = re.match(r"\s*((?:\([A-Z]\)\s*)?‘.*?’(?:\s*\([A-Z]\)\s*‘.*?’)*)(?=\s|$)",
                     before[lettered if 0 <= lettered < quote else quote:], re.S) if quote >= 0 else None
    for form in forms:
        note = "page %d, the headword of an entry" % page
        if parent:
            note += ", a secondary derivation set after an m-dash under %s" % parent
        if dialect:
            note += ", " + dialect
        if gloss:
            note += ", " + english(gloss.group(1))
        elif quote < 0:
            note += ", the same as palaʔ-qín̓, the entry before it"
        paper.add(where, L, "cited form", lillooet(form), note)
        paper.cited_done.add(lillooet(form))
    paper.add(where, A, "note", english(intro), "page %d, the entry: its headword and gloss" % page)
    paper.cited(where, english(intro), sorted({paper.page(one) for one in lines}))
    for said, translation in re.findall(r"\s*(.+?)\s*(‘.*?’)(?=\s|$)", sentences, re.S):
        paper.add(where, L, "transcription", lillooet(said), "page %d, a sentence of the entry" % page)
        paper.add(where, A, "translation", english(translation), "page %d" % page)
    if comment:
        body = english(comment.group(1))
        paper.add(where, A, "note", body, "page %d, the entry's comment, after the double pipe" % page)
        pages = sorted({paper.page(one) for one in lines})
        paper.cited(where, body, pages)
        paper.mentions(where, body, NAMES, "name")
        paper.mentions(where, body, LANGUAGES, "language")


entry.parent = None


def entries(start, where):
    """§2's dictionary: each entry from its line at the margin to the next."""
    end = paper.find(r"^3 +Conclusions", start)
    running = paper.running_numbers_set()
    groups = []
    for number in range(start, end):
        if not paper.text(number) or paper.lines[number][2] or number in running:
            continue
        found = paper.word_positions(number)
        if found and found[0][0] < MARGIN or not groups:
            groups.append([number])
        else:
            groups[-1].append(number)
    for group in groups:
        entry(group)
    return end


blocks = {paper.find(r"^pálaʔ ‘one \(object\):’"): entries}
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, appendix=r"^jvaneijk@",
               front_languages=2)
# The references open on the author's name or, for the next work of the same author, on U+2014.
paper.rows = [row for row in paper.rows if row[0] != "references"]
end = paper.find(r"^References$")
tail = paper.find(r"^jvaneijk@")
paper.add("references", A, "heading", paper.text(end), "page %d" % paper.page(end))
opens = r"^(?:Broselow|Hinkson|Kuipers|Marantz|Van Eijk|Yu), |^—\."
for text, at in paper.references(end + 1, tail - 1, opens=opens):
    paper.add("references", A, "reference", text, "page %d" % at)
paper.add("references", A, "note", paper.text(tail), "page %d, the author's e-mail, under the references"
          % paper.page(tail))
# The abstract is set with no heading under the author's university, its lines one paragraph.
abstract = range(paper.find(r"^The complexity of Lillooet"), paper.find(r"^1 +Introduction"))
texts = {paper.text(one) for one in abstract}
rows = []
for row in paper.rows:
    if row[0] == "front" and row[2] == "note" and row[3] in texts:
        if row[3] == paper.text(abstract[0]):
            rows.append(["front", A, "note", paper.joined(abstract), "page 1, the abstract, set with no heading"])
        continue
    rows.append(list(row))
    if row[2] == "cited form" and row[3] == "ti":
        rows.append([row[0], L, "cited form", "a", "page 3, in the sans face of the forms, the reinforcing enclitic"])
# The forms are set in the sans face, not in italics.
for row in rows:
    row[4] = row[4].replace(", in italics", ", in the sans face of the forms")
paper.rows = rows
paper.write()
