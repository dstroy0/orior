"""The ops of 2011_Littell_Mackie: Patrick Littell and Scott Mackie's Reconsidering sensory evidence
in Nɬeʔkepmxcín. Of the three evidential particles, nukʷ (non-visual), ekʷu (reportative) and nke
(inferential), nukʷ projects, is not at issue and resists denial as the others do, but holds only of
the speaker's present experience and never stands in a question; they propose it is an expressive,
like ouch, oops and alas, that the speaker is being affected by a stimulus.

Each example sets its words, segmented, over a gloss and a translation in quotes, under a context in
italics where it has one; a sentence of an example can wrap onto a second pair of lines, (10), or be
followed by a second sentence with its own translation, (37) and (44). A source at the right of a
translation is a citation, and a parenthesis under one a note. (45), (60) and (61) are Quechua, the
language and its source over the words. (46) and (47) are English dialogues, and (66) to (68)
English lists.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Nɬeʔkepmxcín"
AUTHORS = ["Patrick Littell", "Scott Mackie"]
paper = gen.Paper("2011_Littell_Mackie", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Patricia McKay", "Nɬeʔkepmxcín consultant, thanked"),
         ("Flora Erhardt", "Nɬeʔkepmxcín consultant, thanked"),
         ("Mandy Jimmie", "Nɬeʔkepmxcín consultant, p.c."),
         ("Lisa Matthewson", "thanked for advising and funding"),
         ("Hotze Rullmann", "thanked for advising and funding"),
         ("Thompson and Thompson", "Laurence C. and M. Terry Thompson, The Thompson Language (1992) and the dictionary (1996)"),
         ("Kaplan", "David Kaplan, the meaning of ouch and oops (1999)"),
         ("Potts", "Christopher Potts, The Logic of Conventional Implicatures (2005)"),
         ("Schlenker", "Philippe Schlenker, expressive presuppositions (2007)"),
         ("Aikhenvald", "Alexandra Aikhenvald, Evidentiality (2004)"),
         ("Faller", "Martina Faller, evidentials in Cuzco Quechua (2002, 2006, 2010)"),
         ("Floyd", "Rick Floyd, the direct evidential in Wanka Quechua questions (1996)"),
         ("Izvorski", "Roumyana Izvorski, the present perfect as an epistemic modal (1997)"),
         ("Matthewson", "Lisa Matthewson (2008, 2010, 2011), and with Davis and Rullmann (2007)"),
         ("Murray", "Sarah Murray, evidentiality and the structure of speech acts (2010)"),
         ("Peterson", "Tyler Peterson, Gitksan evidentials (2009, 2010)"),
         ("Waldie", "Ryan Waldie, with Peterson and Mackie (2009)"),
         ("Chung", "Kyung-Sook Chung, spatial deictic tense and evidentials in Korean (2007)"),
         ("McCready and Ogata", "Eric McCready and Norry Ogata, evidentiality, modality and probability (2007)"),
         ("Portner", "Paul Portner, comments on Faller (2006)"),
         ("Roberts", "Craige Roberts, with Simons, Beaver and Tonhauser (2009)"),
         ("Rullmann", "Hotze Rullmann, with Matthewson and Davis (2008)"),
         ("Speas and Tenny", "Margaret Speas and Carol Tenny, point of view roles (2003)"),
         ("Littell", "Patrick Littell, conjectural questions (2010), and with Matthewson and Peterson (2009)"),
         ("Patrick", "Patrick Littell, named in the examples"), ("Cameron", "named in (58)"),
         ("Hannah", "named in (38)"), ("Ines", "named in (45) and (61)"),
         ("Mr. Strang", "named in (14)"), ("Scott", "Scott Mackie, named in (47)")]
LANGUAGES = [(LANGUAGE, "Thompson River Salish, Northern Interior Salishan, of British Columbia"),
             ("Thompson River Salish", "Nɬeʔkepmxcín"), ("Cuzco Quechua", "Faller (2002), in (45) and (61)"),
             ("Wanka Quechua", "Floyd (1996), in (60)"), ("Quechua", "its evidentials in questions"),
             ("Gitksan", "Peterson (2009, 2010), the root n̓akw"), ("St’át’imcets", "Matthewson (2008, 2011), k’a"),
             ("English", "its expressives, oops, ouch, alas, wow and damn")]
FORMS = {"nukʷ": L, "nke": L, "ekʷu": L, "teyt": L, "ʔes-nukʷ": L, "ʔes-": L, "qeʔnim": L,
         "qʷnox̣ʷ": L, "n̓akw": "Gitksan", "k’a": "St’át’imcets"}
ENGLISH = {"wow", "ouch", "oops", "alas", "damn", "goodbye", "ouch, oops, alas", "cow, dance, transubstantiation"}
paper.form_language = lambda run: FORMS.get(run) or ("English" if run in ENGLISH else None)
FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
SKIP = FOOT | set(paper.volume_header()) | paper.running_numbers()
RUNNING = paper.running_numbers_set()
OPEN = re.compile(r"^\((\d+)\)\s*(.*)$")
# A gloss line holds a small-capital label, SENSE, 1SUB, NOM=good=3POSS, and is short; prose that
# names INFERENCE-FROM-SENSES runs the width of the page.
LABEL = re.compile(r"(?:^|[\s=.\-(])[0-9]?[A-Z]{2,}")
# The closing quote of a translation ends the line or stands before a space; the apostrophe of
# you’ve and Hannah’s stands before a letter. The stroke of a ≠ can follow it, (9) and (43).
CLOSED = re.compile(r"^(.*?’̸?)(?=\s|$)\s*(.*)$")


def usable(number):
    return not (number in SKIP or paper.lines[number][2] or not paper.text(number) or number in RUNNING)


def after(number):
    number += 1
    while number <= paper.last and not usable(number):
        number += 1
    return number


def gloss_like(number):
    text = paper.text(number) if number <= paper.last else ""
    return bool(LABEL.search(text)) and len(text) < 62


def words_at(number):
    """A line of words is one a gloss line follows."""
    return usable(number) and len(paper.text(number)) < 70 and gloss_like(after(number))


def example(start, where):
    """Read the example opening on line start and return the line after it."""
    label, text = OPEN.match(paper.text(start)).groups()
    name = "(%s)" % label
    count = 0

    def row(who, kind, form, at, gloss=""):
        nonlocal count
        count += 1
        paper.add("%s line %d" % (name, count), who, kind, form, "page %d%s" % (paper.page(at), gloss))

    number, header = start, []
    # The context, or the language and its source, runs to the first line of words.
    while not (words_at(number) and (number != start or not text.startswith("Context:"))):
        header.append((number, text if number == start else paper.text(number)))
        number = after(number)
    if header:
        joined = " ".join(one for _, one in header)
        if joined.startswith("Context:"):
            row(A, "note", joined, start, ", the context the example answers, in italics")
        else:
            language, source = re.match(r"^(.*?)\s+(\(.*\))$", joined).groups()
            row(A, "language", language, start, ", over the example")
            row(A, "citation", source, start, ", the source at the right of the language")
    stage = "words"
    while True:
        text = paper.text(number) if number != start or header else text
        if stage == "words":
            row(L, "transcription", text, number)
            stage = "gloss"
        elif stage == "gloss":
            row(L, "gloss", text, number)
            stage = "translation"
        else:
            quoted = re.match(r"^(=|≠|Intended:)?\s*(‘.*)$", text)
            # The stream sets the year of (34)'s source, on the line under it, before the
            # translation: ‘[I just noticed that] you’re getting tired.’ Thompson and Thompson (1996).
            year = re.match(r"^\((\d{4})\)\s+(‘.*’)\s+(.*)$", text)
            parts = []
            if year:
                row(A, "translation", year.group(2), number)
                row(A, "citation", "%s (%s)" % (year.group(3), year.group(1)), number,
                    ", the source at the right of the translation, its year on the line under it")
                quoted = year
            elif quoted:
                opening, said = quoted.groups()
                closed = CLOSED.match(said)
                # A translation that wraps, (30), takes the next line up to its closing quote.
                while not closed:
                    number = after(number)
                    said += " " + paper.text(number)
                    closed = CLOSED.match(said)
                said, rest = closed.groups()
                gloss = {"Intended:": ", the intended reading", "=": ", set after =, a reading the sentence has",
                         "≠": ", set after ≠, a reading the sentence lacks"}.get(opening, "")
                row(A, "translation", said, number, gloss)
                if rest.startswith("(Lit:"):
                    row(A, "note", rest, number, ", the literal reading at the right of the translation")
                elif rest:
                    parts.append(rest)
            elif text.startswith("("):
                parts.append(text)
            if parts:
                # A source at the right of the translation, or a note under it, runs to its
                # closing parenthesis, over two lines in (5) and (60).
                piece = parts[0]
                while piece.count("(") > piece.count(")"):
                    number = after(number)
                    piece += " " + paper.text(number)
                source = re.match(r"^\(?(Thompson|Floyd|Faller)", piece)
                if source:
                    row(A, "citation", piece, number, ", the source at the right of the translation")
                else:
                    row(A, "note", piece, number, ", under the translation")
            elif not quoted:
                return number
            stage = "after"
        number = after(number)
        if number > paper.last or OPEN.match(paper.text(number)) and \
                OPEN.match(paper.text(number)).group(1) == str(int(label) + 1):
            return number
        text = paper.text(number)
        if stage == "after":
            # A second sentence, (37), (44), (45) and (50), or the rest of one that wraps, (10).
            if words_at(number):
                stage = "words"
            elif not re.match(r"^(=|≠|Intended:|‘|\()", text):
                return number
            continue
        if stage == "translation" and words_at(number) and not text.startswith(("‘", "Intended:", "=")):
            stage = "words"


def listed(start, where):
    """(46), (47): an English dialogue, a turn a line; (66) to (68): an English list, an item a
    line."""
    label, text = OPEN.match(paper.text(start)).groups()
    number, count = start, 0
    while True:
        count += 1
        paper.add("(%s) line %d" % (label, count), A, "note", text, "page %d, %s" % (
            paper.page(number), "a turn of the dialogue" if re.match(r"^[AB]′?:", text) else "an item of the list"))
        number = after(number)
        text = paper.text(number)
        if not re.match(r"^([AB]′?:|[a-f]\.|\w+:)\s", text) or OPEN.match(text):
            return number


BLOCKS = {}
expected = 1
for number in range(1, paper.last + 1):
    opened = OPEN.match(paper.text(number)) if usable(number) else None
    if opened and opened.group(1) == str(expected):
        BLOCKS[number] = listed if expected in (46, 47, 66, 67, 68) else example
        expected += 1
# Kaplan's words on page 2 are set apart, each line indented; they are one paragraph.
QUOTE = paper.find(r"^“I don’t ask ‘what does goodbye mean")
paper._starts = paper.paragraph_starts() - {QUOTE + 1, QUOTE + 2, QUOTE + 3}
# The headings of 2.1 to 2.3 open on the particle, in lower case.
HEADINGS = paper.headings(1, paper.last, SKIP)
HEADINGS.update({paper.find(r"^2\.%d\s+%s:" % (at, particle)): "2.%d" % at
                 for at, particle in ((1, "nukʷ"), (2, "ekʷu"), (3, "nke"))})
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS, headings=dict(sorted(HEADINGS.items())))
rows = paper.rows
# The Quechua examples are in the language named over them.
for label in ("(45)", "(60)", "(61)"):
    language = next(row[3] for row in rows if row[0].startswith(label + " ") and row[2] == "language")
    for row in rows:
        if row[0].startswith(label + " ") and row[1] == L:
            row[1] = language
# The italic run of qʷnox̣ʷ on page 13 comes off the glyphs with its dot after the raised w, and the
# body sets it on the x; the prose glosses it and teyt and qeʔnim in quotes after them.
at = next(index for index, row in enumerate(rows) if row[2] == "cited form" and row[3] == "teyt")
rows[at][4] += ", (“hungry”)"
rows[at + 1:at + 1] = [["§5", L, "cited form", "qʷnox̣ʷ", "page 13, in italics, (“sick”)"]]
for row in rows:
    if row[2] == "cited form" and row[3] == "qeʔnim":
        row[4] += ", (“hear”)"
# The text layer runs together the words of five places the page spaces, a bite of fish, has a
# feeling, angling for that job, smelling of flowers and Evidence from Evidentiality.
for row in rows:
    for glued, spaced in (("offish", "of fish"), ("afeeling", "a feeling"), ("anglingfor thatjob", "angling for that job"),
                          ("offlowers", "of flowers"), ("Evidencefrom", "Evidence from")):
        row[3] = row[3].replace(glued, spaced)
# (9) and (43) set = over ≠; the text layer puts the stroke of ≠ after the first line.
for index, row in enumerate(rows):
    if row[2] == "translation" and row[3].endswith("’̸"):
        row[3] = row[3][:-1]
        rows[index + 1][4] = rows[index + 1][4].replace("after =, a reading the sentence has",
                                                          "after ≠, a reading the sentence lacks")
# The Peterson (2009) entry's last line, Amherst, MA: GLSA., starts flush with the entries and the
# reference reader takes it for an entry of its own.
at = next(index for index, row in enumerate(rows) if row[2] == "reference" and row[3] == "Amherst, MA: GLSA.")
rows[at - 1][3] += " " + rows[at][3]
del rows[at]
paper.rows = rows
paper.write()
