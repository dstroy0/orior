"""The ops of 2013_Nater: Hank Nater's How Salish is Bella Coola?, the Bella Coola verbo-nominal
lexicon sorted by the origin of each item: 7.4% Coastal Salish, 4.9% Interior Salish, 16.4%
proto-Salish, 16.8% non-Salish, mainly North Wakash, 2.7% areal and 51.8% unknown.

The paper's data stand in ruled tables numbered through by line, 1 to 1407: the Coastal and
Interior Salish, proto-Salish, areal, North Wakash and other non-Salish cognates of §2, ecological
and not, each row a line number, a gloss, the Bella Coola form and its cognates; and §4's list of
the forms of unknown origin, each row a line number, the Na90 entry in the practical orthography
with its gloss, and the phonemic form. A row whose number the paper erases, a derivation or an
allomorph, keeps the number it skips. Each form is a transcription keyed to its line, the gloss a
translation, the cognates a note as printed. The tables are read by page_text.ruled_tables cell by
cell, since the glyph rows run a wrapped cell into its neighbors'; the page text sets each table
row on a line, which each block checks against the cells. §3's table of totals is a note to each
cell, and its tree of the Salish branches a note.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402
import page_text  # noqa: E402

A, L = gen.A, gen.L
STEM = "2013_Nater"
LANGUAGE = "Bella Coola"
AUTHORS = ["Hank Nater"]
paper = gen.Paper(STEM, authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Newman", "Stanley Newman (1973), retention and diffusion in Bella Coola"),
         ("Kuipers", "Aert H. Kuipers (1967, 1969, 1974, 1998, 2002), the Salish etymological dictionary"),
         ("Blevins", "Juliette Blevins (2003), Yurok syllable weight"),
         ("Carrier Dictionary Committee", "the Central Carrier Bilingual Dictionary (1974)"),
         ("Davidson", "Matthew Davidson (2002), Southern Wakashan grammar"),
         ("Davis", "Henry Davis, with Van Eijk (2012), Lillooet bird terminology"),
         ("Van Eijk", "Jan P. van Eijk (1985), the Lillooet language; with Davis (2012)"),
         ("Goddard", "Ives Goddard (1975), Algonquian, Wiyot and Yurok"),
         ("Lincoln", "Neville J. Lincoln, with Rath (1980, 1986), North Wakashan roots and Haisla"),
         ("Rath", "John C. Rath (2010), the Heiltsuk-English dictionary; with Lincoln (1980, 1986)"),
         ("Morice", "A. G. Morice (1932), the Carrier language"),
         ("Pinnow", "Heinz-Jürgen Pinnow (1964), the North American Indian languages, the map of §3"),
         ("Timmers", "J. A. Timmers (1977), the English-Sechelt word list")]
LANGUAGES = [(LANGUAGE, "the language of the paper, Nuxalk"), ("Nuxalk", "Bella Coola, its other name"),
             ("Salish", "the family Bella Coola belongs to"), ("Coastal Salish", "CS, a branch of Salish"),
             ("Coast Salish", "CS, Kuipers's name for Coastal Salish"),
             ("Interior Salish", "IS, a branch of Salish"), ("proto-Salish", "PS, the family's ancestor"),
             ("North Wakash", "NW, the Wakashan languages north of Bella Coola"), ("Wakashan", "the family of NW"),
             ("Haisla", "North Wakash"), ("Heiltsuk", "North Wakash"), ("Kwakiutl", "North Wakash"),
             ("Oowekyala", "North Wakash"), ("Nootka", "South Wakashan"), ("Lillooet", "Interior Salish"),
             ("Shuswap", "Interior Salish"), ("Sechelt", "Coastal Salish"), ("Squamish", "Coastal Salish"),
             ("Halkomelem", "Coastal Salish"), ("Proto-Athabascan", "PA"), ("Athabascan", "a northern neighbor"),
             ("Carrier", "Athabascan"), ("Eyak", "related to Athabascan"), ("Quileute", "a southern areal source"),
             ("Chinook", "a southern areal source"), ("Chinook Jargon", "whose loans are left out"),
             ("Ritwan", "Yurok and Wiyot, the southmost source"), ("Yurok", "Ritwan"), ("Wiyot", "Ritwan"),
             ("Algonquian", "Goddard (1975)"), ("Tsimshian", "sporadic contact"), ("Gitksan", "sporadic contact"),
             ("Tillamook", "Tillamook-Siletz"), ("English", "the glosses and loans left out")]

document = page_text.paper_document(STEM)[0]


def same(one, two):
    return " ".join(one.split()) == " ".join(two.split())


# Each ruled table by the page text line its first row stands on: (page, rows).
TABLES = {}
for number in range(1, len(document) + 1):
    for _, _, rows in page_text.ruled_tables(document[number - 1]):
        first = page_text.ruled_line(rows[0])
        start = next(line for line in range(paper.last + 1)
                     if paper.page(line) == number and not paper.lines[line][2] and same(paper.text(line), first))
        TABLES[start] = (number, rows)
state = {"line": 0, "head": None}
# Tokens of a §4 entry in the practical orthography, upper case: TS = /c/, 7 = /ʔ/, √ a root.
PRACTICAL = re.compile(r"^[A-Z0-9√’'()+:/,\-]+$")


def item(cell, page):
    """The line number a row sets, or the one the paper skips where it erases it; the number a
    note where printed."""
    if cell:
        state["line"] = int(cell[0])
        paper.add("line %d" % state["line"], A, "note", cell[0], "page %d, the line's number" % page)
        return "line %d" % state["line"], ""
    state["line"] += 1
    return "line %d" % state["line"], ", its number erased, a derivation or allomorph not counted"


def table(start, where):
    """A ruled table, its rows checked against the page text's lines."""
    page, rows = TABLES[start]
    for offset, row in enumerate(rows):
        assert same(paper.text(start + offset), page_text.ruled_line(row)), (start + offset, paper.text(start + offset))
    for row in rows:
        cells = [page_text.cell_text(one) for one in row]
        if cells[0] == "Line":
            state["head"] = cells
            for cell in cells:
                paper.add(where, A, "note", cell, "page %d, a column's head" % page)
            continue
        if len(cells) == 8:
            totals(cells, page, where)
            continue
        at, erased = item(row[0], page)
        if len(cells) == 4:
            paper.add(at, A, "translation", cells[1], "page %d, the gloss%s" % (page, erased))
            paper.add(at, L, "transcription", cells[2], "page %d, the Bella Coola form" % page)
            if cells[3]:
                paper.add(at, A, "note", cells[3], "page %d, under %s" % (page, state["head"][3]))
        else:
            # A derivation under an entry can set its gloss alone, pole canoe upriver under line 728.
            words = cells[1].split(" ")
            count = next(at for at, one in enumerate(words + ["a"]) if re.search(r"[a-z]", one))
            spelled, gloss = " ".join(words[:count]), " ".join(words[count:])
            assert all(PRACTICAL.match(one) for one in words[:count]), cells[1]
            if spelled:
                paper.add(at, L, "transcription", spelled, "page %d, the Na90 entry in the practical orthography%s"
                          % (page, erased))
            paper.add(at, A, "translation", gloss, "page %d, the Na90 gloss%s" % (page, "" if spelled else erased))
            paper.add(at, L, "transcription", cells[2], "page %d, the phonemic notation" % page)
    return start + len(rows)


def totals(cells, page, where):
    """A row of §3's table of totals: its label, and each count under its column's head."""
    heads = ["PS", "CS", "IS", "A", "NS", "?", "totals"]
    if not cells[0]:
        for cell in cells[1:]:
            paper.add(where, A, "note", cell, "page %d, the table of totals, a column's head" % page)
        return
    paper.add(where, A, "note", cells[0], "page %d, the table of totals, a row's head" % page)
    for head, cell in zip(heads, cells[1:]):
        if cell:
            paper.add(where, A, "note", cell, "page %d, the table of totals, %s under %s" % (page, cells[0], head))


def tree(start, where):
    """§3's tree of the Salish branches: proto-Salish over Interior Salish and pre-Coastal Salish,
    pre-Coastal Salish over Bella Coola and Coastal Salish, Coastal Salish over Coast-Olympic Salish
    and Tillamook-Siletz; its labels in the lines the page text reads them."""
    end = paper.find(r"^At first blush", start)
    labels = [paper.text(one) for one in range(start, end) if paper.text(one).strip()]
    paper.add(where, A, "note", " / ".join(labels),
              "page %d, a tree: proto-Salish over Interior Salish and pre-Coastal Salish, pre-Coastal Salish over "
              "Bella Coola and Coastal Salish, Coastal Salish over Coast-Olympic Salish and Tillamook-Siletz"
              % paper.page(start))
    return end


BLOCKS = dict.fromkeys(TABLES, table)
BLOCKS[paper.find(r"^Interior Salish$")] = tree
# Davis and Van Eijk's place, Cranbrook, B.C., and Van Eijk's, Amsterdam, Netherlands, wrap onto a
# line of their own that opens like an entry.
paper.reference_lines_run_on = [paper.find(r"^Cranbrook, B\.C\.$", paper.find(r"^References$")),
                                paper.find(r"^Amsterdam, Netherlands\.$", paper.find(r"^References$"))]
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS, appendix=r"^139 Poplar Drive$")


def split_reference(opening):
    """Part the reference row that runs the entry opening onto the entry before it, and return the
    two: the reader opens no entry on Van Eijk, whose name is two words, or on Davis after the
    raised th of 47th, which stands on a row of its own before Davis's line and ends no entry."""
    at = next(at for at, row in enumerate(paper.rows) if row[2] == "reference" and " " + opening in row[3])
    row = paper.rows[at]
    row[3], after = row[3].split(" " + opening, 1)
    paper.rows.insert(at + 1, [row[0], row[1], "reference", opening + after, row[4]])
    return row, paper.rows[at + 1]


davidson, davis = split_reference("Davis, Henry & Jan P. Van Eijk. 2012.")
assert davidson[3].endswith(" (PhD dissertation) th") and " 47 ICSNL." in davis[3], (davidson, davis)
davidson[3] = davidson[3][:-len(" th")]
davis[3] = davis[3].replace(" 47 ICSNL.", " 47th ICSNL.")
davis[4] += ", the raised th of 47th set flat"
split_reference("Van Eijk, J. P. 1985.")
tail = paper.find(r"^139 Poplar Drive$", paper.find(r"^References$"))
paper.add("end", A, "note", paper.joined([tail, tail + 1, tail + 2]),
          "page %d, the author's address and e-mail address" % paper.page(tail))
paper.write()
