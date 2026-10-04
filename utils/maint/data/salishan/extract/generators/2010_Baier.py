"""The ops of 2010_Baier: Nico Baier's Irrealis Morphology in Montana Salish. The irrealis prefix has
two allomorphs, qł- on nominals and qs- on verbs, both surfacing as q- before the pre-locative prefixes
s-, es-, eł- and epł-; irrealis verbs come in an unmarked paradigm and a marked one with continuative
suffixes that carry no continuative meaning; and the cognate prefixes of Kalispel and Okanagan share
the distribution.

Page text read by glyph rows.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Montana Salish"
AUTHORS = ["Nico Baier"]
paper = gen.Paper("2010_Baier", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Sally Thomason", "thanked for help and advice and for her Montana Salish field texts and data, footnote 1"),
         ("Thomason", "Sarah G. Thomason, The Flathead Word (1992) and the Montana Salish fieldnotes and dictionary (2009)"),
         ("Baier & Wdzenczny", "Nico Baier and Dibella Wdzenczny, negation in Montana Salish (2009)"),
         ("Thomason & Everett", "Sarah G. Thomason and Daniel Everett, transitivity in Flathead (1993)"),
         ("Kroeber", "Paul D. Kroeber, The Salish Language Family: Reconstructing Syntax (1999)"),
         ("Kinkade", "M. Dale Kinkade, Proto-Salish irrealis (2001)"),
         ("Mattina", "Anthony Mattina, Interior Salish to-be and intention forms (1996)"),
         ("Hans Vogt", "The Kalispel Language (1940)"), ("Vogt", "Hans Vogt, The Kalispel Language (1940)")]
LANGUAGES = [(LANGUAGE, "Southern Interior Salish, the language of the paper"),
             ("Kalispel", "Southern Interior Salish, Vogt's grammar"),
             ("Okanagan", "Southern Interior Salish, Mattina's paper"),
             ("Southern Interior Salishan", "the branch of Montana Salish"), ("English", "the translations")]
FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
RUNNING = paper.running_numbers_set()
ITEM = re.compile(r"^([a-h])\.\s+(.*)$")
TIERS = ((L, "transcription"), (L, "segmentation"), (A, "gloss"))


def printed(number):
    return bool(paper.text(number)) and not paper.lines[number][2] and number not in RUNNING \
        and number not in FOOT


def after(number):
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def translation(text):
    # (3c)'s translation is printed with no opening quote (render of page 4).
    return text.startswith("‘") or text.startswith("You’re going to give her")


def example(first, where):
    """Each lettered item, or the example itself where it has no letters: a transcription over its
    translation in (1) and (2), elsewhere a transcription, its segmentation and its gloss, the three
    wrapping to a second set in (3b), (3e), (5b), (10), (16e) and (21d), and the translation closing
    it, run on to the line under it where its parenthesis wraps."""
    label, text = gen.EXAMPLE.match(paper.text(first)).groups()
    line, letter, count, tier = first, "", 0, 0
    while True:
        item = ITEM.match(text)
        if item:
            letter, text, count, tier = item.group(1), item.group(2), 0, 0
        elif count == 0 and letter and line != first:
            return line
        count += 1
        here = "(%s%s) line %d" % (label, letter, count)
        if translation(text):
            at = line
            while text.count("(") > text.count(")"):
                line = after(line)
                text += " " + paper.text(line)
            paper.add(here, A, "translation", text, "page %d" % paper.page(at))
            line = after(line)
            if not ITEM.match(paper.text(line)):
                return line
            text, count = paper.text(line), 0
            continue
        who, kind = TIERS[tier % 3]
        paper.add(here, who, kind, text, "page %d" % paper.page(line))
        tier += 1
        line = after(line)
        text = paper.text(line)


def statement(first, where):
    """(11)'s rule, a sentence wrapping to a second line, and the schemas of (12) and (13), a note to
    each printed line, to the prose line that follows."""
    label, text = gen.EXAMPLE.match(paper.text(first)).groups()
    stop = paper.find(STATEMENTS[label], first)
    if label == "11":
        paper.add("(11) line 1", A, "note", text + " " + paper.text(after(first)), "page %d, the rule" % paper.page(first))
        return stop
    paper.add("(%s) line 1" % label, A, "note", text, "page %d, the schema's heading" % paper.page(first))
    count, line = 1, after(first)
    while line < stop:
        count += 1
        paper.add("(%s) line %d" % (label, count), A, "note", paper.text(line), "page %d, a line of the schema" %
                  paper.page(line))
        line = after(line)
    return stop


STATEMENTS = {"11": r"^So, all the forms", "12": r"^Here, ‘…’ represents", "13": r"^The expanded schema"}
BLOCKS = {}
at = 1
for label in range(1, 22):
    at = paper.find(r"^\(%d\)\s" % label, at)
    BLOCKS[at] = statement if str(label) in STATEMENTS else example
    at += 1
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS)
paper.write()
