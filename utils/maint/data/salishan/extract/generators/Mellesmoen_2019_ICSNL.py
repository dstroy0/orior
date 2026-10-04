"""The ops of Mellesmoen_2019_ICSNL: Gloria Mellesmoen on a fricative-first path from Proto-Salish *c
to /s/ and /θ/ in Central Salish: *c first lost its stop and became an [s]-like fricative (Stage II),
which then fronted to /θ/ (Stage IIIa, Mainland Comox, Pentlatch, Halkomelem) or merged with /s/
(Stage IIIb, Island Comox, Lummi, Sooke, Songish); Lummi's coda [ʔs] points to a decomposition and a
debuccalization on the way, and the fronting-first path Thompson, Thompson and Efrat (1974) propose
for Saanich predicts a /tᶿ/ no language records.

Tables 1 and 3 are a note to the caption and a note to each printed line. Table 2 and examples (1),
(3) and (6) give each form a row of its language, with the translation the row prints. (2) is a row
to each part's description, its bracketed forms and its translation. The stage diagrams (4) and (5)
are a note to each printed line.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Central Salish"
AUTHORS = ["Gloria Mellesmoen"]
paper = gen.Paper("Mellesmoen_2019_ICSNL", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Pulleyblank", "Douglas Pulleyblank, on the author's qualifying paper committee"),
         ("Babel", "Molly Babel, on the committee; McGuire & Babel (2012), /θ/ cross-linguistically unstable"),
         ("Henry Davis", "on the committee"),
         ("Joanne Francis", "ʔayʔaǰuθəm language consultant"), ("Elsie Paul", "ʔayʔaǰuθəm language consultant"),
         ("Phyllis Dominic", "ʔayʔaǰuθəm language consultant"), ("Freddie Louie", "ʔayʔaǰuθəm language consultant"),
         ("Maggie Wilson", "ʔayʔaǰuθəm language consultant"), ("Hoss Timothy", "ʔayʔaǰuθəm language consultant"),
         ("Kuipers", "A. H. Kuipers, Salish etymological dictionary (2002)"),
         ("Galloway", "B. D. Galloway, Chilliwack Halkomelem (1977), Nooksack (1982), Samish (1986, 1990), "
                      "Proto-Central Salish sound correspondences (1988)"),
         ("Maddieson", "I. Maddieson, patterns of sounds (1984)"),
         ("Suttles", "W. P. Suttles, Musqueam (2004); Elmendorf & Suttles (1960)"),
         ("Thompson", "L. C. Thompson and M. T. Thompson, with B. S. Efrat, Straits Salish (1974)"),
         ("Efrat", "B. S. Efrat (1974)"), ("Blake", "S. J. Blake, schwa in Sliammon (2000)"),
         ("Boas", "F. Boas, the Pentlatch materials (1890)"), ("Beaumont", "R. C. Beaumont, Sechelt dictionary (2011)"),
         ("Harris", "H. R. Harris, a grammatical sketch of Comox (1981); J. Harris, segmental complexity (1990)"),
         ("Sapir", "E. Sapir (1914)"), ("Gibbs", "G. Gibbs, vocabulary of the Ko-mookhs (1877)"),
         ("Barnett", "H. Barnett, the Coast Salish of British Columbia (1955)"),
         ("Elmendorf", "W. W. Elmendorf (1960)"), ("Kava", "Tiiu Kava, a phonology of Cowichan (1969)"),
         ("Montler", "T. Montler, Saanich (1986), Straits Salishan dialects (1999)"),
         ("Raffo", "Y. A. Raffo, Songish (cited 1972; the references list 2003)"),
         ("Gick", "B. Gick et al., quantal relations (2011)"),
         ("Stavness", "I. Stavness (2011)"), ("Chiu", "C. Chiu (2011)"), ("Fels", "S. Fels (2011)"),
         ("McGuire", "G. McGuire (2012)"), ("Hall", "T. A. Hall (2010)"), ("Żygis", "M. Żygis (2010)"),
         ("Kirchner", "R. M. Kirchner, consonant lenition (1998)"), ("Hayes", "B. Hayes, Toba Batak (1986)"),
         ("Garrett", "A. Garrett, phonetic bias in sound change (2013)"), ("Johnson", "K. Johnson (2013)"),
         ("Ohala", "J. J. Ohala, hypocorrection (1989)"), ("Baker", "A. Baker et al., s-retraction (2011)"),
         ("Archangeli", "D. Archangeli (2011)"), ("Mielke", "J. Mielke (2011)"),
         ("Árnason", "K. Árnason, Icelandic and Faroese (2011)"),
         ("Huijsmans", "M. Huijsmans; Davis & Huijsmans (2017)"),
         ("Davis", "H. Davis (2017, with Huijsmans); J. Davis, Sliammon pronominal paradigms (1978)")]
LANGUAGES = [(LANGUAGE, "the branch of Salish with /s/ or /θ/ from Proto-Salish *c"),
             ("Proto-Salish", "the reconstruction, PS"), ("Pentlatch", "Central Salish, *c > /θ/"),
             ("Comox-Sliammon", "Central Salish, ʔayʔaǰuθəm"), ("Mainland Comox", "Comox-Sliammon, *c > /θ/"),
             ("Island Comox", "Comox-Sliammon, *c > /s/"), ("Halkomelem", "Central Salish, *c > /θ/"),
             ("Musqueam", "a dialect of Halkomelem"), ("Island Halkomelem", "a dialect of Halkomelem"),
             ("Upriver Halkomelem", "a dialect of Halkomelem"), ("Northern Straits", "Central Salish"),
             ("Saanich", "Northern Straits, *c split into /θ/ and /s/"), ("Samish", "Northern Straits"),
             ("Songish", "Northern Straits, *c > /s/"), ("Sooke", "Northern Straits, *c > /s/"),
             ("Lummi", "Northern Straits, *c > /s/ with coda [ʔs]"), ("Sechelt", "Central Salish, *c kept as /c/"),
             ("Squamish", "Central Salish, *c kept as /c/"), ("Klallam", "Central Salish, *c kept as /c/"),
             ("Twana", "Central Salish, Stage I"), ("Lushootseed", "Central Salish, Stage I"),
             ("Nooksack", "Central Salish, tentatively Stage II"), ("Bella Coola", "Salish, occasional *c as /s/"),
             ("Coast Salish", "the languages sharing /θ/"), ("English", "[θ] and the Liverpool dialect"),
             ("Spanish", "coda debuccalization"), ("Icelandic", "half-debuccalization"),
             ("Toba Batak", "debuccalization before a consonant")]

PART = re.compile(r"^([a-i])\.\s+(.*)$")
# A row of (1) or (6): Boas's form, the phonemic form, the translation, and the forms at its right.
TABLE_ROW = re.compile(r"^(\S+)\s+(/?[^‘]*?/?)\s+(‘[^’]*’)\s+(.*)$")


def printed(start, stop=None):
    """The lines from start to a blank one, or to the line stop matches."""
    number, lines = start, []
    running = paper.running_numbers_set()
    while number <= paper.last:
        text = paper.text(number)
        if stop and re.search(stop, text):
            break
        if not stop and not text.strip():
            break
        if text.strip() and not paper.lines[number][2] and number not in running:
            lines.append(number)
        number += 1
    return lines, number


def table(stop=None):
    """A table: its caption a note, and a note to each printed line."""
    def block(start, where):
        label = paper.text(start).split(":")[0]
        paper.add(label, A, "note", paper.text(start), "page %d, the caption" % paper.page(start))
        lines, after = printed(start + 1, stop)
        for count, number in enumerate(lines, 1):
            paper.add("%s line %d" % (label, count), A, "note", paper.text(number),
                      "page %d, a line of the table as the text layer reads it" % paper.page(number))
        # Table 3 alone names Twana and Lushootseed.
        paper.mentions(label, paper.joined(lines), LANGUAGES, "language")
        return after
    return block


def comox_table(start, where):
    """Table 2: its caption and header notes, then each word's Boas, Harris and modern Mainland Comox
    forms, a row each with the English it gives."""
    label = "Table 2"
    paper.add(label, A, "note", paper.text(start), "page %d, the caption" % paper.page(start))
    lines, after = printed(start + 1)
    paper.add("%s line 1" % label, A, "note", paper.text(lines[0]), "page %d, the header" % paper.page(lines[0]))
    for count, number in enumerate(lines[1:], 1):
        english, rest = re.match(r"^(‘[^’]*’)\s+(.*)$", paper.text(number)).groups()
        page = paper.page(number)
        for column, form in zip(("Boas (1890)", "Harris (1981)", "modern Mainland Comox"), rest.split()):
            paper.add("%s row %d" % (label, count), "Comox-Sliammon", "cited form", form,
                      "page %d, %s, %s" % (page, english, column))
    return after


def boas(columns, italic_note, marked=None):
    """(1) or (6): a note on the header, then each lettered row's forms: Boas's Pentlatch definition,
    the phonemic Pentlatch, the translation, and the forms the header names at the right."""
    def block(start, where):
        label, header = gen.EXAMPLE.match(paper.text(start)).groups()
        paper.add("(%s)" % label, A, "note", header, "page %d, the column heads" % paper.page(start))
        number = start + 1
        while PART.match(paper.text(number)):
            letter, rest = PART.match(paper.text(number)).groups()
            name, page = "(%s%s) line" % (label, letter), paper.page(number)
            spelled, phonemic, said, right = TABLE_ROW.match(rest).groups()
            paper.add("%s 1" % name, "Pentlatch", "transcription", spelled,
                      "page %d, Boas (1890), %s%s" % (page, italic_note, (marked or {}).get(letter, "")))
            paper.add("%s 2" % name, "Pentlatch", "phonemic", phonemic, "page %d, the author's phonemic form" % page)
            paper.add("%s 3" % name, A, "translation", said, "page %d" % page)
            for count, (language, form) in enumerate(zip(columns, right.split()), 4):
                if form == "–":
                    paper.add("%s %d" % (name, count), A, "note", form, "page %d, no %s form" % (page, language))
                else:
                    paper.add("%s %d" % (name, count), language, "cited form", form, "page %d, %s" % (page, language))
            number += 1
        return number
    return block


def reflexes(start, where):
    """(2): each part's description, its bracketed forms, starred where the page stars them, and its
    translation."""
    number = start
    while True:
        text = paper.text(number)
        opened = gen.EXAMPLE.match(text)
        found = PART.match(opened.group(2) if opened else text)
        if not found:
            return number
        letter, rest = found.groups()
        name, page = "(2%s) line" % letter, paper.page(number)
        described, forms, said = re.match(r"^(.*?)\s+(\[.*\](?:\s+\(\*\[[^\]]*\]\))?)\s+(‘[^’]*’)$", rest).groups()
        paper.add("%s 1" % name, A, "note", described, "page %d, the part's description" % page)
        plain, _, starred = forms.partition(" (")
        paper.add("%s 2" % name, "Mainland Comox", "phonetic", plain, "page %d, the forms a speaker gives" % page)
        count = 2
        if starred:
            count += 1
            paper.add("%s %d" % (name, count), "Mainland Comox", "phonetic", starred.rstrip(")"),
                      "page %d, starred in parentheses, the form not given" % page)
        paper.add("%s %d" % (name, count + 1), A, "translation", said, "page %d" % page)
        number += 1


def straits(start, where):
    """(3): the column heads, then each row's Klallam and Lummi forms, the translation and the source."""
    paper.add("(3)", A, "note", gen.EXAMPLE.match(paper.text(start)).group(2), "page %d, the column heads" %
              paper.page(start))
    number, row = start + 1, 0
    while paper.text(number).strip():
        row += 1
        page = paper.page(number)
        klallam, lummi, said, source = re.match(r"^(\S+)\s+(\S+)\s+(‘[^’]*’)\s*(\(.*\))?$",
                                               paper.text(number)).groups()
        paper.add("(3) row %d" % row, "Klallam", "cited form", klallam, "page %d, Klallam" % page)
        paper.add("(3) row %d" % row, "Lummi", "cited form", lummi, "page %d, Lummi" % page)
        paper.add("(3) row %d" % row, A, "translation", said, "page %d" % page)
        if source:
            paper.add("(3) row %d" % row, A, "citation", source, "page %d, the source of the example" % page)
        number += 1
    return number


blocks = {paper.find(r"^Table 1: "): table(), paper.find(r"^Table 2: "): comox_table,
          paper.find(r"^Table 3: "): table(r"^While gaps in documentation"),
          paper.find(r"^\(1\) Boas"): boas(("Mainland Comox", "Sechelt"),
                                          "italic in the paper (footnote 2)",
                                          {"b": ", the k underlined", "f": ", the k underlined"}),
          paper.find(r"^\(2\) a\."): reflexes, paper.find(r"^\(3\) Klallam"): straits,
          paper.find(r"^\(6\) Boas"): boas(("Mainland Comox",),
                                          "italic in the paper", {"b": ", the k underlined"})}
# Page 10 sets marks 6 and 7 a space after Kirchner's page, 1998:143). 6,7 Further.
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, displays={"4": (2, True), "5": (3, True)},
               spaced={"6": "(Kirchner 1998:143)."})
paper.write()
