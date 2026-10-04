"""The ops of McKay_2019_ICSNL: Isabel McKay on the internal structure of Montana Salish instrumental
nominals: -m(í)n, whose possessor owns the tool, is the existential binder -m, a verbalizer (vbecome,
null, or vbe, -í) and the instrumental nominalizer -n; -t(í)n, whose possessor is the tool's patient,
is the stative adjectivizer -t over a small clause whose object DP rises to the possessor's place;
-mín-tn is -tín built on a -mín noun.

The trees (5), (6), (21), (23) and (30) are a note on the title and a note to each printed line as the
text layer reads it; (6), (21) and (23) set the example's tiers between the title and the tree. The
semantic derivations (7), (8), (17) to (20), (22) and (25) to (28) are a formula row to each formula,
a line ending on = joined to the line under it, with their translations. (10) sets each part in two
columns, the -mín noun at the left and the -m verb at the right, each column its own run of rows. The
paper numbers two examples (29), on page 12 and on page 13; both keep the number the page prints.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Montana Salish"
AUTHORS = ["Isabel McKay"]
paper = gen.Paper("McKay_2019_ICSNL", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Mengiarini", "Gregory Mengarini et al., the Kalispel dictionary (1877–1879), spelled Mengiarini in "
                        "the citations"),
         ("Mengarini", "Gregory Mengarini, the Kalispel dictionary (1877–1879)"),
         ("Thomason", "Sarah Thomason, the Montana Salish dictionary and lessons (2014), The Flathead Word (1992); "
                      "Lucy Thomason (1994)"),
         ("Baker", "Mark Baker, agent nominalizations (2009)"), ("Vinokurova", "Nadya Vinokurova (2009)"),
         ("Harley", "Heidi Harley, compounding and morpheme order (2011, 2013)"),
         ("Folli", "Raffaella Folli, flavors of v (2005)"), ("Noyer", "Rolf Noyer, Distributed Morphology (1999)"),
         ("Carlson", "Barry F. Carlson, the grammar of Spokan (1972)"),
         ("Spek", "Brenda J. Speck, Father Post's Kalispel grammar (1980), spelled Spek in the citations"),
         ("Post", "John A. Post (1980)"), ("Perlmutter", "David M. Perlmutter, unaccusativity (1978)"),
         ("Alexiadou", "Artemis Alexiadou, instrumental -er nominals (2008)"), ("Schäfer", "Florian Schäfer (2008)"),
         ("Doak", "Ivy Doak, Okanagan -lx (1997)"), ("Mattina", "Anthony Mattina (1997)"),
         ("Willet", "Marie Louise Willett, the grammar of Nxa’amxcin (2003), spelled Willet in the text"),
         ("Matthewson", "Lisa Matthewson, DP in St’át’imcets (1995)"), ("Davis", "Henry Davis (1995)"),
         ("McKay", "Isabel McKay, the author, transitive subjects as adjuncts (2019)")]
LANGUAGES = [(LANGUAGE, "Southern Interior Salish, the Montana dialect of Kalispel-Spokane"),
             ("Flathead", "Montana Salish"), ("Kalispel", "a dialect of the language"),
             ("Spokane", "a dialect of the language"),
             ("English", "-er instrumental and agentive nominals"), ("Salish", "the family"),
             ("Southern Interior Salish", "the branch"), ("Okanagan", "Southern Interior Salish"),
             ("Coeur d’Alene", "Southern Interior Salish"), ("Nxa’amxcin", "Moses-Columbia Salish"),
             ("Moses-Columbia", "Nxa’amxcin"), ("St’át’imcets", "Lillooet Salish"),
             ("Lillooet Salish", "St’át’imcets")]

FOOTNOTES = paper.page_footnotes()
SKIP = {one for parts, _ in FOOTNOTES.values() for one in parts} | set(paper.volume_header())
RUNNING = paper.running_numbers_set()
REFERENCES = paper.find(r"^References:?$")
PART = re.compile(r"^([a-z])\.\s+(.*)$")
# A line of a derivation: a formula opening on a bracket or a lambda, a translation, or a line
# lettered for its part.
FORMULA = re.compile(r"^(?:⟦|𝜆|𝜄|‘|[a-z]\.\s)")
MATH = re.compile(r"[\U0001D400-\U0001D7FF]")


def lines_from(number):
    """The body's lines from number on, passing over footnotes, running numbers and page marks."""
    while number < REFERENCES:
        if number not in SKIP and number not in RUNNING and not paper.lines[number][2] and paper.text(number).strip():
            yield number
        number += 1


def plain(text):
    return " ".join(text.split())


def translated(label, count, text, number):
    """A translation and the pieces at its right, from count on. Returns the last count, or None
    where text ends mid-translation."""
    split = gen.split_translation(text)
    if split is None:
        return None
    said, pieces = split
    count += 1
    paper.add("%s line %d" % (label, count), A, "translation", said, "page %d" % paper.page(number))
    for piece in pieces:
        count += 1
        source = gen.is_source(piece)
        paper.add("%s line %d" % (label, count), A, "citation" if source else "note", piece,
                  "page %d, %s" % (paper.page(number), "the tag or source at the right of the translation" if source
                                   else "at the right of the translation"))
    return count


def tree(stop, tiers=()):
    """A tree: a note on its title, the example's tiers under it where the page sets them, then a
    note to each printed line of the tree to the line stop matches."""
    def block(start, where):
        label, title = gen.EXAMPLE.match(paper.text(start).strip()).groups()
        name = "(%s)" % label
        end = paper.find(stop, start + 1)
        numbers = [number for number in lines_from(start + 1) if number < end]
        paper.add(name, A, "note", plain(title), "page %d, the title of a tree" % paper.page(start))
        count = 0
        for kind, number in zip(tiers, numbers):
            text = plain(paper.text(number))
            if kind == "translation":
                count = translated(name, count, text, number)
            else:
                count += 1
                paper.add("%s line %d" % (name, count), L, kind, text, "page %d" % paper.page(number))
        for line, number in enumerate(numbers[len(tiers):], 1):
            paper.add("%s tree line %d" % (name, line), A, "note", plain(paper.text(number)),
                      "page %d, a line of the tree as the text layer reads it" % paper.page(number))
        return end
    return block


def formulas(start, where):
    """A derivation: each formula a row, a line ending on = joined to the line under it, each
    translation over the lines it runs to, a part's letter opening its own count."""
    label, rest = gen.EXAMPLE.match(paper.text(start).strip()).groups()
    items = [(rest, start)]
    for number in lines_from(start + 1):
        if not FORMULA.match(paper.text(number).strip()) and not (items[-1][0].endswith("=") or items[-1][0].startswith("‘")
                                                                   and gen.split_translation(items[-1][0]) is None):
            break
        text = plain(paper.text(number))
        if items[-1][0].endswith("=") or items[-1][0].startswith("‘") and gen.split_translation(items[-1][0]) is None:
            items[-1] = (items[-1][0] + " " + text, items[-1][1])
        else:
            items.append((text, number))
    else:
        number = REFERENCES
    name, count = "(%s)" % label, 0
    for text, at in items:
        part = PART.match(text)
        if part:
            name, count, text = "(%s%s)" % (label, part.group(1)), 0, part.group(2)
        if text.startswith("‘"):
            count = translated(name, count, text, at)
            continue
        count += 1
        paper.add("%s line %d" % (name, count), L if not MATH.search(text) and "⟦" not in text else A, "formula", text,
                  "page %d, a formula%s" % (paper.page(at), ", its subscripts set inline" if MATH.search(text) else ""))
    return number


def columns(start, where):
    """(10): a note on its title, then each part's two columns, the -mín noun at the left and the -m
    verb at the right, the source under the last part."""
    paper.add("(10)", A, "note", plain(gen.EXAMPLE.match(paper.text(start).strip()).group(2)),
              "page %d, over the example" % paper.page(start))
    end = paper.find(r"^It is also worth noting", start)
    parts, source = [], None
    for number in lines_from(start + 1):
        if number >= end:
            break
        words = paper.word_positions(number)
        if words[0][0] > 330:
            source = (plain(paper.text(number)), number)
            continue
        if PART.match(words[0][1] + " "):
            parts.append([])
            words = words[1:]
        parts[-1].append(([word for place, word in words if place < 330], [word for place, word in words if place >= 330],
                          number))
    for letter, lines in zip("abc", parts):
        name, count = "(10%s)" % letter, 0
        for column, side in ((0, "the -mín noun, the left column"), (1, "the -m verb, the right column")):
            for kind, (left, right, number) in zip(("transcription", "segmentation", "gloss", "translation"), lines):
                text = " ".join((left, right)[column])
                count += 1
                paper.add("%s line %d" % (name, count), A if kind == "translation" else L, kind, text,
                          "page %d, %s" % (paper.page(number), side))
    paper.add("(10c) line %d" % (count + 1), A, "citation", source[0], "page %d, under the translation" % paper.page(source[1]))
    return end


def english(start, where):
    """(1), an English phrase and its two readings, each a note."""
    paper.add("(1)", A, "note", plain(gen.EXAMPLE.match(paper.text(start).strip()).group(2)),
              "page %d, an English example" % paper.page(start))
    for number in (start + 1, start + 2):
        letter, reading = PART.match(paper.text(number).strip()).groups()
        paper.add("(1%s)" % letter, A, "note", plain(reading), "page %d, a reading of the English example"
                  % paper.page(number))
    return start + 3


BLOCKS = {paper.find(r"^\(1\) My poker"): english}
for number in lines_from(1):
    opened = gen.EXAMPLE.match(paper.text(number).strip())
    if not opened:
        continue
    label = opened.group(1)
    if label in ("7", "8", "17", "18", "19", "20", "22", "25", "26", "27", "28"):
        BLOCKS[number] = formulas
    elif label == "10":
        BLOCKS[number] = columns
BLOCKS[paper.find(r"^\(5\) English")] = tree(r"^\(6\) Proposed")
BLOCKS[paper.find(r"^\(6\) Proposed")] = tree(r"^In the following sections, I will", ("segmentation", "gloss", "translation"))
BLOCKS[paper.find(r"^\(21\) Montana")] = tree(r"^The possessive determiner head",
                                              ("transcription", "segmentation", "gloss", "translation"))
BLOCKS[paper.find(r"^\(23\) Proposed")] = tree(r"^3\.1 The Stative", ("transcription", "segmentation", "gloss", "translation"))
BLOCKS[paper.find(r"^\(30\) Proposed")] = tree(r"^5 Conclusion$")
# The second (29), on page 13, repeats a number flow has passed; it is read as the page numbers it.
BLOCKS[paper.find(r"^\(29\) a\. inagaminten")] = lambda start, where: paper.example(start, REFERENCES - 1, SKIP)
# The page sets footnote 6's mark a space after its comma, the preposition t (11), 6 and.
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS, references=r"^References:?$", spaced={"6": "t (11),"})
# The page sets -m-∅-n in italics with the ∅ in an upright face; the italic reader ends the run at it.
for row in paper.rows:
    if row[2] == "cited form" and row[3] == "-m-∅":
        row[3], row[4] = "-m-∅-n", row[4] + ", the ∅ in an upright face"
# (3b)'s k̓̓ʷɫ prints two glottal marks over its k, one over the other; (4b)'s k̓ʷɫ prints one.
for row in paper.rows:
    if row[0] == "(3b) line 2":
        row[4] += ", its k with two glottal marks as printed"
# (2) opens on a title, Three Types of Instrument Nouns in Montana Salish, over its parts.
for row in paper.rows:
    if row[0] == "(2) line 1":
        row[:3], row[4] = ["(2)", A, "note"], row[4] + ", the title over the example"
paper.write()
