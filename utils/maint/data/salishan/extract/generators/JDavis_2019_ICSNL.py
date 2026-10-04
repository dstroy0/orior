"""The ops of JDavis_2019_ICSNL: John Hamilton Davis on the reflexes of Proto-Salish *ʔas- stative
and *ʔis- durative in Mainland Comox, told apart from the /s/ nominalizing proclitic and the CV-
imperfective, with the -î- durative and stative marked by a higher tone, the /s-/ of time
expressions, the /tᶿ/ of first person singular plus /s/, and the history of the language's names.

The examples are set in columns, two numbered examples to a line, with an orthographic form, a
phonetic [..] form and a translation under each, and a line of the morphemes under some:
gen.Paper.columns reads each word into the column it stands under on the page.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Mainland Comox"
AUTHORS = ["John Hamilton Davis"]
paper = gen.Paper("JDavis_2019_ICSNL", authors=AUTHORS[0], language=LANGUAGE)

NAMES = [("Stanley Newman", "reconstructs the Proto-Salish prefixes, Salish and Bella Coola prefixes (1976)"),
         ("Newman", "Stanley Newman, Salish and Bella Coola prefixes (1976)"),
         ("Bill Galligos", "Mainland Comox speaker, born 1908"),
         ("Mary Paul", "gave the greeting of (6) to visitors in the 1960s and 1970s"),
         ("Tommy Paul", "storyteller, the explanation about twins"),
         ("Noel George Harry", "storyteller, born circa 1890, father-in-law of Bill Galligos"),
         ("Mary George", "Sliammon speaker, born 1924"), ("Marion Harry", "born 1937"),
         ("Wayne Suttles", "personal communication, 1978"), ("George Gibbs", "the earliest word list, 1857, published 1877"),
         ("Gibbs", "George Gibbs, vocabulary of the Ko-mookhs (1877)"),
         ("Franz Boas", "recorded Çal‘oltq in 1886"), ("Boas", "Franz Boas"),
         ("Sapir", "Edward Sapir, noun reduplication in Comox (1915)"),
         ("Hagège", "Claude Hagège, le comox lhaamen (1981)"),
         ("Harris", "Herbert R. Harris, a grammatical sketch of Comox (1977)"),
         ("Hoard", "James E. Hoard, syllabication in Northwest Indian languages (1978)"),
         ("Montler", "Timothy Montler, the SENĆOTEN dictionary (2018)"),
         ("Beaumont", "Ronald C. Beaumont, Sechelt statives and the Sechelt dictionary"),
         ("Davis", "John Hamilton Davis, the author's earlier work")]
LANGUAGES = [(LANGUAGE, "ʔayʔaǰuθəm, the language of the Homalco, Klahoose and Sliammon"),
             ("Proto-Salish", "the prefixes *ʔas- stative and *ʔis- durative"),
             ("Homalco", "a Mainland Comox community"), ("Klahoose", "a Mainland Comox community"),
             ("Sliammon", "a Mainland Comox community"), ("Sechelt", "Central Salish, compared"),
             ("Musqueam", "Central Salish, /tᶿ/ synchronically"), ("Island Comox", "Thalholhtwh, the island dialect"),
             ("Comox", "Mainland and Island Comox"), ("Spanish", "no tener ganas and mañana"),
             ("English", "the difficult language")]

notes = paper.page_footnotes()
skip = {one for parts, _ in notes.values() for one in parts}
end = paper.find(r"^References$")
blocks = {}
for number in range(1, end):
    if number not in skip and gen.EXAMPLE.match(paper.text(number)):
        blocks[number] = lambda first, where: paper.columns(first, skip=skip)
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)
# A footnote mark a column keeps on its row, (came (welcome).’1 or [p̉aᵃq̉εm]4, comes off the form,
# and the row's gloss records it.
for row in paper.rows:
    mark = re.search(r"(?:(?<=[^\W\d_])|(?<=[’\]”]))(\d{1,2})$", row[3]) if "in columns" in row[4] else None
    if mark and mark.group(1) in notes:
        row[3] = row[3][:mark.start()]
        row[4] += ", carries footnote " + mark.group(1)
rows = paper.rows


def line(label, number):
    return next(row for row in rows if row[0] == "(%s) line %d" % (label, number))


# (3) stands alone on its lines, and its translation's source at the right and the morphemes under it
# ran on into the translation.
three = line("3", 3)
said, source, parts = re.match(r"^(‘.*’)\s+(\(from the story .*?\))\s+(.*)$", three[3]).groups()
three[3] = said
at = rows.index(three) + 1
rows[at:at] = [["(3) line 4", A, "note", source, "page 1, in columns, at the right of the translation"],
               ["(3) line 5", A, "note", parts, three[4]]]
# The morphemes of (5) run under both columns, and their last one, + -ap ‘you (plural)’, fell to (6)
# after its footnote mark.
six = line("6", 3)
said, rest = re.match(r"^(‘.*’)1\s+(\+ .*)$", six[3]).groups()
six[3], six[4] = said, six[4] + ", carries footnote 1"
line("5", 4)[3] += " " + rest
# (48)'s translation wraps over three lines, unquoted on the page, a note.
first = line("48", 3)
more = [line("48", 4), line("48", 5)]
first[1:4] = [A, "note", " ".join([first[3]] + [row[3] for row in more])]
first[4] += ", the translation, unquoted on the page"
rows = [row for row in rows if not any(row is one for one in more)]
# (63)'s translation carries footnote 7 after its quoted words.
sixty = line("63", 3)
sixty[3], sixty[4] = sixty[3].replace("”7 ", "” ", 1), sixty[4] + ", carries footnote 7"
# A line of morphemes, texw ‘know’ + (n)ewh ‘result transitive’, is a gloss of the example over it.
for row in rows:
    if "in columns" in row[4] and re.search(r"’ \+ ", row[3]):
        row[1:3] = [L, "gloss"]
paper.rows = rows
paper.write()
