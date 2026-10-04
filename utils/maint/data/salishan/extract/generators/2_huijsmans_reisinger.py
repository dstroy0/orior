"""The ops of 2_huijsmans_reisinger: Marianne Huijsmans and D. K. E. Reisinger on the clitics kʷa,
kʷi, ta and ti of ʔayʔaǰuθəm (Comox-Sliammon) as clausal demonstratives, parallel to the regular
demonstratives tiʔi, taʔa, kʷiši and kʷaʔa in visibility and proximity, locating the event situation
relative to the utterance situation and placed in Fin*, after Ramchand and Svenonius (2014).

An example is read line by line: a form over its gloss, a segmentation where it holds a break and a
transcription where it holds none, a pair that wraps under them joined to them, a translation in
quotes or after Intended:, with its source at its right a citation. A Context: line over the form,
a ✓ or ✘ Context under the translation and a Consultant’s comment each run to the line that closes
their sentence and brackets. (26) is a tree drawn as a picture, written as one note read off a
render, and (27) is a formula. Table 1 is a caption note and a note to each printed line. Footnote
8 sets two English sentences as examples, and footnote 9 sets (iii) to (v) in two columns, each
part a. at the left and b. at the right: post-passes write both as the page sets them.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "ʔayʔaǰuθəm"
AUTHORS = ["Marianne Huijsmans", "D. K. E. Reisinger"]
paper = gen.Paper("2_huijsmans_reisinger", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Elsie Paul", "ʔayʔaǰuθəm speaker"), ("Marion Harry", "ʔayʔaǰuθəm speaker"),
         ("Freddie Louie", "ʔayʔaǰuθəm speaker"), ("Phyllis Dominic", "ʔayʔaǰuθəm speaker"),
         ("Henry Davis", "thanked for feedback"), ("Bruno Andreotti", "pointed out the parallels in vowels"),
         ("Watanabe", "Honoré Watanabe, the grammar of Sliammon (2003)"),
         ("Beaumont", "Ronald Beaumont, the Sechelt dictionary (2011)"), ("Blake", "S. J. Blake (2000)"),
         ("Ramchand", "Gillian Ramchand, the functional hierarchy"),
         ("Svenonius", "Peter Svenonius, the functional hierarchy")]
LANGUAGES = [(LANGUAGE, "Comox-Sliammon, Central Salish"), ("Comox-Sliammon", "ʔayʔaǰuθəm"),
             ("Central Salish", "the branch"), ("Sechelt", "Central Salish, the cognates of the four particles"),
             ("English", "the modal will"), ("St’at’imcets", "the predicate does not move as high as C"),
             ("SENĆOŦEN", "second-position clitics"), ("Kwak’wala", "no prefixes and no proclitics"),
             ("Salish", "the family")]

FOOTNOTES = paper.page_footnotes()
SKIP = {one for parts, _ in FOOTNOTES.values() for one in parts} | set(paper.volume_header())
RUNNING = paper.running_numbers_set()
REFERENCES = paper.find(r"^References$")

HEADINGS = paper.headings(1, REFERENCES - 1, skip=SKIP)

# A lettered part, a., and in footnote 9 a part of it, b.i and b.ii.
PART = re.compile(r"^([a-z])(?:\.([iv]+)\.?|\.)\s+(.*)$")
CONTEXT = re.compile(r"^(?:[✓✘]\s*)?Context\b")
JUDGED = re.compile(r"^[✓✘]")
COMMENT = re.compile(r"^Consultant’s comment:")
# A translation, and one after Intended: with or without its opening quote, Intended: Yes, he
# arrived.' in footnote 9.
TRANSLATION = re.compile(r"^(?:‘|Intended:)")


def lines_from(number):
    """The body's lines from number on, passing over footnotes, running numbers and page marks."""
    while number < REFERENCES:
        if number not in SKIP and number not in RUNNING and not paper.lines[number][2] and paper.text(number).strip():
            yield number
        number += 1


def closed(text):
    """Whether a context or a comment ends on text: a stop or an ellipsis closes its sentence, with
    a quote or a bracket after it, and its square brackets are all closed."""
    return bool(re.search(r"[.!?…][’”)\]]*$", text)) and text.count("[") == text.count("]")


def read(number_label, items, prefix=""):
    """One numbered example from items, (text, line) pairs in the order the page sets them, its
    number off the first. Returns the line of the first item no part of the example holds, or None
    where the items run out. A text of None ends the example."""
    state = {"label": number_label, "count": 0, "stage": "start", "open": None, "form": None, "gloss": None,
             "judged": None}

    def add(who, kind, form, at, gloss=""):
        state["count"] += 1
        paper.add("%s(%s) line %d" % (prefix, state["label"], state["count"]), who, kind, " ".join(form.split()),
                  "page %d%s" % (paper.page(at), gloss))
        return len(paper.rows) - 1

    def translation(said, at):
        split = gen.split_translation(said)
        if split is None:
            return False
        said, pieces = split
        add(A, "translation", said, at)
        for piece in pieces:
            source = gen.is_source(piece)
            add(A, "citation" if source else "note", piece, at,
                ", the tag or source at the right of the translation" if source else ", at the right of the translation")
        state["stage"] = "after"
        return True

    def line(text, number):
        """Take one line of the example. Returns False where the line is no part of it."""
        if text is None:
            return False
        part = PART.match(text)
        if part and state["open"] is None:
            state.update(label=number_label + part.group(1) + (part.group(2) or ""), count=0, stage="start",
                         form=None, gloss=None, judged=None)
            text = part.group(3)
        held = state["open"]
        if held:
            held[2].append(text)
            joined = " ".join(held[2])
            if held[0] == "translation":
                if translation(joined, held[3]):
                    state["open"] = None
            elif closed(joined):
                index = add(A, held[0], joined, held[3], held[4])
                state["judged"] = index if JUDGED.match(joined) else None
                state["open"] = None
            return True
        # A ✓ or ✘ context runs on over the lines set in under its mark, She says ti ƛaʔayin ʔaxʷ to
        # me. under ✓ Context 3 in (15), though its first line closes a sentence.
        if state["judged"] is not None and not (CONTEXT.match(text) or COMMENT.match(text) or TRANSLATION.match(text)) \
                and paper.word_positions(number)[0][0] > state["mark"] + 5:
            paper.rows[state["judged"]][3] += " " + " ".join(text.split())
            return True
        state["judged"] = None
        if CONTEXT.match(text) or COMMENT.match(text):
            kind = "speaker comment" if COMMENT.match(text) else "note"
            gloss = ", under the example, the consultant not named" if kind == "speaker comment" else \
                ", over the example" if state["stage"] == "start" else ", the context"
            state["mark"] = paper.word_positions(number)[0][0]
            if closed(text):
                index = add(A, kind, text, number, gloss)
                state["judged"] = index if JUDGED.match(text) else None
            else:
                state["open"] = [kind, A, [text], number, gloss]
            return True
        if state["stage"] == "start" and text.endswith(":"):
            add(A, "note", text, number, ", over the example")
            return True
        if TRANSLATION.match(text):
            if not translation(text, number):
                state["open"] = ["translation", A, [text], number, ""]
            return True
        if state["stage"] == "start":
            kind = "segmentation" if re.search(r"[-=•]", text) else "transcription"
            state["form"] = add(L, kind, text, number)
            state["stage"] = "gloss"
            return True
        if state["stage"] == "gloss":
            state["gloss"] = add(L, "gloss", text, number)
            state["stage"] = "tiers"
            return True
        # A pair that wraps under the form and its gloss, qam-it over accompany-CTR in (5b).
        if state["stage"] in ("tiers", "wrap"):
            row = paper.rows[state["form"] if state["stage"] == "tiers" else state["gloss"]]
            row[3] += " " + " ".join(text.split())
            state["stage"] = "wrap" if state["stage"] == "tiers" else "tiers"
            return True
        return False

    for text, number in items:
        if not line(text, number):
            break
    else:
        number = None
    held = state["open"]
    if held:
        add(A, held[0], " ".join(held[2]), held[3], held[4])
    return number


def example(start, where):
    """One numbered example of the body, from its number to the first line after it that no part
    of it holds."""
    number_label, rest = gen.EXAMPLE.match(paper.text(start)).groups()
    items = [((rest or "").strip(), start)] + \
        [(None if number in HEADINGS or number in blocks else paper.text(number).strip(), number)
         for number in lines_from(start + 1)]
    after = read(number_label, iter(items))
    return REFERENCES if after is None else after


def tree(start, where):
    """(26), a tree drawn as a picture, its labels in no text layer: a note on its number, the
    tree read off a render of page 14 in its gloss."""
    paper.add("(26) line 1", A, "note", "(26)",
              "page %d, a tree, a picture read off a render: CP over C (səm) and Fin*P, Fin*P over Fin* (kʷi) "
              "and TP, TP a triangle over qʷəl̓ ƛ̓iq̓ʷ qaya" % paper.page(start))
    return start + 1


def formula(start, where):
    """(27), the denotation of kʷi."""
    paper.add("(27) line 1", A, "formula", gen.EXAMPLE.match(paper.text(start)).group(2),
              "page %d" % paper.page(start))
    return start + 1


def table(start, where):
    """Table 1: its caption a note, and a note to each printed line to the prose after it."""
    paper.add("Table 1", A, "note", paper.text(start), "page %d, the caption" % paper.page(start))
    count = 0
    for number in lines_from(start + 1):
        if paper.text(number).startswith("This paper is structured"):
            return number
        count += 1
        paper.add("Table 1 line %d" % count, A, "note", " ".join(paper.text(number).split()),
                  "page %d, a line of the table as the text layer reads it" % paper.page(number))
    return REFERENCES


blocks = {}
for number in lines_from(1):
    if gen.EXAMPLE.match(paper.text(number)):
        label = gen.EXAMPLE.match(paper.text(number)).group(1)
        blocks[number] = {"26": tree, "27": formula}.get(label, example)
blocks[paper.find(r"^Table 1:")] = table
paper.standard(AUTHORS, NAMES, LANGUAGES, front_languages=1, blocks=blocks, headings=HEADINGS)

# Footnote 8 sets two English sentences as examples, each with its reading at its right, and the
# note's last sentence under them. The note's example reader takes them for tiers of the language.
held = [row for row in paper.rows if row[0].startswith("footnote 8 (")]
at = paper.rows.index(held[0])
del paper.rows[at:at + len(held)]
mark = len(paper.rows)
for number in range(paper.find(r"^\(i\) Saoirse"), paper.find(r"^For a more detailed analysis") + 1):
    page = paper.page(number)
    said = re.match(r"^\((i+)\)\s+(.*?\.)\s+(\([a-z ]+\))$", paper.text(number))
    if said:
        paper.add("footnote 8 (%s) line 1" % said.group(1), A, "note", said.group(2), "page %d, set as an example" % page)
        paper.add("footnote 8 (%s) line 2" % said.group(1), A, "note", said.group(3),
                  "page %d, at the right of the sentence" % page)
    else:
        paper.add("footnote 8", A, "note", paper.text(number), "page %d, footnote 8" % page)
rows = paper.rows[mark:]
del paper.rows[mark:]
paper.rows[at:at] = rows


def columns_of(first, last, split):
    """The lines first to last of a set of examples set in two columns, as (text, line) pairs: the
    left column's lines, then the right column's (the words from split on), then the lines under
    both, which the left column's parts set alone."""
    left, right, under = [], [], []
    for number in range(first, last + 1):
        words = paper.word_positions(number)
        if not words:
            continue
        own = [word for place, word in words if place < split]
        other = [word for place, word in words if place >= split]
        if other:
            left.append((" ".join(own), number))
            right.append((" ".join(other), number))
        elif right:
            under.append((" ".join(own), number))
        else:
            left.append((" ".join(own), number))
    return left + right + under


# Footnote 9 sets (iii) to (v) in two columns, a. at the left and b. or b.i at the right, and
# b.ii under a.; the note's example reader reads each printed line across. (vi) and (vii) stand
# in one column, and this reader takes them as the body's examples are taken.
held = [row for row in paper.rows if row[0].startswith("footnote 9 (") or
        row[0] == "footnote 9" and row[3].startswith("b.ii.")]
for row in held:
    paper.rows.remove(row)
STARTS = {label: paper.find(r"^\(%s\)\s" % label, paper.find(r"^9 We have not done")) for label in
          ("iii", "iv", "v", "vi", "vii")}
ENDS = {"iii": STARTS["iv"] - 1, "iv": STARTS["v"] - 1, "v": paper.find(r"^Word order evidence") - 1,
        "vi": paper.find(r"^In textual material") - 1, "vii": paper.find(r"^\(vii\)") + 2}
anchors = {"iii": "We have not done", "vi": "Word order evidence", "vii": "In textual material"}
for group in (("iii", "iv", "v"), ("vi",), ("vii",)):
    mark = len(paper.rows)
    for label in group:
        first, last = STARTS[label], ENDS[label]
        items = columns_of(first, last, 230) if label in ("iii", "iv", "v") else \
            [(" ".join(paper.text(number).split()), number) for number in range(first, last + 1) if paper.text(number)]
        items[0] = (re.sub(r"^\(%s\)\s*" % label, "", items[0][0]), items[0][1])
        read(label, iter(items), prefix="footnote 9 ")
    rows = paper.rows[mark:]
    del paper.rows[mark:]
    after = next(index for index, row in enumerate(paper.rows)
                 if row[0] == "footnote 9" and row[3].startswith(anchors[group[0]])) + 1
    paper.rows[after:after] = rows
paper.write()
