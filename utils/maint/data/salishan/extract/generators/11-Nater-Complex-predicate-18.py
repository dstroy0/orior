"""The ops of 11-Nater-Complex-predicate-18: Hank Nater on complex predicate-argument relations in
Bella Coola, the benefactive, deprivative, applicative and causative suffixes that link a predicate
with two arguments, their structures, and their Salish sources.

An example sets its sentence segmented, then a line naming the constituent each stretch of it is
(PREDICATE, SUBJ, DIR OBJ, OBL OBJ, GEN ADJ), then the gloss; a sentence too long for one line
repeats the three lines under it. The label line is a note. The translations follow, two readings
set (i) and (ii), and a reading the sentence does not have stands in brackets after the one it has,
(NOT *‘I may go in for the elders’), a note. Examples (14) to (19) set a sentence with the
applicative in a column on the left and the one without it on the right, each column read apart.
Examples (28) to (34) are other Salish languages from Kiyosawa and Gerdts, each named with its
source over it, a citation. Examples (a) to (c) on pages 2 and 3 are lettered, and (c) sets the
predicate's bracketing under its sentence, a note.

Figure 1 is a table of the suffix slots, a note to each of its rows; Figure 11 is a table of the
Bella Coola suffixes and their Salish cognates, a note to each printed line and each form cited
under its column's language. Figures 2 to 10 and 12 are set as text, a note to each printed line
and a caption.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

STEM = "11-Nater-Complex-predicate-18"
A, L = gen.A, gen.L
LANGUAGE = "Bella Coola"
AUTHORS = ["Hank Nater"]
paper = gen.Paper(STEM, authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Kiyosawa & Gerdts", "Kaoru Kiyosawa and Donna B. Gerdts, Salish applicatives as benefactives and malefactives (2010)"),
         ("Kiyosawa", "Kaoru Kiyosawa, applicatives in Salish languages (2006)"),
         ("Kuipers", "Aert H. Kuipers, the Squamish language (1967) and the Salish etymological dictionary (2002)"),
         ("Van Eijk", "Jan van Eijk, the Lillooet-English dictionary (2013)"),
         ("Speck", "Brenda J. Speck, Father Post's Kalispel grammar (1980)"),
         ("Hinkson", "Mercedes Quesney Hinkson, Salishan lexical suffixes (1999)")]
LANGUAGES = [(LANGUAGE, "Nuxalk, Salish"), ("Salish", "the family"), ("Dutch", "the author's analogues, Germanic"),
             ("English", "the language of the translations"), ("proto-Salish", "the parent of the Salish languages"),
             ("Squamish", "Central Salish"), ("Lillooet", "Northern Interior Salish"),
             ("Halkomelem", "Central Salish"), ("Shuswap", "Northern Interior Salish"), ("Comox", "Central Salish"),
             ("Thompson", "Northern Interior Salish"), ("Okanagan", "Southern Interior Salish"),
             ("Interior Salish", "Salish"), ("Coastal Salish", "the author's name for the coast division of Salish")]
OTHER_SALISH = "Salish"

# A form in the prose is Bella Coola's, save a form the text names a language right ahead of, Squamish
# -nǝxʷ, Lillooet -Vn/-Vn', other Salish -mi(n), and those below.
NAMED = {name: name for name, _ in LANGUAGES}
NAMED.update({LANGUAGE: L, "other Salish": OTHER_SALISH})
NAMED_AHEAD = re.compile(r"(?:^|[\s(])(%s)\s*[*(]*[-–]?$" % "|".join(
    re.escape(one) for one in sorted(NAMED, key=len, reverse=True)))
FORM_LANGUAGE = {
    # Page 7: Dutch be-storven ‘having become orphaned or widowed’ ← *be-sterven ← sterven ‘to die’.
    "be-storven": "Dutch", "*be-sterven": "Dutch", "sterven": "Dutch",
    # Page 10: Dutch be- and the sentences that show it.
    "be-": "Dutch", "ze bespreken de zaak": "Dutch", "ze spreken over de zaak": "Dutch",
    "hij bekeek het huis": "Dutch", "hij keek naar het huis": "Dutch",
    # Page 14: in Halkomelem ... -stǝxʷ is here added to a TR base.
    "stǝxʷ": "Halkomelem",
}


def form_language(run):
    # The italic translations of (1) to (3), (i) ‘these people get John ..., and the titles in the
    # references are no forms.
    if "‘" in run or run[:1].isupper() or len(run.split()) >= 4 and run not in FORM_LANGUAGE:
        return None
    return FORM_LANGUAGE.get(run, L)


def language_before(text):
    named = NAMED_AHEAD.search(text[-40:])
    return NAMED[named.group(1)] if named else None


paper.form_language = form_language
paper.language_before = language_before
# The marks the upright type sets at a short form's edge: the ellipsis of …k on page 13.
AS_PRINTED = {(13, "k"): "…k"}
PAGE_TEXT = {}
for number in range(1, paper.last + 1):
    PAGE_TEXT[paper.page(number)] = PAGE_TEXT.get(paper.page(number), "") + " " + paper.text(number)
for page, runs in paper.italics().items():
    text = PAGE_TEXT.get(page, "")
    # A run that sets two forms, -m, -amk on page 2, -nix/-nxʷ/ on page 16, nix -nix across the
    # heading of §2.3.2.2 and the line under it, is each of them.
    runs[:] = [one for run in runs for one in re.split(r", | ~ |/| (?=-)", run) if one]
    for at, run in enumerate(runs):
        # The italic reader sets a space after the syllabic mark that the page does not print,
        # ʔatm̩ nalst for ʔatm̩nalst on page 7.
        closed = re.sub(r"(?<=[̩-]) +", "", run)
        if run not in text and closed in text:
            run = closed
        held = r"(?<![\w’])%s" % re.escape(run)
        # A bracket the italics open and the upright type closes after a hyphen, m(i of -m(i-) on
        # page 15.
        if run.count("(") > run.count(")"):
            run = next((run + tail for tail in (")", "-)") if run + tail in text), run)
        if (page, run) in AS_PRINTED:
            run = AS_PRINTED[page, run]
        elif not re.search(held + r"(?![\w’])", text) and re.search(held + r"’(?![\w’])", text):
            run += "’"
        # A lone letter the paper also sets in brackets, the n of -n and -n- on page 16 and of
        # -alst(n) on page 15, takes the hyphen the italics leave upright where the page never sets
        # the letter on its own; the cited form would take the n in brackets.
        elif len(run) == 1 and "(%s)" % run in " ".join(PAGE_TEXT.values()) and \
                not re.search(r"(?<![\w’(-])%s(?![\w’])" % re.escape(run), text) and \
                re.search(r"(?<![\w’])-%s(?![\w’])" % re.escape(run), text):
            run = "-" + run
        runs[at] = run
    runs[:] = [run for run in runs if "-" + run not in runs]

found = paper.page_footnotes()
SKIP = {one for parts, _ in found.values() for one in parts} | set(paper.volume_header())
RUNNING = paper.running_numbers_set()
BODY_END = paper.find(r"^References$")
HEADINGS = paper.headings(paper.find(r"^Keywords:") + 1, BODY_END - 1, skip=SKIP)
SECTION = {heading: number for number, heading in HEADINGS.items()}

# The line that names the constituent each stretch of the sentence over it is.
LABELS = re.compile(r"^(?:(?:PREDICATE|SUBJ|DIR|OBJ|OB|OBL|GEN|ADJ)(?: |$))+$")
# A reading's number ahead of its translation, (i) ‘these people get John to build a house’.
READING = re.compile(r"^\((i{1,3})\)\s*(?=[‘“])")
# The other Salish examples name their language and source over the sentence.
SOURCED = re.compile(r"^(Halkomelem|Shuswap|Comox|Thompson|Okanagan) (\(.*\))$")
LETTERED = re.compile(r"^\(([a-c])\) (.*)$")


def content(number):
    """Whether line number holds text of the body: no footnote, page mark or running number."""
    return number not in SKIP and number not in RUNNING and not paper.lines[number][2] and paper.text(number)


def translating(text):
    return text.startswith(("‘", "“")) or READING.match(text) is not None


def tiers(label, items, language, count=0, column=""):
    """The rows of one sentence's lines, items [(text, line)]: segmentation, label line, gloss and
    on, then its translations. Returns the rows' count so far."""
    previous = None
    index = 0

    def row(who, kind, form, number, gloss=""):
        nonlocal count
        count += 1
        paper.add("(%s) line %d" % (label, count), who, kind, form,
                  "page %d%s%s" % (paper.page(number), column, ", " + gloss if gloss else ""))

    while index < len(items):
        text, number = items[index]
        if LABELS.match(text):
            row(A, "note", text, number, "the constituent each stretch of the sentence over it is")
            previous = "note"
        elif text.startswith("//"):
            row(language, "underlying", text, number)
            previous = "underlying"
        elif text.startswith("(((") and not translating(text):
            row(A, "note", text, number, "the predicate's bracketing, each suffix with the slot it fills")
            previous = "note"
        elif translating(text):
            # A reading set against the one given runs over the line in its brackets, (NOT *‘did you
            # eat / something on behalf of his mother?’).
            while text.count("(") > text.count(")") and index + 1 < len(items):
                index += 1
                text += " " + items[index][0]
            reading = READING.match(text)
            said = text[reading.end():] if reading else text
            split = gen.split_translation(said) or (said, [])
            row(A, "translation", split[0], number, "reading (%s)" % reading.group(1) if reading else "")
            for piece in split[1]:
                if gen.is_source(piece):
                    row(A, "citation", piece, number, "the source at the right of the translation")
                else:
                    row(A, "note", piece, number, "at the right of the translation, a reading the sentence does not have")
            previous = "translation"
        else:
            following = items[index + 1][0] if index + 1 < len(items) else ""
            if previous in ("note", "segmentation", "underlying"):
                kind = "gloss"
            elif previous == "gloss" and gen.is_gloss(text) and not LABELS.match(following):
                # The gloss runs over a second line, ART Raven ART under (19).
                kind = "gloss"
            else:
                kind = "segmentation"
            row(language, kind, text, number)
            previous = kind
        index += 1
    return count


def lines_of(start):
    """[(text, line)] of the example opening on line start, to the line after its last translation,
    and the line after it."""
    items, number, translated = [], start, False
    while number < BODY_END:
        if not content(number):
            number += 1
            continue
        text = paper.text(number)
        # A reading's (i) opens no example.
        if items and (gen.EXAMPLE.match(text) and not READING.match(text) or LETTERED.match(text)
                      or number in HEADINGS):
            break
        if translated and not translating(text) and not items[-1][0].count("(") > items[-1][0].count(")"):
            break
        translated = translated or translating(text)
        items.append((text, number))
        number += 1
    return items, number


def example(start, where):
    items, after = lines_of(start)
    first = paper.text(start)
    lettered = LETTERED.match(first)
    label, rest = (lettered.group(1), lettered.group(2)) if lettered else gen.EXAMPLE.match(first).groups()
    sourced = SOURCED.match(rest)
    language = L
    if sourced:
        language = sourced.group(1)
        paper.add("(%s) line 1" % label, A, "citation", rest,
                  "page %d, over the example, its language and source" % paper.page(start))
        tiers(label, items[1:], language, count=1)
    else:
        tiers(label, [(rest, start)] + items[1:], language)
    return after


# The heads of the two columns over (14) to (16), with -m / without -m, and over (17) to (19).
HEADS = {}


def heads(start, where):
    """The heads of the two columns, a note."""
    HEADS["with"], HEADS["without"] = paper.text(start).split(" without ")
    HEADS["without"] = "without " + HEADS["without"]
    paper.add(where, A, "note", paper.text(start),
              "page %d, the heads of the two columns of the examples under it" % paper.page(start))
    return start + 1


def paired(start, where):
    """An example set in two columns, the sentence with the applicative on the left and the one
    without it on the right, each read as an example of its own. The right column opens where its
    translation, ‘id.’, stands."""
    items, after = lines_of(start)
    label = gen.EXAMPLE.match(paper.text(start)).group(1)
    same = next(number for text, number in items if text.endswith("‘id.’"))
    split = next(left for left, word in paper.word_positions(same) if word == "‘id.’") - 4
    left, right = [], []
    for text, number in items:
        words = paper.word_positions(number)
        if number == start:
            words = words[1:]
        on_left = " ".join(word for at, word in words if at < split)
        on_right = " ".join(word for at, word in words if at >= split)
        if on_left:
            left.append((on_left, number))
        if on_right:
            right.append((on_right, number))
    count = tiers(label, left, L, column=", the column " + HEADS["with"])
    tiers(label, right, L, count=count, column=", the column " + HEADS["without"])
    return after


def prose(last):
    """A paragraph from its first line to the line matching last, one note, where the page sets a
    line of it that reads as a list item, (ii) ‘A X-s something for/to B on page 4."""
    def write(start, where):
        end = paper.find(last, start)
        lines = [one for one in range(start, end + 1) if content(one)]
        body = paper.joined(lines)
        pages = sorted({paper.page(one) for one in lines})
        paper.add(where, A, "note", body, "page %d" % pages[0])
        paper.cited(where, body, pages)
        paper.mentions(where, body, NAMES, "name")
        paper.mentions(where, body, LANGUAGES, "language")
        return end + 1
    return write


def quote(start, where):
    """Kiyosawa and Gerdts quoted on page 14, set apart and indented a line to a line, one note."""
    end = paper.find(r"\(Kiyosawa & Gerdts, p\. 27\)$", start)
    paper.add(where, A, "note", paper.joined(range(start, end + 1)), "page %d, the quotation" % paper.page(start))
    paper.mentions(where, paper.joined(range(start, end + 1)), NAMES, "name")
    return end + 1


def points(start, where):
    """The paragraph on page 13 that ends on a list of three points, each opening on a bullet: the
    lines over the list a note, and each point a note."""
    first = paper.find(r"^●", start)
    groups = [[number for number in range(start, first) if content(number)]]
    number = first
    while paper.text(number).startswith("●") or not re.match(r"^[A-Z]", paper.text(number)):
        if content(number):
            if paper.text(number).startswith("●"):
                groups.append([])
            groups[-1].append(number)
        number += 1
    for count, lines in enumerate(groups):
        body = paper.joined(lines)
        if count:
            body = body[1:].strip()
        pages = sorted({paper.page(one) for one in lines})
        paper.add(where, A, "note", body, "page %d%s" % (pages[0], ", a point of the list" if count else ""))
        paper.cited(where, body, pages)
        paper.mentions(where, body, NAMES, "name")
        paper.mentions(where, body, LANGUAGES, "language")
    return number


def figure(label, first_line):
    def write(start, where):
        """A figure set as text: a note to each printed line, then its caption."""
        count, number = 0, start
        while not re.match(r"^%s " % label, paper.text(number)):
            if content(number):
                count += 1
                paper.add("%s line %d" % (label, count), A, "note", paper.text(number),
                          "page %d, a line of the figure" % paper.page(number))
            number += 1
        paper.add(label, A, "note", paper.text(number), "page %d, the caption" % paper.page(number))
        return number + 1
    return first_line, write


# Figure 1 on page 2 is a table; the text layer reads the names of rows 2 to 4, aspect, RDR and voice,
# set upright up the page, a letter to a line among the cells. Its rows as the page prints them, each
# row's cells in reading order.
FIGURE_1 = ["BASE", "1 -alst(n) deprivative ← lexical",
            "2 aspect transition – development stative – completive",
            "3 RDR TR -m, -amk applicative causative -nix NC causative ITR causative – communal",
            "4 voice transitive, medium, antipassive reflexive, reciprocal",
            "5 desiderative", "6 inchoative, modifying", "7 -ɬ past", "8 -(s)tu- causative", "9 object",
            "10 subject", "11 -tχʷ optative"]


def figure_1(start, where):
    for count, text in enumerate(FIGURE_1, 1):
        paper.add("Figure 1 line %d" % count, A, "note", text, "page 2, a row of the table as the page prints it")
        paper.cited("Figure 1 line %d" % count, text, [2])
    caption = paper.find(r"^Figure 1 ", start)
    paper.add("Figure 1", A, "note", paper.text(caption), "page %d, the caption" % paper.page(caption))
    return caption + 1


def figure_11(start, where):
    """Figure 11, a table: a note to each printed line, and each form of a cell cited under its
    column's language with the gloss after it, Bella Coola on the left and other Salish on the
    right."""
    count, number = 0, start
    while not paper.text(number).startswith("Figure 11 "):
        if content(number):
            count += 1
            name = "Figure 11 line %d" % count
            paper.add(name, A, "note", paper.text(number), "page %d, a line of the table" % paper.page(number))
            if count > 1:
                for language, cell in zip((L, OTHER_SALISH), re.split(r"\s{3,}", paper.spaced[number])):
                    for forms, gloss in re.findall(r"(\S[^‘]*?)\s*(‘[^’]*’)", cell):
                        for form in forms.split(", "):
                            # The italic run of -(a)min is a)min.
                            paper.cited_done.update((form, form.strip("-"), form.lstrip("-(")))
                            paper.add(name, language, "cited form", form,
                                      "page %d, Figure 11, %s" % (paper.page(number), gloss))
        number += 1
    paper.add("Figure 11", A, "note", paper.text(number), "page %d, the caption" % paper.page(number))
    return number + 1


blocks = {}
for letter in "abc":
    blocks[paper.find(r"^\(%s\) " % letter)] = example
previous = SECTION["2.1"]
for count in range(1, 35):
    at = paper.find(r"^\(%d\) " % count, previous)
    blocks[at] = paired if 14 <= count <= 19 else example
    previous = at + 1
for head in (r"^with -m without -m$", r"^with -amk without -amk$"):
    blocks[paper.find(head)] = heads
blocks[paper.find(r"^X-\(s\)tu-B-A ")] = prose(r"Examples are provided in \(1\)–\(3\)\.$")
blocks[paper.find(r"^“Redirective applicatives")] = quote
blocks[paper.find(r"^-amk is originally complex\.")] = points
blocks[paper.find(r"^BASE$")] = figure_1
blocks[paper.find(r"^Bella Coola Other Salish$")] = figure_11
for label, pattern, after in (("Figure 2", r"^act of X-ing$", SECTION["2.1"]),
                              ("Figure 3", r"^act of X-ing$", SECTION["2.1.1"]),
                              ("Figure 4", r"^act of X-ing$", SECTION["2.1.2"]),
                              ("Figure 5", r"^SUBJ ITR OBL$", SECTION["2.1.2"]),
                              ("Figure 6", r"^experiencing loss$", SECTION["2.2"]),
                              ("Figure 7", r"^experiencing benefit$", SECTION["2.2"]),
                              ("Figure 8", r"^act of X-ing, state of being X$", SECTION["2.3.1"]),
                              ("Figure 9", r"^act of X-ing, state of being X$", SECTION["2.3.2.1"]),
                              ("Figure 10", r"^state of being X$", SECTION["2.3.2.2"]),
                              ("Figure 12", r"^proto-Salish$", SECTION["4"])):
    at, write = figure(label, paper.find(pattern, after))
    blocks[at] = write

# Page 2 opens a paragraph, A Bella Coola verbo-nominal, indented; the page's examples set its
# common left edge further right, and the indent is not read.
paper.paragraph_starts().add(paper.find(r"^A Bella Coola verbo-nominal "))
paper.standard(AUTHORS, NAMES, LANGUAGES, front_languages=1, blocks=blocks, headings=HEADINGS,
               notes_title=tuple(gen.TITLE_MARKS) + ("1",))
# The reference list opens an entry on a single surname; Van Eijk, Jan. (2013) is read into Speck's
# entry before it.
EIJK = " Van Eijk, Jan. (2013)"
speck = next(row for row in paper.rows if row[2] == "reference" and EIJK in row[3])
at = paper.rows.index(speck)
head, tail = speck[3].split(EIJK)
paper.rows[at:at + 1] = [[speck[0], A, "reference", head, speck[4]],
                         [speck[0], A, "reference", EIJK.strip() + tail, speck[4]]]
# The contact line at the foot of page 1 carries no mark, and the footnotes read it as the last line
# of note 1; it is a note of its own.
CONTACT = "Contact info: hanknater@gmail.com"
note = next(row for row in paper.rows if row[0] == "footnote 1")
assert note[3].endswith(" " + CONTACT)
note[3] = note[3][:-len(CONTACT) - 1]
at = paper.rows.index(note) + 1
paper.rows.insert(at, ["front", A, "note", CONTACT, "page 1, the note at the foot of the page with no mark"])
paper.write()
