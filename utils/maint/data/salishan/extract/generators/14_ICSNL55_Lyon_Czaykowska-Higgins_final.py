"""The ops of 14_ICSNL55_Lyon_Czaykowska-Higgins_final: John Lyon and Ewa Czaykowska-Higgins on
orbital clitics in Nxaʔamxčín and Nsyilxcn, the intransitive subject pronouns, tense and yes/no
question clitics that precede or follow a prosodic host, linearized by STAY and STRONG-START
constraints.

Each example closes on its source, a speaker's initials (LL, VF) or a page of Kinkade's field
notebooks (W2.88), and the language's name in parentheses, which sets the example's language.
Table 1 is a caption and a note to each printed line.

The examples are read here, a lettered part at a time, by what each line holds. A part's first line
is its form, a prediction marked p or p* included, with what the page sets right of it: a
translation, a source, a cross-reference (cf. 6a) or a label (tense orbit) a note. Under it a line
in quotes is the translation, a line in capitals the gloss, a line of constraints (STAYω > *
STR-STω, T ⊕ F) a note, and any other line the segmentation, a prosodic parse among them with the
ranking the page sets right of it a note. The rule lists (42) and (43) set a Nsyilxcn form, its
translation and the form after ⟹; the templates, hierarchies and rankings are a note to each part.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Nxaʔamxčín"
AUTHORS = ["John Lyon", "Ewa Czaykowska-Higgins"]
paper = gen.Paper("14_ICSNL55_Lyon_Czaykowska-Higgins_final", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = []
LANGUAGES = [(LANGUAGE, "Moses-Columbian, Southern Interior Salish"), ("Nsyilxcn", "Okanagan, Southern Interior Salish")]
FOOTNOTES = paper.page_footnotes()
AT_FOOT = {one for parts, _ in FOOTNOTES.values() for one in parts}
RUNNING = paper.running_numbers_set()
REFERENCES = paper.find(r"^References$")
HEADINGS = paper.headings(1, REFERENCES - 1, skip=AT_FOOT)

# The examples set as templates, hierarchies, constraints and rankings, a note to each part.
DISPLAYS = {12, 36, 47, 48, 49, 50, 52, 54, 57, 59, 62, 66, 73, 74, 75, 76, 81}
# The rule lists, their heading over lettered Nsyilxcn forms.
RULES = {42, 43}
# (50b)'s definition wraps onto a line of the full measure, and (Selkirk 2011) under it.
ENDS = {50: r"^The basic idea is that if STAY-family"}
PART = re.compile(r"^([a-z])\.\s+(.*)$")
CONSTRAINT = re.compile(r"^!?\s*(?:STAY|STR-ST|STRONG-START|BINARITY|PROMOTE|\(\*?\s*STAY|\(STR-ST|\(\*STR-ST)|⊕|→")
LABELS = ("tense orbit", "second-position")
ENGLISH = {"the", "of", "and", "to", "in", "is", "that", "for", "as", "with", "are", "be", "by", "this",
           "which", "we", "on", "it", "not", "or", "from", "can", "will", "an", "these", "their"}


def printed(number):
    return paper.text(number).strip() and not paper.lines[number][2] and number not in RUNNING \
        and number not in AT_FOOT


def prose(text):
    """Whether a line is running prose: eight words or more at about the full measure, two of them
    English function words, few of them labels in capitals, and no constraint's arrow or revolving
    door. A segmentation as long, ʔay ʔač-kíč-št-m-š yaʕˀtú ... in (27), holds no English."""
    words = text.split()
    if len(words) < 8 or len(text) < 55 or text[:1] in "‘“([!>*√#" or re.search(r"[⊕→↔⟹‿]", text):
        return False
    english = sum(1 for one in words if one.lower().strip(",.;:()") in ENGLISH)
    labels = sum(1 for one in words if re.search(r"[A-Z]{2,}|\d(?:SG|PL)", one))
    return english >= 2 and labels < 0.25 * len(words)


def parts_of(start, number_label):
    """The example's lettered parts from line start, each [letter, [lines]], and the line after."""
    parts = [[None, [start]]]
    number = start + 1
    until = re.compile(ENDS[int(number_label)]) if int(number_label) in ENDS else None
    while number < REFERENCES:
        if not printed(number):
            number += 1
            continue
        text = paper.text(number)
        # (38)'s line of (i) (iii) (ii) under its brackets opens no example.
        if re.match(r"^\(\d+\)", text) or number in HEADINGS or (until.search(text) if until else prose(text)):
            break
        if PART.match(text):
            parts.append([PART.match(text).group(1), [number]])
        else:
            parts[-1][1].append(number)
        number += 1
    if parts[0][1] == [start] and len(parts) > 1 and not gen.EXAMPLE.match(paper.text(start)).group(2):
        parts.pop(0)
    # The first line holds the first part's letter, (11) a. p* kn ...
    first = PART.match(gen.EXAMPLE.match(paper.text(start)).group(2) or "")
    if first:
        parts[0][0] = first.group(1)
    return parts, number


def body(number, spaced=False):
    """Line number with its example's number and its part's letter taken off."""
    text = paper.spaced[number] if spaced else paper.text(number)
    text = re.sub(r"^\(\d+\)\s*", "", text)
    return re.sub(r"^[a-z]\.\s+", "", text).strip()


def example(start, where):
    number_label = gen.EXAMPLE.match(paper.text(start)).group(1)
    parts, end = parts_of(start, number_label)
    who = "Nsyilxcn" if int(number_label) in RULES else L
    for letter, lines in parts:
        label = "(%s%s)" % (number_label, letter or "")
        count = [0]

        def row(person, kind, form, line, gloss=None):
            count[0] += 1
            paper.add("%s line %d" % (label, count[0]), person, kind, form,
                      "page %d%s" % (paper.page(line), ", " + gloss if gloss else ""))

        def pieces_right(pieces, line, where_said):
            for piece in pieces:
                piece = " ".join(piece.split())
                if piece.startswith("‘"):
                    translation(piece, line)
                    continue
                # A source and a tag set one space apart, (LL, VF) (Nsyilxcn), are two pieces.
                for one in gen.pieces_of(piece) if piece.startswith("(") else [piece]:
                    row(A, "citation" if gen.is_source(one) else "note", one, line, where_said)

        def translation(said, line):
            split = gen.split_translation(said) or (said, [])
            row(A, "translation", split[0], line)
            for piece in split[1]:
                row(A, "citation" if gen.is_source(piece) else "note", piece, line,
                    "the tag or source at the right of the translation")

        if int(number_label) in DISPLAYS:
            row(A, "note", " ".join(body(one) for one in lines), lines[0], "set as an example")
            continue
        for index, line in enumerate(lines):
            text, spaced = body(line), body(line, spaced=True)
            if int(number_label) in RULES and letter is None:
                row(A, "note", text, line, "heading the rules under it")
                continue
            if index == 0:
                arrow = spaced.split("⟹")
                pieces = re.split(r"\s{3,}", arrow[0].strip())
                form, rest = pieces[0], pieces[1:]
                quoted = form.find(" ‘")
                if quoted > 0:
                    form, rest = form[:quoted], [form[quoted + 1:]] + rest
                row(who, "transcription", " ".join(form.split()), line)
                pieces_right(rest, line, "at the right of the form")
                if len(arrow) > 1:
                    row(who, "transcription", " ".join(arrow[1].split()), line, "the form after ⟹")
            elif text.startswith("‘"):
                pieces = re.split(r"\s{3,}", spaced)
                translation(" ".join(pieces[0].split()), line)
                pieces_right(pieces[1:], line, "at the right of the translation")
            elif re.fullmatch(r"(?:\((?:i{1,3}|iv)\)\s*)+", text):
                row(A, "note", text, line, "the numbers of the clitics' kinds under each bracket")
            elif re.fullmatch(r"(?:\([^()]+\)\s*)+", text):
                pieces_right(gen.pieces_of(text), line, "under the translation")
            elif CONSTRAINT.search(text) and not text.startswith("(ω") and not re.match(r"^\((?:φ|ι|ω)", text):
                row(A, "note", text, line, "the constraints the parse satisfies or violates")
            else:
                pieces = re.split(r"\s{3,}", spaced)
                tail = pieces[-1] if len(pieces) > 1 else ""
                if tail and (CONSTRAINT.search(tail) or tail in LABELS):
                    text = " ".join(" ".join(pieces[:-1]).split())
                row(who, "gloss" if gen.is_gloss(text) else "segmentation", text, line)
                if tail and (CONSTRAINT.search(tail) or tail in LABELS):
                    row(A, "note", " ".join(tail.split()), line,
                        "the ranking at the right of the parse" if CONSTRAINT.search(tail) else
                        "at the right of the segmentation")
    return end


def table(start, where):
    """Table 1: its caption a note and each printed line under it a note, to the paragraph after."""
    paper.add("Table 1", A, "note", paper.text(start), "page %d, the table's caption" % paper.page(start))
    end = paper.find(r"^In this paper, we will focus", start)
    for count, line in enumerate(range(start + 1, end), 1):
        paper.add("Table 1 line %d" % count, A, "note", paper.text(line), "page %d, Table 1" % paper.page(line))
    return end


blocks = {paper.find(r"^Table 1: "): table}
at = 1
for label in range(1, 84):
    at = next(one for one in range(at, REFERENCES) if one not in AT_FOOT
              and re.match(r"^\(%d\)(?:\s|$)" % label, paper.text(one)))
    blocks[at] = example
paper.standard(AUTHORS, NAMES, LANGUAGES, front_languages=2, blocks=blocks, headings=HEADINGS)
print("# untagged:", paper.languages_by_tag({"(Nxaʔamxčín)": LANGUAGE, "(Nsyilxcn)": "Nsyilxcn"}), file=sys.stderr)

# Note 55 sets two examples side by side, (i) at the left and (ii) a. to d. at the right, and the
# note's example reader takes the two columns as one example (i). Each tier of (i) holds as many
# words as its form, the rest of the tier is (iia)'s, the second quote is (iia)'s translation, and
# b. to d. are (ii)'s parts.
side = [row for row in paper.rows if row[0].startswith("footnote 55 (")]
first = paper.rows.index(side[0])
tiers = {row[0]: row for row in side}
left, right = tiers["footnote 55 (i) line 1"][3].split(" (ii) a. ")
words = len(left.split())
split = [left, right]
for line in (2, 3):
    tier = tiers["footnote 55 (i) line %d" % line][3].split()
    split += [" ".join(tier[:words]), " ".join(tier[words:])]
said = re.match(r"^(‘.*?’)\s+(‘.*’)$", tiers["footnote 55 (i) line 4"][3])
split += [said.group(1), said.group(2)]
rebuilt = []
for column, label in enumerate(("(i)", "(iia)")):
    for line in range(1, 5):
        row = list(tiers["footnote 55 (i) line %d" % line])
        row[0], row[3] = "footnote 55 %s line %d" % (label, line), split[2 * (line - 1) + column]
        rebuilt.append(row)
for row in side:
    if re.match(r"^footnote 55 \(i[b-d]\)", row[0]):
        rebuilt.append([row[0].replace("(i", "(ii", 1)] + row[1:])
paper.rows[first:first + len(side)] = rebuilt
paper.write()
