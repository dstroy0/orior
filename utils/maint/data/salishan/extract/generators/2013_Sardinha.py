"""The ops of 2013_Sardinha: Katie Sardinha's Nominal, verbal, and idiomatic uses of -nukʷ in
Kwak'wala.

Every example is read here and not by gen.example(). A sentence sets two tiers, the line and its
gloss, over the translation, and (68) and (69) set three. What stands after the tiers is the
paper's own: a tag in parentheses at the right of the translation, or of the gloss where no
translation is given, a literal or intended reading in brackets, the speaker's comments, and the
author's question to the speaker, Katie:. Each caption and context over an example is given its
printed line count, since a context runs on past a stop, (17). The side by side parts of (20), (67)
and (72), the table of (27), the Wakashan family, Tables 1 and 2 and the Appendix's word lists are
read by the gaps the page sets between their columns, each checked against the printed lines letter
for letter.
"""
import os
import collections
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Kwak’wala"
AUTHORS = ["Katie Sardinha"]
paper = gen.Paper("2013_Sardinha", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("RDC", "the author's consultant, thanked in the note on the title"),
         ("Jen Abel", "thanked for help eliciting data on -nukʷ"),
         ("Henry Davis", "thanked for comments"), ("Gary Holland", "thanked for comments"),
         ("Boas", "Franz Boas (1911, 1947), the grammars the paper compares with"),
         ("Hunt", "George Hunt, Boas and Hunt's (1902) Kwakiutl Texts"),
         ("Levine", "Robert D. Levine (1980, 1984), a lexical analysis of the passive"),
         ("Anonby", "Stan J. Anonby (1997), the dialect areas"),
         ("Fortescue", "Michael Fortescue (2006), the grammaticalization divide in Wakashan"),
         ("Grubb", "David McC. Grubb (1977), a practical writing system and dictionary"),
         ("Anderson", "Stephen R. Anderson (1984), Kwakwala syntax"),
         ("Chung", "Yunhee Chung (2007), the Kwak’wala nominal domain"),
         ("Nicholson", "Marianne Nicolson (2009, with Adam Werle), the determiner systems"),
         ("Werle", "Adam Werle (2009, with Marianne Nicolson), the determiner systems")]
LANGUAGES = [(LANGUAGE, "Northern Wakashan"), ("Wakashan", "the family"), ("English", "the translations"),
             ("Haisla", "Northern Wakashan"), ("Heiltsuk", "Northern Wakashan"), ("Oowekyala", "Northern Wakashan"),
             ("Ditidaht", "Southern Wakashan"),
             ("Makah", "Southern Wakashan")]
HEADINGS = {paper.find(pattern): label for label, pattern in (
    ("1", r"^1 Introduction$"), ("2", r"^2 Kwak’wala Language Overview$"), ("3", r"^3 The synchronic"),
    ("3.1", r"^3\.1 "), ("3.2", r"^3\.2 "), ("3.3", r"^3\.3 "), ("5", r"^5 Conclusion$"),
    ("Appendix", r"^Appendix: Word Lists$"))}
FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
RUNNING = paper.running_numbers_set()

# The printed lines of each caption over an example, and of each context: (3) wraps, and a context
# runs on past a stop, (17)'s candy. / She tells her mom.
CAPTIONS = {"1": 1, "2": 1, "3": 2, "4": 1, "5": 1, "12": 1, "21": 1, "22": 1, "23": 1, "28": 1, "29": 1,
            "30": 1, "37": 1, "45": 1, "46": 1, "47": 1, "52": 1, "53": 1, "63": 1}
CONTEXTS = {"16": 1, "17": 3, "18": 2, "19": 1, "24": 2, "51": 2, "58": 3, "59": 2, "61": 2, "62": 2}
# (68) and (69) set Boas's word over its segmentation and gloss.
THREE = {"68", "69"}
TAG = r"\((?:VF|JF|TR|TF)(?:/(?:VF|JF|TR|TF))?\)\d?|\(Boas \d{4}: \d+\)"
TAGGED = re.compile(r"^(.*?\S)\s*(%s)$" % TAG)
BOAS = re.compile(r"^\(Boas \d{4}: \d+\)$")
UNREAD = []


def printed(number):
    return bool(paper.text(number).strip()) and not paper.lines[number][2] and number not in RUNNING \
        and number not in FOOT


def after(number):
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def columns(number):
    """The columns of a printed line, split at the gaps the page sets between them."""
    return re.split(r"\s{2,}", paper.spaced[number].strip())


def same_letters(cells, first, last):
    """Check that the cells written from lines first to last hold the printed letters, all of them
    and no others, whatever the order the columns set them in."""
    written = sorted("".join(cells).replace(" ", ""))
    source = sorted("".join(paper.text(one) for one in range(first, last + 1) if printed(one)).replace(" ", ""))
    assert written == source, (first, last, "".join(written), "".join(source))


def example_lines():
    """The line each example opens on: its number in sequence, set a gap before what follows it
    ((27) and (37) set a space), and not a line of prose that opens on a reference, (46) below.5."""
    found, expected = [], 1
    for number in range(1, paper.last + 1):
        opened = gen.EXAMPLE.match(paper.text(number))
        if printed(number) and opened and opened.group(1) == str(expected) and \
                (re.match(r"^\(\d+\)\s{2,}", paper.spaced[number]) or opened.group(1) in ("27", "37")):
            found.append(number)
            expected += 1
    assert expected == 73, expected
    return found


EXAMPLES = example_lines()


def is_translation(text):
    return text.startswith(("‘", "“", "Translation:"))


def is_after(text):
    return text.startswith(("[", "Speaker", "Katie:", "Context:")) or bool(BOAS.match(text))


def example(first, where):
    """A numbered example, from the line it opens on to the first line after it that is none of
    its own; returns that line."""
    label, rest = gen.EXAMPLE.match(paper.text(first)).groups()
    counts = collections.Counter()
    state = {"label": label, "letter": "`"}
    items, number = [(first, rest)], after(first)
    while number <= paper.last and number not in EXAMPLES:
        items.append((number, paper.text(number)))
        number = after(number)
    ends = number

    def row(who, kind, text, at, gloss=None):
        counts[state["label"]] += 1
        paper.add("(%s) line %d" % (state["label"], counts[state["label"]]), who, kind, text,
                  "page %d%s" % (paper.page(at), ", " + gloss if gloss else ""))

    def tagged(who, kind, text, at, gloss, where_tag):
        """A row with the tag at its right a citation of its own."""
        split = TAGGED.match(text)
        row(who, kind, split.group(1) if split else text, at, gloss)
        if split:
            row(A, "citation", split.group(2), at, "the tag at the right of %s" % where_tag)

    def translation(text, at):
        split = TAGGED.match(text)
        said, tag = (split.group(1), split.group(2)) if split else (text, None)
        note = re.match(r"^(.*[’”])\s*(\[[^\[\]]*\]|\([a-z][^()]*\))$", said)
        row(A, "translation", note.group(1) if note else said, at)
        if note:
            row(A, "note", note.group(2), at, "at the right of the translation")
        if tag:
            row(A, "citation", tag, at, "the tag at the right of the translation")

    def letter(text):
        """The lettered part a line opens, the letter the next in turn; (29b) sets no stop."""
        expected = chr(ord(state["letter"]) + 1)
        return re.match(r"^(%s)(?:\.\s+|\s+)(.*)$" % expected, text)

    def take(count, gloss):
        row(A, "note", " ".join(one for _, one in items[:count]), first, gloss)
        return count

    index = 0
    if label in CAPTIONS:
        index = take(CAPTIONS[label], "over the example")
    elif label in CONTEXTS:
        index = take(CONTEXTS[label], "the context over the example")
    mode = "start"
    while index < len(items):
        at, text = items[index]
        part = letter(text)
        following = items[index + 1][1] if index + 1 < len(items) else ""
        if part:
            state["letter"] = part.group(1)
            state["label"] = label + part.group(1)
            text = part.group(2)
            mode = "start"
            if text.endswith(":") or text.startswith("Katie:"):
                row(A, "note", text, at, "the author's question to the speaker" if text.startswith("Katie:")
                    else "over the part")
                index += 1
                continue
        elif text.endswith(":") and letter(following):
            state["label"] = label
            row(A, "note", text, at, "over the parts under it")
            index += 1
            mode = "start"
            continue
        if mode == "start":
            # The tiers, to a translation, to what stands after them or to the next part.
            kinds = ("transcription", "segmentation", "gloss") if label in THREE else ("transcription", "gloss")
            count = 0
            while True:
                tagged(L, kinds[count % len(kinds)], text, at, None, "the line")
                count += 1
                index += 1
                if index >= len(items):
                    return ends
                at, text = items[index]
                if is_translation(text) or is_after(text) or letter(text):
                    break
            assert count <= 4, (label, count)
            if is_translation(text):
                parts = [text]
                # A translation runs on where its line closes on no quote, tag or bracket.
                while not re.search(r"[’”\])]\d?$", parts[-1]) and index + 1 < len(items) and \
                        not (is_after(items[index + 1][1]) or letter(items[index + 1][1])):
                    index += 1
                    parts.append(items[index][1])
                translation(" ".join(parts), at)
                index += 1
            mode = "after"
            continue
        # After the tiers.
        if text.startswith("["):
            parts = [text]
            while " ".join(parts).count("[") > " ".join(parts).count("]"):
                index += 1
                parts.append(items[index][1])
            tagged(A, "note", " ".join(parts), at, "in brackets under the example", "the note")
        elif text.startswith(("Speaker", "Katie:")):
            parts = [text]
            while " ".join(parts).count("“") > " ".join(parts).count("”") or \
                    (text.startswith("Katie:") and index + 1 < len(items) and not re.search(r"[?.)”]$", parts[-1])):
                index += 1
                parts.append(items[index][1])
            speaker = text.startswith("Speaker")
            tagged(A, "speaker comment" if speaker else "note", " ".join(parts), at,
                   "the speaker's comment" if speaker else "the author's question to the speaker", "the comment")
        elif text.startswith("Context:"):
            tagged(A, "note", text, at, "the context under the example", "the context")
        elif BOAS.match(text):
            row(A, "citation", text, at, "the source under the translation")
        else:
            UNREAD.append((label, at, text))
            return at
        index += 1
    return ends


def word_list(first, where):
    """(10) and (11): a word to each lettered line and its meaning beside it."""
    label, caption = gen.EXAMPLE.match(paper.text(first)).groups()
    paper.add("(%s)" % label, A, "note", caption, "page %d, over the example" % paper.page(first))
    number = after(first)
    while number <= paper.last and number not in EXAMPLES:
        found = re.match(r"^([a-h])\.\s+(\S+)\s+(‘.*’)$", paper.text(number))
        if not found:
            break
        page = paper.page(number)
        paper.add("(%s%s) line 1" % (label, found.group(1)), L, "transcription", found.group(2), "page %d" % page)
        paper.add("(%s%s) line 2" % (label, found.group(1)), A, "translation", found.group(3), "page %d" % page)
        number = after(number)
    return number


def twenty(first, where):
    """(20): a question over its gloss, then the possible responses, i. and ii. side by side."""
    lines = [first]
    while len(lines) < 8:
        lines.append(after(lines[-1]))
    context, question, gloss, said, head, forms, glosses, answers = lines
    page = paper.page(first)
    paper.add("(20)", A, "note", gen.EXAMPLE.match(paper.text(context)).group(2),
              "page %d, the context over the example" % page)
    paper.add("(20a) line 1", L, "transcription", re.sub(r"^a\.\s+", "", paper.text(question)), "page %d" % page)
    paper.add("(20a) line 2", L, "gloss", paper.text(gloss), "page %d" % page)
    split = TAGGED.match(paper.text(said))
    paper.add("(20a) line 3", A, "translation", split.group(1), "page %d, printed without quotes" % page)
    paper.add("(20a) line 4", A, "citation", split.group(2), "page %d, the tag at the right of the translation" % page)
    paper.add("(20b)", A, "note", re.sub(r"^b\.\s+", "", paper.text(head)), "page %d, over the responses" % page)
    heads = columns(forms)
    assert heads[0] == "i." and heads[2] == "ii." and len(heads) == 4, heads
    for side, part in enumerate(("i", "ii")):
        why = "page %d, response %s., %s" % (page, part, ("on the left", "on the right")[side])
        answer = TAGGED.match(columns(answers)[side])
        paper.add("(20b%s) line 1" % part, L, "transcription", heads[1 + 2 * side], why)
        paper.add("(20b%s) line 2" % part, L, "gloss", columns(glosses)[side], why)
        paper.add("(20b%s) line 3" % part, A, "translation", answer.group(1), why)
        paper.add("(20b%s) line 4" % part, A, "citation", answer.group(2), why + ", the tag at the right of the translation")
    same_letters([paper.text(one) for one in lines[:5]] + heads + columns(glosses) + columns(answers), first, answers)
    return after(answers)


def twenty_seven(first, where):
    """(27): the semantic features of -nukʷ as a table, the meaning in the first column, each
    context in the second and whether -nukʷ is used in the third."""
    page = paper.page(first)
    last = paper.find(r"^predicate\. ", first)
    paper.add("(27)", A, "note", gen.EXAMPLE.match(paper.text(first)).group(2), "page %d, over the example" % page)
    heads = ("nominal stem + -nukʷ:", "Context", "-nukʷ")
    meaning = ("Y exists and X has Y, where X denotes a possessor subject and Y denotes a non-specific member "
               "of the set denoted by the nominal predicate.")
    rows = (("assert existence & assert possession; question existence & possession", "Yes"),
            ("assert existence; not possession", "No"), ("assert possession; not existence", "No"),
            ("assert absence and/or non-possession", "No"))
    same_letters(list(heads) + [meaning] + [one for pair in rows for one in pair], after(first), last)
    for head in heads:
        paper.add("(27)", A, "note", head, "page %d, a column's head" % page)
    paper.add("(27)", A, "note", meaning, "page %d, under %s, the meaning beside every row" % (page, heads[0]))
    for context, used in rows:
        paper.add("(27)", A, "note", context, "page %d, under Context" % page)
        paper.add("(27)", A, "note", used, "page %d, under -nukʷ, for %s" % (page, context))
    return after(last)


def side_by_side(first, where):
    """(67) and (72): two parts set side by side, a. on the left and b. on the right, each its
    word, segmentation and gloss over the translation; (67b) sets a stem over its gloss."""
    label = gen.EXAMPLE.match(paper.text(first)).group(1)
    opens = paper.find(r"^a\.\s", first)
    page = paper.page(first)
    caption = [one for one in range(first, opens) if printed(one)]
    paper.add("(%s)" % label, A, "note", re.sub(r"^\(\d+\)\s+", "", paper.joined(caption)),
              "page %d, over the example" % page)
    lines = [opens]
    while len(lines) < 4:
        lines.append(after(lines[-1]))
    head = columns(opens)
    assert head[0] == "a." and head[2] == "b." and len(head) == 4, head
    cells = [columns(one) for one in lines[1:]]
    left = [head[1]] + [one[0] for one in cells]
    right = [head[3]] + [one[1] for one in cells if len(one) > 1]
    same_letters([paper.text(one) for one in caption] + ["a.", "b."] + left + right, first, lines[-1])
    for part, tiers, side in (("a", left, "on the left"), ("b", right, "on the right")):
        kinds = ("transcription", "segmentation", "gloss", "translation")[4 - len(tiers):] if len(tiers) == 4 else \
            ("transcription", "gloss", "translation")
        for count, (kind, text) in enumerate(zip(kinds, tiers), 1):
            paper.add("(%s%s) line %d" % (label, part, count), A if kind == "translation" else L, kind, text,
                      "page %d, %s" % (page, side))
    return after(lines[-1])


def wakashan(first, where):
    """The Wakashan family, its two branches set as columns."""
    page = paper.page(first)
    last = paper.find(r"^Kwak’wala$", first)
    paper.add(where, A, "note", paper.text(first), "page %d, the title over the family's branches" % page)
    northern = ["Haisla", "Heiltsuk", "Oowekyala", "Kwak’wala"]
    southern = ["Nuu-Cha-Nulth", "Ditidaht", "Makah"]
    same_letters(["Northern", "Southern"] + northern + southern, after(first), last)
    for head, languages in (("Northern", northern), ("Southern", southern)):
        paper.add(where, A, "note", head, "page %d, a branch's head" % page)
        for language in languages:
            paper.add(where, A, "note", language, "page %d, under %s" % (page, head))
    # The prose breaks Nuu-Chah-Nulth over a line, and the name is found only here.
    paper.add(where, A, "language", "Nuu-Cha-Nulth", "Southern Wakashan")
    return after(last)


def table(first, label, caption, heads, rows, last):
    """A table: its caption, its column heads and each row's head and cells, the cells run on
    where they wrap."""
    page = paper.page(first)
    paper.add(label, A, "note", paper.joined(caption), "page %d, the table's caption" % page)
    same_letters(list(heads) + [one for name, cells in rows for one in (name,) + cells], after(caption[-1]), last)
    for head in heads:
        paper.add(label, A, "note", head, "page %d, a column's head" % page)
    for name, cells in rows:
        paper.add(label, A, "note", name, "page %d, the row's head" % page)
        for head, cell in zip(heads, cells):
            paper.add(label, A, "note", cell, "page %d, %s, under %s" % (page, name, head))
    return after(last)


def table_1(first, where):
    rows = (("Morphological distribution",
             ("‘Stem suffix’: attaches to the right edge of word stems without their ‘formal, completive endings’",
              "‘Word’ suffix’: attaches to the right of ‘formal, completive’ endings, including derivational "
              "suffixes.’")),
            ("Phonological restrictions",
             ("Not added to stems ending in m, n, l. In these cases, -nukʷ is added instead.", "None")),
            ("Phonological effects on the base",
             ("‘Weakening’ suffix: results in lenition of certain preceding consonants.", "None")))
    return table(first, "Table 1", [first, after(first)], ("-ad", "-nukʷ"), rows,
                 paper.find(r"^preceding consonants\.$", first))


def table_2(first, where):
    rows = (("Morphological distribution",
             ("Not constrained: attaches directly to stems or to stems with ‘completive’ endings.",)),
            ("Phonological restrictions", ("Tends to not attach to stems ending in –(n)ukʷ (haplology constraint)",)),
            ("Phonological effects on the base", ("None",)))
    return table(first, "Table 2", [first], ("-nukʷ",), rows, paper.find(r"^Phonological effects on the base", first))


TITLE = re.compile(r"^(?:Words containing|Synonyms containing|Both suffixes)")


def appendix(first, where):
    """The Appendix's word lists: each list's title, a note, and each word a transcription beside
    its meaning, a translation, with what stands after the meaning a note. A line with no gap
    after a word runs on the one above it. Forms set together, ɬiwad, ɬiwiʔnukʷ, are a row each."""
    number, entries = first, []
    ends = paper.find(r"^References$", first)
    while number < ends:
        text = paper.text(number)
        if TITLE.match(text):
            lines = [number]
            while not re.search(r"[):]$", paper.text(lines[-1])):
                lines.append(after(lines[-1]))
            entries.append(["title", paper.joined(lines), None, number])
            number = after(lines[-1])
            continue
        cells = columns(number)
        paired = re.match(r"^(\S+ or \S+) (‘.*)$", text)
        if paired:
            cells = list(paired.groups())
        if len(cells) == 2:
            entries.append(["word", cells[0], cells[1], number])
        else:
            assert entries[-1][0] == "word", number
            entries[-1][2] += " " + text
        number = after(number)
    for kind, word, said, at in entries:
        page = paper.page(at)
        if kind == "title":
            source = re.match(r"^(.*”)\s+(\(Boas \d{4}: \d+\))$", word)
            paper.add(where, A, "note", source.group(1) if source else word, "page %d, a word list's title" % page)
            if source:
                paper.add(where, A, "citation", source.group(2), "page %d, the source of the quotation in the title" % page)
            continue
        forms = re.split(r"\s*(?:,|~|\bor\b)\s*", word)
        for form in forms:
            paper.add(where, L, "transcription", form, "page %d%s" % (
                page, ", one of the forms set together, %s" % word if len(forms) > 1 else ""))
        split = re.match(r"^(‘.*?’\.?)\s+([\[(].*)$", said)
        paper.add(where, A, "translation", split.group(1) if split else said, "page %d" % page)
        if split:
            paper.add(where, A, "note", split.group(2), "page %d, after the meaning" % page)
    return ends


BLOCKS = {number: example for number in EXAMPLES}
BLOCKS.update({EXAMPLES[9]: word_list, EXAMPLES[10]: word_list, EXAMPLES[19]: twenty,
               EXAMPLES[26]: twenty_seven, EXAMPLES[66]: side_by_side, EXAMPLES[71]: side_by_side,
               paper.find(r"^The Wakashan Language family$"): wakashan,
               paper.find(r"^Table 1: "): table_1, paper.find(r"^Table 2: "): table_2,
               paper.find(r"^Words containing –nukʷ \(First Voices\)$"): appendix})
found = paper.standard(AUTHORS, NAMES, LANGUAGES, headings=HEADINGS, blocks=BLOCKS, appendix=r"^Katie Sardinha$")
# An example ends on the first line that is none of its own. Every such line here opens prose.
assert all(re.match(r"^[A-Z][a-z]*,? ", text) for _, _, text in UNREAD), UNREAD

# The references: Orlando, FL opens the last line of Anderson's entry and Vancouver, BC that of
# Sardinha's, and First Voices, an entry with no year, runs on Fortescue's. The URL is kept as the
# text layer joins it, a space after www. where the line breaks.
for row in [row for row in paper.rows if row[2] == "reference" and row[3].startswith(("Orlando, FL", "Vancouver, BC"))]:
    before = paper.rows[paper.rows.index(row) - 1]
    assert before[2] == "reference", before
    before[3] += " " + row[3]
    paper.rows.remove(row)
fortescue = next(row for row in paper.rows if row[2] == "reference" and row[3].startswith("Fortescue"))
fortescue[3], voices = fortescue[3].split(" First Voices, ")
paper.rows.insert(paper.rows.index(fortescue) + 1, [fortescue[0], A, "reference",
                  "First Voices, " + voices, fortescue[4]])
tail = paper.find(r"^Katie Sardinha$", paper.find(r"^References$"))
paper.add("end", A, "note", paper.joined([tail, after(tail)]),
          "page %d, the author's name and e-mail address" % paper.page(tail))
paper.write()
