"""The ops of 2010_Turner: Claire K. Turner's Aspectual properties of SENĆOŦEN reflexives, applying
Kiyota's (2008) situation type tests to the control and limited control reflexives -sat and -naŋət,
in their core and grammaticized uses.

Page text read by glyph rows, with the AboriginalSerif faces mapped through page_text.py
PAPER_TOUNICODE. The examples set a line in Dave Elliott's SENĆOŦEN orthography over its NAPA
segmentation and gloss, or the NAPA and gloss alone for examples cited from Kiyota, wrapping to further
sets, then the translation, with a source, a context or a reading set right of it. (9) and (10) are
Halkomelem, cited from Gerdts, each with the form it comes from, (>q̓ʷaqʷ-ət-sət), at the right. Tables
1 and 2 are read off renders of pages 8 and 20, a note to each cell.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "SENĆOŦEN"
AUTHORS = ["Claire K. Turner"]
paper = gen.Paper("2010_Turner", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Kiyota", "Masaru Kiyota (2007, 2008), the situation type tests of SENĆOŦEN"),
         ("Gerdts", "Donna B. Gerdts (1989, 2000), the Halkomelem reflexives"),
         ("Montler", "Timothy Montler (1986, 2003), SENĆOŦEN morphology"),
         ("Dave Elliott", "the SENĆOŦEN orthography of the examples"),
         ("Bar-el", "Leora Bar-el (2005), Bar-el et al. (2006), culmination cancellation"),
         ("Jacobs", "Peter Jacobs (2007, to appear), control in Skwxwú7mesh"),
         ("Greville Corbett", "thanked for feedback"), ("Dunstan Brown", "thanked for feedback"),
         ("Leora Bar-el", "thanked for feedback"), ("Janet Leonard", "thanked for feedback")]
LANGUAGES = [(LANGUAGE, "North Straits Salish, the dialect of the Saanich community"),
             ("North Straits Salish", "the language SENĆOŦEN is a dialect of"),
             ("Halkomelem", "Central Salish, Gerdts's reflexives"),
             ("Skwxwú7mesh", "Jacobs's control distinctions")]
HALKOMELEM = "Halkomelem"
FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
RUNNING = paper.running_numbers_set()
SOURCE = re.compile(r"^\((?:Kiyota|Gerdts) \d{4}[^)]*\)$")


def printed(number):
    return bool(paper.text(number)) and not paper.lines[number][2] and number not in RUNNING \
        and number not in FOOT


def after(number):
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def orthographic(text):
    """A line in the SENĆOŦEN orthography: every word in capitals, W̱ȻEḴETS, SU, and DEDAY,EK., or a
    name, Katie; a gloss sets its capitals inside words that hold a stop or a hyphen, FEM.DET."""
    def capitals(one):
        # The possessive s stays lowercase after the capitals, WÁĆs in (7).
        bare = re.sub(r"(?<=\w)s$", "", re.sub(r"[,.?!]?\d?$", "", one))
        return bool(bare) and not any(ch.islower() or ch in "-=." for ch in bare) and any(ch.isalpha() for ch in bare)
    return all(capitals(one) or re.fullmatch(r"[A-Z][a-z]+[,.]?", one) for one in text.split())


def formed(text):
    """A form and the form it comes from, set right of it in parentheses: (10)'s (>q̓ʷaqʷ-ət-sam̓š-əs)."""
    found = re.match(r"^(.*?)\s*(\(>[^)]*\))$", text)
    return (found.group(1), found.group(2)) if found else (text, None)


def translation(first):
    """The translation from line first, wrapping while a line ends mid-sentence, and what stands right
    of it: (the translation, what stands right, the line after)."""
    parts, line = [paper.text(first)], after(first)
    while not re.search(r"[’”.)!?\]]$", parts[-1]) and not gen.EXAMPLE.match(paper.text(line)):
        parts.append(paper.text(line))
        line = after(line)
    joined = " ".join(parts)
    split = re.search(r"’\s+(?=[(\[])", joined)
    if split:
        return joined[:split.start() + 1], joined[split.end():], line
    return joined, None, line


def example(first, where):
    label = re.match(r"^\((\d+)\)", paper.text(first)).group(1)
    language = HALKOMELEM if label == "10" else L
    lines, line = [], first
    while True:
        text = re.sub(r"^\(\d+\)\s+", "", paper.text(line)) if line == first else paper.text(line)
        if re.match(r"^(?:attempt at: )?[‘“]", text):
            break
        lines.append((text, paper.page(line)))
        line = after(line)
    count = 0

    def add(who, kind, text, why):
        nonlocal count
        count += 1
        paper.add("(%s) line %d" % (label, count), who, kind, text, why)

    at = 0
    while at < len(lines):
        if orthographic(lines[at][0]):
            tiers = (("transcription", "the SENĆOŦEN orthography"), ("segmentation", "NAPA"), ("gloss", None))
        else:
            tiers = (("transcription", "NAPA"), ("gloss", None))
        assert at + len(tiers) <= len(lines), (label, lines)
        for (kind, what), (text, page) in zip(tiers, lines[at:at + len(tiers)]):
            form, source = formed(text)
            add(language, kind, form, "page %d%s" % (page, ", " + what if what else ""))
            if source:
                add(language, "underlying" if kind != "gloss" else "gloss", source,
                    "page %d, at the right, the form it comes from" % page)
        at += len(tiers)
    page = paper.page(line)
    said, right, line = translation(line)
    add(A, "translation", said, "page %d" % page)
    if right:
        add(A, "citation" if SOURCE.match(right) else "note", right,
            "page %d, at the right of the translation" % page)
    text = paper.text(line)
    if SOURCE.match(text):
        add(A, "citation", text, "page %d, under the translation" % paper.page(line))
        line = after(line)
    elif text.startswith("Speaker’s comment:"):
        parts = [text]
        while not parts[-1].endswith("”"):
            line = after(line)
            parts.append(paper.text(line))
        add(A, "speaker comment", " ".join(parts), "page %d, under the example" % paper.page(line))
        line = after(line)
    return line


def nine(first, where):
    """(9): three Halkomelem forms side by side, a to c, each over its gloss and translation, and at
    the right the form c comes from and the source."""
    page = paper.page(first)
    forms = re.match(r"^\(9\) a\. (\S+) b\. (\S+) c\. (\S+) (\(>\S+\))$", paper.text(first))
    glosses = re.match(r"^(\S+) (\S+) (\S+) (\(>\S+\))$", paper.text(after(first)))
    said = after(after(first))
    meanings = re.findall(r"‘[^’]*’", paper.text(said))
    source = re.search(r"\([^)]*\)$", paper.text(said)).group(0)
    assert forms and glosses and len(meanings) == 3, (paper.text(first), paper.text(after(first)))
    for index, letter in enumerate("abc"):
        rows = [(HALKOMELEM, "transcription", forms.group(index + 1)), (HALKOMELEM, "gloss", glosses.group(index + 1)),
                (A, "translation", meanings[index])]
        if letter == "c":
            rows[1:1] = [(HALKOMELEM, "underlying", forms.group(4))]
            rows[3:3] = [(HALKOMELEM, "gloss", glosses.group(4))]
            rows.append((A, "citation", source))
        for count, (who, kind, text) in enumerate(rows, 1):
            why = "page %d, column %s" % (page, letter)
            if kind == "underlying" or text == glosses.group(4):
                why += ", at the right, the form it comes from"
            if kind == "citation":
                why = "page %d, at the right, the source of (9)" % page
            paper.add("(9%s) line %d" % (letter, count), who, kind, text, why)
    return after(said)


# Table 1, read off the render of page 8: the two column heads, then a row an author, each cell a
# note to each bullet.
TABLE_1 = (("Core control reflexive [base+C.TR+REFL]", "Inchoative reflexive [base+REFL]"), (
    ("Montler (1986)", ("Contain control transitive suffix (schwa).", "Have “control” meaning."),
     ("Do not contain control transitive suffix.", "Have “non-control” meaning.")),
    ("Gerdts (2000)", ("Contain control transitive suffix.", "Have reflexive meaning.", "Contain unaccusative roots."),
     ("Are reanalyzed without control transitive, though its formal presence is detectable.",
      "Contain stative/unergative roots.")),
    ("Kiyota (2008)", ("Not discussed.",), ("Are derived from homogeneous states.", "Pattern with non-states."))))
# Table 2, read off the render of page 20: the four test heads, then a row a reflexive, its head a
# name over a cited form and its gloss; the inchoative row's last two cells are shaded and empty.
TABLE_2 = (("Perfect", "Out of the blue", "Almost", "Culmination cancellation"), (
    ("Core control reflexives", "k̓ʷənsət", "‘look at self’", ("Telic", "Telic", "Accomplishment", "Achievement")),
    ("Core limited control reflexives", "mekʷəɬnaŋət", "‘hurt self’", ("Telic", "Telic", "Achievement", "Achievement")),
    ("Inchoative reflexives", "sčuetsət", "‘get smart’", ("Atelic", "Atelic", None, None)),
    ("Managed-to reflexives", "nəqʷnanət", "‘fall asleep’",
     ("Telic", "Telic", "Accomplishment", "Accomplishment"))))


def table_1(first, where):
    page = paper.page(first)
    heads, rows = TABLE_1
    paper.add("Table 1", A, "note", paper.text(first), "page %d, the table's caption" % page)
    for head in heads:
        paper.add("Table 1", A, "note", head, "page %d, a column head" % page)
    for author, core, inchoative in rows:
        paper.add("Table 1", A, "note", author, "page %d, the row's head" % page)
        for column, cells in ((heads[0], core), (heads[1], inchoative)):
            for cell in cells:
                paper.add("Table 1", A, "note", cell, "page %d, %s, %s, a bullet" % (page, author, column))
    return paper.find(r"^When taken together", first)


def table_2(first, where):
    page = paper.page(first)
    heads, rows = TABLE_2
    paper.add("Table 2", A, "note", paper.text(first), "page %d, the table's caption" % page)
    for head in heads:
        paper.add("Table 2", A, "note", head, "page %d, a test's column head" % page)
    for name, form, gloss, cells in rows:
        paper.add("Table 2", A, "note", name, "page %d, the row's head" % page)
        paper.add("Table 2", L, "cited form", form, "page %d, in italics, %s, under %s" % (page, gloss, name))
        paper.cited_done.add(form)
        for head, cell in zip(heads, cells):
            if cell:
                paper.add("Table 2", A, "note", cell, "page %d, %s, %s" % (page, name, head))
            else:
                paper.add("Table 2", A, "notation", "shaded", "page %d, %s, %s, the cell shaded and empty" % (page, name, head))
    return paper.find(r"^5\s+Discussion", first)


NINE = paper.find(r"^\(9\) a\.")
BLOCKS = {NINE: nine, paper.find(r"^Table 1 Core"): table_1, paper.find(r"^Table 2 Summary"): table_2}
for number in range(1, paper.last + 1):
    # A label runs to two digits, and the years that open lines of Table 1 and the prose, (1986),
    # are none; neither is (14) opening a sentence of §3.2.
    if number != NINE and re.match(r"^\(\d{1,2}\)\s(?!differs)", paper.text(number)) and printed(number):
        BLOCKS[number] = example
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS)
# Every entry opens on a surname and gives its year, or to appear; a wrap after an editor's name,
# Ussery. / Amherst, MA: GLSA., or onto page numbers, Martin A. / Oberg, 285-292., runs on the entry
# above it. The author's name and address under the last entry close the paper, a note.
paper.merge_references(r"^[A-Z][\w’'\-]+, .{0,80}?(?:\b(?:19|20)\d\d[a-z]?|to appear)\.")
ADDRESS = " Claire K. Turner c.turner@surrey.ac.uk"
last = max(index for index, row in enumerate(paper.rows) if row[2] == "reference")
assert paper.rows[last][3].endswith(ADDRESS), paper.rows[last][3]
paper.rows[last][3] = paper.rows[last][3][:-len(ADDRESS)]
paper.rows.insert(last + 1, ["references", A, "note", ADDRESS.strip(),
                             "page %d, the author's name and address" % paper.page(paper.last)])
paper.write()
