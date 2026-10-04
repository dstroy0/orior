"""The ops of 13_KSardinha_Deriving-Eventual-ity-Types-in-Kwak’wala: Katie Sardinha on three Kwak’wala
suffixes that derive Greene's (2013) eventuality types, -aɬa States, -la Processes and -xʔid
Transitions, and the readings of duqʷ- ‘see’ predicates with each.

The interlinear examples are read by gen.Paper.example. (1), (2), (4) and (6) set two readings
under their gloss, i. and ii., each a translation with its source. A Speaker: line under (7), (17)
and (26) is a speaker comment with its source; (17) and (26) have no other English. (16), (18) and
(19) open on an exchange, each turn of KS (the author),
HG (Hannah Greene) and the speaker a row of its own, before the tiers. The lists (9) to (12) and
(21) are a note for the title and, for each item, the verb and its gloss, and each root or suffix
in the parentheses after it with its gloss. (15) and (40) to (42) are formulas; (22), (38) and (39)
a note to each lettered part, (38a) a formula. Table 1 is a note to each printed line. Tables 2 and
3 are a note to each column head and, for each suffix or predicate, a row to each cell, wrapped
cells joined. The page's bold and italic are not in the text layer.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Kwak’wala"
AUTHORS = ["Katie Sardinha"]
paper = gen.Paper("13_KSardinha_Deriving-Eventual-ity-Types-in-Kwak’wala", authors=", ".join(AUTHORS),
                  language=LANGUAGE)

SPEAKER = "Kwak’wala language consultant, thanked in the note on the title"
NAMES = [("Violet Bracic", SPEAKER), ("Mildred Child", SPEAKER), ("Ruby Dawson Cranmer", SPEAKER),
         ("Lily Johnny", SPEAKER), ("Julia Nelson", SPEAKER),
         ("Hannah Greene", "the author of the verb classes the paper builds on, HG in the examples"),
         ("Line Mikkelsen", "thanked in the note on the title")]
LANGUAGES = [(LANGUAGE, "Northern Wakashan"), ("Wakashan", "the family, a keyword"),
             ("English", "the translations"), ("Finnish", "a language with no grammatical perfective")]
REFERENCES = paper.find(r"^References$")
RUNNING = paper.running_numbers_set()
ENGLISH = {"the", "of", "and", "to", "in", "is", "that", "for", "as", "with", "are", "be", "by", "this",
           "which", "we", "on", "it", "not", "or", "from", "can", "an", "these", "has", "have"}
# The section headings. 4.1, 4.2 and 5.1 to 5.3 open on a suffix or a form in lower case, and the
# appendices on a letter.
HEADINGS = {paper.find(pattern): label for label, pattern in (
    ("1", r"^1 Introduction$"), ("2", r"^2 Greene \(2013\) on"), ("3", r"^3 The proposal:"),
    ("4", r"^4 Greene’s \(2013\) analysis"), ("4.1", r"^4\.1 "), ("4.2", r"^4\.2 "), ("5", r"^5 Evidence from"),
    ("5.1", r"^5\.1 "), ("5.2", r"^5\.2 "), ("5.3", r"^5\.3 "), ("5.4", r"^5\.4 "), ("6", r"^6 Conclusion$"),
    ("Appendix A", r"^Appendix A: "), ("Appendix B", r"^Appendix B: "))}


def printed(number):
    """Whether line number holds printed text: page breaks, page numbers and blank lines hold none."""
    return not paper.lines[number][2] and paper.text(number) and number not in RUNNING


def after(number):
    """The next printed line after number."""
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def row(label, count, who, kind, form, number, gloss=None):
    paper.add("(%s) line %d" % (label, count), who, kind, form,
              "page %d%s" % (paper.page(number), ", " + gloss if gloss else ""))


def prose(text):
    """Whether a line is running prose: nine words or more, three of them English function words."""
    words = text.split()
    return len(words) >= 9 and sum(1 for one in words if one.lower().strip(",.;:()") in ENGLISH) >= 3


def table(start, where):
    """Table 1: its caption and each printed line under it a note, to the prose after it."""
    paper.add("Table 1", A, "note", paper.text(start), "page %d, the table's caption" % paper.page(start))
    line, count = after(start), 0
    while line < REFERENCES and not prose(paper.text(line)):
        count += 1
        paper.add("Table 1 line %d" % count, A, "note", paper.text(line), "page %d, Table 1" % paper.page(line))
        line = after(line)
    return line


# Tables 2 and 3: the column heads, then the body, each run as one line and cut at each row's first
# cell. The heads of Table 3 share a printed line, Eventuality type English.
GRIDS = {
    "Table 2": (r"^-", r"^(Suffix form\(s\)) (Eventuality Type .*?\(my analysis\)) (Gloss in Boas \(1911, 1947\)) "
                r"(Analysis .*)$",
                r"(-\S+(?:, -\S+)?) (State|Process|Transition) ((?:‘[^’]*’ ?)+) ?(.*?)"
                r"(?= -\S+(?:, -\S+)? (?:State|Process|Transition) |$)"),
    "Table 3": (r"^duqʷaɬa", r"^(duqʷ- ‘see’ predicate) (Eventuality type) (English translation\(s\)) (Description)$",
                r"(du\S+(?: ʷ\S+)?) (derived (?:State|Process|Transition)) ((?:‘[^’]*’,? ?)+) ?(.*?)"
                r"(?= du\S+(?: ʷ\S+)? derived |$)")}


def grid(start, where):
    """Table 2 or 3: the caption a note, each column head a note, and for each row of the body its
    first cell a language row (a suffix a cited affix, each form of -xʔid its own; a predicate a
    transcription), its type and its last column a note, and each quoted gloss a note (Boas's glosses
    in Table 2) or a translation (Table 3)."""
    name = re.match(r"^(Table \d+)", paper.text(start)).group(1)
    first, heads, body = GRIDS[name]
    paper.add(name, A, "note", paper.text(start), "page %d, the table's caption" % paper.page(start))
    line, head, rows = after(start), [], []
    while not re.match(first, paper.text(line)):
        head.append(paper.text(line))
        line = after(line)
    while line < REFERENCES and not prose(paper.text(line)):
        rows.append(paper.text(line))
        line = after(line)
    page = paper.page(start)
    columns = re.match(heads, " ".join(head)).groups()
    for column in columns:
        paper.add("%s line 1" % name, A, "note", column, "page %d, %s, a column head" % (page, name))
    for count, cells in enumerate(re.findall(body, " ".join(rows)), 2):
        label = "%s line %d" % (name, count)
        form, kind, glosses, last = cells
        for one in form.split(", "):
            paper.add(label, L, "cited affix" if name == "Table 2" else "transcription", one,
                      "page %d, %s, %s" % (page, name, columns[0]))
        paper.add(label, A, "note", kind, "page %d, %s, %s" % (page, name, columns[1]))
        for gloss in re.findall(r"‘[^’]*’", glosses):
            paper.add(label, A, "note" if name == "Table 2" else "translation", gloss,
                      "page %d, %s, %s" % (page, name, columns[2]))
        paper.add(label, A, "note", last, "page %d, %s, %s" % (page, name, columns[3]))
    return line


def readings(start, where):
    """(1), (2), (4) or (6): the tiers, then each reading under them, i. and ii., a translation
    with its source at the right."""
    label = gen.EXAMPLE.match(paper.text(start)).group(1)
    first = paper.find(r"^i\.\s", start)
    paper.example(start, last=first - 1)
    line = first
    while gen.ROMAN_PART.match(paper.text(line)):
        roman, said = gen.ROMAN_PART.match(paper.text(line)).groups()
        said, pieces = gen.split_translation(said)
        row(label + roman, 1, A, "translation", said, line, "reading %s." % roman)
        for count, piece in enumerate(pieces, 2):
            row(label + roman, count, A, "citation", piece, line, "the source at the right of the reading")
        line = after(line)
    return line


def spoken(start, where):
    """(7), (17) or (26): the example to the Speaker: line under it, then that line, with the source
    at its right or on the line under it."""
    speaker = paper.find(r"^Speaker:", start)
    paper.example(start, last=speaker - 1)
    label, count = re.match(r"^\((\w+)\) line (\d+)$", paper.rows[-1][0]).groups()
    count = int(count)
    translated = any(one[0].startswith("(%s) line " % label) and one[2] == "translation" for one in paper.rows)
    said, source = re.match(r"^(Speaker:\s*“[^”]*”)\s*(\(.*\))?$", paper.text(speaker)).groups()
    count += 1
    row(label, count, A, "speaker comment", said, speaker,
        "the speaker's words under the example%s" % ("" if translated else ", its only English"))
    line = after(speaker)
    if source:
        count += 1
        row(label, count, A, "citation", source, speaker, "the source at the right of the speaker's words")
    elif re.match(r"^\([^()]*\)$", paper.text(line)):
        count += 1
        row(label, count, A, "citation", paper.text(line), line, "under the speaker's words")
        line = after(line)
    return line


TURNS = {"Context": ("note", "the context"), "KS": ("note", "the author's words, KS in the examples"),
         "HG": ("note", "Hannah Greene's words, HG in the examples"),
         "Speaker": ("speaker comment", "the speaker's answer")}


def exchange(start, where):
    """(16), (18) or (19): its context and each turn of the exchange over it a row, a turn run on to
    its closing quote and the context to its stop, then the tiers or the lettered parts under them."""
    label, text = gen.EXAMPLE.match(paper.text(start)).groups()
    line, turns = start, []
    while True:
        opened = re.match(r"^(Context|KS|HG|Speaker):", text)
        held = turns[-1][1] if turns else ""
        if opened:
            turns.append([opened.group(1), text, line])
        elif turns and (held.count("“") > held.count("”") or
                        turns[-1][0] == "Context" and not re.search(r"[.\]]$", held)):
            turns[-1][1] += " " + text
        else:
            break
        line = after(line)
        text = paper.text(line)
    for count, (who, said, at) in enumerate(turns, 1):
        kind, gloss = TURNS[who]
        row(label, count, A, kind, said, at, gloss)
    written = len(paper.rows)
    number = paper.example(line, resume=label)
    # The tiers count on from the turns over them.
    for one in paper.rows[written:]:
        numbered = re.match(r"^\(%s\) line (\d+)$" % label, one[0])
        if numbered:
            one[0] = "(%s) line %d" % (label, int(numbered.group(1)) + len(turns))
    return number


def listed(start, where):
    """(9) to (12) or (21): the title a note, and for each item the verb a transcription and its gloss
    a translation, then each root (a suffix a cited affix) in the parentheses after it with its gloss.
    An item runs on to the line that closes its parentheses."""
    label, title = gen.EXAMPLE.match(paper.text(start)).groups()
    row(label, 1, A, "note", title, start, "the list's title")
    line, items = after(start), []
    while True:
        text = paper.text(line)
        sub = gen.SUB.match(text)
        if sub:
            items.append([sub.group(1), sub.group(2), line])
        elif items and items[-1][1].count("(") > items[-1][1].count(")"):
            items[-1][1] += " " + text
        else:
            break
        line = after(line)
    for letter, text, at in items:
        parts = []
        if text.endswith(")"):
            depth = 0
            for index in range(len(text) - 1, -1, -1):
                depth += {")": 1, "(": -1}.get(text[index], 0)
                if depth == 0:
                    break
            parts = re.split(r",\s+(?=\S+\s+‘)", text[index + 1:-1])
            text = text[:index].strip()
        verb, gloss = text.split(None, 1)
        row(label + letter, 1, L, "transcription", verb, at)
        row(label + letter, 2, A, "translation", gloss, at)
        count = 2
        for part in parts:
            form, meaning = re.match(r"^(\S+)\s+(‘.*)$", part).groups()
            # A lexical suffix carries the root's sign too, √-ʔstu in (12a).
            affix = form.lstrip("√").startswith("-")
            count += 1
            row(label + letter, count, L, "cited affix" if affix else "root", form, at,
                "the %s in the parentheses after the verb" % ("suffix" if affix else "root"))
            count += 1
            row(label + letter, count, A, "translation", meaning, at,
                "the gloss of the %s" % ("suffix" if affix else "root"))
    return line


def formula(start, where):
    """(15) and (40) to (42): the formula, and (15)'s source on the line under it. (15) carries the
    mark of footnote 6, which place_footnotes looks for after a form: the row is written as a
    transcription and made a formula once the footnotes stand."""
    label, text = gen.EXAMPLE.match(paper.text(start)).groups()
    row(label, 1, A, "transcription" if label == "15" else "formula", text, start)
    line = after(start)
    if re.match(r"^\([^()]*\)$", paper.text(line)):
        row(label, 2, A, "citation", paper.text(line), line, "under the formula")
        line = after(line)
    return line


PARTS = {"22": ("note", "an English sentence set as a lettered part"),
         "38": ("note", "Dowty's (1979) note on the operator, quoted"),
         "39": ("note", "a definition from Dowty (1979)")}


def lettered(start, where):
    """(22), (38) or (39): a context over the parts a note, and each lettered part a note run on to
    the line that ends on a stop or a closing parenthesis; (38a) is a formula, and the page cited at
    the right of (38b) a citation."""
    label, text = gen.EXAMPLE.match(paper.text(start)).groups()
    line, parts = start, []
    while True:
        sub = gen.SUB.match(text)
        if sub:
            parts.append([label + sub.group(1), sub.group(2), line])
        elif not parts and gen.CONTEXT.match(text):
            parts.append([label, text, line])
        elif parts and not re.search(r"[.)]$", parts[-1][1]):
            parts[-1][1] += " " + text
        else:
            break
        line = after(line)
        text = paper.text(line)
    for part, said, at in parts:
        if part == label:
            row(part, 1, A, "note", said, at, "the context")
        elif part == "38a":
            row(part, 1, A, "formula", said, at, "DO as Dowty (1979) defines it")
        else:
            cited = re.match(r"^(.*”)\s+(\(pg\. \d+\))$", said)
            kind, gloss = PARTS[label]
            row(part, 1, A, kind, cited.group(1) if cited else said, at, gloss)
            if cited:
                row(part, 2, A, "citation", cited.group(2), at, "the page of Dowty (1979) at the right")
    return line


def at(label):
    return paper.find(r"^\(%s\) " % label)


blocks = {paper.find(r"^Table 1: "): table, paper.find(r"^Table 2: "): grid, paper.find(r"^Table 3: "): grid}
for labels, reader in ((("1", "2", "4", "6"), readings), (("7", "17", "26"), spoken),
                       (("16", "18", "19"), exchange), (("9", "10", "11", "12", "21"), listed),
                       (("15", "40", "41", "42"), formula), (("22", "38", "39"), lettered)):
    blocks.update({at(label): reader for label in labels})
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, headings=HEADINGS)

# (15) is a formula.
next(one for one in paper.rows if one[0] == "(15) line 1")[2] = "formula"
# A one-word wrap whose transcription and segmentation read the same, q̓ʷəmdzuy̓u over q̓ʷəmdzuy̓u
# over dress in (17), has no translation under it for example() to count back from, and its three
# tiers read segmentation, gloss, segmentation: they are the transcription, its segmentation and
# its gloss.
for index in range(1, len(paper.rows) - 2):
    over, three = paper.rows[index - 1], paper.rows[index:index + 3]
    if [one[2] for one in three] == ["segmentation", "gloss", "segmentation"] and over[2] == "gloss" and \
            three[0][3] == three[1][3] and len({one[0].split(" line ")[0] for one in [over] + three}) == 1:
        for one, kind in zip(three, ("transcription", "segmentation", "gloss")):
            one[2] = kind
# Page 6 breaks the suffix at the line's end, illustrating - / aɬa as a stativizer; the note keeps the
# break's space as flowed, and the cited form is the suffix whole.
next(one for one in paper.rows if one[2] == "cited form" and one[3] == "- aɬa")[3] = "-aɬa"
# Footnote 9's example (i) closes on the speaker's words and their source, which footnote() runs
# into the note after the example.
index = next(index for index, one in enumerate(paper.rows)
             if one[0] == "footnote 9" and one[3].startswith("Speaker:"))
where, who, kind, form, gloss = paper.rows[index]
said, source = re.match(r"^(Speaker:\s*“[^”]*”)\s+(\(.*\))$", form).groups()
count = max(int(one[0].rsplit(" ", 1)[1]) for one in paper.rows if one[0].startswith("footnote 9 (i) line "))
paper.rows[index:index + 1] = [
    ["footnote 9 (i) line %d" % (count + 1), A, "speaker comment", said, gloss + ", the speaker's words"],
    ["footnote 9 (i) line %d" % (count + 2), A, "citation", source, gloss + ", at the right of the speaker's words"]]
paper.write()
