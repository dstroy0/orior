"""The ops of 2011_Laturnus: Rebecca Laturnus on the two future preverbs of Ktunaxa, tsxaɬ and ts
(written ¢xaⱡ and ¢), argued to be modals that differ as weak and strong epistemic future, not as
distant and proximate future, with tsxaɬ felicitous as an offer and ts not.

Each example sets its words, segmented, over a gloss and a translation in quotes, under a context and
over the speaker's comments. The text layer left the Times New Roman letters of its Identity-H fonts
blank; page_text reads them mended from the font program.
"""
import re
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Ktunaxa"
AUTHORS = ["Rebecca Laturnus"]
paper = gen.Paper("2011_Laturnus", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Dryer", "Matthew S. Dryer, preverbs in Kutenai and Algonquian (2002) and Kutenai in areal perspective (2007)"),
         ("Morgan", "Lawrence R. Morgan, a description of the Kutenai language (1991)"),
         ("Celle", "Agnès Celle, the French future and English will as markers of epistemic modality (2005)"),
         ("Copley", "Bridget Copley, the semantics of the future (2002)"),
         ("Matthewson", "Lisa Matthewson, temporal semantics in a supposedly tenseless language (2006)"),
         ("Glougie", "Jennifer Glougie, future expressions in St'at'imcets (2007, 2008)"),
         ("Toews", "Carmela Toews, Siamou future expressions (2010)"),
         ("John", "the one named in (10), (11), (16) and (17)"),
         ("Vi Birdstone", "the author's consultant, thanked"),
         ("Martina Wiltschko", "thanked for guidance"), ("Carmela Towes", "thanked"),
         ("Lisa Matthewson", "thanked"), ("Emily Blamire", "thanked")]
LANGUAGES = [(LANGUAGE, "a language isolate of south-eastern British Columbia, northern Idaho and "
              "north-western Montana"),
             ("English", "its will and be going to"),
             ("Algonquian", "suggested to be areally related to Ktunaxa")]
OPENS = re.compile(r"^\((\d+)\)\s*(.*)$")
RIGHT = re.compile(r"^(.*’)\s+(\(.*\))$")


def blank(number):
    return number > paper.last or not paper.text(number).strip()


def example(start, where):
    """An example under the context it answers, where it has one: the context's lines a note, then
    the words, segmented, the gloss and the translation, with the source at its right a citation,
    and the speaker's comments under it a note, to the blank line."""
    number, count = start, 0
    context = []
    while not OPENS.match(paper.text(number)):
        context.append(number)
        number += 1
    label, words = OPENS.match(paper.text(number)).groups()
    name = "(%s)" % label

    def row(who, kind, text, gloss):
        nonlocal count
        count += 1
        paper.add("%s line %d" % (name, count), who, kind, text, gloss)
        return "%s line %d" % (name, count)

    if context:
        row(A, "note", paper.joined(context), "page %d, the context the example answers" % paper.page(start))
    page = paper.page(number)
    row(L, "transcription", words, "page %d" % page)
    row(L, "gloss", paper.text(number + 1), "page %d" % page)
    said = RIGHT.match(paper.text(number + 2))
    row(A, "translation", said.group(1) if said else paper.text(number + 2), "page %d" % page)
    if said:
        row(A, "citation", said.group(2), "page %d, the source at the right of the translation" % page)
    number += 3
    if paper.text(number).startswith("Speaker comments:"):
        last = number
        # The comments end at the blank line, or at the page's number where the page ends on them.
        while not blank(last + 1) and not re.fullmatch(r"\d+", paper.text(last + 1).strip()):
            last += 1
        text = paper.joined(range(number, last + 1))
        at = row(A, "note", text, "page %d, the speaker's comments under the example" % page)
        paper.mentions(at, text, NAMES, "name")
        number = last + 1
    return number


def display(start, where):
    """Dryer's formula for the verbal complex, (3), set apart over two lines."""
    paper.add("(3) line 1", A, "note", re.sub(r"^\(3\)\s*", "", paper.joined([start, start + 1])),
              "page %d, the verbal complex after Dryer (2002), set as a display" % paper.page(start))
    return start + 2


def opens(number):
    """An example opens on its number with its translation two lines down; a wrapped reference to
    one, (9). or (2) the hearer, does not."""
    return bool(OPENS.match(paper.text(number))) and paper.text(number + 2).startswith("‘")


blocks = {}
for number in range(1, paper.last + 1):
    text = paper.text(number)
    if text.startswith("Context:"):
        blocks[number] = example
    elif text.startswith("(3) Verb Complex"):
        blocks[number] = display
    elif opens(number) and not any(paper.text(one).startswith("Context:") for one in range(max(1, number - 4), number)):
        blocks[number] = example
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)
rows = paper.rows
# The abstract, under the university, is one note.
lines = [index for index, row in enumerate(rows) if row[0] == "front" and row[2] == "note"
         and row[4] == "page 1, under Rebecca Laturnus"][1:]
if lines:
    rows[lines[0]][3] = " ".join(rows[index][3] for index in lines)
    rows[lines[0]][4] = "page 1, the abstract"
    rows = [row for index, row in enumerate(rows) if index not in lines[1:]]
# Footnote 1's mark opens a line of page 6, "in the present / 1, depending on the context", and the
# placer takes the p.1 of Dryer's page for it; the note follows the paragraph that carries it.
note = [row for row in rows if row[0].startswith("footnote 1")]
rows = [row for row in rows if not row[0].startswith("footnote 1")]
at = next(index for index, row in enumerate(rows) if row[2] == "note" and "in the present 1, depending" in row[3])
rows[at + 1:at + 1] = note
# The Jóhannsdóttir and Matthewson entry's last line, Amherst, MA: GLSA., starts flush with the next
# entry's and the reference reader gives it to Kootenay Culture Committee.
for index, row in enumerate(rows):
    if row[2] == "reference" and row[3].startswith("Amherst, MA: GLSA. "):
        rows[index - 1][3] += " Amherst, MA: GLSA."
        row[3] = row[3][len("Amherst, MA: GLSA. "):]
# §1 gives each preverb's pronunciation in brackets after it, ¢xaⱡ [t͡sxaɬ] and ¢ [t͡s].
at = next(index for index, row in enumerate(rows) if row[2] == "cited form" and row[3] == "¢") + 1
rows[at:at] = [["§1", L, "cited form", "[t͡sxaɬ]", "page 1, in brackets, the pronunciation of ¢xaⱡ"],
               ["§1", L, "cited form", "[t͡s]", "page 1, in brackets, the pronunciation of ¢"]]
# The author's name and address close page 7 under the references.
signature = " Rebecca Laturnus laturnusr@gmail.com"
if rows[-1][3].endswith(signature):
    rows[-1][3] = rows[-1][3][:-len(signature)]
    rows.append(["references", A, "note", signature.strip(), "page 7, the author's name and address under the references"])
paper.rows = rows
paper.write()
