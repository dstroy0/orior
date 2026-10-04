# Context for Henry Zenk, Victoria Howard's Chinook Jargon in context. The engine's rows stand for
# the front matter and §1 to §3.3. From §4 on the text layer ran the examples into the prose and
# the engine took a citation of (8), Appendix 4, set 7, for a heading, and every row after it came
# out under "appendix". That part is read off the page again here: the prose one paragraph a row, the
# examples (1) to (35) one line a row, Figures 1 and 2 one cell a row, the references one entry a
# row, and the six appendices set by set. The Chinook Jargon the prose cites is set in italics,
# which italic_runs.py reads off the glyphs; those forms follow the paragraph that cites them.
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
from workdir import PRIVATE  # noqa: E402
from workdir import WORK  # noqa: E402
import residue  # noqa: E402

STEM = "ZenkICSNL60"
AUTHORS = "Henry Zenk"
CJ = "Chinook Jargon"
CLACK = "Clackamas Kiksht"
MOLALA = "Molala"
VICTOIRE = "Victoria Howard"
WB = "Wilson Bobb Sr."

TITLE = "Victoria Howard's Chinook Jargon in context"
BYLINE = "Henry Zenk, Confederated Tribes of Grand Ronde, Oregon"

_repair = residue.paper_repair(STEM)
with open(os.path.join(PRIVATE, "pagetext", STEM + ".txt"), encoding="utf-8") as handle:
    RAW = handle.read().split("\n")
PAGE = [" ".join(_repair(one).split()) for one in RAW]

with open(os.path.join(WORK, STEM + ".draft.tsv"), encoding="utf-8") as handle:
    DRAFT = [one.rstrip("\n").split("\t") for one in handle][1:]

# The draft rows kept: those before the heading of §4.
DRAFT_UNTIL = next(number for number, one in enumerate(DRAFT) if one[3].startswith("4 Chinook Jargon as spoken"))


def at(opening, after=0):
    """The index of the page line opening on these words, the first after index after."""
    for number, text in enumerate(PAGE):
        if number >= after and text.startswith(opening):
            return number
    raise SystemExit("no page line opens with %s" % opening)


PAGE_NUMBER = []
_page = 0
for _text in PAGE:
    _marker = re.match(r"^===== page (\d+) =====$", _text)
    if _marker:
        _page = int(_marker.group(1))
    PAGE_NUMBER.append(_page)


def skipped(index):
    """A page marker, a blank line or the printed page number."""
    text = PAGE[index]
    return text.startswith("=====") or not text or text.isdigit()


def draft_form(where, opening):
    for one in DRAFT[:DRAFT_UNTIL]:
        if one[0] == where and one[3].startswith(opening):
            return one[3]
    raise SystemExit("no draft row %s opens with %s" % (where, opening))


def footnotes():
    """The lines of each footnote from 7 on: a footnote opens on its number after a blank line at the
    foot of a page, or on the number after the one above it, and runs to the end of the page."""
    notes, current = {}, None
    for index in range(at("4 Chinook Jargon as spoken"), len(PAGE)):
        text = PAGE[index]
        if text.startswith("====="):
            current = None
            continue
        opening = re.match(r"^(\d{1,2}) \S", text)
        if opening and 7 <= int(opening.group(1)) <= 13 and \
                (not PAGE[index - 1] or (current is not None and int(opening.group(1)) == current + 1)):
            current = int(opening.group(1))
            notes[current] = [index]
        elif current is not None and text and not text.isdigit():
            notes[current].append(index)
    return notes


FOOTNOTES = footnotes()
FOOTNOTE_LINES = {index for lines in FOOTNOTES.values() for index in lines}
FOOTNOTE_START = {lines[0]: number for number, lines in FOOTNOTES.items()}


def footnote_row(number):
    lines = FOOTNOTES[number]
    return ("footnote %d" % number, AUTHORS, "note", " ".join(PAGE[one] for one in lines),
            "page %d" % PAGE_NUMBER[lines[0]])


# The Chinook Jargon the prose sets in italics, read with italic_runs.py, page by page. Each follows
# the first paragraph or footnote on its page that holds it. kind and who default to a cited form of
# the language.
CITED = [
    (16, "mayka", "the 2SG long form"),
    (16, "ma", "the 2SG short form"),
    (16, "qʰata mayka", "‘what is the matter with you?’, ‘how are you?’, a common idiom"),
    (16, "ikta-qʰata", "‘what is the matter?; something’s wrong’"),
    (16, "qʰata", "used by itself"),
    (16, "<íkdɑ qɑ́ ˑdɑ / something flooey>", "the idiom as Jacobs's field notebook writes it, with Victoire's English"),
    (17, "lulu", "a verb of (4), stressed and accented"),
    (17, "mash", "a verb of (4), stressed and accented"),
    (17, "yaka", "the pronoun of (4), unstressed"),
    (17, "kʰapa", "the preposition of (4), unstressed"),
    (18, "ixt ˈulman", "‘an old man’, the stress mark of its first word set against expect before it"),
    (18, "ˈixt ˈ úlman", "‘an old man’, the accent on the head noun"),
    (18, "ˈíxt ˈulman", "‘a certain old man’, the accent shifted to the attribute"),
    (18, "munk-", "the causative"),
    (18, "nayka", "the 1SG full form"),
    (19, "tənəs- ˈsán", "‘morning’"),
    (19, "ˈmásh- ˈsaya", "‘get rid of’"),
    (19, "wik- ˈsaya", "‘near’"),
    (19, "ˈkʰanawi- ˈqʰá", "‘everywhere’"),
    (19, "<wɩ́klíˑlɩ>", "Jacobs's spelling of ‘in a little while’"),
    (19, "ˈwik- ˈlíli", "‘in a little while’"),
    (19, "<kɑ́nɑwɩʼɩ́kdɑ>", "Jacobs's spelling of ‘everything’"),
    (19, "ˈkʰanawi- ˈíkta", "‘everything’"),
    (20, "nay", "‘1SG’, accented in (8)"),
    (20, "sax̣ali", "‘high up’, lengthened in (9)"),
    (21, "uk", "‘DET’"),
    (21, "hayu-", "‘DUR’"),
    (21, "chakwa", "an emphasis form of chaku"),
    (21, "chaku", "‘come; come to be’"),
    (22, "ya", "the 3SG short form"),
    (22, "nayka", "the 1SG full form"),
    (22, "ntsayka", "the 1PL full form"),
    (22, "mamuk", "the causative of the regional Jargon"),
    (22, "munk", "the causative of Grand Ronde Jargon"),
    (23, "mamuk", "Kenoyer's causative"),
    (23, "munk", "the contracted causative"),
    (23, "munk-", "the causative Grand Ronde speakers prefer"),
    (23, "mamuk-", "the regional causative"),
    (25, "yax̣ka", "the stressed long-form 3SG"),
    (25, "ma", "‘2SG’ of (25)"),
    (26, "saliks", "‘anger’, ‘be angry’"),
    (26, "yax̣ka", "the subject pronoun of (29)"),
    (26, "kʰapá", "‘over there’"),
    (27, "ya", "the 3SG, WB's truncation"),
    (27, "ɬas(k)", "the 3PL, Victoire's truncation"),
    (28, "mamuk", "the causative of the Grand Ronde Mission text"),
    (29, "CHUCK", "‘water’ as Priscilla Howe spells it"),
    (29, "CODA MIGA", "‘Hello & how are you’ as Priscilla Howe spells it"),
    (29, "SUPPLIE", "‘Bread’ as Priscilla Howe spells it"),
    (29, "SQUAKLE", "‘eels’ as Priscilla Howe spells it"),
    (29, "tsəqw", "‘water’ in CWDP spelling"),
    (29, "~chəqw", "‘water’, the other CWDP spelling"),
    (29, "qʰata mayka?", "‘how are you?’"),
    (29, "saplil~sable", "‘bread’"),
    (29, "skakʰwəl", "‘lamprey eels’"),
    (29, "Mólalla", "Molalla, Oregon, as Harrington's notes spell it, marked [sic]", "place", AUTHORS),
    (30, "yax̣ka", "the 3SG emphasis, focus and object form"),
]
_EDGE_BEFORE = " (‘“[<"
_EDGE_AFTER = " ,.;:?!)’”]>"


def cited(where, text, pages, used):
    """The cited forms of CITED that stand in this paragraph as words, each once."""
    rows = []
    for number, entry in enumerate(CITED):
        page, form, gloss = entry[:3]
        kind, who = entry[3:] if len(entry) > 3 else ("cited form", CJ)
        if number in used or page not in pages:
            continue
        for found in re.finditer(re.escape(form), text):
            before = text[found.start() - 1] if found.start() else " "
            after = text[found.end()] if found.end() < len(text) else " "
            if before in _EDGE_BEFORE and (after in _EDGE_AFTER or form.endswith("-")):
                used.add(number)
                rows.append((where, who, kind, form, "page %d, %s" % (page, gloss)))
                break
    return rows


# Where a paragraph or a quotation begins in §4 and §5, besides a line the page indents: the block
# quotations, the lines of the example format, the three statements of §4.3, the turns of the
# closing exchange, and the prose that resumes after each example.
BREAKS = (
    "The phenomenon of clustering bears", "But might not these",
    "My format for citing examples", "The following examples from CR and WB",
    "Yes, the congruence is far from perfect", "This point is underscored by the Grand Ronde",
    "Unlike his teacher and mentor", "And what of Silverstein’s", "In closing, I would like",
    "Line 1: IPA phonetic", "Line 2: line 1, respelled", "Line 3: interlinear", "Line 4: free translation",
    "Consider the following example",
    "A verbal predicate normally shows", "The subject of an attributive predicate", "A limitation of this schema",
    "Expressions like “make run,”", "Angulo’s “real causative”",
    "In[formant] says she can talk", "So, the Howards’ daughter",
    "Sorry, I cannot tell you", "These reminiscences suggest",
    "if you want to emphasize", "Lotta times that Jargon", "It’s quite a language",
    "HZ: Why would you talk", "WB Oh yeah they spoke", "HZ Why do you think you", "WB ‘Cause they were",
    "HZ Talking Indian", "WB Like we feel better",
)
RESUME = (
    "Jacobs’s field display of Victoire’s Jargon", "In spite of how obviously different",
    "The considerations guiding my retranslation", "Rather than a series of discrete",
    "The first clause in (5) above", "As (6) illustrates", "Accordingly, my CWDP transliterations",
    "All three speakers sometimes pair", "An increment of stress and/or accent",
    "Victoire attributed the text from which", "mamuk had also acquired", "WB’s observations regarding",
    "It is ironic that the", "Victoire usually shows the subject", "An apparent exception to this rule",
    "The use of the stressed long-form", "It is possible to draw a contrast", "A search of Victoire’s texts",
    "The contrast between two basic types", "I must admit that when",
)
QUOTED = {
    "The phenomenon of clustering bears": "a block quotation of Jacobs 1932:38",
    "Expressions like “make run,”": "a block quotation of Angulo and Freeland 1929",
    "In[formant] says she can talk": "a block quotation of Harrington's notes of Agatha (Howard) Howe Bloom",
    "Sorry, I cannot tell you": "a block quotation of Priscilla Howe's letter to the author",
    "if you want to emphasize": "Wilson Bobb Sr.'s words, quoted",
    "Lotta times that Jargon": "Wilson Bobb Sr.'s words, quoted",
    "It’s quite a language": "Wilson Bobb Sr.'s words, quoted",
    "HZ: Why would you talk": "the author's question to Wilson Bobb Sr.",
    "WB Oh yeah they spoke": "Wilson Bobb Sr.'s answer",
    "HZ Why do you think you": "the author's question to Wilson Bobb Sr.",
    "WB ‘Cause they were": "Wilson Bobb Sr.'s answer",
    "HZ Talking Indian": "the author",
    "WB Like we feel better": "Wilson Bobb Sr.'s answer",
}
HEADINGS = (("4 Chinook Jargon as spoken", "§4", 1), ("4.1 Word clusters and word stress", "§4.1", 1),
            ("4.2 Long forms and short forms", "§4.2", 1), ("4.3 A Chinookan substrate?", "§4.3", 1),
            ("5 Conclusions: re-assessing", "§5", 2))

# An example opens on its number and a bracketed first line, a lettered set or the exchange of (18);
# the prose can open a line on a number too, (9) shows shifts.
EXAMPLE = re.compile(r"^\((\d+)\) (?=[\[<]|a\. |HZ )")
SOURCE = re.compile(r"^\((?:Vict in|Zenk|Appendix|CR in|WB in|from a home|quoted speech)")


def trailing(text):
    """The translation and the parentheses the page sets after it, sources and notes, in order."""
    tail = []
    while True:
        end, dot = text.rstrip(), ""
        if end.endswith(")."):
            end, dot = end[:-1], "."
        if not end.endswith(")"):
            break
        depth, start = 0, None
        for index in range(len(end) - 1, -1, -1):
            if end[index] == ")":
                depth += 1
            elif end[index] == "(":
                depth -= 1
                if not depth:
                    start = index
                    break
        group = end[start:] if start is not None else ""
        if not (SOURCE.match(group) or group.startswith(("(*", "(Note", "(note"))):
            break
        tail.insert(0, group + dot)
        text = end[:start]
    return text.strip(), tail


def block(where, lines, literary=False):
    """An example or an appendix set: the bracketed first line, the CWDP lines and their interlinear
    translations, the free translation, and the source or note after it. lines are (text, page)."""
    texts = [text for text, page in lines]
    page = lines[0][1]
    opening, closing = ("[", "]") if texts[0].startswith("[") else ("<", ">")
    depth, used = 0, 0
    for text in texts:
        used += 1
        depth += text.count(opening) - text.count(closing)
        if depth <= 0:
            break
    first = " ".join(texts[:used])
    if closing not in first:
        raise SystemExit("no closing %s in %s: %s" % (closing, where, texts[:3]))
    cut = first.rindex(closing) + 1
    first, elided = first[:cut], first[cut:].strip()
    if opening == "[":
        gloss = "page %d, the IPA transcript of the word clusters, adapted from Zenk ca. 1990" % page
    elif literary:
        gloss = "page %d, the spelling of the Grand Ronde Mission text, in <…>" % page
    else:
        gloss = "page %d, Jacobs's field transcription, in <…>" % page
    if elided:
        gloss += ", the elision mark %s after it" % elided
    rows = [(where % 1, CJ, "transcription", first, gloss)]
    rest = texts[used:]
    english = next(number for number, text in enumerate(rest) if text.startswith(("‘", "'", "’")))
    for number, text in enumerate(rest[:english]):
        if number % 2:
            rows.append((where % len(rows) + "", CJ, "gloss", text, "page %d, the interlinear translation of the line above" % page))
        else:
            rows.append((where % len(rows) + "", CJ, "segmentation", text,
                         "page %d, respelled and segmented in the CWDP alphabet" % page))
    rows = [(where % (number + 1),) + row[1:] for number, row in enumerate(rows)]
    translation, tail = trailing(" ".join(rest[english:]))
    rows.append((where % (len(rows) + 1), AUTHORS, "translation", translation, "page %d, the free translation" % page))
    for one in tail:
        if SOURCE.match(one):
            rows.append((where % len(rows), AUTHORS, "citation", one, "page %d, the source" % page))
        else:
            rows.append((where % len(rows), AUTHORS, "note", one, "page %d, a note after the translation" % page))
    return rows


def line(opening, after=0):
    return PAGE[at(opening, after)]


def joined(opening, count, after=0):
    """The page line opening on these words and the count - 1 lines of text after it."""
    index, out = at(opening, after), []
    while len(out) < count:
        if not skipped(index):
            out.append(PAGE[index])
        index += 1
    return " ".join(out)


def turn(opening, count=1, after=0, label=None):
    """A turn of an exchange with its speaker's label taken off."""
    text = joined(opening, count, after)
    return text[len(label):].strip() if label else text


# (1), (2), (3) and (24) set a field version, the published version and a third line under a., b.
# and c.; (18) is an exchange. Each is read off the page here.
_ONE = at("(1) a.")
HAND = {
    1: [("(1a) line 1", CLACK, "transcription", line("(1) a.")[len("(1) a. "):],
         "page 15, the Clackamas field set, Jacobs 1929-30, 69:92"),
        ("(1a) line 2", VICTOIRE, "word gloss", joined("They got there where", 2),
         "page 15, the field translation Victoire gave, set under the words"),
        ("(1b) line 1", CLACK, "transcription", line("b. gatgíyamx̣")[len("b. "):], "page 15, the set as published, Jacobs 1958-59:551"),
        ("(1b) line 2", "Jacobs 1958-59", "translation", joined("‘They reached the place (Dayton)", 2), "page 15, the published translation"),
        ("(1c) line 1", CLACK, "segmentation", line("c. ɡa–tɡ–í–yam–x̣")[len("c. "):], "page 15, parsed by Duncan 2022:297"),
        ("(1c) line 2", CLACK, "gloss", line("MYT.PST–3PL–EP–arrive–USIT"), "page 15, Duncan's glosses and his note on the word"),
        ("(1c) line 3", CLACK, "segmentation", line("k̓ú ∅–qə́–d–u–x̣–t"), "page 15"),
        ("(1c) line 4", CLACK, "gloss", line("gather PRS–3INDEF"), "page 15"),
        ("(1c) line 5", AUTHORS, "note", line("(particle verb:"), "page 15, Duncan's note on the particle and its auxiliary"),
        ("(1c) line 6", CLACK, "segmentation", line("qáx̣–ba"), "page 15"),
        ("(1c) line 7", CLACK, "gloss", line("where–LOC"), "page 15")],
    2: [("(2a) line 1", CJ, "transcription", line("(2) a.")[len("(2) a. "):], "page 15, the field set, Jacobs 1929-30, 68:113"),
        ("(2a) line 2", VICTOIRE, "word gloss", line("She dug camas not long"),
         "page 15, the field translation Victoire gave, set under the words"),
        ("(2a) line 3", VICTOIRE, "word gloss", line("Pretty soon"), "page 15, set under not long"),
        ("(2b) line 1", CJ, "transcription", line("b. yámuŋk–laɡámas")[len("b. "):], "page 15, the set as published, Jacobs 1936:9"),
        ("(2b) line 2", "Jacobs 1936", "translation", line("‘She dug camas, in no long time"), "page 15, the published translation")],
    3: [("(3a) line 1", CJ, "transcription", line("(3) a.")[len("(3) a. "):], "page 16, the field set, Jacobs 1929-30, 68:103"),
        ("(3a) line 2", VICTOIRE, "word gloss", line("what’s the trouble with you"),
         "page 16, the field translation Victoire gave, set under the words"),
        ("(3b) line 1", CJ, "transcription", line("b. qáˑda?")[len("b. "):], "page 16, the set as published, Jacobs 1936:7"),
        ("(3b) line 2", "Jacobs 1936", "translation", line("‘What is the matter? are you sick?’"), "page 16, the published translation"),
        ("(3c) line 1", CJ, "segmentation", line("c. qʰata?")[len("c. "):], "page 16, Jacobs's reformulation respelled in the CWDP alphabet"),
        ("(3c) line 2", AUTHORS, "translation", line("‘What is the matter? Is it you"), "page 16, the author's translation of the reformulation")],
    24: [("(24a) line 1", CJ, "transcription", line("(24) a.")[len("(24) a. "):], "page 25, the field original, Jacobs 1929-30, 69:19"),
         ("(24a) line 2", VICTOIRE, "word gloss", line("no (more) a person now"),
          "page 25, the field translation Victoire gave, set under the words"),
         ("(24b) line 1", CJ, "transcription", line("b. wíˑk dílxam")[len("b. "):], "page 25, the published version, Jacobs 1936:3, its [sic] the author's"),
         ("(24b) line 2", "Jacobs 1936", "translation", line("‘(She is) no (longer)"), "page 25, the published translation"),
         ("(24c) line 1", CJ, "segmentation", line("c. ˈwik ˈtilxam")[len("c. "):], "page 25, a. in the CWDP alphabet"),
         ("(24c) line 2", CJ, "gloss", line("NEG person now 3SG"), "page 25, the corrected interlinear translation"),
         ("(24c) line 3", AUTHORS, "translation", line("‘She is not a person now"), "page 25, the corrected free translation")],
    18: [("(18) line 1", CJ, "segmentation", turn("(18) HZ (reading)", label="(18) HZ (reading)"),
          "page 23, HZ (reading) from Angulo's Chinook Jargon typescript, in CWDP spellings"),
         ("(18) line 2", WB, "speaker comment", turn("WB (responding) I'd say", label="WB (responding)"), "page 23, WB (responding)"),
         ("(18) line 3", AUTHORS, "note", turn("HZ but see though you said", 2, label="HZ"), "page 23, HZ"),
         ("(18) line 4", CJ, "segmentation", turn("HZ (re-reading)", label="HZ (re-reading)"), "page 23, HZ (re-reading)"),
         ("(18) line 5", WB, "speaker comment", turn("WB (responding) yeah ya", 3, label="WB (responding)"), "page 23, WB (responding)"),
         ("(18) line 6", AUTHORS, "note", turn("HZ OK, ˈyáx̣ka", label="HZ"), "page 23, HZ, saying it after WB"),
         ("(18) line 7", WB, "speaker comment", turn("WB see, if you said that", 2, label="WB"), "page 23, WB"),
         ("(18) line 8", AUTHORS, "note", turn("HZ but your, your way", label="HZ"), "page 23, HZ"),
         ("(18) line 9", WB, "speaker comment", turn("WB shouldn't be, well", label="WB"), "page 23, WB"),
         ("(18) line 10", AUTHORS, "note", turn("HZ why do you think he would", label="HZ"), "page 23, HZ")],
}


# Figure 1 on page 19 and its excerpt, Figure 2, on page 27, typed from the page one cell a row.
PERSONS = (
    ("1SG", "I, me, my, mine", ("nayka",), ("nay",), ("na",), ("nayka",)),
    ("2SG", "you, your, yours", ("mayka",), ("may",), ("ma",), ("mayka",)),
    ("3SG", "he, she, him, her, his, hers", ("yaka", "yax̣ka"), ("ya",), ("ya",), ("yaka", "yax̣ka")),
    ("1PL", "we, us, our, ours", ("n(t)sayka", "nisayka"), ("n(t)say", "tsay", "say", "nisay"), ("n(t)sa", "sa"),
     ("n(t)sayka", "nisayka")),
    ("2PL", "y’all, y’all’s", ("msayka", "misayka"), ("msay", "misay"), ("msa",), ("msayka", "misayka")),
    ("3PL", "they, them, their, theirs", ("ɬaska",), ("ɬas",), ("ɬas",), ("ɬaska",)),
)
COLUMNS = ("long/standard", "truncated", "short (clitic)", "focus/emphasis")


def figure(number, first, last, columns):
    """A pronoun table: the caption, the column heads, where each column's forms stand, and a row a
    form under each person."""
    page = PAGE_NUMBER[first]
    lines = [PAGE[index] for index in range(first, last + 1) if not skipped(index)]
    caption = " ".join(lines[lines.index(next(one for one in lines if one.startswith("Figure %d." % number))):])
    # The descriptions, each over several lines, from Unstressed: to the first person.
    text = " ".join(lines[lines.index(next(one for one in lines if one.startswith("Unstressed:"))):
                          lines.index(next(one for one in lines if one.startswith("1SG")))])
    cells = [one.strip() for one in re.split(r"(?=Unstressed:|Pre‑predicate subject|Any role;)", text) if one.strip()]
    where = "Figure %d" % number
    rows = [(where, AUTHORS, "note", caption, "page %d, the caption" % page)]
    # The page has no bar between cells. Each head and each description is a row of its own.
    for head, cell, column in zip(("LONG/ STANDARD", "TRUNCATED", "SHORT (CLITIC)", "FOCUS/ EMPHASIS"), cells, COLUMNS):
        rows += [(where, AUTHORS, "note", head, "page %d, the head of the %s column" % (page, column)),
                 (where, AUTHORS, "note", cell, "page %d, where the %s forms stand" % (page, column))]
    for person, english, *forms in PERSONS:
        if columns == 4:
            rows.append(("%s %s" % (where, person), AUTHORS, "note", english, "page %d, the English" % page))
        for column, cell in zip(COLUMNS[:columns], forms):
            for form in cell:
                note = ", in parentheses under yaka" if (form == "yax̣ka" and column == "long/standard") else ""
                rows.append(("%s %s" % (where, person), CJ, "cited form", form,
                             "page %d, %s, the %s form%s" % (page, person, column, note)))
    return rows


_FIGURE_1 = at("LONG/", at("4.2 Long forms"))
_FIGURE_2 = at("LONG/", at("4.3 A Chinookan"))
FIGURES = {_FIGURE_1: (at("(adapted from Larsen 2002)"), 1, 4), _FIGURE_2: (at("Figure 2. Excerpted"), 2, 3)}


def body():
    """§4 to §5, in page order."""
    rows, pending, used = [], [], set()
    section, unit = "§4", None
    last = at("References")

    def close():
        nonlocal unit
        if unit:
            text, pages = unit
            gloss = "page %d" % pages[0]
            quoted = next((note for opening, note in QUOTED.items() if text.startswith(opening)), None)
            if quoted:
                gloss += ", " + quoted
            rows.append((section, AUTHORS, "note", text, gloss))
            rows.extend(cited(section, text, pages, used))
            unit = None
        for number in pending:
            rows.append(footnote_row(number))
            rows.extend(cited("footnote %d" % number, rows[-1][3], [PAGE_NUMBER[FOOTNOTES[number][0]]], used))
        pending.clear()

    def boundary(index):
        text = PAGE[index]
        return EXAMPLE.match(text) or text.startswith(RESUME) or index in FIGURES or \
            any(text.startswith(opening) for opening, where, count in HEADINGS) or index >= last

    index = at("4 Chinook Jargon as spoken")
    while index < last:
        text = PAGE[index]
        if index in FOOTNOTE_START:
            pending.append(FOOTNOTE_START[index])
            if unit is None:
                close()
        if skipped(index) or index in FOOTNOTE_LINES:
            index += 1
            continue
        heading = next((one for one in HEADINGS if text.startswith(one[0])), None)
        if heading:
            close()
            section = heading[1]
            rows.append((section, AUTHORS, "heading", joined(heading[0], heading[2], index), "page %d" % PAGE_NUMBER[index]))
            index = at(heading[0], index) + heading[2]
            continue
        if index in FIGURES:
            close()
            end, number, columns = FIGURES[index]
            rows.extend(figure(number, index, end, columns))
            index = end + 1
            continue
        example = EXAMPLE.match(text)
        if example:
            close()
            number = int(example.group(1))
            lines, index = [(text[example.end():], PAGE_NUMBER[index])], index + 1
            while not boundary(index):
                if not skipped(index) and index not in FOOTNOTE_LINES:
                    lines.append((PAGE[index], PAGE_NUMBER[index]))
                index += 1
            rows.extend(HAND[number] if number in HAND else block("(%d) line %%d" % number, lines, number >= 33))
            continue
        indented = RAW[index].startswith(" ") and not RAW[index].startswith("  ")
        if unit and (indented or text.startswith(BREAKS) or text.startswith(RESUME)):
            close()
        if unit is None:
            unit = [text, [PAGE_NUMBER[index]]]
        else:
            unit[0] += " " + text
            if PAGE_NUMBER[index] not in unit[1]:
                unit[1].append(PAGE_NUMBER[index])
        index += 1
    close()
    return rows


def references():
    """One reference a paragraph: blank lines part the entries, and the blank lines of a page break
    do not."""
    entries, current, broken = [], None, False
    for index in range(at("Angulo, Jaime de"), at("Appendices")):
        text = PAGE[index]
        if text.startswith("====="):
            broken = True
            continue
        if text.isdigit():
            broken = False
            continue
        if not text:
            if not broken:
                current = None
            continue
        if current is None:
            current = [text, PAGE_NUMBER[index]]
            entries.append(current)
        else:
            current[0] += " " + text
    rows = [("references", AUTHORS, "heading", "References", "page 31")]
    for text, page in entries:
        rows.append(("references", AUTHORS, "reference", text, "page %d" % page))
        title = re.search(r"2012\. (Chinuk Wawa kakwa .*? nsayka) /", text)
        if title:
            rows.append(("references", CJ, "cited form", title.group(1),
                         "page %d, the title of Chinuk Wawa Dictionary Project 2012, Chinuk Wawa as our elders teach us to speak it" % page))
        title = re.search(r"“(Nsaïka .*?lepape\.)”", text)
        if title:
            rows.append(("references", CJ, "cited form", title.group(1),
                         "page %d, the title of Grand Ronde Mission ca. 1884 in its literary spelling, [sic] the author's" % page))
        if text.startswith("Gatschet"):
            rows += [("references", AUTHORS, "name", "Atfálati", "page %d, the Tualatin, in Gatschet's title" % page),
                     ("references", AUTHORS, "place", "Willámet", "page %d, the Willamette, in Gatschet's title" % page)]
    return rows


# The CWDP alphabet on page 34: the letters of a row, and the IPA they stand for.
def alphabet():
    first, last = at("CWDP IPA"), at("*Revision of original CWDP usage")
    rows = [("CWDP alphabet", AUTHORS, "note", "CWDP IPA", "page 34, the column heads")]
    for index in range(first + 1, last):
        text = PAGE[index]
        if skipped(index):
            continue
        # The repair closes the space after the dot of x̣, x̣[χ].
        cut = min(one for one in (text.find("["), text.find("(")) if one > 0)
        letters, value = text[:cut].strip(), text[cut:]
        for letter in letters.split(", "):
            if letter in ("ˈ", "ˊ"):
                rows.append(("CWDP alphabet", AUTHORS, "notation", letter,
                             "page 34, syllable stress" if letter == "ˈ" else "page 34, the higher prominence of an accented syllable"))
            else:
                rows.append(("CWDP alphabet", CJ, "cited form", letter, "page 34, a letter of the CWDP alphabet"))
        rows.append(("CWDP alphabet", AUTHORS, "note", value,
                     "page 34, %s %s" % ("the note on" if value.startswith("(") else "the IPA for", letters)))
    return rows


def sets(number, first, last, literary=False):
    """The numbered sets of an appendix, each parsed as an example."""
    rows, current = [], None
    groups = []
    for index in range(first, last):
        text = PAGE[index]
        if skipped(index) or index in FOOTNOTE_LINES:
            continue
        opening = re.match(r"^(\d{1,2}) (?=[<\[])", text)
        if opening:
            current = [int(opening.group(1)), [(text[opening.end():], PAGE_NUMBER[index])]]
            groups.append(current)
        elif current:
            current[1].append((text, PAGE_NUMBER[index]))
    for set_number, lines in groups:
        rows.extend(block("Appendix %d set %d line %%d" % (number, set_number), lines, literary))
    return rows


def appendix_heading(number):
    index = at("Appendix %d. " % number, at("Appendices"))
    text = joined("Appendix %d. " % number, 2 if number != 5 else 1, index)
    return index, text


def appendices():
    rows = [("appendix", AUTHORS, "heading", "Appendices", "page 34")]
    for opening in ("Abbreviations:", "CWDP: Chinuk Wawa Dictionary Project", "CTGR: Confederated Tribes",
                    "CWDP alphabet:"):
        rows.append(("appendix", AUTHORS, "note", line(opening, at("Appendices")), "page 34"))
    rows.append(("appendix", AUTHORS, "note", joined("This alphabet uses English letters", 2), "page 34"))
    rows += alphabet()
    rows.append(("appendix", AUTHORS, "note", line("*Revision of original CWDP usage"), "page 34"))
    rows.append(("appendix", AUTHORS, "note", line("Template (Appendices 1"), "page 34, the template of the sets"))
    for opening, what in (("# source spellings:", "line 1, carrying footnote 12"), ("Transliteration of line 1", "line 2"),
                          ("Interlinear translation of line 2", "line 3"), ("Free translation.", "line 4")):
        rows.append(("appendix", AUTHORS, "note", line(opening, at("Template (Appendices 1")), "page 34, the template, %s" % what))
    rows.append(("appendix", AUTHORS, "note", "Contents:", "page 34"))
    for number in range(1, 7):
        rows.append(("appendix", AUTHORS, "note", line("Appendix %d: " % number, at("Contents:")), "page 34, the contents"))
    rows.append(footnote_row(12))
    headings = [appendix_heading(number) for number in range(1, 7)]
    for number in range(1, 6):
        index, text = headings[number - 1]
        where = "Appendix %d" % number
        # The caption of Appendix 5 runs on into the note on where the whole text is found.
        cut = text.find(" The complete text")
        rows.append((where, AUTHORS, "heading", text[:cut] if cut > 0 else text, "page %d" % PAGE_NUMBER[index]))
        first = index + 2
        if number == 5:
            rows.append((where, AUTHORS, "note", text[cut + 1:] + " " + joined("https://chinookjargon.com", 2, index),
                         "page 42, where the whole text is found, carrying footnote 13 before it"))
            rows.append((where, AUTHORS, "note", line("The text and lists of names show many typos"), "page 42"))
            rows.append(footnote_row(13))
            first = at("The text and lists of names show many typos") + 1
        rows.extend(sets(number, first, headings[number][0], literary=number == 5))
    rows.extend(APPENDIX_6)
    return rows


# Appendix 6 is an exchange over Kenoyer's text: Angulo and Freeland's transcript (Ms) and its
# English, the author reading it (HZ), and Wilson Bobb Sr. answering (WB).
_SIX = at("Appendix 6. ")
_HEAD_6 = joined("Appendix 6. ", 2)
APPENDIX_6 = [
    ("Appendix 6", AUTHORS, "heading", _HEAD_6[:_HEAD_6.index(" Template:")], "page 44"),
    ("Appendix 6", AUTHORS, "note", "Template:", "page 44"),
    ("Appendix 6", AUTHORS, "note", line("(#) Ms", _SIX), "page 44, the template, line 1"),
    ("Appendix 6", AUTHORS, "note", line("Ms Reproduction of Angulo and Freeland's (1929) interlinear", _SIX), "page 44, the template, line 2"),
    ("Appendix 6", AUTHORS, "note", line("HZ Audio of Zenk", _SIX), "page 44, the template, line 3"),
    ("Appendix 6", AUTHORS, "note", line("WB Audio of WB's", _SIX), "page 44, the template, line 4"),
    ("Appendix 6 set 1 line 1", CJ, "transcription", turn("1 Ms <ɑ́ldɔ̃ṛ", label="1 Ms"), "page 44, Ms, Angulo and Freeland's transcript"),
    ("Appendix 6 set 1 line 2", "Angulo and Freeland 1929", "word gloss", turn("Ms <then he get the meat>", label="Ms"),
     "page 44, Ms, Angulo and Freeland's interlinear translation"),
    ("Appendix 6 set 1 line 3", CJ, "segmentation", turn("HZ ˈálta ˈyáx̣ka", label="HZ"), "page 44, HZ reading line 1 to WB, in CWDP spellings"),
    ("Appendix 6 set 1 line 4", WB, "speaker comment", turn("WB ˈalta ya ˈískam", label="WB"), "page 44, WB"),
    ("Appendix 6 set 2 line 1", CJ, "transcription", turn("2 Ms <yɑ́x̣gɑ", label="2 Ms"), "page 44, Ms, Angulo and Freeland's transcript"),
    ("Appendix 6 set 2 line 2", "Angulo and Freeland 1929", "word gloss", turn("Ms <he make hang up>", label="Ms"),
     "page 44, Ms, Angulo and Freeland's interlinear translation"),
    ("Appendix 6 set 2 line 3", CJ, "segmentation", turn("HZ ˈyáx̣ka ˈmamuk ˈqʰwétɬ ˈsáx̣ali.", after=_SIX, label="HZ"),
     "page 44, HZ reading line 1 to WB, in CWDP spellings"),
    ("Appendix 6 set 2 line 4", WB, "speaker comment", turn("WB I'd say it about the same", after=_SIX, label="WB"), "page 44, WB"),
    ("Appendix 6 set 2 line 5", AUTHORS, "note", turn("HZ but see though you said it different", 2, after=_SIX, label="HZ"), "page 44, HZ"),
    ("Appendix 6 set 3 line 1", AUTHORS, "note", turn("3 [HZ follows up", label="3"), "page 44"),
    ("Appendix 6 set 3 line 2", CJ, "transcription", turn("Ms <yɑ́x̣gɑ mɑˑmʋk qwɛtɫ sɑ́xlι qɑx", 2, label="Ms"),
     "page 44, Ms, Angulo and Freeland's transcript of the whole example (2)"),
    ("Appendix 6 set 3 line 3", "Angulo and Freeland 1929", "word gloss", turn("Ms <he make hang up where we", 2, label="Ms"),
     "page 44, Ms, Angulo and Freeland's interlinear translation"),
    ("Appendix 6 set 3 line 4", CJ, "segmentation", turn("HZ ˈyáx̣ka ˈmamuk ˈqʰwétɬ ˈsax̣li ˈqʰá", 2, label="HZ"),
     "page 44, HZ reading the whole text to WB, in CWDP spellings"),
    ("Appendix 6 set 3 line 5", WB, "speaker comment", turn("WB yeah ya ˈmamuk, yeah", 3, after=_SIX, label="WB"), "page 44, WB"),
    ("Appendix 6 set 3 line 6", AUTHORS, "note", turn("HZ OK, ˈyáx̣ka", after=_SIX, label="HZ"), "page 44, HZ, saying it after WB"),
    ("Appendix 6 set 3 line 7", WB, "speaker comment", turn("WB see, if you said that", 2, after=_SIX, label="WB"), "page 44 and 45, WB"),
    ("Appendix 6 set 3 line 8", AUTHORS, "note", turn("HZ but your way of Jargon", after=_SIX, label="HZ"), "page 45, HZ"),
    ("Appendix 6 set 3 line 9", WB, "speaker comment", turn("WB shouldn't be, well", after=_SIX, label="WB"), "page 45, WB"),
    ("Appendix 6 set 3 line 10", AUTHORS, "note", turn("HZ why do you think he would", after=_SIX, label="HZ"), "page 45, HZ"),
]

BUILT = body() + references() + appendices()

FORMS = {
    "vɪkˈtʰwɑ:r": ("name", AUTHORS, "page 1, Victoire, her name in its local use, in IPA"),
    "t͡ʃʰɪˈnʊk": ("cited form", CJ, "Chinook, the ethnic name, in IPA"),
    "ɑwɑ": ("cited form", CJ, "page 1, wawa ‘speech, language’ in IPA, the text layer setting a space after its w"),
    "ˈɡwɑjɑk̓ɪti": ("cited form", CLACK, "page 2, Gwayakiti, a Clackamas name, in IPA"),
    "ˈʃkɑ(j)int͡ʃ": ("cited form", MOLALA, "page 2, Shkaintch, a Molala name, in IPA"),
    "ɢɑjuɬən": ("name", AUTHORS, "page 2, Wagayuthlen in IPA, the text layer setting a space after the stress mark"),
    "ɑˈt͡ʃʼi:nu": ("name", AUTHORS, "page 3, Wacheno in IPA, the text layer setting a space after its w"),
    "ˈwɑsusɡɑni": ("name", AUTHORS, "page 3, Wasusgani in IPA"),
    "Watchínu": ("name", AUTHORS, "page 4, Wacheno as Jacobs's note spells it"),
    "wɑˈt͡ʃʼi:nu": ("name", AUTHORS, "page 4, Wacheno in IPA"),
    "ɑ́lɑti": ("name", AUTHORS, "page 4, the Tualatin as Jacobs writes the name, the text layer setting a space after its w"),
    "twɑ́lɑti": ("name", AUTHORS, "page 6, the Tualatin as Jacobs writes the name"),
    "iˈt͡ʃəmut": ("cited form", CLACK, "page 7, ‘stepfather’, in IPA"),
    "itcə́mut": ("cited form", CLACK, "page 7, ‘my step-father’, as the Clackamas field gloss writes it"),
    "ɩ́ʼɩnhúˑdɩ": ("cited form", MOLALA, "page 7, a Molala word in Jacobs's field note"),
    "ʼ)íʼinhúˑdi>": ("cited form", MOLALA, "page 8, ‘our sister-in-law’, the Molala-attributed term"),
    "yatʰum": ("cited form", CJ, "page 8, ‘sister-in-law’ (CWDP 256, 420)"),
    "idə́mɑɬ": ("cited form", CLACK, "page 8, ‘the ocean’, a Clackamas term"),
    "kʼɑˈtɑmʃ": ("cited form", CLACK, "page 8, Augustin's Clackamas name, in IPA"),
    "kʼɑtɑ́mc": ("cited form", CLACK, "page 8, Augustin's Clackamas name as Jacobs writes it"),
    "ɬɡ̇ɑ́iɬɡ̇ɑi": ("name", AUTHORS, "page 9, the Indian name of Steven Kaikai's father as Jacobs writes it, uncertainly Molala or Clackamas"),
    "ɡ̇ɑ́iɬɡ̇ɑi": ("name", AUTHORS, "page 9, the same name again, the text layer setting a space after its first letter"),
    "hayash-tutúsh": ("cited form", CJ, "page 9, ‘big breasts’, a Jargon nickname in CWDP spelling"),
    "hɑ́yɑs": ("cited form", CJ, "page 9, the same nickname as Jacobs writes it"),
}
DROP = ("ˈw", "wɑˈ", "ᵐ", "à", "ˈ", "ɬ", "тuтuˑ́c", "métisse")

SPLIT = [
    ("front", "Henry Zenk Confederated", {"where": "title", "kind": "title", "gloss": "page 1, the title, its star the acknowledgement footnote's"}, {}),
    ("front", "Confederated Tribes of Grand Ronde, Oregon Abstract", {"where": "title", "kind": "name", "gloss": "page 1, the author"}, {}),
    ("front", "Abstract:", {"where": "title", "gloss": "page 1, the affiliation"}, {"gloss": "page 1, the abstract"}),
    ("front", "Keywords:", {}, {"gloss": "page 1, the keywords"}),
]
# Block quotations the draft ran into the paragraph after them, and paragraphs into the quotation.
for _where, _marker in (("§2.1", "In favor of Jacobs’s identification"),
                        ("§2.3", "An endnote to the published version"), ("§2.3", "Mrs. Howard thought that this myth"),
                        ("§2.3", "A more glaring disjunct"), ("§2.3", "[As published in Clackamas"),
                        ("§2.3", "Judging by the sequence"), ("§2.3", "The text is framed"),
                        ("§2.3", "The Molala"), ("§2.3", "Mrs. H. says she is vague"),
                        ("§3", "kʼɑtɑ́mc, Augustin."), ("§3", "Uncertainty regarding"), ("§3", "Man, half Molale"),
                        ("§3", "Slave origin complicates"), ("§3", "As these and other annotations"),
                        ("§3.1", "But as also pointed out above"), ("§3.1", "In many respects, the record"),
                        ("§3.1", "…a… primal logic"), ("§3.1", "An additional factor of which"),
                        ("§3.1", "Drechsel's characterization"), ("§3.1", "The following contemporary notice"),
                        ("§3.2", "It must be remembered"), ("§3.3", "A type of mixed language that develops")):
    SPLIT.append((_where, _marker, {}, {}))

SET = {
    # The rule above the footnotes of page 1 stands before the paragraph of page 2 in the draft.
    ("§1", draft_form("§1", "_____")): {"form": draft_form("§1", "_____").lstrip("_ ")},
    ("§1", draft_form("§1", "*I wish")): {"where": "footnote *"},
    ("§1", "ɑwɑ"): {"form": "ˈw ɑwɑ"},
    ("§2", "ɢɑjuɬən"): {"form": "wɑˈ ɢɑjuɬən"},
    ("§2", "ɑˈt͡ʃʼi:nu"): {"form": "w ɑˈt͡ʃʼi:nu"},
    ("§2.1", "ɑ́lɑti"): {"form": "tw ɑ́lɑti"},
    ("§2.3", "ʼ)íʼinhúˑdi>"): {"form": "<( ʼ)íʼinhúˑdi>"},
    ("§3", "ɡ̇ɑ́iɬɡ̇ɑi"): {"form": "ɬ ɡ̇ɑ́iɬɡ̇ɑi"},
    ("§3", "hɑ́yɑs"): {"form": "hɑ́yɑs тuтuˑ́c"},
}

# The words a footnote cites, which the engine left in the section around it.
_note = None
for _where, _who, _kind, _form, _gloss in DRAFT[:DRAFT_UNTIL]:
    _page = re.search(r"page (\d+)", _gloss)
    _page = int(_page.group(1)) if _page else 0
    if _kind == "note" and "footnote" in _gloss:
        _mark = re.match(r"^(\d{1,2}|\*) ", _form)
        _note = (_mark.group(1), _page) if _mark else None
        continue
    if _kind != "cited form" or not _note or _page != _note[1]:
        _note = None
        continue
    SET.setdefault((_where, _form), {})["where"] = "footnote %s" % _note[0]

ADD = tuple(
    [(("title", AUTHORS), ("title", AUTHORS, "name", "Victoria Howard", "Victoire, her local name; Mrs. H. in Jacobs's notebooks, Vict in the tags")),
     (("title", AUTHORS), ("title", AUTHORS, "name", "Melville Jacobs", "who transcribed her Clackamas and Jargon texts in 1929 and 1930")),
     (("title", AUTHORS), ("title", AUTHORS, "name", "Wilson Bobb Sr.", "WB, a Grand Ronde elder the author recorded, 1891-1985")),
     (("title", AUTHORS), ("title", AUTHORS, "name", "Clara Riggs", "CR, a Grand Ronde elder the author recorded, 1891-1983")),
     (("title", AUTHORS), ("title", AUTHORS, "name", "John B. Hudson", "whose 1941 recording gives (4)")),
     (("title", AUTHORS), ("title", AUTHORS, "name", "Louis Kenoyer", "whose Jargon text Appendix 6 reviews")),
     (("title", AUTHORS), ("title", AUTHORS, "name", "Yvonne Hajda", "who recorded Grand Ronde elders with the author")),
     (("§2", "Father: Wishikin..."), ("§2", AUTHORS, "name", "ˈwɪʃɪkɪn", "page 2, Wishikin in IPA, from Gatschet 1877:78")),
     (("§3", "Uncertainty regarding..."), ("§3", AUTHORS, "name", "ɬɢɑɪɬɢɑɪ", "page 9, the name of Kaikai's father in IPA, of uncertain tribal provenance"))]
    + [(None, one) for one in BUILT]
    + [(None, ("all", AUTHORS, "notation", "_", "a break between word clusters, marked in Jacobs's transcripts and the author's")),
     (None, ("all", AUTHORS, "notation", "ˈ", "a stressed syllable")),
     (None, ("all", AUTHORS, "notation", "ˊ", "a stressed and accented syllable")),
     (None, ("all", AUTHORS, "notation", "{ … }", "a compound in the interlinear translation, its meaning in context in the free translation")),
     (None, ("all", AUTHORS, "notation", "<…>", "a source spelling, Jacobs's or the Grand Ronde Mission text's")),
     (None, ("all", AUTHORS, "notation", "[…]", "an IPA spelling")),
     (None, ("all", AUTHORS, "notation", "*", "in Appendix 5 and (35), a typo of the literary text")),
     (None, ("all", AUTHORS, "notation", "/…/", "an elision"))]
)

WHOSE = (
    "The language is Chinook Jargon, Chinuk Wawa, as spoken in the Grand Ronde community of northwest "
    "Oregon. Victoria Howard's Jargon comes from Melville Jacobs's field notebooks of 1929 and 1930, "
    "tagged Vict; Wilson Bobb Sr. (WB) and Clara Riggs (CR) from the author's recordings, tagged by "
    "their initials; John B. Hudson from a 1941 disc recording; the Grand Ronde Mission text of about "
    "1884 in its literary spelling; and Louis Kenoyer through Angulo and Freeland's 1929 transcript. "
    "Example (1) is Clackamas Kiksht, and §2 and §3 cite Clackamas, Molala and Tualatin names.\n\n"
    "Every tier of an example and each set of an appendix gives its first line, its CWDP lines and its "
    "interlinear lines to the language. The field translations set under the words in (1) to (3) and "
    "(24) are Victoria Howard's English, the published translations Jacobs's, and the free "
    "translations the author's. WB's answers in (18) and Appendix 6 are his own words."
)

LETTERS = (
    "The first line of an example is IPA in square brackets, or Jacobs's field transcription in angle "
    "brackets with his ʋ, ι, ɩ, ɡ̇ and ɫ; the second is the CWDP alphabet of the Confederated Tribes of "
    "Grand Ronde, with ɬ, x̣, qʰ, kʰ, ʔ and the apostrophe of an ejective. ˈ marks stress and an acute "
    "on a stressed vowel the accent. The Grand Ronde Mission text writes a French-based spelling, "
    "nsaïka and tlaska."
)

PAGE_NOTES = (
    "The text layer sets a space inside several IPA spellings of names, [ˈw ɑwɑ], [w ɑˈt͡ʃʼi:nu] and "
    "[wɑˈ ɢɑjuɬən], after a letter that stands before a stress mark or a wide vowel; the rows keep "
    "the space. From §4 on the draft ran the examples into the prose, and the rows there are read off "
    "the page again, the prose a paragraph a row, each example and appendix set a line a row, and "
    "Figures 1 and 2 a cell a row."
)
