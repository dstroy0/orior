"""The ops of 17_ICSNL55_Sardinha1_final: Katie Sardinha on the Kwak̕wala aspectual suffix /-x'id/,
its history in Boas's grammars, its syntax and semantics after Greene (2013), the rules that choose
among its ten allomorphs, and the optional loss of syllable-final d that makes five of them.

The interlinear examples are read by gen.Paper.example. The sets of allomorphs, (1), (2), (19) and
(29), are a transcription row to each allomorph; Boas's descriptions quoted in (2) and (3) a note
and a citation each. The rules of (20), (21) and (22) are a note to each rule, with its wrapped
lines run on; under a rule each derivation is a phonemic row to each morpheme left of the arrow and
a phonetic row for the bracketed form right of it, then each form it glosses a transcription row
(a phonemic one between slashes) and each gloss a translation row, with the source a citation.
(30)'s examples stand under a line for each allomorph pair, A. -x’id → -x’i, each a note. Table 1,
the denotation (13), the rule of d deletion (28) and Table 3 are a note to each printed line; Table
2 a note to each row the page prints, its heading's two lines and its cells joined; the glossing
abbreviations a note to each entry, its wrapped notes run on. The page's bold on the allomorph in
(20), (27) and (30) is not in the text layer.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Kwak̕wala"
AUTHORS = ["Katie Sardinha"]
paper = gen.Paper("17_ICSNL55_Sardinha1_final", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = []
LANGUAGES = [(LANGUAGE, "Northern Wakashan")]
FOOTNOTES = paper.page_footnotes()
AT_FOOT = {one for parts, _ in FOOTNOTES.values() for one in parts}
REFERENCES = paper.find(r"^References$")
ABBREVIATIONS = paper.find(r"^Glossing Abbreviations$")
RUNNING = paper.running_numbers_set()
ENGLISH = {"the", "of", "and", "to", "in", "is", "that", "for", "as", "with", "are", "be", "by", "this",
           "which", "we", "on", "it", "not", "or", "from", "can", "an", "these", "has", "have"}
PART = re.compile(r"^([a-z])\.\s+(.*)$")
# A rule's label, A. to E. or a. to e., or a derivation's, i. to x.
LABEL = re.compile(r"^(?:([A-E])|([a-e])|(i{1,3}|iv|vi{0,3}|ix|x))\.\s+(.*)$")
SOURCE = re.compile(r"\s*(\((?:Boas|Greene) \d{4}[^()]*\))$")
# The line after each block of rules, where the prose resumes.
RULES_END = {"20": r"^In some cases, more than one", "21": r"^I have not checked with contemporary",
             "22": r"^I am not aware of any roots"}


def prose(text):
    """Whether a line is running prose: nine words or more, three of them English function words."""
    words = text.split()
    return len(words) >= 9 and sum(1 for one in words if one.lower().strip(",.;:()") in ENGLISH) >= 3


def printed(number):
    return paper.text(number).strip() and not paper.lines[number][2] and number not in AT_FOOT \
        and number not in RUNNING


def adder(counts):
    def add(label, who, kind, form, line, gloss=None):
        counts[label] = counts.get(label, 0) + 1
        paper.add("(%s) line %d" % (label, counts[label]), who, kind, form,
                  "page %d%s" % (paper.page(line), ", " + gloss if gloss else ""))
    return add


def head(start):
    opened = gen.EXAMPLE.match(paper.text(start))
    return opened.group(1), opened.group(2) or ""


def allomorphs(text, add, label, line, gloss):
    """A transcription row to each allomorph of a set printed with commas between them."""
    for piece in text.strip("{} ").split(", "):
        add(label, L, "transcription", piece.strip(), line, gloss)


def sets(start, where):
    """(1) and (29): a title closing on a colon, a note, over the set in braces, or the set alone."""
    label, text = head(start)
    add = adder({})
    line = start
    while line < REFERENCES:
        if line > start and printed(line) and (prose(paper.text(line)) or gen.EXAMPLE.match(paper.text(line))):
            break
        if printed(line):
            text = text if line == start else paper.text(line)
            if text.startswith("{"):
                allomorphs(text, add, label, line, "an allomorph of the set the page prints in braces")
            else:
                add(label, A, "note", text, line, "the title over the set")
        line += 1
    return line


def entry(start, where):
    """(2) and (3): a suffix's entry in Boas (1947), its forms, then each description quoted from it
    a note and its source a citation. (2)'s first form carries footnote 2's mark, set against it."""
    label, text = head(start)
    add = adder({})
    forms = text.rstrip(":")
    first, _, rest = forms.partition(", ")
    first = re.sub(r"\s+(\d)$", r"\1", first)
    add(label, L, "transcription", first, start, "the form of Boas's entry")
    if rest:
        allomorphs(rest, add, label, start, "a form of Boas's entry")
    line, said = start + 1, []
    while line < REFERENCES:
        if printed(line):
            text = paper.text(line)
            if gen.EXAMPLE.match(text) or not said and prose(text) and not text.startswith("“"):
                break
            said.append(text)
            source = SOURCE.search(text)
            if source:
                add(label, A, "note", " ".join(said)[:-len(source.group(0))].strip(), line,
                    "quoted from Boas (1947)")
                add(label, A, "citation", source.group(1), line)
                said = []
        line += 1
    return line


def lines(start, where):
    """(13) and (28): a note to each printed line to the prose after it, a source at a line's end a
    citation."""
    label, text = head(start)
    add = adder({})
    line = start
    while line < REFERENCES:
        if printed(line):
            if line > start:
                text = paper.text(line)
                if prose(text) or gen.EXAMPLE.match(text):
                    break
            source = SOURCE.search(text)
            add(label, A, "note", text[:source.start()] if source else text, line, "set as an example")
            if source:
                add(label, A, "citation", source.group(1), line)
        line += 1
    return line


def table(start, where):
    """Table 1: its caption a note and each printed line under it a note, to the prose after it."""
    name = re.match(r"^(Table \d+)", paper.text(start)).group(1)
    paper.add(name, A, "note", paper.text(start), "page %d, the table's caption" % paper.page(start))
    line, count = start + 1, 0
    while line < REFERENCES:
        if printed(line):
            text = paper.text(line)
            if prose(text):
                break
            count += 1
            paper.add("%s line %d" % (name, count), A, "note", text, "page %d, %s" % (paper.page(line), name))
        line += 1
    return line


def listed(start, where):
    """(19): each lettered set of allomorphs, a transcription row to each, and what the page sets
    right of it a note."""
    label, text = head(start)
    add = adder({})
    add(label, A, "note", text, start, "the title over the sets")
    line = start + 1
    while line < REFERENCES:
        if printed(line):
            part = PART.match(paper.text(line))
            if not part:
                break
            split = re.match(r"^(.*?[^\s,])\s+([A-Z].*)$", part.group(2))
            allomorphs(split.group(1), add, label + part.group(1), line, "a set of allomorphs")
            add(label + part.group(1), A, "note", split.group(2), line, "right of the set")
        line += 1
    return line


def glossed(text, add, label, line):
    """The forms a derivation glosses: each form before its gloss a transcription row, or phonemic
    between slashes, each gloss in its quotes a translation row with a footnote mark after it kept,
    and the source at the end a citation."""
    source = SOURCE.search(text)
    if source:
        text = text[:source.start()]
    rest = text.strip()
    while rest:
        opened = rest.find("‘")
        if opened < 0:
            add(label, A, "note", rest, line, "after the glosses")
            break
        form = rest[:opened].strip(" ,")
        if form:
            add(label, L, "phonemic" if form.startswith("/") else "transcription", form, line)
        closed = re.search(r"’(?=[\s,\d]|$)", rest[opened:])
        end = opened + closed.end() if closed else len(rest)
        mark = re.match(r",?\d{1,2}(?=\s|$)", rest[end:])
        end += len(mark.group(0)) if mark else 0
        add(label, A, "translation", rest[opened:end], line)
        rest = rest[end:].strip(" ,")
    if source:
        add(label, A, "citation", source.group(1), line)


def rules(start, where):
    """(20), (21) and (22): a note to each rule, its wrapped lines run on, and each derivation under
    it: a phonemic row to each morpheme left of the arrow, a phonetic row for the form right of it,
    then the forms it glosses, on the line under it in (20) and (21) and right of it in (22)."""
    number, text = head(start)
    add = adder({})
    end = paper.find(RULES_END[number], start)
    add(number, A, "note", text, start, "the title over the rules")
    state = {"upper": "", "lower": "", "held": None}

    def flush():
        held = state["held"]
        if held and held[0] == "rule":
            add(held[1], A, "note", " ".join(held[2]), held[3], "a rule")
        elif held and held[2]:
            glossed(" ".join(held[2]), add, held[1], held[3])

    for line in range(start + 1, end):
        if not printed(line):
            continue
        text = paper.text(line)
        labeled = LABEL.match(text)
        if not labeled:
            state["held"][2].append(text)
            continue
        flush()
        capital, small, roman, rest = labeled.groups()
        if capital:
            state["upper"], state["lower"] = capital, ""
        elif small:
            state["lower"] = small
        label = number + state["upper"] + state["lower"] + (roman or "")
        if not rest.startswith("/"):
            state["held"] = ("rule", label, [text], line)
            continue
        left, _, right = rest.partition("→")
        for morpheme in left.split(" + "):
            add(label, L, "phonemic", morpheme.strip(), line, "left of the arrow")
        formed = re.match(r"^\s*(\[[^\]]*\]\d{0,2})\s*(.*)$", right)
        add(label, L, "phonetic", formed.group(1), line, "right of the arrow")
        state["held"] = ("gloss", label, [formed.group(2)] if formed.group(2) else [], line)
    flush()
    return end


def retention(start, where):
    """(30): its title, then under each allomorph pair, A. -x'id → -x'i, a note, the lettered
    examples, read by gen.Paper.example as parts of (30A) to (30E)."""
    number, text = head(start)
    add = adder({})
    add(number, A, "note", text, start, "the title over the examples")
    end = next(line for line in range(start + 1, REFERENCES) if printed(line) and prose(paper.text(line)))
    pairs = [line for line in range(start + 1, end) if printed(line)
             and re.match(r"^[A-E]\.\s+\S+ → ", paper.text(line))]
    for pair, after in zip(pairs, pairs[1:] + [end]):
        letter = paper.text(pair)[0]
        add(number + letter, A, "note", paper.text(pair), pair, "the allomorph pair over the examples")
        first = next(line for line in range(pair + 1, after) if printed(line))
        paper.example(first, last=after - 1, skip=AT_FOOT, resume=number + letter)
    return end


HEADINGS = paper.headings(1, REFERENCES - 1, skip=AT_FOOT)
READERS = {"1": sets, "2": entry, "3": entry, "13": lines, "19": listed, "20": rules, "21": rules,
           "22": rules, "28": lines, "29": sets, "30": retention}
blocks = {paper.find(r"^Table 1: "): table}
at = 1
for label in sorted(READERS, key=int):
    at = next(one for one in range(at, REFERENCES) if one not in AT_FOOT
              and re.match(r"^\(%s\)(?:\s|$)" % label, paper.text(one)))
    blocks[at] = READERS[label]
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, headings=HEADINGS, appendix=r"^Glossing Abbreviations$")

# (10)'s part of speech, verb + /-x'id/, stands right of each transcription.
for index, (where, who, kind, form, gloss) in enumerate(paper.rows):
    split = re.match(r"^(\S+)\s+((?:verb|noun|adjective|adverb) \+ /-x’id/)$", form)
    if where.startswith("(10") and kind == "transcription" and split:
        paper.rows[index:index + 1] = [[where, who, kind, split.group(1), gloss],
                                       [where, A, "note", split.group(2), gloss + ", right of the transcription"]]
# (6)'s intended reading, under the gloss of a starred sentence, is its translation.
for index, (where, who, kind, form, gloss) in enumerate(paper.rows):
    if where.startswith("(6)") and form.startswith("Intended:"):
        source = SOURCE.search(form)
        paper.rows[index:index + 1] = [[where, A, "translation", form[:source.start()], gloss],
                                       [where, A, "citation", source.group(1), gloss]]
        break

# The glossing abbreviations, a note to each entry with its wrapped notes run on, and the appendix:
# Table 2 a note to each row the page prints, Table 3 a note to each printed line.
ENTRY = re.compile(r"^(?:[A-Z0-9][A-Z0-9.]*|[-=~!°])\s")
APPENDIX = paper.find(r"^Appendix A: ")
TABLE_2, TABLE_3 = paper.find(r"^Table 2: "), paper.find(r"^Table 3: ")
ROW_HEAD = re.compile(r"^(?:plain|glottalized|voiced|fricatives)\b")
back = {"here": "abbreviations", "count": 0, "held": None}


def put():
    if back["held"]:
        back["count"] += 1
        paper.add("%s line %d" % (back["here"], back["count"]), A, "note", " ".join(back["held"][0]),
                  back["held"][1])
    back["held"] = None


def start_section(here):
    put()
    back["here"], back["count"] = here, 0


for line in range(ABBREVIATIONS, paper.last + 1):
    if not printed(line):
        continue
    text = paper.text(line)
    here = back["here"]
    if line in (ABBREVIATIONS, APPENDIX):
        start_section("abbreviations" if line == ABBREVIATIONS else "Appendix A")
        paper.add(back["here"], A, "heading", text, "page %d" % paper.page(line))
    elif line in (TABLE_2, TABLE_3):
        start_section(re.match(r"^(Table \d+)", text).group(1))
        paper.add(back["here"], A, "note", text, "page %d, the table's caption" % paper.page(line))
    elif here == "abbreviations" and text == "Gloss Morphs Notes":
        back["count"] += 1
        paper.add("%s line %d" % (here, back["count"]), A, "note", text,
                  "page %d, the column heads" % paper.page(line))
    elif here == "abbreviations":
        if ENTRY.match(text):
            put()
            back["held"] = ([text], "page %d, an entry, its wrapped notes run on" % paper.page(line))
        else:
            back["held"][0].append(text)
    elif here == "Table 2":
        if ROW_HEAD.match(text):
            put()
            back["held"] = ([text], "page %d, a row of Table 2, its heading's lines and its cells joined"
                            % paper.page(line))
        else:
            back["held"][0].append(text)
    elif here == "Table 3":
        back["count"] += 1
        paper.add("%s line %d" % (here, back["count"]), A, "note", text, "page %d, Table 3" % paper.page(line))
put()
paper.write()
