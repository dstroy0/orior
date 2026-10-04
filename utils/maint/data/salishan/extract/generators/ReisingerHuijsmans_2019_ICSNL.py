"""The ops of ReisingerHuijsmans_2019_ICSNL: D. K. E. Reisinger and Marianne Huijsmans on the auxiliary
ǰaqa in ʔayʔaǰuθəm. Its readings, wishes, surprises, undesirable consequences and undesirable
repetitions, are unified by treating ǰaqa as an overt exclamation operator in the sense of Grosz
(2011; 2014), expressing the speaker's emotion towards a proposition on a contextually salient scale;
the clitics č̓a and gut and the particle ʔiy are cues selecting the scale. Klallam and SENĆOŦEN yəq
and Sechelt -k̲a, yák̲a and yék̲á are compared as cognates.

Page text read by glyph rows. The ʔayʔaǰuθəm examples are segmented on their first line (opening =
"segmentation"), glossed under it and translated. The three tables, the dictionary entries of (37),
(41) and (44), the scale overview (51), the lexical entries (52) and (58), the English optatives (56),
the constraint (57), the scale under (59) and the wrapped context of (79) are read by blocks. The
paper numbers its examples (76), (79), with no (77) or (78).
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
LANGUAGE = "ʔayʔaǰuθəm"
L = gen.L
AUTHORS = ["D. K. E. Reisinger", "Marianne Huijsmans"]
paper = gen.Paper("ReisingerHuijsmans_2019_ICSNL", authors=" and ".join(AUTHORS), language=LANGUAGE)
paper.opening = "segmentation"
NAMES = [("Elsie Paul", "a ʔayʔaǰuθəm speaker the authors thank"),
         ("Marion Harry", "a ʔayʔaǰuθəm speaker"), ("Freddie Louie", "a ʔayʔaǰuθəm speaker"),
         ("Phyllis Dominick", "a ʔayʔaǰuθəm speaker"), ("Margaret Vivier", "a ʔayʔaǰuθəm speaker"),
         ("Randy Timothy", "a ʔayʔaǰuθəm speaker"), ("Joanne Francis", "a ʔayʔaǰuθəm speaker"),
         ("Lisa Matthewson", "thanked; Matthewson (2004), semantic fieldwork"),
         ("Henry Davis", "held the SSHRC Insight grant"),
         ("Grosz", "Patrick G. Grosz, optative constructions (2011) and optative markers as cues (2014)"),
         ("Kroeber", "Paul Kroeber, the Salish language family (1999) and clitics (2002)"),
         ("Watanabe", "Honoré Watanabe, Sliammon (2003) and insubordination (2016)"),
         ("Montler", "Timothy Montler, Saanich (1986), Straits auxiliaries (2003), Klallam (2012, 2015)"),
         ("Beaumont", "Ronald Beaumont, Sechelt dictionary (2011)"),
         ("Truckenbrodt", "Hubert Truckenbrodt; Matthewson and Truckenbrodt (2018)"),
         ("Quirk", "Randolph Quirk et al., a comprehensive grammar of English (1985)"),
         ("Rosengren", "Inger Rosengren, exclamation (1992)"),
         ("Scholz", "Ulrike Scholz, Wunschsätze im Deutschen (1991)"),
         ("Leonard", "Janet Leonard; Leonard and Huijsmans (2018), SENĆOŦEN wh-questions"),
         ("Cable", "Seth Cable; Tom and Mittens (2014)"), ("Rolka", "Rolka and Cable (2014), so cited"),
         ("T. S. Arthur", "After the Storm (1868)")]
LANGUAGES = [(LANGUAGE, "Central Salish, also known as Comox-Sliammon, ISO 639-3 coo"),
             ("Comox-Sliammon", "ʔayʔaǰuθəm"), ("Central Salish", "the branch"),
             ("Coast Salish", "ʔayʔaǰuθəm"), ("Salish", "the family"),
             ("Klallam", "yəq, iq (Montler 2012, 2015)"), ("SENĆOŦEN", "yəq, a dialect of Northern Straits"),
             ("Northern Straits", "SENĆOŦEN"), ("Sechelt", "-k̲a, yák̲a, yék̲á (Beaumont)"),
             ("English", "a covert EX operator"), ("German", "a covert EX operator"),
             ("Classical Greek", "cupitive and potential optatives")]

# The headings by number and the words their titles open on. 4.2 and 4.3.3.1 open on a form of the
# language in lower case, which gen's heading pattern leaves out, and with it every heading after.
TITLES = [("1", "Introduction"), ("2", "The Readings"), ("2.1", "Wishes"), ("2.2", "Surprises"),
          ("2.3", "Undesirable"), ("2.4", "Excessive"), ("2.5", "Summary"), ("3", "Data from"),
          ("3.1", "Klallam"), ("3.2", "SENĆOŦEN"), ("3.3", "Sechelt"), ("3.4", "Summary"),
          ("4", "Towards"), ("4.1", "Grosz"), ("4.1.1", "Optatives"), ("4.1.2", "The EX"),
          ("4.1.3", "The Role"), ("4.2", "ǰaqa as"), ("4.3", "The Particles"), ("4.3.1", "Polar"),
          ("4.3.2", "Optatives"), ("4.3.3", "Adversatives"), ("4.3.3.1", "ga$"), ("4.3.3.2", "ʔut$"),
          ("4.3.4", "Summary"), ("4.4", "Supporting"), ("4.4.1", "Syntactic"), ("4.4.2", "Speaker"),
          ("5", "Conclusion")]
HEADINGS, previous = {}, 1
for label, title in TITLES:
    previous = paper.find(r"^%s\s+%s" % (re.escape(label), title), previous)
    HEADINGS[previous] = label

FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
SKIP = FOOT | set(paper.volume_header()) | paper.running_numbers()


def lines_from(number):
    """The body's lines from number on, passing over footnotes, page numbers and page marks."""
    while number <= paper.last:
        if number not in SKIP and not paper.lines[number][2] and paper.text(number).strip():
            yield number
        number += 1


def until(start, ends):
    """[(line, text)] of the body from start to the line before the first matching ends, and that
    line's number."""
    taken = []
    for number in lines_from(start):
        if re.match(ends, paper.text(number)):
            return taken, number
        taken.append((number, paper.text(number)))
    raise ValueError(ends)


def put(where, who, kind, form, gloss):
    paper.add(where, who, kind, form, gloss)


# The tables, set cell by cell from the pages: the layer interleaves the column heads that wrap
# onto two lines and the cells that hold two forms (150 dpi renders of pages 7, 10 and 23).
TABLE_ROWS = {
    "Table 1": [
        ("note", A, "Wishes Surprises Undesirable Consequences Undesirable Repetitions", "the column heads"),
        ("note", A, "Form", "the first row's head"),
        ("cited form", L, "ǰaqa=č̓a", "Table 1, Wishes, the form"),
        ("cited form", L, "ǰaqa=as=χʷəʔt", "Table 1, Wishes, the form"),
        ("cited form", L, "ǰaqa (ʔiy)", "Table 1, Surprises, the form"),
        ("cited form", L, "ǰaqa", "Table 1, Undesirable Consequences, the form"),
        ("cited form", L, "ǰaqa=gut", "Table 1, Undesirable Repetitions, the form"),
        ("cited form", L, "ǰaqa=ʔut", "Table 1, Undesirable Repetitions, the form"),
        ("note", A, "Function", "the second row's head"),
        ("note", A, "counterfactual wishes", "Table 1, Wishes, the function"),
        ("note", A, "surprises, unexpected events, accidents", "Table 1, Surprises, the function"),
        ("note", A, "undesirable consequences in conditionals", "Table 1, Undesirable Consequences, the function"),
        ("note", A, "undesired and unpleasant repetitions", "Table 1, Undesirable Repetitions, the function")],
    "Table 2": [
        ("note", A, "Undesirable Consequences Wishes Surprises Disapproval / Unwanted Repetition", "the column heads"),
        ("cited form", L, "ǰaqa", "Table 2, ʔayʔaǰuθəm, Undesirable Consequences"),
        ("cited form", L, "ǰaqa=č̓a", "Table 2, ʔayʔaǰuθəm, Wishes"),
        ("cited form", L, "ǰaqa=as=χʷət", "Table 2, ʔayʔaǰuθəm, Wishes"),
        ("cited form", L, "ǰaqa (ʔiy)", "Table 2, ʔayʔaǰuθəm, Surprises"),
        ("cited form", L, "ǰaqa=gut", "Table 2, ʔayʔaǰuθəm, Disapproval / Unwanted Repetition"),
        ("cited form", L, "ǰaqa=ʔut", "Table 2, ʔayʔaǰuθəm, Disapproval / Unwanted Repetition"),
        ("cited form", "Sechelt", "yák̲a", "Table 2, Sechelt, Undesirable Consequences"),
        ("cited form", "Sechelt", "-k̲a", "Table 2, Sechelt, Wishes"),
        ("note", A, "?", "Table 2, Sechelt, Surprises, no form"),
        ("cited form", "Sechelt", "yék̲á", "Table 2, Sechelt, Disapproval / Unwanted Repetition"),
        ("note", A, "?", "Table 2, SENĆOŦEN, Undesirable Consequences, no form"),
        ("cited form", "SENĆOŦEN", "yəq", "Table 2, SENĆOŦEN, Wishes"),
        ("note", A, "?", "Table 2, SENĆOŦEN, Surprises, no form"),
        ("note", A, "?", "Table 2, SENĆOŦEN, Disapproval / Unwanted Repetition, no form"),
        ("note", A, "?", "Table 2, Klallam, Undesirable Consequences, no form"),
        ("cited form", "Klallam", "yəq / iq", "Table 2, Klallam, Wishes"),
        ("note", A, "?", "Table 2, Klallam, Surprises, no form"),
        ("note", A, "?", "Table 2, Klallam, Disapproval / Unwanted Repetition, no form")],
    "Table 3": [
        ("note", A, "Form Standard use Use as a cue in EX constructions", "the column heads"),
        ("cited form", L, "ʔiy", "Table 3, the form"),
        ("note", A, "conjunction / linker", "Table 3, the standard use of ʔiy"),
        ("note", A, "promotes scale of speaker-unlikelihood (≈ polar exclamatives)", "Table 3, the use of ʔiy as a cue"),
        ("cited form", L, "č̓a", "Table 3, the form"),
        ("note", A, "epistemic modal", "Table 3, the standard use of č̓a"),
        ("note", A, "promotes scale of speaker-preference (≈ optatives)", "Table 3, the use of č̓a as a cue"),
        ("cited form", L, "gut (ʔut)", "Table 3, the form"),
        ("note", A, "ɢᴀ + scalar exclusive", "Table 3, the standard use of gut (ʔut)"),
        ("note", A, "promotes scale of speaker-dispreference (≈ adversatives)", "Table 3, the use of gut (ʔut) as a cue")]}
TABLE_ENDS = {"Table 1": r"^3 Data from", "Table 2": r"^4 Towards", "Table 3": r"^4\.4 Supporting"}
TABLES = {paper.find(r"^%s:" % label): label for label in TABLE_ENDS}


def table(start, where):
    label = TABLES[start]
    at = paper.page(start)
    put(label, A, "note", paper.text(start), "page %d, the table's caption" % at)
    for kind, who, form, gloss in TABLE_ROWS[label]:
        put(label, who, kind, form, "page %d, %s" % (at, gloss))
    return until(start + 1, TABLE_ENDS[label])[1]


def entry(start, where):
    """A dictionary entry of Beaumont's: the head a Sechelt form, its English definition a note and
    the source a citation."""
    label = gen.EXAMPLE.match(paper.text(start)).group(1)
    at = paper.page(start)
    taken, end = until(start + 1, r"^\(\d+\)")
    text = " ".join(line for _, line in taken)
    head, definition, source = re.match(r"^(\S+)\s+(.*?)\s*(\[[^\]]+\])$", text).groups()
    put("(%s) line 1" % label, A, "note", "Dictionary entry:", "page %d" % at)
    put("(%s) line 2" % label, "Sechelt", "cited form", head, "page %d, the entry's head" % at)
    put("(%s) line 3" % label, A, "note", definition, "page %d, the entry's definition" % at)
    put("(%s) line 4" % label, A, "citation", source, "page %d, the tag or source at the right" % at)
    return end


def scales(start, where):
    """(51): the overview of the constructions and their scales, a row a line."""
    at = paper.page(start)
    put("(51) line 1", A, "note", "Constructions and their respective scales:", "page %d" % at)
    put("(51) line 2", A, "note", "CONSTRUCTION EMOTION SALIENT SCALE", "page %d, the column heads" % at)
    taken, end = until(start + 2, r"^In addition to its scalar")
    for _, text in taken:
        letter, rest = re.match(r"^([a-c])\. (.*)$", text).groups()
        put("(51%s) line 1" % letter, A, "note", rest, "page %d, the construction, its emotion and its scale" % at)
    return end


def lexical_entry(start, where):
    """(52): Grosz's lexical entry for EX, its felicity condition a formula between notes."""
    at = paper.page(start)
    taken, end = until(start, r"^To sum up, an utterance")
    lines = [text for _, text in taken]
    put("(52) line 1", A, "note", gen.EXAMPLE.match(lines[0]).group(2) + " " + lines[1], "page %d" % at)
    put("(52) line 2", A, "formula", lines[2], "page %d, a formula, its subscripts set inline" % at)
    put("(52) line 3", A, "note", " ".join(lines[3:5]), "page %d, the formula's paraphrase" % at)
    put("(52) line 4", A, "note", " ".join(lines[5:]), "page %d" % at)
    return end


def english(start, where):
    """(56): three English optatives, the last with its source."""
    at = paper.page(start)
    taken, end = until(start, r"^According to Grosz")
    for _, text in taken:
        letter, rest = re.match(r"^(?:\(56\) )?([a-c])\. (.*)$", text).groups()
        cited = re.match(r"^(.*?)\s+(\[[^\]]+\])$", rest)
        put("(56%s) line 1" % letter, A, "note", cited.group(1) if cited else rest, "page %d, an English example" % at)
        if cited:
            put("(56%s) line 2" % letter, A, "citation", cited.group(2), "page %d, the tag or source at the right" % at)
    return end


def constraint(start, where):
    """(57): Grosz's `Utilize Cues`, its two clauses a note each."""
    at = paper.page(start)
    put("(57) line 1", A, "note", "Utilize Cues:", "page %d" % at)
    taken, end = until(start + 1, r"^Essentially, this constraint")
    clauses = []
    for _, text in taken:
        opened = re.match(r"^([ab])\. (.*)$", text)
        if opened:
            clauses.append([opened.group(1), opened.group(2)])
        else:
            clauses[-1][1] += " " + text
    for letter, text in clauses:
        put("(57%s) line 1" % letter, A, "note", text, "page %d" % at)
    return end


def felicity(start, where):
    """(58): the lexical entry for ǰaqa, two formulas, a note and the source."""
    at = paper.page(start)
    taken, end = until(start, r"^Adopting Grosz")
    lines = [text for _, text in taken]
    put("(58) line 1", A, "formula", gen.EXAMPLE.match(lines[0]).group(2),
        "page %d, a formula, its subscripts and superscripts set inline" % at)
    put("(58) line 2", A, "formula", lines[1], "page %d, a formula, its subscripts set inline" % at)
    rest, source = re.match(r"^(.*?)\s+(\[[^\]]+\])$", " ".join(lines[2:])).groups()
    put("(58) line 3", A, "note", rest, "page %d" % at)
    put("(58) line 4", A, "citation", source, "page %d, the tag or source at the right" % at)
    return end


def scale(start, where):
    """(59): the example's tiers, then the scale of speaker-preference drawn under it."""
    at = paper.page(start)
    taken, end = until(start, r"^The ‘surprise’ readings")
    lines = [text for _, text in taken]
    put("(59) line 1", L, "segmentation", gen.EXAMPLE.match(lines[0]).group(2), "page %d" % at)
    put("(59) line 2", L, "gloss", lines[1], "page %d" % at)
    put("(59) line 3", A, "translation", lines[2], "page %d" % at)
    for count, text in enumerate(lines[3:], 4):
        gloss = "page %d, the scale drawn under the example" % at
        if set(text) == {"-"}:
            gloss = "page %d, the scale's dashed line, the threshold" % at
        put("(59) line %d" % count, A, "note", text, gloss)
    return end


def storyboard(start, where):
    """(79): the context wraps onto a second line, its source in brackets."""
    at = paper.page(start)
    taken, end = until(start, r"^This requires further")
    lines = [text for _, text in taken]
    put("(79) line 1", A, "note", gen.EXAMPLE.match(lines[0]).group(2) + " " + lines[1],
        "page %d, over the example" % at)
    put("(79) line 2", L, "segmentation", lines[2], "page %d" % at)
    put("(79) line 3", L, "gloss", lines[3], "page %d" % at)
    put("(79) line 4", A, "translation", lines[4], "page %d" % at)
    return end


blocks = {at: table for at in TABLES}
for number in (37, 41, 44):
    blocks[paper.find(r"^\(%d\) Dictionary entry:" % number)] = entry
blocks[paper.find(r"^\(51\) Constructions")] = scales
blocks[paper.find(r"^\(52\) For any scale")] = lexical_entry
blocks[paper.find(r"^\(56\) a\.")] = english
blocks[paper.find(r"^\(57\) Utilize Cues")] = constraint
blocks[paper.find(r"^\(58\) ⟦")] = felicity
blocks[paper.find(r"^\(59\) ǰaqa")] = scale
blocks[paper.find(r"^\(79\) Context")] = storyboard
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, headings=HEADINGS, glued={"8": "threshold."})

rows = paper.rows
# Page 1 ends the paragraph that lists the readings on (4).1, footnote 1's mark after the example
# number; the footnote finder takes the line for an unmarked note. It closes the paragraph, and
# footnote 1 follows it.
stub = next(row for row in rows if row[0] == "front" and row[3] == "(4).1")
rows.remove(stub)
at = next(index for index, row in enumerate(rows) if row[0] == "§1" and row[3].endswith("undesirable"))
rows[at][3] += " (4).1"
footnote = [row for row in rows if row[0] == "footnote 1"]
for row in footnote:
    rows.remove(row)
at = next(index for index, row in enumerate(rows) if row[0] == "§1" and row[3].endswith("(4).1"))
while rows[at + 1][2] in ("cited form", "language", "name") and rows[at + 1][0] == "§1":
    at += 1
rows[at + 1:at + 1] = footnote

# (1)'s consultant's comment opens on a capital C, which the example reader takes for prose.
for index, row in enumerate(rows):
    if row[0] == "§1" and row[3].startswith("Consultant’s Comment:"):
        rows[index] = ["(1) line 4", A, "speaker comment", row[3], row[4] + ", under the example, the consultant not named"]
    if row[0] == "(32) line 3":
        row[4] += ", carries footnote 4"

# §3.2's paragraph lists its readings (i) and (ii), and the list reader opens a note on each.
first = next(index for index, row in enumerate(rows) if row[0] == "§3.2" and row[3].startswith("In a more recent"))
parts = [index for index in range(first + 1, first + 5) if rows[index][0] == "§3.2" and rows[index][2] == "note"]
for index in parts:
    rows[first][3] += " " + rows[index][3]
for index in reversed(parts):
    del rows[index]

# The language of each example not in ʔayʔaǰuθəm, by its number.
BY_NUMBER = {29: "Klallam", 30: "Klallam", 70: "Klallam", 71: "Klallam", 68: "Northern Straits",
             69: "SENĆOŦEN", 72: "Northern Straits", 73: "Northern Straits"}
BY_NUMBER.update({number: "SENĆOŦEN" for number in range(31, 37)})
BY_NUMBER.update({number: "Sechelt" for number in range(37, 48)})
ENGLISH = {"(48a)", "(48b)", "(49a)", "(49b)", "footnote 5 (i)", "footnote 5 (ii)", "footnote 10 (i)"}
GERMAN = {"(48c)", "(49c)", "(50a)", "(50b)", "(53)", "(54)", "(55)", "footnote 10 (ii)"}
UNGLOSSED = {29, 30, 38, 39, 40, 42, 43, 45, 46, 47}
ORTHOGRAPHY = {"(69) line 1", "(72) line 2", "(73) line 2"}
kept = []
for row in rows:
    where, who, kind, form, gloss = row
    example = re.match(r"^(\((\d+)[a-z]?\)|footnote \d+ \([iv]+\))", where)
    if kind == "cited form" and form in ("ε", "φ", "express ε"):
        continue
    if example and who == L:
        label = example.group(1)
        number = int(example.group(2)) if example.group(2) else None
        if label in ENGLISH:
            row[1:3], row[4] = [A, "note"], gloss + ", an English example"
        elif label in GERMAN:
            row[1] = "German"
            row[2] = "transcription" if kind == "segmentation" else kind
        elif number in BY_NUMBER:
            row[1] = BY_NUMBER[number]
            if number in UNGLOSSED and kind == "segmentation":
                row[2] = "transcription"
        if where in ORTHOGRAPHY:
            row[2], row[4] = "transcription", gloss + ", the SENĆOŦEN orthography"
        if where == "(73) line 1":
            row[1:3], row[4] = [A, "note"], gloss + ", over the example"
    if kind == "cited form" and who == L:
        if where == "§3.1":
            row[1] = "Klallam"
        elif where == "§3.2":
            row[1] = "SENĆOŦEN"
        elif where in ("§3.3", "§3.4"):
            row[1] = "Sechelt"
        elif form in ("ʔiʔ", "čəntéŋ"):
            row[1] = "Northern Straits"
    kept.append(row)
    # (84a)'s context quotes the ʔayʔaǰuθəm question it answers.
    if where == "(84a) line 1":
        kept.append(["(84a) line 1", L, "transcription", "čɛlas θukʷnačtən kʷikʷa θoʔna. ho=ga mat.",
                     gloss + ", the sentence the context answers"])

# §3.3's italic Sechelt forms: the italic runs are read off the glyphs, which carry no underline,
# and match none of the repaired text. The paragraph that opens on Yák̲a is joined to the one before.
def after(opening, rows_after):
    at = next(index for index, row in enumerate(kept) if row[0] == "§3.3" and row[3].startswith(opening))
    kept[at + 1:at + 1] = rows_after
    return at


at = after("In addition, Beaumont", [])
first, second = kept[at][3].split(" Yák̲a seems", 1)
kept[at][3] = first
kept[at + 1:at + 1] = [["§3.3", "Sechelt", "cited form", "yák̲a", "page 9, in italics"],
                       ["§3.3", "Sechelt", "cited form", "yék̲á", "page 9, in italics"],
                       ["§3.3", A, "note", "Yák̲a seems" + second, kept[at][4]],
                       ["§3.3", "Sechelt", "cited form", "Yák̲a", "page 9, in italics"]]
after("The first of these elements", [["§3.3", "Sechelt", "cited form", "-k̲a", "page 8, in italics"]])
# Footnote 10's English optative carries its source at the right.
for index, row in enumerate(kept):
    if row[0] == "footnote 10 (i) line 1":
        form, source = re.match(r"^(.*?)\s+(\[[^\]]+\])$", row[3]).groups()
        row[3] = form
        kept.insert(index + 1, ["footnote 10 (i) line 2", A, "citation", source,
                                row[4].split(",")[0] + ", the tag or source at the right"])
        break
paper.rows = kept
paper.write()
