"""The ops of 6_ICSNL2018_MarshallBird: Mackenzie Marshall and Sonya Bird on the rhythm of
Hul'q'umi'num', measured on 3.54 minutes of a story told by Bernard David (Tl'isla): by the vocalic
metrics %V, ΔV and VarcoV the language patterns with the stress-timed languages, English and Dutch;
by the consonantal metrics ΔC and VarcoC it patterns with no documented language.

(1) sets four words and phrases, each beside its translation. Table 1 is the consonant inventory,
each consonant a row; Table 2 sets the metrics of five languages. Figures 2 to 6 plot the languages
over two metrics, each axis's title and values and each point's label a note, read off the render.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Hul’q’umi’num’"
AUTHORS = ["Mackenzie Marshall", "Sonya Bird"]
paper = gen.Paper("6_ICSNL2018_MarshallBird", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Bernard David", "Tl’isla, of Stz’uminus, the Elder who told the story"),
         ("Margaret Seymour", "his granddaughter, to whom the story was told; thanked"),
         ("Donna Gerdts", "linguist, transcribed the story with Ruby Peter; thanked; Gerdts and Werle (2014)"),
         ("Delores Louie", "translated the story with Ruby Peter; thanked"),
         ("Ruby Peter", "translated and transcribed the story; thanked"),
         ("Thomas Jones", "thanked"),
         ("Parker", "A. Parker, Salish schwa (2011)"),
         ("Werle", "A. Werle, Halkomelem clitic types, with Gerdts (2014)"),
         ("James", "A. L. James, the first to describe rhythm differences (1940)"),
         ("Pike", "K. L. Pike, the intonation of American English (1945)"),
         ("Abercrombie", "D. Abercrombie, elements of general phonetics (1967)"),
         ("Trubetzkoy", "N. S. Trubetzkoy, principles of phonology (1969)"),
         ("Nazzi", "T. Nazzi, language discrimination by newborns, with Bertoncini and Mehler (1998)"),
         ("Bertoncini", "J. Bertoncini, with Nazzi and Mehler (1998)"),
         ("Mehler", "J. Mehler, with Nazzi and Bertoncini (1998), Ramus and Dupoux (2003), Ramus and Nespor (1999)"),
         ("Ramus", "F. Ramus, the metrics ΔV, ΔC and %V (1999), the reality of rhythm classes (2003)"),
         ("Dupoux", "E. Dupoux, with Ramus and Mehler (2003)"),
         ("Rathcke", "T. V. Rathcke, speech timing and rhythm, with Smith (2015)"),
         ("Smith", "R. H. Smith, with Rathcke (2015)"),
         ("Dauer", "R. M. Dauer, stress-timing and syllable-timing (1983, 1987)"),
         ("Nespor", "M. Nespor, with Ramus and Mehler (1999)"),
         ("Dellwo", "V. Dellwo, the variation coefficients VarcoV and VarcoC (2006)"),
         ("Grabe", "E. Grabe, acoustic correlates of rhythm class, with Low (2002)"),
         ("Low", "E. L. Low, the pairwise variability index (2000), with Grabe (2002)"),
         ("White", "L. White, calibrating rhythm, with Mattys (2007)"),
         ("Mattys", "S. L. Mattys, with White (2007)"),
         ("Ordin", "M. Ordin, rhythm in second languages, with Polyanskaya (2014, 2015)"),
         ("Polyanskaya", "L. Polyanskaya, with Ordin (2014, 2015)"),
         ("Boersma", "P. Boersma, Praat, with Weenink (2017)"),
         ("Weenink", "David Weenink, Praat, with Boersma (2017)"),
         ("Wang", "Q. Wang, the acoustic phonetics lab manual, with Bird, Onosson and Benner (2015)"),
         ("Onosson", "S. Onosson, with Bird, Wang and Benner (2015)"),
         ("Benner", "A. Benner, with Bird, Wang and Onosson (2015)"),
         ("Payne", "E. Payne, measuring child rhythm (2012)"),
         ("Kell", "S. Kell, pronunciation in SENĆOŦEN revitalization, with Bird (2017)"),
         ("Leonard", "J. Leonard, SENĆOŦEN story telling, with Bird and Czaykowska-Higgins (2012)"),
         ("Czaykowska-Higgins", "E. Czaykowska-Higgins, with Bird and Leonard (2012)"),
         ("Warner", "N. Warner, stops and flaps in spontaneous speech, with Tucker (2011)"),
         ("Tucker", "B. V. Tucker, with Warner (2011)")]
LANGUAGES = [(LANGUAGE, "Central Salish; Halkomelem, Island dialect"),
             ("SENĆOŦEN", "Central Salish; Northern Straits"),
             ("English", "stress-timed"), ("Dutch", "stress-timed"), ("French", "syllable-timed"),
             ("Spanish", "syllable-timed")]
RUNNING = paper.running_numbers_set()


def printed(number):
    return paper.text(number).strip() and not paper.lines[number][2] and number not in RUNNING


def after(number):
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def words(first, where):
    """(1): its title, wrapping to a second line, then each word or phrase beside its translation."""
    title = paper.text(first)[len("(1) "):] + " " + paper.text(after(first))
    paper.add("(1) line 1", A, "note", title, "page %d, the example's title" % paper.page(first))
    line, count = after(after(first)), 1
    while re.search(r"‘[^’]*’$", paper.text(line)):
        form, translation = re.match(r"^(.*?)\s*(‘[^’]*’)$", paper.text(line)).groups()
        count += 1
        paper.add("(1) line %d" % count, L, "transcription", form, "page %d, clusters in bold" % paper.page(line))
        count += 1
        paper.add("(1) line %d" % count, A, "translation", translation, "page %d, beside the form" % paper.page(line))
        line = after(line)
    return line


def inventory(first, where):
    """Table 1: its caption, then each printed line of consonants a note."""
    page = paper.page(first)
    paper.add("Table 1", A, "note", paper.text(first), "page %d, the caption" % page)
    line = after(first)
    for count in range(1, 8):
        paper.add("Table 1 line %d" % count, A, "note", paper.text(line), "page %d, a line of the inventory" % page)
        line = after(line)
    return line


def metrics(first, where):
    """Table 2: its caption, then each printed line a note, the metric and its values."""
    page = paper.page(first)
    paper.add("Table 2 line 1", A, "note", paper.text(first), "page %d, the caption" % page)
    line = after(first)
    for count in range(2, 8):
        paper.add("Table 2 line %d" % count, A, "note", paper.text(line), "page %d, the metric and its values" % page)
        line = after(line)
    return line


def chart(first, where):
    """A figure plotting the languages over two metrics: the y axis's title and its values, top to
    bottom, each point's label, the x axis's values and its title, then the caption. The text layer
    reads the chart by lines across it, the rotated title of the y axis a letter or two to a line
    among the values and labels; the rows are read off the render."""
    caption = paper.find(r"^Figure \d+:", first)
    number = re.match(r"^Figure (\d+):", paper.text(caption)).group(1)
    page = paper.page(caption)
    # A chart at the head of its page opens on its top value, which gen takes for a running number.
    while not paper.lines[first - 1][2] and paper.page(first - 1) == page and \
            re.fullmatch(r"\d{1,3}", paper.text(first - 1)):
        first -= 1
    title = Y_TITLES[number]
    paper.add("Figure %s, the y axis" % number, A, "note", title, "page %d, the title of the y axis, set rotated" % page)
    # The letters of the rotated title stand among the values and labels; each printed row of the
    # chart is a note without them.
    # The layer reads the rotated title from the foot up, oc in VarcoC.
    fragments = {piece[at:at + size] for piece in (title, title[::-1]) for size in (1, 2) for at in range(len(piece))}
    count = 0
    for line in range(first, caption):
        # The values of the y axis stand alone on their lines, set like page numbers.
        if not paper.text(line).strip() or paper.lines[line][2]:
            continue
        text = " ".join(word for word in paper.text(line).split() if word not in fragments)
        if text:
            count += 1
            paper.add("Figure %s line %d" % (number, count), A, "note", text,
                      "page %d, a row of the chart, its value on the y axis and the labels beside it" % page)
    paper.add("Figure %s" % number, A, "note", paper.text(caption), "page %d, the caption" % page)
    return after(caption)


# The title of each chart's y axis, read off the render; the text layer sets it a letter or two to a
# line. The label Hul'q'umi'num' wraps, in Figure 2 after nu and in Figures 4 to 6 before its final
# straight quote.
Y_TITLES = {"2": "ΔC", "3": "VarcoC", "4": "ΔV", "5": "VarcoV", "6": "ΔC"}


def screenshot(first, where):
    """Figure 1, a screenshot of the Praat textgrid the text layer holds nothing of: its caption."""
    paper.add("Figure 1", A, "note", paper.text(first), "page %d, the caption of the screenshot" % paper.page(first))
    return after(first)


blocks = {paper.find(r"^\(1\) Complex consonant"): words,
          paper.find(r"^Figure 1: Praat"): screenshot,
          paper.find(r"^Table 1:"): inventory,
          paper.find(r"^Table 2:"): metrics}
# Each chart opens on its highest value on the y axis, the line after the prose before it.
for pattern in (r"^English and Dutch according to %V\.$", r"^these metrics\.$", r"^group clearly with either class"):
    for line in range(1, paper.last + 1):
        if re.match(pattern, paper.text(line)):
            blocks[after(line)] = chart
blocks[after(paper.find(r"^other languages\.$"))] = chart
blocks[after(paper.find(r"^with other stress-timed languages according to these metrics\.$"))] = chart
FOUND = paper.page_footnotes()
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)
# Note 2's mark stands on %V2:, a capital and a digit that gen reads as a label; its rows go after
# the paragraph that carries it.
count = len(paper.rows)
parts, at = FOUND["2"]
paper.footnote("2", parts, at, NAMES, LANGUAGES, gloss="page %d, footnote 2" % at)
note = paper.rows[count:]
del paper.rows[count:]
carrier = next(index for index, row in enumerate(paper.rows) if "%V2:" in row[3])
while paper.rows[carrier + 1][0] == paper.rows[carrier][0] and paper.rows[carrier + 1][2] != "note":
    carrier += 1
paper.rows[carrier + 1:carrier + 1] = note
paper.write()
