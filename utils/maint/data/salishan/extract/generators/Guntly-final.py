"""The ops of Guntly-final: Erin A. Guntly on training category expansion with stops and ejectives
in Ktunaxa: a workshop with the Ktunaxa Nation that trained adult learners to hear the velar and
uvular stops and their ejectives, with a pretest and a posttest.

The consonant, vowel and orthography tables, the nonce tokens and the two tables of results are
notes a printed line each. The token triplet of Table 4 and the minimal pairs of Tables 5 to 8 give
each token a cited form and its translation a translation row. A pair's cells are cut at the gaps
the page sets between them, and a cell wrapping onto the next line goes to the column it stands
under. The confusion matrix of Table 11 carries its rotated label, Correct response, a letter or two
to a line among its rows; the label is read off the render.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Ktunaxa"
AUTHORS = ["Erin A. Guntly"]
paper = gen.Paper("Guntly-final", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Vi Birdstone", "speaker, shared her language and the minimal pairs"),
         ("Birdstone", "Vi Birdstone, personal communication"),
         ("Melanie Sam", "Director of Ktunaxa Traditional Knowledge and Language"),
         ("Sam", "Melanie Sam, personal communication"), ("Martina Wiltschko", "acknowledged"),
         ("Morgan", "Lawrence R. Morgan, A Description of the Kutenai Language (1991)"),
         ("Gravelle", "the Kootenay dictionary (1988)"), ("Flege", "second language speech learning (1995)"),
         ("McAuliffe", "the acoustics of Ktunaxa plosives and ejectives (2011)"),
         ("Maddieson", "the phonetics of Tlingit (2001)"), ("Logan", "training Japanese listeners (1991)"),
         ("Lively", "the long-term retention (1994)"), ("Haynes", "adult learners of Numu (2010)"),
         ("Kerswill", "koineization (2008)"), ("Wolfram", "language death (2008)"),
         ("Chambers", "Handbook of Language Variation and Change (2008)")]
LANGUAGES = [(LANGUAGE, "an isolate of the interior of BC, northern Idaho and Montana"),
             ("English", "the learners' first language"), ("Tlingit", "its ejectives compared"),
             ("Japanese", "the listeners trained on /l/ and /ɹ/")]
RUNNING = paper.running_numbers_set()


def printed(number):
    """Whether a line holds the page's print: not blank, not a page's break, not its number."""
    return paper.text(number).strip() and not paper.lines[number][2] and number not in RUNNING


def after(number):
    """The next printed line after number."""
    number += 1
    while not printed(number):
        number += 1
    return number


def caption(start):
    """A table's caption, a note of its own; returns the table's number."""
    number = re.match(r"^Table (\d+):", paper.text(start)).group(1)
    paper.add("Table %s" % number, A, "note", paper.text(start), "page %d, the caption" % paper.page(start))
    return number


# The first line after each plain table, the prose or heading that follows it.
AFTER = {"1": r"^Table 2:", "2": r"^Ktunaxa orthography is roughly", "3": r"^1\.2 Ktunaxa ejectives$",
         "9": r"^Logan et al\. \(1991\) listed", "10": r"^These results are encouraging"}


def lines(start, where):
    """A table read as notes: its caption, then each printed line under it."""
    number = caption(start)
    end = paper.find(AFTER[number], start)
    count = 0
    for line in range(start + 1, end):
        if printed(line):
            count += 1
            paper.add("Table %s line %d" % (number, count), A, "note", paper.text(line),
                      "page %d, Table %s" % (paper.page(line), number))
    return end


def cells(line):
    """[(left, text)] for the cells of a line, cut at the wide gaps the page sets between them."""
    words = iter(paper.word_positions(line))
    found = []
    for cell in (paper.spaced[line] or paper.text(line)).split("   "):
        pieces = cell.split()
        left = next(words)[0]
        for _ in pieces[1:]:
            next(words)
        found.append((left, cell.strip()))
    return found


def triplet(start, where):
    """Table 4: three tokens over their translations, and the source under them."""
    number = caption(start)
    tokens, translations = paper.spaced[start + 1].split("   "), paper.spaced[start + 2].split("   ")
    page = paper.page(start)
    paper.add("Table %s" % number, A, "note", tokens[0], "page %d, Table %s, heads the tokens" % (page, number))
    paper.add("Table %s" % number, A, "note", translations[0].split()[0],
              "page %d, Table %s, heads the translations" % (page, number))
    # The layer runs the first two translations together, cloud glove, over their tokens.
    glosses = " ".join(translations).split()[1:]
    for count, (token, gloss) in enumerate(zip(tokens[1:], glosses), 1):
        at = "Table %s row %d" % (number, count)
        paper.add(at, L, "cited form", token, "page %d, Table %s, the token" % (page, number))
        paper.cited_done.add(token)
        paper.add(at, A, "translation", gloss, "page %d, Table %s, its translation" % (page, number))
    paper.add("Table %s" % number, A, "note", paper.text(start + 3), "page %d, Table %s, under it" % (page, number))
    return start + 4


def pairs(start, where):
    """A table of minimal pairs: Token A, its translation, Token B and its translation. A pair opens
    on a line with a token in its first column; a line with none continues the translations, each
    cell going to the column whose edge it stands nearest."""
    number = caption(start)
    head = start + 1
    page = paper.page(start)
    paper.add("Table %s" % number, A, "note", paper.text(head), "page %d, Table %s, heads the columns" % (page, number))
    rows, edges = [], None
    line = head + 1
    while printed(line) and not re.match(r"^Table \d+:", paper.text(line)):
        found = cells(line)
        if edges is None:
            edges = [left for left, _ in found]
        if len(found) == 4 and abs(found[0][0] - edges[0]) < 10:
            rows.append((line, [[text] for _, text in found]))
        else:
            for left, text in found:
                column = min(range(4), key=lambda index: abs(edges[index] - left))
                rows[-1][1][column].append(text)
        line += 1
    for count, (first, columns) in enumerate(rows, 1):
        at = "Table %s row %d" % (number, count)
        page = paper.page(first)
        for token, pieces, name in ((0, columns[1], "A"), (2, columns[3], "B")):
            form = columns[token][0]
            gloss = " ".join(pieces)
            paper.add(at, L, "cited form", form, "page %d, Table %s, Token %s" % (page, number, name))
            paper.cited_done.add(form)
            paper.add(at, A, "translation", gloss, "page %d, Table %s, Token %s's translation" % (page, number, name))
    return line


# The rotated label of Table 11's rows, read off the render.
LABEL = "Correct response"
FRAGMENTS = {piece[at:at + size] for piece in (LABEL, LABEL[::-1]) for size in (1, 2) for at in range(len(piece))}


def matrix(start, where):
    """Table 11, the confusion matrix: its caption, the label over its columns, the label of its rows
    set rotated, and each printed row a note without the label's letters."""
    number = caption(start)
    page = paper.page(start)
    paper.add("Table %s" % number, A, "note", LABEL, "page %d, Table %s, the label of the rows, set rotated" % (page, number))
    end = paper.find(r"^5 Conclusions$", start)
    count = 0
    for line in range(start + 1, end):
        words = (paper.spaced[line] or paper.text(line)).split("   ")
        if words and words[0].split(" ")[0] in FRAGMENTS:
            head = words[0].split(" ", 1)
            words[0] = head[1] if len(head) > 1 else ""
        text = "   ".join(word for word in words if word)
        if text.strip():
            count += 1
            paper.add("Table %s line %d" % (number, count), A, "note", text, "page %d, Table %s" % (page, number))
    return end


def figure(start, where):
    """A figure's caption, a note of its own; the spectrogram holds no text."""
    number = re.match(r"^Figure (\d+):", paper.text(start)).group(1)
    paper.add("Figure %s" % number, A, "note", paper.text(start), "page %d, the caption" % paper.page(start))
    return start + 1


blocks = {}
for line in range(1, paper.last + 1):
    opened = re.match(r"^(Table|Figure) (\d+):", paper.text(line))
    if opened and opened.group(1) == "Figure":
        blocks[line] = figure
    elif opened:
        blocks[line] = {"4": triplet, "11": matrix}.get(opened.group(2), lines if opened.group(2) in AFTER else pairs)
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)
paper.write()
