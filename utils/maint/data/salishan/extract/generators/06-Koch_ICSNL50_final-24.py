"""The ops of 06-Koch_ICSNL50_final-24: Karsten A. Koch on phrase boundary effects in the duration and
aspiration of /t/ in Nɬeʔkepmxcin, Thompson River Salish: the /t/ of the 1pl marker /kt/ is longer
phrase finally, longer still at an i-phrase boundary, and aspirated longer with an earlier peak there.

Most examples set the phrasing over their tiers, each bracket drawn over the words it takes and
labeled below its closing parenthesis, (   )i-phrase, and (19) and (20) set syntactic brackets,
[PP   ]. Each such printed line is a note whose gloss names the words each bracket stands over on the
page, read from the glyph positions; the example's caption at the right of it is a note, and its tiers
are read the common way under it. Examples (1) and (3) to (5) set a line of column heads over the tiers,
a note. (6) and (16) are lists, a note to each item. Table 1 is a caption, its column heads and a note
to each printed line; Tables 2 to 6 are the same, a row's label cells run onto its figures.
"""
import os
import difflib
import re
import sys
import unicodedata

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Nɬeʔkepmxcin"
AUTHORS = ["Karsten A. Koch"]
paper = gen.Paper("06-Koch_ICSNL50_final-24", authors=", ".join(AUTHORS), language=LANGUAGE)
paper.opening = "auto"

NAMES = [("Flora Ehrhardt", "consultant"), ("Patricia McKay", "consultant"),
         ("Thompson and Thompson", "Laurence C. Thompson and M. Terry Thompson, the grammar (1992) and dictionary (1996)"),
         ("Kroeber", "Paul Kroeber (1997, 1999)"), ("Beck", "David Beck, phrasing in Lushootseed (1996, 1999)"),
         ("Barthmaier", "Paul Barthmeier in the references, intonation units in Okanagan (2004)"),
         ("Caldecott", "Marion Caldecott, prosodic phrases in St’át’imcets (2009)"),
         ("Egesdal", "Steven M. Egesdal, narratives (1984)"), ("Nespor and Vogel", "the prosodic hierarchy (1986)"),
         ("Selkirk and Kratzer", "Elizabeth Selkirk and Angelika Kratzer (2007)"),
         ("Butcher and Harrington", "Andrew Butcher and Jonathan Harrington, Warlpiri (2003)"),
         ("Niebuhr", "Oliver Niebuhr, German /t/ aspiration (2008)"),
         ("Shahin", "Kimary N. Shahin, St’át’imcets gutturals (2003)"),
         ("Bessell", "Nicola J. Bessell, St’át’imcets (1997, 1998)"),
         ("J.H. Davis", "John H. Davis, glides in Comox (2005)"), ("Van Eijk", "Jan P. van Eijk, Lillooet (2001)"),
         ("Esling", "John H. Esling, laryngoscopic studies"), ("Cohen", "Jacob Cohen, effect sizes (1988)")]
LANGUAGES = [(LANGUAGE, "Thompson River Salish, Interior Salish"),
             ("Thompson River Salish", LANGUAGE), ("Thompson Salish", LANGUAGE),
             ("Nłeʔkepmxcin", "spelled with ł on page 8"), ("ƛ̓q̓emcín", "the Lytton dialect of " + LANGUAGE),
             ("Lushootseed", "Central Salish, Beck's p-phrases"), ("Okanagan", "Interior Salish, Barthmaier 2004"),
             ("St’át’imcets", "Lillooet, Interior Salish"), ("Lillooet", "St’át’imcets"),
             ("Nuxalk", "Bella Coola"), ("Bella Coola", "Nuxalk"), ("Cowichan", "Bianco 1996"),
             ("hən’q’əmin’əm’", "Shaw 2002"), ("Upriver Halq’eméylem", "Marinakis 2004"),
             ("Upriver Halkomelem", "Brown and Thompson 2006"), ("Comox", "J.H. Davis 2005"),
             ("Moses-Columbia", "Nxa’amxcín, Czaykowska-Higgins"), ("Nxa’amxcín", "Moses-Columbia Salish"),
             ("Warlpiri", "Butcher and Harrington 2003"), ("Blackfoot", "Frantz 2009, Windsor and Cobler 2013"),
             ("German", "Niebuhr 2008"), ("English", "voiceless stops aspirated as onsets"),
             ("Bengali", "Hayes and Lahiri 1991"), ("Korean", "Schafer and Jun 2002"),
             ("Israeli Sign Language", "Nespor and Sandler 1999"), ("Japanese", "Ishihara 2007")]

# The volume's header opens on In Papers for the International Conference, which
# gen.Paper.volume_header does not take for its opening.
HEADER_FIRST = paper.find(r"^In Papers for the International Conference", 1, 20)
HEADER = list(range(HEADER_FIRST, paper.find(r"\b(?:19|20)\d\d\.$", HEADER_FIRST, HEADER_FIRST + 2) + 1))
paper.volume_header = lambda: HEADER

# gen composes the italic runs; they still part the raised w from its letter, q wac, and each is read as
# the page text sets it so cited() finds it.
ITALIC_W = {"q wac": "qʷac", "ne citxw": "ne citxʷ", "cukw": "cukʷ", "e ʔem’cnxw": "e ʔem’cnxʷ"}
ITALICS = {page: [ITALIC_W.get(run, run) for run in runs] for page, runs in paper.italics().items()}
paper.italics = lambda: ITALICS

FOOTNOTES = paper.page_footnotes()
AT_FOOT = {one for parts, _ in FOOTNOTES.values() for one in parts} | set(HEADER)
RUNNING = paper.running_numbers_set()
REFERENCES = paper.find(r"^References$")
# A printed line of phrasing: brackets drawn over the words, (   )i-phrase, or syntactic ones, [PP   ],
# then what stands at the right of them, the example's caption.
BRACKETS = re.compile(r"^((?:\s*(?:\(|\)[\w-]+|\[[A-Z]+\s*\]))+)\s*(.*)$")


def printed(number):
    return paper.text(number).strip() and not paper.lines[number][2] and number not in AT_FOOT \
        and number not in RUNNING


def after(number):
    number += 1
    while number < REFERENCES and not printed(number):
        number += 1
    return number


def drawn(text):
    """The brackets of a printed line of phrasing and the words at the right of them, or None."""
    found = BRACKETS.match(text)
    if found and re.search(r"[)\]]", found.group(1)):
        return found.group(1).strip(), found.group(2)
    return None


def key(text):
    return [one for one in unicodedata.normalize("NFD", text)
            if not one.isspace() and not unicodedata.combining(one) and unicodedata.category(one) != "Lm"]


def row_of(number):
    """The index of the printed glyph row of line number on its page."""
    line, best, ratio = key(paper.text(number)), None, 0.0
    for index, row in enumerate(paper.glyph_rows(paper.page(number))):
        glyphs = [one for _, symbol in row for one in key(symbol)]
        found = difflib.SequenceMatcher(None, line, glyphs, autojunk=False).ratio()
        if found > ratio:
            best, ratio = index, found
    return best


def spans(brackets, tier, above):
    """The words of line tier each bracket of brackets stands over: the bracket row is the above-th
    row holding a bracket over the tier's row (1 the nearest), an example's own number left out."""
    rows = paper.glyph_rows(paper.page(tier))
    index, seen = row_of(tier), 0
    while seen < above:
        index -= 1
        if any(symbol in "()[]" for _, symbol in rows[index]):
            seen += 1
    glyphs = rows[index]
    text = "".join(symbol for _, symbol in glyphs)
    number = re.match(r"^\(\d+\)", text)
    glyphs = glyphs[len(number.group(0)):] if number else glyphs
    tokens = re.findall(r"\(|\)[\w-]+|\[[A-Z]+\s*\]", brackets)
    labels = [one[1:] if one.startswith(")") else one[1:-1].strip() for one in tokens if one != "("]
    opens = [left for left, symbol in glyphs if symbol in "(["]
    closes = [left for left, symbol in glyphs if symbol in ")]"]
    words = paper.word_positions(tier)
    said = []
    for label, start, end in zip(labels, opens, closes):
        over = [word for left, word in words if start - 1 <= left < end]
        if not over:
            print("# %s over no word on line %d" % (label, tier), file=sys.stderr)
        said.append("%s over %s" % (label, " ".join(over)))
    return said


def phrased(start, where):
    """An example with its phrasing drawn over the tiers: a note to each printed line of brackets, its
    gloss the words each bracket stands over, the caption at the right a note, then the tiers. A line
    of brackets inside the tiers, (23)'s second, stands over the tier line under it too."""
    opened = gen.EXAMPLE.match(paper.text(start))
    label = opened.group(1)
    caption, lines = [], []
    line, text = start, opened.group(2) or ""
    while True:
        found = drawn(text)
        if found:
            lines.append((line, found[0]))
            if found[1]:
                caption.append(found[1])
        elif text:
            caption.append(text)
        line = after(line)
        text = paper.text(line)
        if not drawn(text):
            break
    first = line
    inside = set()
    while not paper.text(line).startswith("‘"):
        if drawn(paper.text(line)):
            lines.append((line, drawn(paper.text(line))[0]))
            inside.add(line)
        line = after(line)
    if caption:
        paper.add("(%s)" % label, A, "note", " ".join(caption),
                  "page %d, the example's caption at the right of its phrasing" % paper.page(start))
    for at, brackets in lines:
        tier = after(at)
        while drawn(paper.text(tier)):
            tier = after(tier)
        above = len([one for one, _ in lines if at <= one < tier])
        square = brackets.startswith("[")
        paper.add("(%s)" % label, A, "note", brackets,
                  "page %d, the %s drawn over the line under it: %s" % (
                      paper.page(at), "syntactic brackets" if square else "phrasing",
                      "; ".join(spans(brackets, tier, above))))
    line = paper.example(first, skip=inside, resume=label)
    # A literal translation in parentheses that wraps, under (23): a note.
    if paper.text(line).startswith("(more literally"):
        text, at = paper.text(line), line
        while text.count("(") > text.count(")"):
            line = after(line)
            text += " " + paper.text(line)
        count = len([one for one in paper.rows if one[0].startswith("(%s) line " % label)])
        paper.add("(%s) line %d" % (label, count + 1), A, "note", text, "page %d, under the translation" % paper.page(at))
        line = after(line)
    return line


def headed(start, where):
    """An example with a line of column heads over its tiers, Verb 2CL Subject Object: a note."""
    opened = gen.EXAMPLE.match(paper.text(start))
    paper.add("(%s)" % opened.group(1), A, "note", opened.group(2),
              "page %d, the column heads over the example" % paper.page(start))
    return paper.example(after(start), resume=opened.group(1))


def listed(start, where):
    """A numbered list, (6) and (16): its caption a note, each lettered item a note with its wrapped
    lines, and each bullet under an item a note, to the paragraph after."""
    opened = gen.EXAMPLE.match(paper.text(start))
    label = opened.group(1)
    end = paper.find(r"^In the present study, I primarily focus" if label == "6" else
                     r"^In addition to descriptive statistics", start)
    paper.add("(%s)" % label, A, "note", opened.group(2), "page %d, the list's caption" % paper.page(start))
    items, letter = [], ""
    for line in range(start + 1, end):
        if not printed(line):
            continue
        text = paper.text(line)
        lettered = gen.SUB.match(text)
        if lettered:
            letter = lettered.group(1)
            items.append(["(%s%s)" % (label, letter), text, line, "an item of the list"])
        elif text.startswith("•"):
            items.append(["(%s%s)" % (label, letter), text[1:].strip(), line, "an item of the bulleted list"])
        else:
            items[-1][1] += " " + text
    for here, text, line, what in items:
        paper.add(here, A, "note", text, "page %d, %s" % (paper.page(line), what))
    return end


def table_1(start, where):
    """Table 1, the inventory: its caption, the consonants' column heads, set a cell over two lines
    where it wraps, a note, each printed line a note, the vowels over the page break under their own
    heads."""
    end = paper.find(r"^Like all Salish languages", start)
    paper.add("Table 1", A, "note", paper.text(start), "page %d, the table's caption" % paper.page(start))
    first = paper.find(r"^Stops ", start)
    heads = [one for one in range(start + 1, first) if printed(one)]
    paper.add("Table 1", A, "note", paper.joined(heads), "page %d, the column heads" % paper.page(start))
    count = 0
    for line in range(first, end):
        if printed(line):
            count += 1
            paper.add("Table 1 line %d" % count, A, "note", paper.text(line), "page %d, Table 1" % paper.page(line))
    return end


def statistics(start, where):
    """Tables 2 to 6: the caption, the column heads a note, then each row a note, its label cells,
    Phrase / internal, run onto its figures, to the blank line or the heading after the table."""
    name = re.match(r"^Table \d+", paper.text(start)).group(0)
    paper.add(name, A, "note", paper.text(start), "page %d, the table's caption" % paper.page(start))
    line, heads = after(start), []
    while not re.match(r"^(?:Phrase|\d )", paper.text(line)):
        heads.append(line)
        line = after(line)
    paper.add(name, A, "note", paper.joined(heads), "page %d, the column heads" % paper.page(start))
    count, pending = 0, []
    while line < REFERENCES and paper.text(line).strip() and not gen.HEADING.match(paper.text(line)):
        text = paper.text(line)
        if re.search(r"\d", text):
            count += 1
            paper.add("%s line %d" % (name, count), A, "note", " ".join(pending + [text]),
                      "page %d, %s" % (paper.page(line), name))
            pending = []
        else:
            pending.append(text)
        line += 1
        while line < REFERENCES and (paper.lines[line][2] or line in RUNNING):
            line += 1
    return line


blocks = {paper.find(r"^Table 1 Phonemic inventory"): table_1}
for number in range(1, REFERENCES):
    if not printed(number):
        continue
    text = paper.text(number)
    if re.match(r"^Table [2-6] ", text):
        blocks[number] = statistics
    opened = gen.EXAMPLE.match(text)
    if not opened or not opened.group(1).isdigit():
        continue
    if opened.group(1) in ("1", "3", "4", "5"):
        blocks[number] = headed
    elif opened.group(1) in ("6", "16"):
        blocks[number] = listed
    elif drawn(opened.group(2) or "") or drawn(paper.text(after(number))):
        blocks[number] = phrased
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)

# The references read again from the page: an entry opens on a surname, Van Eijk's of two words, and its
# year in parentheses, Hayes, Bruce. (1989).; a wrapped line that opens Surname, X, Youmans, eds. or
# Cambridge, MA, runs on the entry above it.
ENTRY = re.compile(r"^(?:Van )?[A-Z][\w’'\-]+, [^()]*\(\d{4}[a-z]?\)\.")
entries = []
for number in range(REFERENCES + 1, paper.last + 1):
    if not printed(number):
        continue
    if ENTRY.match(paper.text(number)) or not entries:
        entries.append([paper.text(number), paper.page(number)])
    else:
        entries[-1][0] += " " + paper.text(number)
first = next(index for index, row in enumerate(paper.rows) if row[2] == "reference")
paper.rows = [row for row in paper.rows if row[2] != "reference"]
paper.rows[first:first] = [["references", A, "reference", text, "page %d" % page] for text, page in entries]
paper.write()
