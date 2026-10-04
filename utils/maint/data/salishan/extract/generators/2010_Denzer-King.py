"""The ops of 2010_Denzer-King: Ryan Denzer-King's A preliminary look at restricted counting in
Proto-Salish. The Salishan numerals agree from 1 to 4 and scatter above it, where most are analyzable
and many hold the lexical suffix for 'hand'; a Proto-Salish root for 4 means 'ready/completed', and
Bella Coola has a subtractive 3 built on the root for 4. The paper reads these as a count that once
ended at four, under the decimal system every modern Salishan language has, and argues against a
quaternary system by setting Okanagan and Upper Chehalis beside Barbareño Chumash.

Page text read by glyph rows, the forms' Windows subset fonts through their PAPER_TOUNICODE entry.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Proto-Salish"
AUTHORS = ["Ryan Denzer-King"]
paper = gen.Paper("2010_Denzer-King", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Thompson & Thompson", "Laurence C. Thompson and M. Terry Thompson, The Thompson Language (1992) "
                                 "and Thompson River Salish Dictionary (1996)"),
         ("Anderson", "Gregory D. S. Anderson, Reduplicated Numerals in Salish (1999)"),
         ("Aoki", "Haruo Aoki, Nez Perce Dictionary (1994) and The East Plateau Linguistic Diffusion Area (1975)"),
         ("Dixon & Kroeber", "Roland B. Dixon and A. L. Kroeber, Numeral Systems of the Languages of California (1907)"),
         ("Beeler", "Madison Beeler, Chumash Numerals (1986) and Senary Counting in California Penutian (1961)"),
         ("Rood & Taylor", "David Rood and Allan Taylor, Sketch of Wichita, a Caddoan Language (1996)"),
         ("O’Meara", "John O’Meara, Delaware-English English-Delaware Dictionary (1996)"),
         ("Blevins", "Juliette Blevins, Origins of Northern Costanoan šak:en ‘Six’ (2005)"),
         ("Collins & Collins", "Raymond Collins and Sally Jo Collins, Upper Kuskokwim Athapaskan Dictionary (1966)"),
         ("Hymes", "Virginia Dosch Hymes, Athapaskan Numeral Systems (1955)"),
         ("Rigsby", "Bruce Rigsby, Linguistic Relations in the Southern Plateau (1965)"),
         ("Mithun", "Marianne Mithun, whose classification of the Salishan languages the paper uses (1999)"),
         ("Kuipers", "A. H. Kuipers, Salish Etymological Dictionary (2002), the source of the reconstructions"),
         ("Eels", "W. C. Eells, Number Systems of the North American Indians (1913), spelled Eels in footnote 4"),
         ("Eells", "W. C. Eells, Number Systems of the North American Indians (1913)"),
         ("Nater", "Hank Nater, The Bella Coola Language (1984)"),
         ("Sapir", "Edward Sapir, The Collected Works of Edward Sapir 6 (1991), the Comox data"),
         ("Beaumont", "Ronald Beaumont, She Shashishalhem: The Sechelt Language (1985)"),
         ("Galloway", "Brent Galloway, the Halkomelem, Nooksack and Samish sources (1984, 1990, 1993)"),
         ("Montler", "Timothy Montler, North Straits Salish Classified Word List (1991)"),
         ("Efrat", "Barbara Efrat, A Grammar of Non-Particles in Sooke (1969)"),
         ("Bates, Hess, & Hilbert", "Dawn Bates, Thom Hess and Vi Hilbert, Lushootseed Dictionary (1994)"),
         ("Drachman", "Gaberell Drachman, Twana Phonology (1969)"),
         ("Modrow", "Ruth Modrow, The Quinault Dictionary (1971)"),
         ("Snow", "Charles Snow, A Lower Chehalis Phonology (1969)"),
         ("Kinkade", "M. Dale Kinkade, the Upper Chehalis, Cowlitz and Columbian sources (1981, 1991, 2004)"),
         ("Edel", "May Edel, The Tillamook Language (1939)"),
         ("van Eijk", "Jan van Eijk, The Lillooet Language (1997)"),
         ("Mattina", "Anthony Mattina, the Okanagan sources (1987, 2005)"),
         ("Carlson & Flett", "Barry Carlson and Pauline Flett, Spokane Dictionary (1989)"),
         ("Greene", "Rebecca J. Greene, An Edition of Snshitsu’umshtsn (2004)"),
         ("Michael Silverstein", "the source of the Chinookan data, a personal communication"),
         ("Rob Moore", "the source of the Chinookan data, a personal communication"),
         ("Bagge", "Lilian M. Bagge, The Early Numerals (1906)"),
         ("Le Corre & Carey", "Mathieu Le Corre and Susan Carey, One, Two, Three, Four, Nothing More (2007)"),
         ("Whalen et al.", "John Whalen, C. R. Gallistel and Rochel Gelman, Nonverbal Counting in Humans (1999)"),
         ("Holterman", "Jack Holterman and others, A Blackfoot Language Study (1996)"),
         ("Boas", "Franz Boas, Handbook of American Indian Languages (1911)"),
         ("Patrick & Tress", "Dorothy Patrick and Susie Tress, Nedut’en (Babine) Bilingual Classroom Dictionary (1991)"),
         ("Goddard", "Pliny Earle Goddard, Beaver Dialect and Texts (1917)"),
         ("Rice", "Keren Rice, Hare Dictionary (1978)"),
         ("Ray", "Verne F. Ray, Cultural Relations in the Plateau of North America (1939)"),
         ("Teit", "James Alexander Teit, The Salishan Tribes of the Western Plateaus (1930)")]
LANGUAGES = [(LANGUAGE, "the protolanguage whose counting system the paper reconstructs"),
             ("Salishan", "the family whose numerals the paper compares"),
             ("Columbian", "Southern Interior Salish, its naqs '1' borrowed"),
             ("Nez Perce", "Sahaptian, its naaqc '1' and quinary counting"),
             ("Quinault", "Tsamosan Salish"),
             ("Thompson", "Northern Interior Salish"),
             ("Navajo", "Athapaskan, unanalyzable numerals below its base"),
             ("English", "unanalyzable numerals below its base, and the paper's glosses"),
             ("Lushootseed", "Central Salish, a column of (1)"),
             ("Tillamook", "Coast Salish, a column of (1)"),
             ("Shuswap", "Northern Interior Salish, a column of (1)"),
             ("Squamish", "Central Salish"),
             ("Okanagan", "Southern Interior Salish, a column of (5)"),
             ("Spokane-Kalispel-Flathead", "Southern Interior Salish"),
             ("Wichita", "Caddoan, counting on five"),
             ("Munsee Delaware", "Algonquian, counting on five"),
             ("Upper Kuskokwim", "Athapaskan, counting multiples of twenty"),
             ("Bella Coola", "Salishan, the subtractive ʔasmús '3'"),
             ("Comox", "Central Salish"),
             ("Sechelt", "Central Salish"),
             ("Halkomelem", "Central Salish"),
             ("Nooksack", "Central Salish, its data partial"),
             ("Northern Straits", "Central Salish"),
             ("Klallam", "Central Salish"),
             ("Twana", "Central Salish"),
             ("Lower Chehalis", "Tsamosan Salish, its data partial"),
             ("Upper Chehalis", "Tsamosan Salish, a column of (5)"),
             ("Cowlitz", "Tsamosan Salish"),
             ("Lillooet", "Northern Interior Salish"),
             ("Coeur d’Alene", "Southern Interior Salish"),
             ("Pentlatch", "Central Salish, with no published numerals"),
             ("Proto-Coast-Salish", "the reconstructions for 7 to 9 in (2)"),
             ("Chinookan", "the source of Nez Perce 9 and perhaps of the Columbian 4"),
             ("Chumash", "Chumashan, a quaternary system"),
             ("Proto-Indo-European", "a restricted count ending at four, after Bagge"),
             ("Saanich", "Northern Straits Salish"),
             ("Samish", "Northern Straits Salish"),
             ("Colville-Okanagan", "Southern Interior Salish"),
             ("Miluk", "Coosan, subtractive forms for 6 to 9"),
             ("Proto-Sahaptian", "a possible source of the decimal system"),
             ("Sahaptian", "neighbors of the Salishan languages"),
             ("Blackfoot", "Algonquian, decimal"),
             ("Barbareño", "Chumash, a column of (5)")]
RUNNING = paper.running_numbers_set()
FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
# The language named last before a form in its paragraph is the form's, Lushootseed dᶻəlačiʔ ...
# comes from dᶻál-; a star form after PS or PCS is that protolanguage's.
SHORT = {"PS": "Proto-Salish", "PCS": "Proto-Coast-Salish"}
NAMED = re.compile(r"(?<![\w-])(%s)(?![\w-])" % "|".join(
    re.escape(one) for one in sorted([name for name, _ in LANGUAGES] + list(SHORT), key=len, reverse=True)))


def language_before(text):
    found = list(NAMED.finditer(text))
    if not found:
        return None
    name = found[-1].group(1)
    return SHORT.get(name, name) if name not in ("Salishan", "English") else None


paper.language_before = language_before


def form_language(run):
    # Most of the italic forms are defined in plain letters, *mus ‘four’ and cilkst ‘five’; the
    # italic a priori on page 3 is English.
    return None if run == "a priori" or not re.search(r"[^\W\d_]", run) else L


paper.form_language = form_language


def printed(number):
    return bool(paper.text(number)) and not paper.lines[number][2] and number not in RUNNING \
        and number not in FOOT


def after(number):
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def title(first, head):
    """The label and title of a numbered display, from its first line to the line before head."""
    label, text = gen.EXAMPLE.match(paper.text(first)).groups()
    lines, line = [text], after(first)
    while line < head:
        lines.append(paper.text(line))
        line = after(line)
    return "(%s)" % label, " ".join(lines)


def numeral_table(heads, what):
    """A table of numerals, one column to a language, the heads their names: its title, each row's
    numeral a note and each cell a cited form of the column's language."""
    def block(first, where):
        head = paper.find(r"^%s$" % r"\s+".join(re.escape(one) for one in heads), first)
        label, text = title(first, head)
        page = paper.page(first)
        paper.add(label, A, "note", text, "page %d, the table's title" % page)
        paper.add(label, A, "note", "   ".join(heads), "page %d, the table's column heads, %s" % (page, what))
        line = after(head)
        while True:
            cells = re.split(r"\s{3,}", paper.spaced[line].strip())
            if not re.fullmatch(r"\d{1,2}", cells[0]) or len(cells) != len(heads) + 1:
                return line
            paper.add(label, A, "note", cells[0], "page %d, the row's numeral" % page)
            for language, form in zip(heads, cells[1:]):
                paper.add(label, language, "cited form", form, "page %d, %s %s" % (page, language, cells[0]))
            line = after(line)
    return block


def similarity(first, where):
    """(2): each numeral's most common root, a Proto-Salish or Proto-Coast-Salish reconstruction, and
    the percentage of the languages with a reflex of it."""
    head = paper.find(r"^Most common root\s+Percentage of languages$", first)
    label, text = title(first, head)
    page = paper.page(first)
    paper.add(label, A, "note", text, "page %d, the table's title" % page)
    paper.add(label, A, "note", "Most common root   Percentage of languages", "page %d, the table's column heads" % page)
    line = after(head)
    while True:
        cells = re.split(r"\s{3,}", paper.spaced[line].strip())
        root = re.fullmatch(r"(PS|PCS) (\S+)", cells[1]) if len(cells) == 3 else None
        if not re.fullmatch(r"\d{1,2}", cells[0]) or not root:
            return line
        paper.add(label, A, "note", cells[0], "page %d, the row's numeral" % page)
        paper.add(label, A, "note", root.group(1), "page %d, the protolanguage of the root for %s" % (page, cells[0]))
        paper.add(label, SHORT[root.group(1)], "cited form", root.group(2),
                  "page %d, the most common root for %s" % (page, cells[0]))
        paper.add(label, A, "note", cells[2], "page %d, the percentage of languages with a reflex of %s"
                  % (page, root.group(2)))
        line = after(line)


def chart(first, where):
    """(3) and (4), bar charts drawn as graphics: the title, and the labels of the two axes a note
    apiece, the percentages up the side and the numerals 1 to 10 along the foot."""
    axis = paper.find(r"^100%$", first)
    label, text = title(first, axis)
    page = paper.page(first)
    paper.add(label, A, "note", text, "page %d, the chart's title" % page)
    side, line = [], axis
    while re.fullmatch(r"\d{1,3}%", paper.text(line)):
        side.append(paper.text(line))
        line = after(line)
    paper.add(label, A, "note", " ".join(side), "page %d, the percentages up the chart's side" % page)
    paper.add(label, A, "note", paper.text(line), "page %d, the numerals along the chart's foot, a bar over each" % page)
    return after(line)


BLOCKS = {paper.find(r"^\(1\) Comparison of Lushootseed"): numeral_table(("Lushootseed", "Tillamook", "Shuswap"),
                                                                         "a numeral's form in each language"),
          paper.find(r"^\(2\) Similarity in Salishan numerals"): similarity,
          paper.find(r"^\(3\) Percentage of cognates"): chart,
          paper.find(r"^\(4\) Percentage of Salishan numerals"): chart,
          paper.find(r"^\(5\) Numerals 1-8 in Barbareño"): numeral_table(("Barbareño", "Okanagan", "Upper Chehalis"),
                                                                         "a numeral's form in each language")}
# Footnote 2's mark closes the range 5-8 on page 3, 5-82.
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS, glued={"2": "5-8"})
# The front matter reader takes the affiliation, as long as a line of the abstract, for the
# abstract's first line; it is the note under the author.
for at, row in enumerate(paper.rows):
    affiliation = "Rutgers, the State University of New Jersey"
    if row[0] == "front" and row[2] == "note" and row[3].startswith(affiliation + " "):
        row[3] = row[3][len(affiliation) + 1:]
        paper.rows.insert(at, ["front", A, "note", affiliation, "page 1, under %s" % AUTHORS[0]])
        break
for row in paper.rows:
    # Page 5 sets the 1 of C₁ as a subscript.
    row[3] = row[3].replace("C1 reduplicated", "C₁ reduplicated")
    # The suffixes for 'hand' on page 8 are the family's, not Shuswap's.
    if row[2] == "cited form" and row[3] in ("-kst", "-əs"):
        row[1] = "Salishan"
paper.write()
