"""The ops of van-Eijk_SubObNPa_final: Subject and object NPs in a Lillooet text collection, by Jan
P. van Eijk: the order of subject and object phrases, PSO against POS, in two northern Lillooet
dictionaries and in Qwa7yán'ak's St'át'imcets narratives, and where the two orders may come from.

Each example opens on its number and the page of its source, (1, p. 6), where the page's
examples open on a bare number; they are read by hand. A sentence may wrap onto a line or two, and
is one transcription; its translation, which may wrap too, is one row, a second quoted reading a
row of its own, and the author's comment set after the closing quote, (kélhen ‘to take off’), a
note. The page of the source is a citation row, named for the work the section takes it from.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
L = gen.L
LANGUAGE = "Lillooet"
AUTHORS = ["Jan P. van Eijk"]
paper = gen.Paper("van-Eijk_SubObNPa_final", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Qwa7yán’ak", "Carl Alexander, the speaker of the narratives in Callahan et al. 2016"),
         ("Carl Alexander", "Qwa7yán’ak"), ("Thompson", "Lawrence C. Thompson (1979)"),
         ("Hess", "Thom Hess (1973)"), ("Hukari", "Hukari 1976"), ("Kroeber", "Paul D. Kroeber (1999)"),
         ("Davis", "Henry Davis"), ("Henry Davis", "referred the author to the sound files"),
         ("Kuipers", "Aert H. Kuipers (1967)"), ("Callahan", "Callahan et al. 2016"),
         ("John Lyon", "translated the narratives in Alexander et al. 2016"),
         ("Arlotto", "Anthony Arlotto (1972)"), ("Teit", "James A. Teit (1906)")]
LANGUAGES = [(LANGUAGE, "Northern Interior Salish"), ("St’át’imcets", "Lillooet, in its own name"),
             ("Lushootseed", "Central Salish"), ("Halkomelem", "Central Salish"), ("Straits", "Central Salish"),
             ("Bella Coola", "Salish"), ("Squamish", "Central Salish"), ("Russian", "Slavic"),
             ("Turkish", "Turkic"), ("Persian", "Iranian")]
# The work each run of examples is quoted from, by the first number of the run.
SOURCES = [(1, "Upper St’át’imc Language, Culture and Education Society 1995"),
           (13, "Frank and Whitley 2000"), (19, "Callahan et al. 2016"), (42, "Alexander et al. 2016")]
RUNNING = paper.running_numbers_set()
FOUND = paper.page_footnotes()
SKIP = {one for parts, _ in FOUND.values() for one in parts}
LABEL = re.compile(r"^\((\d{1,2})(?:, (p\. \d+))?\)\s+(\S.*)$")


def printed(number):
    return paper.text(number).strip() and not paper.lines[number][2] and number not in RUNNING \
        and number not in SKIP


def source(number):
    return [name for first, name in SOURCES if first <= number][-1]


def depth(text):
    return text.count("(") - text.count(")")


def example(start, where):
    """One example: its sentence to the line opening its translation, then the translation's lines,
    each opening lowercase, on a parenthesis or on a quote, to the first line of prose."""
    number, cited, first = LABEL.match(paper.text(start)).groups()
    label = "(%s)" % number
    page = paper.page(start)
    sentence, line = [first], start + 1
    while not paper.text(line).startswith("‘"):
        sentence.append(paper.text(line))
        line += 1
    paper.add(label + " line 1", L, "transcription", " ".join(" ".join(sentence).split()), "page %d" % page)
    if cited:
        paper.add(label + " line 1", A, "citation", cited, "the page of %s" % source(int(number)))
    readings, end = [], line
    while line <= paper.last:
        if not printed(line):
            if paper.lines[line][2] or line in SKIP or line in RUNNING:
                line += 1
                continue
            break
        text = paper.text(line)
        if LABEL.match(text):
            break
        if text.startswith("‘") and not (readings and depth(readings[-1][1])):
            readings.append([line, text])
        elif readings and re.match(r"^[a-z(‘]", text):
            readings[-1][1] += " " + text
        else:
            break
        line += 1
        end = line
    for at, (opens, text) in enumerate(readings, 2):
        text = " ".join(text.split())
        comment = re.match(r"^(‘.*’)\s+(\(.*\))$", text)
        said = comment.group(1) if comment else text
        paper.add("%s line %d" % (label, at), A, "translation", said, "page %d" % paper.page(opens))
        if comment:
            paper.add("%s line %d" % (label, at), A, "note", comment.group(2),
                      "page %d, the author's comment after the translation" % paper.page(opens))
    return end


def caption(start, where):
    """Figure 1's caption, a line of its own under the scan of the draft it names: a note."""
    paper.add("Figure 1", A, "note", paper.text(start), "page %d, the caption" % paper.page(start))
    return start + 1


blocks = {one: example for one in range(1, paper.last + 1) if LABEL.match(paper.text(one)) and printed(one)}
blocks[paper.find(r"^Figure 1 Example")] = caption
# Two of the headings hold the stop of vs., which the heading pattern reads as a sentence's.
HEADINGS = {paper.find(r"^1\s+Introduction$"): "1", paper.find(r"^2\s+Lillooet PSO vs\. POS$"): "2",
            paper.find(r"^3\s+PSO vs\. POS: recent insights$"): "3",
            paper.find(r"^4\s+Preliminary conclusions$"): "4"}
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, headings=HEADINGS)
# The references run over two page breaks and set their entries at a page's own margin, 43 on an
# even page and 65 on an odd one, each wrap of an entry some 28 points in; footnote 7's last lines
# stand between the first entry and its wrap. An entry opens at its page's margin.
start = paper.find(r"^References$")
lines = [one for one in range(start + 1, paper.last + 1) if printed(one) and paper.word_positions(one)]
margins = {}
for one in lines:
    margins[paper.page(one)] = min(margins.get(paper.page(one), 10 ** 6), paper.word_positions(one)[0][0])
entries = []
for one in lines:
    if paper.word_positions(one)[0][0] < margins[paper.page(one)] + 10 or not entries:
        entries.append([paper.page(one), paper.text(one)])
    else:
        entries[-1][1] += " " + paper.text(one)
first = next(at for at, row in enumerate(paper.rows) if row[2] == "reference")
last = max(at for at, row in enumerate(paper.rows) if row[2] == "reference")
paper.rows[first:last + 1] = [["references", A, "reference", " ".join(text.split()), "page %d" % page]
                              for page, text in entries]
# A translation's footnote mark, ‘…(kwtamts)’1, places the note after it and then comes off the form.
for row in paper.rows:
    mark = re.search(r"(?<=’)(\d{1,2})$", row[3]) if row[2] == "translation" else None
    if mark and mark.group(1) in FOUND:
        row[3] = row[3][:mark.start()]
        row[4] += ", carries footnote " + mark.group(1)
paper.write()
