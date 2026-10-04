"""The ops of 5_Lyon-Davis_2018: John Lyon and Henry Davis, ‘The Outlaws’: An Upper St’át’imcets
Tale by Sam Mitchell, recorded by Randy Bouchard in 1968: the murder near Clinton, Moses Paul's and
Paul Spintlum's years on the run, and their surrender, trial and sentencing.

The tale is set four ways: Sam Mitchell's own English (§2), notes; the St’át’imcets telling (§3), a
transcription row to each sentence; a direct English translation (§4), notes; and the interlinear
gloss (§5), 150 numbered lines. The page is read by glyph rows (page_text.py rows), which reads the
small capitals of the glosses from their font's cipher. An interlinear line is a segmentation over
its gloss, wrapped as often as the line needs, and under them the translation, set bare with no
quotes around it. The appendix is the conversion chart, a row to each printed line, the notes on
the orthography and the glossing abbreviations.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "St’át’imcets"
AUTHORS = ["John Lyon", "Henry Davis"]
paper = gen.Paper("5_Lyon-Davis_2018", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Sam Mitchell", "the storyteller, of Cácl’ep (Fountain), 1894-1985"),
         ("Randy Bouchard", "recorded the tale in August 1968, BC Indian Languages Project"),
         ("Dorothy Kennedy", "BC Indian Languages Project"),
         ("Jan van Eijk", "recorded a third version in 1971 or 1972"),
         ("Baptiste Ritchie", "Lower St’át’imcets storyteller from Lil’wat7úl"),
         ("Charlie Mack Seymour", "Lower St’át’imcets storyteller from Lil’wat7úl"),
         ("Slim Jackson", "storyteller raised in Upper St’át’imc territory"),
         ("Carl Alexander", "helped transcribe the harder passages"),
         ("Linda Redan", "helped transcribe the harder passages"),
         ("Marianne Ignace", "held the SSHRC Partnership Grant"),
         ("Simon Fraser", "the explorer in Sam Mitchell's The Drifters"),
         ("Pyal", "Old Pierre, who heard the story of first contact"),
         ("Moses Paul", "one of the two outlaws"), ("Paul Spintlum", "one of the two outlaws"),
         ("Cultus Jack", "Jack McMillan, hired by the police"), ("Tom Evans", "a tracker, Sam Mitchell's source"),
         ("Joe Russell", "guarded the bridge at Lillooet"), ("Chief Major", "of Leon’s Creek, turned the outlaws in"),
         ("Henry Costello", "the outlaws' lawyer"), ("Stuart Henderson", "the outlaws' lawyer"),
         ("Johnny Pollard", "source of §4.3's story, Johnny Pólat in St’át’imcets"),
         ("Teit", "James Teit, Mythology of the Thompson Indians (1912)"),
         ("Clark", "C. Clark, Phantoms of the Rangeland (2014)")]
LANGUAGES = [(LANGUAGE, "Northern Interior Salish (Lillooet), ISO 639-3 lil"),
             ("Upper St’át’imcets", "the Fountain dialect, Sam Mitchell's"),
             ("Lower St’át’imcets", "the Lil’wat7úl (Mount Currie) dialect"),
             ("Nɬeʔkepmx", "Thompson River Salish"), ("Interior Salish", "the branch"),
             ("Syilx", "Okanagan, Lyon's version of The Abandoned Boy"), ("English", "the translations")]

# The volume's header at the foot of page 1 opens on In Proceedings of, which gen's reader of the
# header does not take.
HEADER = paper.find(r"^In Proceedings of the International Conference")
paper.volume_header = lambda: [one for one in range(HEADER, HEADER + 3)]
FOOTNOTES = paper.page_footnotes()
AT_FOOT = {one for parts, _ in FOOTNOTES.values() for one in parts}
RUNNING = paper.running_numbers_set()
REFERENCES = paper.find(r"^References$")
TELLING = paper.find(r"^3 St’át’imcets$")
DIRECT = paper.find(r"^4 Direct English Translation$")
INTERLINEAR = paper.find(r"^5 Interlinear Gloss$")
APPENDIX = paper.find(r"^6 Appendices$")
CHART = paper.find(r"^Conversion Chart:", APPENDIX)
CHART_END = paper.find(r"^Notes on the version", CHART)
ABBREVIATIONS = paper.find(r"^Abbreviations$", CHART_END)
HEADINGS = paper.headings(1, REFERENCES - 1, skip=AT_FOOT)
# A heading too long for the line wraps onto the next, which opens in lower case: q'ám'ta t'u7
# under 3.3.
WRAPS = {}
for number in HEADINGS:
    under = number + 1
    while paper.lines[under][2] or not paper.text(under) or under in RUNNING:
        under += 1
    if paper.text(under)[:1].islower():
        WRAPS[number] = under
# Sam Mitchell's aside in English inside the telling, a sentence of its own.
ASIDES = {"Put on his clothes."}
# Forms in italics defined in plain letters, which gen's orthographic test passes over: sptakwlh
# on page 2 and the orthography notes' examples on pages 46 and 47. The notes' single letters and
# the sh of ‘ship’ are no forms.
FORMS = {"sptakwlh", "c.walh", "cwak", "t’iqwt", "stsut.s", "t’iq", "t’iiq", "nli7x", "nlii7x", "kelh", "klh",
         "t’elh", "t’lh"}
paper.form_language = lambda run: L if gen.orthographic(run) or run in FORMS else None


def printed(number):
    return paper.text(number) and not paper.lines[number][2] and number not in RUNNING and number not in AT_FOOT


def heading(number):
    """The heading on line number with the line it wraps onto, and the line after them."""
    after = WRAPS.get(number, number) + 1
    return paper.joined([number] + ([WRAPS[number]] if number in WRAPS else [])), after


def telling(start, where):
    """§3, the telling in St'át'imcets: its headings, and each paragraph cut into its sentences, a
    transcription row to each."""
    starts = paper.paragraph_starts()
    state = {"where": where, "lines": []}

    def flush():
        if not state["lines"]:
            return
        page = paper.page(state["lines"][0])
        # A hesitation's ellipsis inside a sentence, nilh s... ts7a ku száyten., is no sentence's
        # end: the words after it open in lower case.
        pieces = []
        for piece in gen.sentences(paper.joined(state["lines"])):
            if pieces and piece[:1].islower():
                pieces[-1] += " " + piece
            else:
                pieces.append(piece)
        for sentence in pieces:
            if sentence in ASIDES:
                paper.add(state["where"], A, "note", sentence, "page %d, Sam Mitchell's aside in English" % page)
            else:
                paper.add(state["where"], L, "transcription", sentence, "page %d, the telling in St’át’imcets" % page)
        del state["lines"][:]

    number = start
    while number < DIRECT:
        if not printed(number):
            number += 1
            continue
        if number in HEADINGS:
            flush()
            state["where"] = "§" + HEADINGS[number]
            text, number = heading(number)
            paper.add(state["where"], A, "heading", text, "page %d" % paper.page(number - 1))
            continue
        if number in starts:
            flush()
        state["lines"].append(number)
        number += 1
    flush()
    return DIRECT


def wrapped(number, where):
    """The line a §5 heading wraps onto, run on to the heading's row."""
    paper.rows[-1][3] += " " + paper.text(number)
    return number + 1


def opens_translation(lines, index):
    """Whether lines[index], an even number of tiers under the example's first line, opens its
    translation."""
    if index + 1 >= len(lines):
        return True
    text, below = paper.text(lines[index]), paper.text(lines[index + 1])
    if gen.is_gloss(below):
        return False
    # A tier over a gloss of lexical words holds as many words, sxek. over maybe in (5).
    if len(text.split()) == len(below.split()) and below[:1].islower():
        return False
    # A segmentation opens in lower case on a form with a clitic's or an affix's join or a letter
    # past plain English, necnactám' in the Indian in (69); a translation can open on an ellipsis
    # before its English, ...they blamed Moses Paul in (3).
    bare = text.lstrip(".“‘")
    return not (bare[:1].islower() and re.search(r"[=\-]|[^\x00-\x7f’‘“”…]", bare))


def example(start, where):
    """A line of the interlinear gloss: its segmentation over its gloss, a row to each printed
    line of either, and the translation under them, one row."""
    label = gen.EXAMPLE.match(paper.text(start)).group(1)
    lines, number = [], start
    while number < APPENDIX:
        if number != start and (gen.EXAMPLE.match(paper.text(number)) or number in HEADINGS):
            break
        if printed(number):
            lines.append(number)
        number += 1
    count = 0

    def row(who, kind, form, at, gloss=None):
        nonlocal count
        count += 1
        paper.add("(%s) line %d" % (label, count), who, kind, form,
                  "page %d%s" % (paper.page(at), ", " + gloss if gloss else ""))

    index = 0
    while index < len(lines):
        text = paper.text(lines[index])
        if index == 0:
            text = gen.EXAMPLE.match(text).group(2)
        elif index % 2 == 0 and opens_translation(lines, index):
            row(A, "translation", paper.joined(lines[index:]), lines[index])
            break
        if text in ASIDES or index % 2 and paper.text(lines[index - 1]) in ASIDES:
            row(A, "note", text, lines[index], "Sam Mitchell's aside in English, set as a tier" if index % 2 == 0
                else "under the aside, set as its gloss")
        else:
            row(L, "gloss" if index % 2 else "segmentation", text, lines[index])
        index += 1
    return number


def chart(start, where):
    """The conversion chart: its caption, the column heads, and each printed line of two pairs, van
    Eijk's letter and the A.P.A.'s. The van Eijk letters of the retracted consonants are underlined
    on the page (a render of page 46): ts, ts', s, l and l' set over c̣, c̣̓, ṣ, ḷ, ḷ̓. The text holds
    no underline."""
    here = "Appendix chart"
    caption = [start] + [one for one in range(start + 1, CHART_END) if paper.text(one).startswith("(A.P.A.)")]
    paper.add(here, A, "heading", paper.joined(caption), "page %d, the chart's caption" % paper.page(start))
    paper.add(here, A, "note", "Van Eijk",
              "page %d, the van Eijk columns: an underline marks a retracted consonant, note (ii), and is "
              "drawn under ts, ts’, s, l and l’, no letter of the text; read off a render" % paper.page(start))
    count = 0
    for number in range(caption[-1] + 1, CHART_END):
        if not printed(number):
            continue
        text = paper.text(number)
        count += 1
        if text.startswith("Van Eijk"):
            why = "the column heads, twice over"
        else:
            why = "van Eijk and A.P.A., two pairs side by side"
            if text.split()[1] in ("c̣", "c̣̓", "ṣ", "ḷ", "ḷ̓"):
                why += ", the first cell underlined on the page"
        paper.add("%s line %d" % (here, count), A, "note", text, "page %d, %s" % (paper.page(number), why))
    return CHART_END


def notes(start, where):
    """The notes on the orthography: the line over them, and each numbered note, (i) to (vii), with
    the lines it wraps onto. Page 46's chart moves the edge gen's indent test measures from, and
    the wrapped lines there read as paragraphs of their own."""
    items = []
    for number in range(start, ABBREVIATIONS):
        if not printed(number):
            continue
        if number == start or re.match(r"^\([ivx]{1,4}\)\s", paper.text(number)):
            items.append([number])
        else:
            items[-1].append(number)
    for lines in items:
        body = paper.joined(lines)
        pages = sorted({paper.page(one) for one in lines})
        paper.add(where, A, "note", body, "page %d" % pages[0])
        paper.cited(where, body, pages)
    return ABBREVIATIONS


def abbreviations(start, where):
    """The glossing abbreviations, a note to each printed line: the label and its name."""
    here = "Appendix abbreviations"
    paper.add(here, A, "heading", paper.text(start), "page %d" % paper.page(start))
    count = 0
    for number in range(start + 1, REFERENCES):
        if printed(number):
            count += 1
            paper.add("%s line %d" % (here, count), A, "note", paper.text(number),
                      "page %d, the label and its name" % paper.page(number))
    return REFERENCES


blocks = {TELLING + 1: telling, CHART: chart, CHART_END: notes, ABBREVIATIONS: abbreviations}
for number in range(INTERLINEAR, APPENDIX):
    if printed(number) and re.match(r"^\(\d{1,3}\)\s", paper.text(number)):
        blocks[number] = example
for number, under in WRAPS.items():
    if INTERLINEAR < number < APPENDIX:
        blocks[under] = wrapped
# The telling's headings are its block's; a wrapped heading's second line in §2 and §4 is none.
flowed = {number: label for number, label in HEADINGS.items() if not TELLING < number < DIRECT}
found = paper.standard(AUTHORS, NAMES, LANGUAGES, front_languages=1, blocks=blocks, headings=flowed)

# The note on the title runs on to the authors' contact at the foot of page 1, a note of its own.
for index, row in enumerate(paper.rows):
    if row[0] == "footnote *" and "Contact info:" in row[3]:
        row[3], contact = row[3].split(" Contact info:")
        paper.rows.insert(index + 1, [row[0], A, "note", "Contact info:" + contact, "page 1, the authors' contact"])
        break

# The last entry of the references opens on van Eijk in lower case, and gen's reader runs it on to
# Teit's before it.
for index, row in enumerate(paper.rows):
    if row[2] == "reference" and " van Eijk, J. P. and Williams" in row[3]:
        row[3], later = row[3].split(" van Eijk, J. P. and Williams")
        paper.rows.insert(index + 1, [row[0], A, "reference", "van Eijk, J. P. and Williams" + later,
                                      "page %d" % paper.page(paper.find(r"^van Eijk, J\. P\. and Williams"))])
        break

# Notes 5, 6 and 7 are marked in a segmentation, at the end of (67), (96) and (103); each goes after
# the example's last row.
for mark, label in (("5", "67"), ("6", "96"), ("7", "103")):
    paper.rows[:] = [row for row in paper.rows if row[0] != "footnote " + mark]
    held, paper.rows = paper.rows, []
    parts, page = found[mark]
    paper.footnote(mark, parts, page, NAMES, LANGUAGES, gloss="page %d, footnote %s" % (page, mark))
    written, paper.rows = paper.rows, held
    at = max(index for index, row in enumerate(paper.rows) if row[0].startswith("(%s) line" % label)) + 1
    paper.rows[at:at] = written
paper.write()
