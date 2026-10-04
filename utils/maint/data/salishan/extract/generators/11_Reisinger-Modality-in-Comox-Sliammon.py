"""The ops of 11_Reisinger-Modality-in-Comox-Sliammon: D. K. E. Reisinger on the modal system of
ʔayʔaǰuθəm (Comox-Sliammon), from data elicited from twelve speakers: the inferential č̓a, the
reportative k̓ʷa and the clitic strings səm=kʷa, səm=kʷi and səm=kʷu as variable force epistemic
modals, the borrowed have to for priority modality, the auxiliary ǰaqaʔ, the modal-temporal
interactions of each, and lexical entries after Peterson (2010).

The examples take many shapes and one reader takes them all, line by line: an English sentence
is a note to its lettered part; a CONTEXT, a ✓ CONTEXT and its wrapped lines are a note; a line of
the language over a gloss is a segmentation (a transcription where it holds no break) and a gloss,
and a pair that wraps under them before any translation is joined to them; a quotation, a Prompt:
and a Literally: are translations; a Comment (by E.P.) is a speaker comment; a reading in
brackets, [PAST PERSPECTIVE | FUT. ORIENTATION], is a note. An example ends at a line of prose. The
paper has no example (11). The formulas of (62) to (67) are a note for each title and each where
clause and a formula for each line opening on ⟦ with the lines it wraps onto. The four tables are
a caption note and a note to each printed line.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "ʔayʔaǰuθəm"
AUTHORS = ["D. K. E. Reisinger"]
paper = gen.Paper("11_Reisinger-Modality-in-Comox-Sliammon", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("E.P.", "the consultant whose comments are quoted under the examples"),
         ("Phyllis Dominic", "ʔayʔaǰuθəm consultant, Tla’amin"), ("Jerry Francis", "ʔayʔaǰuθəm consultant, Tla’amin"),
         ("Karen Galligos", "ʔayʔaǰuθəm consultant, Tla’amin"), ("Freddie Louie", "ʔayʔaǰuθəm consultant, Tla’amin"),
         ("Elsie Paul", "ʔayʔaǰuθəm consultant, Tla’amin"), ("Margaret Vivier", "ʔayʔaǰuθəm consultant, Tla’amin"),
         ("Betty Wilson", "ʔayʔaǰuθəm consultant, Tla’amin"), ("Maggie Wilson", "ʔayʔaǰuθəm consultant, Tla’amin"),
         ("Bill Blainey", "ʔayʔaǰuθəm consultant, Homalco"), ("Joanne Francis", "ʔayʔaǰuθəm consultant, Homalco"),
         ("Marion Harry", "ʔayʔaǰuθəm consultant, Homalco"), ("Herman Francis", "ʔayʔaǰuθəm consultant, Klahoose"),
         ("Kratzer", "Angelika Kratzer, the theory of modality"),
         ("Condoravdi", "Cleo Condoravdi, modal-temporal interactions"),
         ("Portner", "Paul Portner, Modality (2009)"), ("Rullmann", "Hotze Rullmann, modals as distributive indefinites"),
         ("Peterson", "Tyler Peterson, the strengthening account"), ("Menzies", "Stacey Menzies, Nsyilxcen modality"),
         ("Watanabe", "Honoré Watanabe, the grammar of Sliammon (2003)"), ("Sweetser", "Eve Sweetser (1991)"),
         ("Nauze", "Fabrice Nauze, modality in typological perspective")]
LANGUAGES = [(LANGUAGE, "Comox-Sliammon, Central Salish, ISO 639-3 coo"),
             ("Comox-Sliammon", "ʔayʔaǰuθəm"), ("Central Salish", "the branch"), ("Salish", "the family"),
             ("English", "the translations and the borrowing have to"),
             ("St’át’imcets", "Northern Interior Salish, in (9a)"), ("Nsyilxcen", "Southern Interior Salish, in (9b) and (10)"),
             ("Skwxwú7mesh", "Central Salish, in (9c)"), ("Sechelt", "Central Salish, the cognates k̓ʷa and yaka"),
             ("SENĆOŦEN", "Central Salish, the modal yeq"), ("Gitksan", "Tsimshianic, Peterson's modals"),
             ("Homalco", "a ʔayʔaǰuθəm speech community"), ("Klahoose", "a ʔayʔaǰuθəm speech community"),
             ("Tla’amin", "a ʔayʔaǰuθəm speech community")]

# The language of an example's tiers where it is no ʔayʔaǰuθəm: by the source line over them in
# (9), and (10) from the prose before it.
SOURCES = ("St’át’imcets", "Nsyilxcen", "Skwxwú7mesh")
EXAMPLE_LANGUAGE = {"10": "Nsyilxcen"}

CONTEXT = re.compile(r"^(?:[✓#]\s*)?CONTEXT\b")
# A lettered part, a judgment before its letter or not, and b., c. set over one form.
SUB = re.compile(r"^([✓#*?]\s*)?([a-z](?:\.,\s*[a-z])*)\.\s+(.*)$")
LABELED = re.compile(r"^(?:Prompt|Literally):\s*‘")
QUOTE = re.compile(r"^[?#*]*\s*‘")
COMMENT = re.compile(r"^Comment \(by [A-Z.]+\):")
SOURCE = re.compile(r"^(\S+) \([^()]+\):$")
# A reading at the right of a line, Mary cmay ac-qíc-lx [POSSIBILITY: EPISTEMIC].
READING = re.compile(r"^(.*\S)\s+(\[[A-Z][^\]]*\])$")
# A letter outside English's, or a morpheme break: what an English sentence does not hold.
LANGUAGE_LETTER = re.compile(r"[-=~]|[^\x00-\x7f‘’“”–]")

found = paper.page_footnotes()
SKIP = {one for parts, _ in found.values() for one in parts} | set(paper.volume_header())
RUNNING = paper.running_numbers_set()
BODY_END = paper.find(r"^References$")
HEADINGS = paper.headings(paper.find(r"^Keywords:") + 1, BODY_END - 1, skip=SKIP)


def lines_from(number):
    """The body's lines from number on, passing over footnotes, running numbers and page marks."""
    while number < BODY_END:
        if number not in SKIP and number not in RUNNING and not paper.lines[number][2] and paper.text(number).strip():
            yield number
        number += 1


def after(number):
    """The next body line after number, or None."""
    return next(lines_from(number + 1), None)


def is_tier(number, text):
    """Whether text, line number's words, is a line of the language: the line under it is a gloss,
    and it is none, holds a letter or a break English does not, and ends on no stop."""
    below = after(number)
    found_reading = READING.match(text)
    text = found_reading.group(1) if found_reading else text
    if below is None or re.search(r"[.!?]$", text) or not LANGUAGE_LETTER.search(text) or gen.is_gloss(text):
        return False
    under = paper.text(below)
    return gen.is_gloss(under) and not CONTEXT.match(under) and not under.startswith(("[", "‘")) and \
        not LABELED.match(under) and not SUB.match(under)


def example(start, where):
    """One numbered example, from its number to the first line of prose after it."""
    number_label = gen.EXAMPLE.match(paper.text(start)).group(1)
    state = {"label": number_label, "who": EXAMPLE_LANGUAGE.get(number_label, L), "open": None,
             "english": True, "tiers": False, "counts": {}}

    def add(who, kind, form, gloss):
        counts = state["counts"]
        counts[state["label"]] = counts.get(state["label"], 0) + 1
        paper.add("(%s) line %d" % (state["label"], counts[state["label"]]), who, kind, form, gloss)

    def close():
        held = state["open"]
        if held:
            kind, who, parts, at, gloss = held
            add(who, kind, " ".join(parts), "page %d%s" % (at, gloss))
        state["open"] = None

    def open_(kind, who, text, at, gloss=""):
        close()
        state["open"] = (kind, who, [text], at, gloss)

    def balanced(held, left, right):
        joined = " ".join(held[2])
        return joined.count(left) <= joined.count(right)

    def tier(text, number):
        """A line of the language and its gloss, and a pair that wraps under them. Returns the last
        line it used."""
        close()
        found_reading = READING.match(text)
        form, reading = found_reading.groups() if found_reading else (text, None)
        gloss_line = after(number)
        forms, glosses = [form.strip()], [paper.text(gloss_line)]
        last = gloss_line
        following = after(last)
        while following is not None and following not in HEADINGS and following not in STARTS and \
                not SUB.match(paper.text(following)) and is_tier(following, paper.text(following)):
            forms.append(paper.text(following))
            glosses.append(paper.text(after(following)))
            last = after(following)
            following = after(last)
        joined = " ".join(forms)
        at = paper.page(number)
        add(state["who"], "segmentation" if re.search(r"[-=~]", joined) else "transcription", joined, "page %d" % at)
        add(state["who"], "gloss", " ".join(glosses), "page %d" % at)
        if reading:
            add(A, "note", reading, "page %d, the reading at the right of the line" % at)
        state["english"] = False
        return last

    def item(text, number):
        """Take what one line holds, its number or its letter off. Returns the last line it used,
        or None where the line is prose and the example is over."""
        at = paper.page(number)
        if CONTEXT.match(text):
            open_("note", A, text, at, ", the context")
            return number
        source = SOURCE.match(text)
        if source and source.group(1) in SOURCES:
            close()
            state["who"] = source.group(1)
            add(A, "note", text, "page %d, the language and the source" % at)
            return number
        if LABELED.match(text) or QUOTE.match(text):
            open_("translation", A, text, at)
            if balanced(state["open"], "‘", "’"):
                close()
            state["english"] = False
            return number
        if COMMENT.match(text):
            open_("speaker comment", A, text, at, ", under the example, E.P.")
            if balanced(state["open"], "[", "]") and balanced(state["open"], "“", "”"):
                close()
            return number
        if text.startswith("["):
            close()
            add(A, "note", text, "page %d, the reading" % at)
            return number
        if is_tier(number, text):
            return tier(text, number)
        held = state["open"]
        # A line that carries on what is open: a context to its next element, a quotation or a
        # comment to its close, a sentence onto a line that opens in lower case.
        if held and (held[4] == ", the context" or held[0] in ("speaker comment", "translation") or
                     held[4] == ", set as an example" and re.match(r"^[a-z]", text)):
            held[2].append(text)
            if held[0] == "speaker comment" and balanced(held, "[", "]") and balanced(held, "“", "”") or \
                    held[0] == "translation" and balanced(held, "‘", "’"):
                close()
            return number
        if state["english"]:
            open_("note", A, text, at, ", set as an example")
            state["english"] = False
            return number
        return None

    def line(text, number):
        """One line of the example: a lettered part opens a new label, then its words are read."""
        lettered = SUB.match(text)
        if lettered and not CONTEXT.match(text):
            close()
            judgment, letters, rest = lettered.groups()
            state["label"] = number_label + re.sub(r"[.,\s]", "", letters)
            state["english"] = True
            text = (judgment or "") + rest
        return item(text.strip(), number)

    last = line(gen.EXAMPLE.match(paper.text(start)).group(2) or "", start)
    for number in lines_from(start + 1):
        if number <= last:
            continue
        if number in HEADINGS or number in STARTS or number in TABLES:
            close()
            return number
        used = line(paper.text(number), number)
        if used is None:
            close()
            return number
        last = used
    close()
    return BODY_END


def formula(start, where):
    """A formula block of (62) to (67): its title a note, each line opening on ⟦ a formula and each
    where clause a note, with the lines each wraps onto."""
    label = gen.EXAMPLE.match(paper.text(start)).group(1)
    rows = [["note", [gen.EXAMPLE.match(paper.text(start)).group(2)], paper.page(start)]]
    for number in lines_from(start + 1):
        text = paper.text(number)
        if number in HEADINGS or number in STARTS:
            break
        if text.startswith("⟦"):
            rows.append(["formula", [text], paper.page(number)])
        elif text.startswith("..."):
            rows.append(["note", [text], paper.page(number)])
        elif re.match(r"^[a-z]", text):
            rows[-1][1].append(text)
        else:
            break
    for count, (kind, parts, at) in enumerate(rows, 1):
        paper.add("(%s) line %d" % (label, count), A, kind, " ".join(parts), "page %d" % at)
    return number


def table(start, where):
    """A table: its caption a note, and a note to each printed line to the prose after it."""
    caption = paper.text(start)
    label = TABLES[start]
    paper.add(label, A, "note", caption, "page %d, the caption" % paper.page(start))
    count = 0
    for number in lines_from(start + 1):
        if number in HEADINGS or re.match(TABLE_ENDS[label], paper.text(number)):
            return number
        count += 1
        paper.add("%s line %d" % (label, count), A, "note", paper.text(number),
                  "page %d, a line of the table as the text layer reads it" % paper.page(number))
    return BODY_END


STARTS, previous = {}, 1
for count in range(1, 68):
    if count == 11:
        continue
    at = paper.find(r"^\(%d\)\s+(?!presents|Marking)[^.]" % count, previous)
    STARTS[at] = str(count)
    previous = at + 1

TABLE_ENDS = {"Table 1": r"^3 The Modal Inventory$", "Table 2": r"^3\.4 ", "Table 3": r"^In the circumstantial domain",
              "Table 4": r"^5 Variable Force Modals$"}
TABLES = {paper.find(r"^%s:" % label): label for label in TABLE_ENDS}

blocks = {at: formula if int(label) >= 62 else example for at, label in STARTS.items()}
blocks.update({at: table for at in TABLES})
paper.standard(AUTHORS, NAMES, LANGUAGES, front_languages=1, blocks=blocks, headings=HEADINGS)
paper.write()
