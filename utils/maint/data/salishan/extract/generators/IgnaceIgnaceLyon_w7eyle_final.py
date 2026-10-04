"""The ops of IgnaceIgnaceLyon_w7eyle_final: W7éyle, the Moon's Wife (Wala), a stsptekwll of Teit's
The Shuswap (1909) told back into Secwepemctsín by the Skeetchestn elders' group of Marianne Ignace,
John Lyon and Ronald E. Ignace, with its astronomical and ecological knowledge and the grammar of the
telling.

The story is fourteen stanzas, (1) to (14). Each opens on its unbroken line in the practical
orthography, a transcription that runs on to the line ending its sentence; cascading pairs follow, a
segmentation over its gloss; and the English translation closes the stanza, opening on a capital
where a segmentation would stand, every segmentation of the story opening in lower case. The page
draws the glottal mark as an apostrophe over its letter, and page_text sets it on its letter from
PAPER_OVERSET.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Secwepemctsín"
AUTHORS = ["Marianne Ignace", "John Lyon", "Ronald E. Ignace"]
paper = gen.Paper("IgnaceIgnaceLyon_w7eyle_final", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("W7éyle", "the Moon's wife, the protagonist; Wala in Teit"),
         ("Sxwéy̓lecken", "storyteller of Big Bar and Dog Creek, recorded by Teit; the likely teller"),
         ("Sisyúl̓ecw", "storyteller of Simpcw, recorded by Teit"),
         ("Teit", "James A. Teit, The Shuswap (1909) and other works"),
         ("Boas", "Franz Boas, The Shuswap (1890) and other works"),
         ("Dawson", "G. M. Dawson, Notes on the Shuswap People (1891)"),
         ("Henry Tate", "Boas's associate"), ("George Hunt", "Boas's associate"),
         ("John Swanton", "Haida texts (1905, 1908)"), ("William Beynon", "field notebooks (Anderson and Halpin 2000)"),
         ("Kuipers", "Aert H. Kuipers, The Shuswap Language (1974) and later works"),
         ("Seymour Pitel", "storyteller recorded by Kuipers"), ("Charlie Draney", "storyteller; the Trout Children epic"),
         ("Lena Bell", "storyteller recorded by Kuipers"),
         ("Randy Bouchard", "recorded stories in the 1970s; Bouchard and Kennedy (1979)"),
         ("Dorothy Kennedy", "recorded stories in the 1970s; Bouchard and Kennedy (1979)"),
         ("Ike Willard", "storyteller"), ("Aimee August", "storyteller"),
         ("Ida William", "Sisyúlecw's grand-daughter, storyteller"), ("Louisa Basil", "St̓uxtéws storyteller"),
         ("Bridget Dan", "proof-read the stsptekwll"), ("Cecilia DeRose", "proof-read the stsptekwll"),
         ("Clara Camille", "proof-read the stsptekwll"), ("Mary Thomas", "elder; the term tellqelm̓úcw"),
         ("Christine Simon", "elder; birch-bark shovels"), ("Braden Hallett", "drew Figure 1"),
         ("Daniel Calhoun", "speaker consulted on the stative -t"), ("Ron Ignace", "Ronald E. Ignace"),
         ("Wendy Wickwire", "on Teit's work (1994, 1998, 2001)"), ("Kroeber", "P. Kroeber (1999)"),
         ("Armstrong", "J. Armstrong (2009)"), ("Matthewson", "L. Matthewson, evidentials (2007)"),
         ("Andrei Anghelescu", "an editor of the volume"), ("Michael Fry", "an editor of the volume"),
         ("Marianne Huijsmans", "an editor of the volume"), ("Daniel Reisinger", "an editor of the volume")]
LANGUAGES = [(LANGUAGE, "Northern Interior Salish (Shuswap), its Western dialect"),
             ("Secwépemc", "the people and their language"), ("Shuswap", "Secwepemctsín"),
             ("St’át’imcets", "Northern Interior Salish (Lillooet)"), ("Chinook Jargon", "a trade language"),
             ("English", "the translations")]

# The paper defines some of its italic forms in plain letters, stsptekwll, pelltsitcwem and lu7; the
# other plain italics are titles, the English deeds, and a lone 7 on page 4.
PLAIN = {"stsptekwll", "stsptekwle", "stspekwll", "pelltsitcwem", "Pelltsitcwem", "pell", "tsitcw",
         "tell", "lu7", "<7>"}
# Footnote 8 on page 11 cites the suffixes ekwe and enke bare; the paragraph of §5 that runs from
# page 11 onto page 12 holds them only as -ekwe and -enke, which page 12 sets in italics.
BARE = ("ekwe", "enke")
paper.form_language = lambda run: L if gen.orthographic(run) or run in PLAIN else None
RUNNING = paper.running_numbers_set()
AT_FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
# The stanza's unbroken line ends on its sentence's stop, a footnote's mark after it, (10).
ENDED = re.compile(r"[.?!](?:\s+\d)?$")


def printed(number):
    return bool(paper.text(number).strip()) and not paper.lines[number][2] and number not in RUNNING \
        and number not in AT_FOOT


def after(number):
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def stanza(start, where):
    """A stanza: its unbroken line a transcription, then each cascading pair a segmentation and a
    gloss, then the English translation to the next stanza or heading."""
    opened = gen.EXAMPLE.match(paper.text(start))
    label, count = opened.group(1), 0

    def row(who, kind, text, at, gloss=""):
        nonlocal count
        count += 1
        paper.add("(%s) line %d" % (label, count), who, kind, text, "page %d%s" % (paper.page(at), gloss))

    line, text = start, opened.group(2)
    while not ENDED.search(text):
        line = after(line)
        text += " " + paper.text(line)
    row(L, "transcription", text, start, ", the unbroken line")
    line = after(line)
    while not paper.text(line)[:1].isupper():
        row(L, "segmentation", paper.text(line), line)
        line = after(line)
        row(L, "gloss", paper.text(line), line)
        line = after(line)
    at, text = line, paper.text(line)
    line = after(line)
    while line <= paper.last and not gen.EXAMPLE.match(paper.text(line)) and not gen.HEADING.match(paper.text(line)):
        text += " " + paper.text(line)
        line = after(line)
    row(A, "translation", text, at)
    return line


def quotation(start, where):
    """The indented quotation of Ignace and Ignace (2017: 63) in §1, pages 1 and 2: one note, whose
    every line the page indents."""
    lines = [start]
    while not paper.text(lines[-1]).endswith("63).1"):
        lines.append(after(lines[-1]))
    body = paper.joined(lines)
    paper.add(where, A, "note", body, "page %d" % paper.page(start))
    paper.cited(where, body, sorted({paper.page(one) for one in lines}))
    paper.mentions(where, body, NAMES, "name")
    paper.mentions(where, body, LANGUAGES, "language")
    return after(lines[-1])


def caption(start, where):
    """Figure 1's caption at the head of page 5, the figure a drawing over it."""
    paper.add("Figure 1", A, "note", paper.text(start), "page %d, the figure's caption; the figure is a drawing"
              % paper.page(start))
    paper.mentions("Figure 1", paper.text(start), NAMES, "name")
    return after(start)


CAPTION = paper.find(r"^Figure 1: ")
blocks = {paper.find(r"^Our Secwépemc stsptékwle ", 1, 60): quotation, CAPTION: caption}
for number in range(1, paper.last + 1):
    if printed(number) and re.match(r"^\(\d+\) ", paper.text(number)):
        blocks[number] = stanza
# The page raises footnote 7's number after tspaq̓tu7semi7 in (13), whose 7 is the glottal stop;
# residue's correction sets it a space off the form, and the mark is found after the form's name.
SPACED = {"7": "tspaq̓tu7semi7"}
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, spaced=SPACED)
# The mark placed its note and comes off the form.
for row in paper.rows:
    for mark, form in SPACED.items():
        if row[2] == "transcription" and form + " " + mark + " " in row[3]:
            row[3] = row[3].replace(form + " " + mark + " ", form + " ")
            row[4] += ", carries footnote " + mark
    # And footnote 6's number after the stop of (10)'s unbroken line.
    ended = re.search(r"(?<=[.?!]) (\d)$", row[3]) if row[2] == "transcription" else None
    if ended:
        row[3] = row[3][:ended.start()]
        row[4] += ", carries footnote " + ended.group(1)
# The paragraph the caption stands in runs from page 4 on under it; its end joins its start, and the
# caption follows it.
at = next(number for number, row in enumerate(paper.rows) if row[0] == "Figure 1")
start = max(number for number in range(at) if paper.rows[number][0] == "§2" and paper.rows[number][2] == "note")
paper.rows[start][3] += " " + paper.rows[at + 1][3]
del paper.rows[at + 1]
# Edward Stobie Billy is broken over two lines, Edward Sto- bie Billy, and the note keeps the break.
at = next(number for number, row in enumerate(paper.rows) if row[2] == "name" and row[3] == "Charlie Draney")
paper.rows[at + 1:at + 1] = [paper.rows[at][:3] + ["Edward Stobie Billy", "storyteller recorded by Kuipers; "
                                                   "printed Edward Sto- bie Billy over a line break"]]
# Boelscher [Ignace] (1989) opens an entry the reference reader takes for a line of Boas and Hunt
# (1906), its bracket standing between the name and the comma.
at = next(number for number, row in enumerate(paper.rows) if row[2] == "reference" and " Boelscher [" in row[3])
first, second = paper.rows[at][3].split(" Boelscher [")
paper.rows[at + 1:at + 1] = [paper.rows[at][:3] + ["Boelscher [" + second, paper.rows[at][4]]]
paper.rows[at][3] = first
at = next(number for number, row in enumerate(paper.rows) if row[0] == "footnote 8" and row[2] == "note")
paper.rows[at + 1:at + 1] = [["footnote 8", L, "cited form", one, "page 11, in italics"] for one in BARE]
paper.write()
