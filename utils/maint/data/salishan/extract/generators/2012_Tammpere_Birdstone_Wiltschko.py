"""The ops of 2012_Tammpere_Birdstone_Wiltschko: Laura Tammpere, Violet Birdstone and Martina
Wiltschko's Independent pronouns in Ktunaxa. Ktunaxa marks its arguments on the verb, and its
independent pronouns, ka-min and ninku- with the possessive and plural affixes of possessed nouns, serve
where agreement cannot: obliques, possession of nominalized forms, predicates of possession, dislocated
phrases, contrast and focus. They take determiners and can be bound, by a quantifier among others, and
the paper classes them as pro-ɸPs in Déchaine and Wiltschko's typology.

Page text read by glyph rows.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Ktunaxa"
AUTHORS = ["Laura Tammpere", "Violet Birdstone", "Martina Wiltschko"]
paper = gen.Paper("2012_Tammpere_Birdstone_Wiltschko", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Déchaine and Wiltschko", "Rose-Marie Déchaine and Martina Wiltschko, Decomposing pronouns (2002)"),
         ("Morgan", "L. R. Morgan, A description of the Kutenai language (1991)"),
         ("Dryer", "Matthew Dryer, Grammatical relations in Ktunaxa (Kutenai) (1996)"),
         ("Dahlstrom", "Amy Dahlstrom, Independent pronouns in Fox (1988)"),
         ("Wiltschko", "Martina Wiltschko, The syntax of pronouns: Evidence from Halkomelem Salish (2002)")]
LANGUAGES = [(LANGUAGE, "a language isolate, the language of the paper"),
             ("Fox", "Algonquian, Dahlstrom's two series of independent pronouns"),
             ("Salishan", "the languages Morgan compares with Ktunaxa"),
             ("English", "the translations")]
FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
RUNNING = paper.running_numbers_set()
ITEM = re.compile(r"^([a-h])\.\s*(.*)$")
TIERS = ((L, "transcription"), (L, "segmentation"), (A, "gloss"))
# The line an example opens with where it is no transcription: the tree's title in (4), the source
# of (18) and the question (19) answers.
HEADS = {"4": "the title of the tree", "18": "the example's source", "19": "the question the answers answer"}


def printed(number):
    return bool(paper.text(number)) and not paper.lines[number][2] and number not in RUNNING \
        and number not in FOOT


def after(number):
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def example(first, where):
    """Each lettered item, or the example itself where it has no letters: a transcription, its
    segmentation and its gloss, the three wrapping to a second set in (1a), (14), (22b), (23b), (23c),
    (25) and (29), and the translation closing it; a context the example or an item opens with is a
    note, and so is the heading line of (18) and (19)."""
    label, text = gen.EXAMPLE.match(paper.text(first)).groups()
    line, letter, count, tier = first, "", 0, 0
    if label in HEADS:
        paper.add("(%s) line 1" % label, A, "note", text, "page %d, %s" % (paper.page(line), HEADS[label]))
        line, count = after(line), 1
        text = paper.text(line)
    while True:
        item = ITEM.match(text)
        if item:
            letter, text, count, tier = item.group(1), item.group(2), 0, 0
        count += 1
        here = "(%s%s) line %d" % (label, letter, count)
        if text.startswith("Context:"):
            paper.add(here, A, "note", text, "page %d, the context" % paper.page(line))
        elif text[:1] in "‘’“":
            paper.add(here, A, "translation", text, "page %d" % paper.page(line))
            line = after(line)
            if not ITEM.match(paper.text(line)):
                return line
            text = paper.text(line)
            continue
        else:
            who, kind = TIERS[tier % 3]
            paper.add(here, who, kind, text, "page %d" % paper.page(line))
            tier += 1
        line = after(line)
        text = paper.text(line)


def tree(first, where):
    """(4)'s title, then the two trees side by side a note to each printed row, to the prose after."""
    stop = paper.find(r"^Déchaine and Wiltschko \(2002\) predict that a pro", first)
    label, text = gen.EXAMPLE.match(paper.text(first)).groups()
    paper.add("(4) line 1", A, "note", text, "page %d, %s" % (paper.page(first), HEADS["4"]))
    count, line = 1, after(first)
    while line < stop:
        count += 1
        paper.add("(4) line %d" % count, A, "note", paper.text(line),
                  "page %d, a row of the two trees, ka-min's and ninku-ʔis's" % paper.page(line))
        line = after(line)
    return stop


def table_one(first, where):
    """Table 1, English throughout: its column heads, each row's head and cells a note apiece, the
    heads Internal Syntax and Binding-theoretic status and the cell D syntax; morphologically complex
    each wrapping to the line under them, and its caption under it."""
    page = paper.page(first)
    paper.add("Table 1", A, "note", "Pro-DP   Pro-ɸP   Pro-NP", "page %d, the table's column heads" % page)
    for head, cells in (("Internal Syntax", ("D syntax; morphologically complex", "neither D nor N syntax", "N syntax")),
                        ("Distribution", ("argument", "argument or predicate", "predicate")),
                        ("Semantics", ("definite", "–", "constant")),
                        ("Binding-theoretic status", ("R-expression", "variable", "–"))):
        paper.add("Table 1", A, "note", head, "page %d, the row's head" % page)
        for column, cell in zip(("Pro-DP", "Pro-ɸP", "Pro-NP"), cells):
            paper.add("Table 1", A, "note", cell, "page %d, %s, %s" % (page, head, column))
    caption = paper.find(r"^Table 1: ", first)
    paper.add("Table 1", A, "note", paper.text(caption), "page %d, the table's caption" % page)
    return caption + 1


def table_two(first, where):
    """Table 2, the paradigm: each cell a pronoun over its gloss, a cited form and a gloss row."""
    page = paper.page(first)
    paper.add("Table 2", A, "note", "Singular   Plural", "page %d, the table's column heads" % page)
    for head, cells in (("1st Person", (("ka-min", "1POSS-1IP"), ("ka-mn-aɬa", "1POSS-1IP-1PL"))),
                        ("2nd Person", (("ninku-(nis)", "2,3IP-(2POSS)"), ("ninku-nis-kiɬ", "2,3IP-2POSS-2PL"))),
                        ("3rd Person", (("ninku-ʔis", "2,3IP-3POSS"),))):
        paper.add("Table 2", A, "note", head, "page %d, the row's head" % page)
        for column, (form, meaning) in zip(("Singular", "Plural"), cells):
            paper.add("Table 2", L, "cited form", form, "page %d, %s %s" % (page, head, column))
            paper.add("Table 2", A, "gloss", meaning, "page %d, the gloss of %s" % (page, form))
    caption = paper.find(r"^Table 2: ", first)
    paper.add("Table 2", A, "note", paper.text(caption), "page %d, the table's caption" % page)
    return caption + 1


def functions(first, where):
    """§1's list of the pronouns' other functions, i to v, a note to each item and its wrapped lines."""
    stop = paper.find(r"^These functions confirm", first)
    items, line = [], first
    while line < stop:
        if re.match(r"^(?:i|ii|iii|iv|v) [A-Z]", paper.text(line)) or not items:
            items.append([line])
        else:
            items[-1].append(line)
        line = after(line)
    for numbers in items:
        paper.add("§1", A, "note", paper.joined(numbers), "page %d, an item of the list of functions" % paper.page(numbers[0]))
    return stop


def form_language(run):
    """The language of an italic run: the ɸ of Déchaine and Wiltschko's pro-ɸP is set in the face of
    the forms and is no form; ka a:ksak ‘my leg’ in §3.1 is Ktunaxa in plain letters."""
    if run.startswith("ɸP"):
        return None
    return L if gen.orthographic(run) or run == "ka a:ksak" else None


paper.form_language = form_language
BLOCKS = {}
at = 1
for label in range(1, 31):
    # A sentence of §3.3 wraps onto (14) is in the same position, no example.
    at = paper.find(r"^\(%d\)\s+(?!is )" % label, at)
    BLOCKS[at] = tree if label == 4 else example
    at += 1
BLOCKS[paper.find(r"^Pro-DP\s+Pro-ɸP\s+Pro-NP$")] = table_one
BLOCKS[paper.find(r"^Singular\s+Plural$")] = table_two
BLOCKS[paper.find(r"^i Certain verbs require")] = functions
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS)
# The glosses' SUBJ.1 and SUBJ.2 end on the digits of footnotes 1 and 2, and the placer takes them
# for the marks in (1a) and (8); the marks stand on (3).1 in §2 and (17a).2 in §3.4. Each footnote's
# rows go after the note that carries its mark.
for mark, host in (("1", "as in (3).1"), ("2", "as in (17a).2")):
    moved = [row for row in paper.rows if row[0] == "footnote " + mark or row[0].startswith("footnote %s " % mark)]
    kept = [row for row in paper.rows if not any(row is one for one in moved)]
    at = next(index for index, row in enumerate(kept) if row[2] == "note" and host in row[3])
    paper.rows = kept[:at + 1] + moved + kept[at + 1:]
paper.write()
