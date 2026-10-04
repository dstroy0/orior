"""The ops of 2012_Jacobs: Peter Jacobs's Vowel harmony and schwa strengthening in Sḵwx̱wu7mesh, the
vowel V before the directive -n taken as an epenthetic schwa, its copy vowel realizations derived by
vowel harmony, phonemic and allophonic, and its stressed /á/ by schwa strengthening, a schwa or its
coda too light for the foot.

Page text read by glyph rows.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Sḵwx̱wu7mesh"
AUTHORS = ["Peter Jacobs"]
paper = gen.Paper("2012_Jacobs", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Jason Brown", "thanked for helping make the thoughts and writing clearer"),
         ("Dyck", "Ruth Anne Dyck, Squamish stress assignment and the directive's copy vowel (2004)"),
         ("Kuipers", "Aert H. Kuipers, the Squamish grammar (1967) and lexicon (1969, 1989)"),
         ("Demers", "Richard A. Demers, with Horn, stress assignment in Squamish (1978)"),
         ("Horn", "George M. Horn, with Demers (1978)"),
         ("Shaw", "Patricia A. Shaw et al., stress in hunkaminum (Musqueam) (1999)"),
         ("Suttles", "Wayne P. Suttles, the Musqueam reference grammar (2004)")]
LANGUAGES = [(LANGUAGE, "Central Salish, Squamish in its own name, the language of the paper"),
             ("Squamish", "Sḵwx̱wu7mesh"), ("Skwxwu7mesh", "Sḵwx̱wu7mesh, as the references spell it"),
             ("Musqueam", "Halkomelem, the Downriver dialect"), ("hunkaminum", "Musqueam in its own name"),
             ("Halkomelem", "Central Salish"), ("Shuswap", "Interior Salish, Kuipers's report (1989)"),
             ("Salish", "the family"), ("English", "the language of the translations")]
# Three marks stand glued to a form whose letters hold digits of their own: yúts'-u-n2 in (1a),
# x̱íts-ḵ-e-n-(t)-Ø4 in (6a) and ?lheḵ'el9 in (43f).
GLUED = {"2": "yúts’-u-n", "4": "x̱íts-ḵ-e-n-(t)-Ø", "9": "?lheḵ’el"}
# The paper sets an example's first letter against its number, (10)a. lhich'-i-t, or without its
# stop, (4) a na t'am̓-á-n-t-m; the common reader looks for the number and the letter apart, each
# with its stop. The label is read so here, and the rows keep the form as printed.
for number, (text, page, marker) in enumerate(paper.lines):
    if text and re.match(r"^\(\d+\)(?:[a-h]\.?\s|\s+[a-h]\s)", text):
        paper.lines[number] = (re.sub(r"^(\(\d+\))\s*([a-h])\.?\s+", r"\1 \2. ", text), page, marker)
# (72a) on page 39 draws the blank of its translation, `‘Let’s ________’`, a line below the quotes
# (render at 110 dpi); the common reader takes the blank for a line of prose.
BLANK = paper.find(r"^‘Let’s\s+’$")
paper.spaced[BLANK] = "‘Let’s ________’"
paper.lines[BLANK] = (paper.spaced[BLANK],) + tuple(paper.lines[BLANK][1:])
paper.spaced[BLANK + 1] = ""
paper.lines[BLANK + 1] = ("",) + tuple(paper.lines[BLANK + 1][1:])
SKIP = {one for parts, _ in paper.page_footnotes().values() for one in parts} | set(paper.volume_header()) | \
    paper.running_numbers_set()
LETTER = re.compile(r"^([a-k])(?:\.\s*|\s{2,})(.*)$")


def start_of(label):
    """The line example label opens on."""
    return paper.find(r"^\(%s\)(?:\s|$)" % label)


def printed(start, end):
    """[(line, text)] of the body from start to the line before end, passing footnotes, page numbers
    and page marks."""
    return [(number, paper.spaced[number]) for number in range(start, end)
            if number not in SKIP and not paper.lines[number][2] and paper.text(number).strip()]


def after(start, pattern):
    """The first line after start that matches pattern."""
    number = start + 1
    while not re.search(pattern, paper.text(number) or ""):
        number += 1
    return number


def kind_of(cell):
    """A table cell's kind: a quoted translation, a phonetic form in brackets, a root, a rule, a
    parse into syllables and feet, a gloss with its labels in capitals, or else a form."""
    if cell.startswith("‘"):
        return "translation"
    if re.fullmatch(r"\[[^\]]*\](?:\s*~\s*\[[^\]]*\])*", cell):
        return "phonetic"
    if cell.startswith("√"):
        return "root"
    if re.search(r"-{3,}>|_{3,}", cell):
        return "rule"
    if re.fullmatch(r"[*✓✗#]?\S*", cell) and re.search(r"\([^()\s]*\.[^()\s]*\)|\)\(", cell):
        return "parse"
    if re.search(r"\b[A-Z]{2,}|\d[A-Z]", cell):
        return "gloss"
    return "transcription"


def split_label(text, label, letter):
    """The text of a printed line without the example's number and letter, and the letter now."""
    text = re.sub(r"^\(%s\)\s*" % label, "", text)
    lettered = LETTER.match(text)
    if lettered:
        return lettered.group(2), lettered.group(1)
    return text, letter


def table(end, heads=None, first=None, whole=None):
    """A table set as an example, to the line matching end: each printed line a row to each of its
    cells, set apart by wide gaps, a cell's kind read from its shape. A line matching heads is the
    example's heading or its column heads, a note; first names the kind of each line's first cell,
    the description beside a form in (39); whole, the kind of each line taken whole, the rules of
    (33)."""
    def block(start, where):
        label = re.match(r"\((\d+)\)", paper.text(start)).group(1)
        stop = after(start, end)
        letter, lines = "", {}
        for number, text in printed(start, stop):
            text, letter = split_label(text, label, letter)
            if not text:
                continue
            here = "(%s%s)" % (label, letter)
            lines[here] = lines.get(here, 0) + 1
            where = "%s line %d" % (here, lines[here])
            page = "page %d" % paper.page(number)
            if heads and re.search(heads, text):
                paper.add(where, A, "note", re.sub(r"\s+", " ", text), page + ", a heading in the example")
                continue
            if whole:
                paper.add(where, L, whole, re.sub(r"\s+", " ", text), page)
                continue
            for index, cell in enumerate(re.split(r"\s{3,}", text)):
                kind = first if first and index == 0 else kind_of(cell)
                paper.add(where, A if kind in ("translation", "note") else L, kind, cell, page)
        return stop
    return block


def diminutives(end):
    """(21) to (23), a base form and its English beside the diminutive and its English. The English
    wraps under itself; a line holding only a form is a second diminutive, tsi7-tsáw̓in in (22h). A
    word the layer holds twice at the end of a line, of) dog dog in (22a) and little bullhead
    bullhead in (22g), is set once on the page (300 dpi renders of pages 12 and 13)."""
    def block(start, where):
        label = re.match(r"\((\d+)\)", paper.text(start)).group(1)
        stop = after(start, end)
        items, heads = [], []
        letter = ""
        for number, text in printed(start, stop):
            text, now = split_label(text, label, letter)
            if now != letter:
                letter = now
                items.append([letter, number, [text]])
            elif items:
                items[-1][2].append(text)
            else:
                heads.append((number, text))
        for index, (number, text) in enumerate(heads, 1):
            paper.add("(%s) line %d" % (label, index), A, "note", re.sub(r"\s+", " ", text),
                      "page %d, %s" % (paper.page(number), "the example's heading" if index == 1 else "the column heads"))
        for letter, number, texts in items:
            cells = [one for one in re.split(r"\s{3,}", texts[0]) if one]
            tokens = " ".join(cells).split()
            base = tokens[0]
            at = next(index for index, token in enumerate(tokens) if index and "-" in token)
            english, diminutive, rest = " ".join(tokens[1:at]), tokens[at], " ".join(tokens[at + 1:])
            others = []
            for text in texts[1:]:
                parts = [one for one in re.split(r"\s{3,}", text) if one]
                if len(parts) == 1 and " " not in parts[0] and "-" in parts[0]:
                    others.append(parts[0])
                elif not english and len(parts) == 2:
                    english, rest = parts
                else:
                    rest = (rest + " " + parts[0]).strip()
            rest = re.sub(r"\b(\w+) \1$", r"\1", rest)
            page = "page %d" % paper.page(number)
            rows = [("transcription", base, ", the base form"), ("word gloss", english, ", the base form's English"),
                    ("transcription", diminutive, ", the diminutive")] + \
                [("transcription", one, ", the diminutive as also said") for one in others] + \
                [("word gloss", rest, ", the diminutive's English")]
            for line, (kind, text, why) in enumerate([row for row in rows if row[1]], 1):
                paper.add("(%s%s) line %d" % (label, letter, line), A if kind == "word gloss" else L, kind, text,
                          page + why)
        return stop
    return block


def columns(end):
    """(41) to (44) and (81), each lettered form beside its reduplicated form, with their glosses and
    translations under them in two columns, to the line matching end. A line holding one cell runs on
    the right column's line above it, a translation's end; (43f) sets its one
    translation, ‘to make inquiries’, under the reduplicated form (render of page 23)."""
    return lambda start, where: two_columns(start, after(start, end))


def two_columns(start, stop):
    label = re.match(r"\((\d+)\)", paper.text(start)).group(1)
    letter, heads, items = "", [], []
    for number, text in printed(start, stop):
        text, now = split_label(text, label, letter)
        if now != letter:
            letter = now
            items.append([letter, []])
        if not items:
            heads.append((number, text))
            continue
        parts = [one for one in re.split(r"\s{3,}", text) if one]
        tiers = items[-1][1]
        if len(parts) == 2:
            tiers.append([number, parts[0], parts[1]])
        elif parts[0].startswith("‘") and not (tiers and tiers[-1][2].startswith("‘") and not tiers[-1][2].endswith("’")):
            tiers.append([number, "", parts[0]])
        elif tiers and len(parts) == 1:
            # residue's correction for the paper has already carried a piece set before a hyphen up
            # to the line that opens it.
            if not tiers[-1][2].endswith(parts[0]):
                tiers[-1][2] += ("" if parts[0].startswith("-") else " ") + parts[0]
        else:
            tiers.append([number, "", text])
    for index, (number, text) in enumerate(heads, 1):
        paper.add("(%s) line %d" % (label, index), A, "note", re.sub(r"\s+", " ", text),
                  "page %d, %s" % (paper.page(number), "the example's heading" if index == 1 else "the column heads"))
    for letter, tiers in items:
        for side, column in ((1, "the non-reduplicated column"), (2, "the reduplicated column")):
            line = 0
            for tier, row in enumerate(tiers):
                if not row[side]:
                    continue
                kind = "transcription" if tier == 0 else kind_of(row[side])
                kind = "gloss" if kind == "transcription" and tier else kind
                line += 1
                paper.add("(%s%s) %s line %d" % (label, letter, "left" if side == 1 else "right", line),
                          A if kind == "translation" else L, kind, row[side], "page %d, %s" % (paper.page(row[0]), column))
    return stop


def bounded(end):
    """An example the common reader runs on into the prose or the heading after it: read to the line
    before the first that matches end."""
    def block(start, where):
        stop = after(start, end)
        paper.example(start, last=stop - 1)
        return stop
    return block


def listing(end):
    """A list set as an example, (76) and (77): a note to each lettered item."""
    def block(start, where):
        stop = after(start, end)
        paper.display(start, stop - start)
        return stop
    return block


BLOCKS = {start_of("11"): table(r"^As with unstressed"),
          start_of("20"): lambda start, where: paper.display(start, 2, per_line=True),
          start_of("21"): diminutives(r"^This next set"),
          start_of("22"): diminutives(r"^I have found one"),
          start_of("23"): diminutives(r"^The first example in"),
          start_of("27"): table(r"^One problem for analyzing"),
          start_of("29"): table(r"^The partially reduced form \(29b\)", heads=r"form of the clitic"),
          start_of("30"): table(r"^The same vowel", heads=r"form of the clitic|^Vowel harmony with"),
          start_of("31"): table(r"^The partially reduced form \(31b\)", heads=r"form of the clitic"),
          start_of("32"): table(r"^With these clitics", heads=r"form of the clitic|from of the clitic"),
          start_of("33"): table(r"^Note in particular", whole="rule"),
          start_of("34"): table(r"^\(35\)", heads=r"^/[a-z]/$|^\[[^\]]+\]$"),
          start_of("35"): table(r"^\(36\)", heads=r"^/[a-z]/$|^\[[^\]]+\]$"),
          start_of("36"): table(r"^3\.2\.2", heads=r"^/[a-z]/$|^\[[^\]]+\]$"),
          start_of("37"): table(r"^In the first set"),
          start_of("39"): table(r"^\(40\)", heads=r"^Domains for", first="note"),
          start_of("40"): table(r"^4\s", heads=r"^Domains for", first="note"),
          start_of("41"): columns(r"^\(42\)"), start_of("42"): columns(r"^In all the examples so far"),
          start_of("43"): columns(r"^\(44\)"), start_of("44"): columns(r"^The initial vowels of the base"),
          start_of("81"): columns(r"^In examples \(81a-b\)"),
          start_of("59"): table(r"^In example \(59a\)"),
          start_of("72"): table(r"^Importantly, for our discussion"),
          start_of("69"): bounded(r"^5\.1\.6"),
          start_of("70"): table(r"^\(71\)"),
          start_of("71"): table(r"^From our account"),
          start_of("73"): table(r"^However, if the subjunctive", whole="rule"),
          start_of("76"): listing(r"^Only two cases"),
          start_of("77"): listing(r"^In by far"),
          start_of("86"): bounded(r"^5\.2\.3")}
paper.standard(AUTHORS, NAMES, LANGUAGES, glued=GLUED, blocks=BLOCKS)

LINE = re.compile(r"^(\(\d+[a-k]?\)) line (\d+)$")
TRAILING_PARSE = re.compile(r"^([^()\s][^()]*?)\s+([*✓✗]?\(.*)$")
BRACKETS = re.compile(r"\[[^\]]*\](?:\s*~\s*\[[^\]]*\])*")


def mended(rows):
    """The common reader's rows of the examples it read, set right: a form beside its parse into
    feet, sát-a-n (sá.tan) in (57) to (69), a parse and its translation, (tl’íy.ya7) ‘to stop’ in
    (66a), and a form beside its phonetic forms, sí-siḵ [sé·sɛq] in (75a), each split in two; a line
    all parse, (mikw’)(shnám̓.chen) in (61b), and the derivation of (47) typed; a translation's
    parenthetical the reader split off, ‘to hook (something) in (57b), run back on; and the heading
    of (2), DIR in small capitals, a note."""
    out = []
    for row in rows:
        where, who, kind, form, gloss = row
        numbered = LINE.match(where)
        if not numbered or kind not in ("transcription", "note"):
            out.append(row)
            continue
        before = out[-1] if out else None
        if kind == "note" and form.startswith("(") and before and before[2] == "translation" and \
                LINE.match(before[0]).group(1) == numbered.group(1) and not before[3].endswith("’"):
            before[3] += " " + form
            continue
        if kind == "note":
            out.append(row)
        elif where == "(2) line 1":
            out.append([where, A, "note", form, gloss + ", the example's heading"])
        elif re.search(r"-{3,}>", form):
            out.append([where, who, "rule", form, gloss])
        elif kind_of(form) == "parse":
            out.append([where, who, "parse", form, gloss])
        elif re.fullmatch(r"(\S*\([^)]*\)\S*)\s+(‘.*’)", form):
            parse, english = form.rsplit(" ‘", 1)
            out += [[where, who, "parse", parse, gloss], [where, A, "translation", "‘" + english, gloss]]
        elif TRAILING_PARSE.match(form) and kind_of(TRAILING_PARSE.match(form).group(2).replace(" ", "")) == "parse":
            head, parse = TRAILING_PARSE.match(form).groups()
            out += [[where, who, kind, head, gloss], [where, who, "parse", parse, gloss]]
        elif " [" in form and not form.startswith("["):
            head, rest = form.split(" [", 1)
            out.append([where, who, kind, head, gloss])
            out += [[where, who, "phonetic", one, gloss] for one in BRACKETS.findall("[" + rest)]
        else:
            out.append(row)
    return out


def renumbered(rows):
    """The lines of each example counted again from 1 once a run-on line is gone, the cells of one
    printed line keeping one number."""
    seen = {}
    for row in rows:
        numbered = LINE.match(row[0])
        if numbered:
            lines = seen.setdefault(numbered.group(1), {})
            lines.setdefault(numbered.group(2), len(lines) + 1)
            row[0] = "%s line %d" % (numbered.group(1), lines[numbered.group(2)])
    return rows


def footnote_examples(rows):
    """Footnote 8's example of a nominalized clause, on page 16, and footnote 11's five forms of the
    root chá-, on page 28, which the common reader runs into the footnote's note: each a row a tier,
    the note keeping the prose before them."""
    out = []
    for row in rows:
        if row[0] == "footnote 8" and row[2] == "note" and "complement: " in row[3]:
            out.append(row[:3] + [row[3].split("complement: ")[0] + "complement:", row[4]])
            start = paper.find(r"^chen ta7áw̓n")
            for line, kind in enumerate(("transcription", "gloss", "translation"), 1):
                out.append(["footnote 8 line %d" % line, A if kind == "translation" else L, kind,
                            paper.text(start + line - 1), "page %d" % paper.page(start)])
        elif row[0] == "footnote 11" and row[2] == "note" and "include: " in row[3]:
            out.append(row[:3] + [row[3].split("include: ")[0] + "include:", row[4]])
            start = paper.find(r"^i\. chá-nem")
            for number in range(start, start + 5):
                numeral, form, english, gloss = re.match(r"^([iv]+)\.\s+(\S+)\s+(.*?)\s+(\(\S*\))$",
                                                         paper.text(number)).groups()
                where = "footnote 11 (%s) line %%d" % numeral
                cells = [("transcription", L, form)] + \
                    [("translation", A, one) for one in re.split(r"(?<=’),\s+(?=‘)", english)] + [("gloss", L, gloss)]
                for line, (kind, who, text) in enumerate(cells, 1):
                    out.append([where % line, who, kind, text, "page %d" % paper.page(number)])
        else:
            out.append(row)
    return out


def references(rows):
    """Kuipers's 1969 and 1989 entries, set under a rule for his name, each an entry of its own, and
    the author's name and address under the last entry, on page 48, a note of the paper's end."""
    out = []
    for row in rows:
        if row[2] != "reference":
            out.append(row)
            continue
        text, signed = re.match(r"^(.*?)(?:\s+(Peter Jacobs pejacobs@uvic\.ca))?$", row[3]).groups()
        out += [row[:3] + [one, row[4]] for one in re.split(r"\s+(?=———\.)", text)]
        if signed:
            out.append(["end", A, "note", signed, row[4]])
    return out


paper.rows = references(footnote_examples(renumbered(mended(paper.rows))))
paper.write()
