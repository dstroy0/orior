"""The ops of 05-DavisJ-ICSNL50_final-10: John Hamilton Davis on three phonetic details of Homalco,
Mainland Comox: the offglide between a palatal or velar obstruent and a following vowel, Sapir's
rearticulated vowels; stød, the glottal stop that alternates with creaky voice before a
laryngealized resonant; and the separate enunciation of consonants in sequence, which makes the
language mora timed.

An example sets its tiers side by side on one printed line, (3) /phonemic/ [phonetic] ~ [phonetic]
orthography, the gloss on the line under it: each /../ a phonemic row, each [..] a phonetic row, the
orthography written for learners a transcription row (each variant after a ~ its own), the gloss a
translation. A word between the tiers, or in (12) or not recorded as in (16), goes in the gloss of
the form after it. The table of §3 sets the same tiers with the gloss on the line, under a header
row that is a note.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Homalco"
AUTHORS = ["John Hamilton Davis"]
paper = gen.Paper("05-DavisJ-ICSNL50_final-10", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Noel George Harry", "Homalco speaker, born 1892"), ("Bill Galligos", "Homalco speaker, born 1903"),
         ("Jimi Wilson", "Homalco speaker, born 1945, raised in Church House"),
         ("Mary George", "Sliammon speaker, born 1924"), ("Tommy Paul", "asked about sasquatch"),
         ("Thom Hess", "had worked with Bill Galligos"), ("Jim Hoard", "James Hoard, Syllabification (1978)"),
         ("Franz Boas", "the Island Comox file slips, around 1887"),
         ("Edward Sapir", "Noun Reduplication in Comox (1915)"),
         ("Sonya Bird", "laryngealized resonants in St’at’imcets (2011)"),
         ("Susan Blake", "Sliammon phonology (1992)"), ("Phil Hoole", "University of Munich, Danish stød"),
         ("Ladefoged", "Peter N. Ladefoged, phonation types (1965)"),
         ("McDonough", "Joyce McDonough, with Whalen (2008)"), ("Whalen", "D. H. Whalen, with McDonough (2008)")]
LANGUAGES = [(LANGUAGE, "the northernmost dialect of Mainland Comox, Central Salish"),
             ("Mainland Comox", "Central Salish, three dialects"), ("Island Comox", "Boas's dialect"),
             ("Comox", "Mainland and Island Comox"), ("Sliammon", "a dialect of Mainland Comox"),
             ("Danish", "stød"), ("St’at’imcets", "Northern Interior Salish, Bird 2011"),
             ("Otomanguean", "Mazatec and Mixtec"), ("Mazatec", "Otomanguean"), ("Mixtec", "Otomanguean"),
             ("Hokan", "Oaxaca Chontal"), ("Oaxaca Chontal", "Hokan"), ("Panoan", "Capanhua"),
             ("Capanhua", "Panoan, spelled Capanahua in the references"), ("Achumawi", "laryngealized stops"),
             ("Hausa", "laryngealized stops"), ("Quileute", "cited by Hoard"), ("Nisqually", "cited by Hoard"),
             ("Columbian", "cited by Hoard"), ("Nez Perce", "cited by Hoard"), ("Bella Coola", "cited by Hoard"),
             ("Chinese", "the Pinyin alphabet"), ("English", "the contact language")]

# The volume's header opens on In Papers for the International Conference, which
# gen.Paper.volume_header does not take for its opening.
HEADER_FIRST = paper.find(r"^In Papers for the International Conference", 1, 20)
HEADER = list(range(HEADER_FIRST, paper.find(r"\b(?:19|20)\d\d\.$", HEADER_FIRST, HEADER_FIRST + 2) + 1))
paper.volume_header = lambda: HEADER

FOOTNOTES = paper.page_footnotes()
AT_FOOT = {one for parts, _ in FOOTNOTES.values() for one in parts} | set(HEADER)
RUNNING = paper.running_numbers_set()
REFERENCES = paper.find(r"^References:$")
# A tier: [phonetic] or /phonemic/, the phonemic one opening on a letter; any other word stands alone.
TIER = re.compile(r"\[[^\]]+\]|/[^/\s][^/]*/|\S+")


def printed(number):
    return paper.text(number).strip() and not paper.lines[number][2] and number not in AT_FOOT \
        and number not in RUNNING


def tiers(text, where, line):
    """The tiers of a printed line, text without its number or gloss: a row to each form."""
    words = TIER.findall(text)
    forms = [at for at, word in enumerate(words) if re.fullmatch(r"\[.+\]|/.+/", word) and len(word) > 2]
    between = []
    for word in words[:forms[-1] + 1]:
        if re.fullmatch(r"\[.+\]", word) or (re.fullmatch(r"/.+/", word) and len(word) > 2):
            kind = "phonetic" if word.startswith("[") else "phonemic"
            said = " ".join(between).strip("()")
            # Words of the author's between the tiers, not recorded as, are a note of their own.
            if len(between) > 1:
                paper.add(where, A, "note", said, "page %d, between the tiers" % paper.page(line))
            paper.add(where, L, kind, word, "page %d%s" % (paper.page(line), ", " + said if said else ""))
            between = []
        elif word not in ("~", "/", ")"):
            between.append(word)
    written = " ".join(words[forms[-1] + 1:])
    for variant in written.split(" ~ ") if written else []:
        paper.add(where, L, "transcription", variant, "page %d, written for learners" % paper.page(line))


def example(start, where):
    """A numbered example: its tiers on the line, then the gloss on the line under it, if any."""
    opened = gen.EXAMPLE.match(paper.text(start))
    label = "(%s)" % opened.group(1)
    tiers(opened.group(2), label, start)
    line = start + 1
    while line < REFERENCES and not printed(line):
        line += 1
    gloss = paper.text(line)
    if not gloss.startswith("‘"):
        return line
    while not gloss.endswith("’"):
        line += 1
        gloss += " " + paper.text(line)
    paper.add(label, A, "translation", gloss, "page %d" % paper.page(line))
    return line + 1


def table(start, where):
    """The table of §3: the header a note, then a row to each tier of each line, the gloss on the line
    and run on to the next line when it wraps."""
    paper.add("§3 table", A, "note", paper.text(start), "page %d, the table's header" % paper.page(start))
    line, count = start + 1, 0
    while printed(line):
        text = paper.text(line)
        count += 1
        tiers(text[:text.index("‘")], "§3 table line %d" % count, line)
        gloss = text[text.index("‘"):]
        while not gloss.endswith("’"):
            line += 1
            gloss += " " + paper.text(line)
        paper.add("§3 table line %d" % count, A, "translation", gloss, "page %d" % paper.page(line))
        line += 1
    return line


def quotation(start, where):
    """Sapir's words of §2, set apart between blank lines: one note."""
    line, text = start, []
    while printed(line):
        text.append(paper.text(line))
        line += 1
    paper.add(where, A, "note", " ".join(text), "page %d, Sapir 1915 set as a block quotation" % paper.page(start))
    return line


blocks = {paper.find(r"^Phonemic Phonetic Written for learners Gloss$"): table,
          paper.find(r"^As not infrequently happens in American Indian languages"): quotation}
for number in range(1, REFERENCES):
    if printed(number) and gen.EXAMPLE.match(paper.text(number)):
        blocks[number] = example
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, references=r"^References:$")

# An entry that wraps after an editor's initial, & T. / Peterson, eds.), runs on the entry above it:
# every entry opens Surname, X.
paper.merge_references()
paper.write()
