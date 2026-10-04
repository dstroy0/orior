"""The ops of Forbes_2019_ICSNL: Clarissa Forbes on the structure of transitivity in Gitksan. The
Gitksan vP is split into v and Voice: the transitivizers si-, -xw, -T, -in, di-, sil and gun sit in
v and are no allomorphs of each other, while the transitive vowel of independent clauses and the
ergative clitics of dependent ones realize transitive Voice, which merges the external argument.

The page text is read by glyph rows (page_text.py rows): the examples set each word over its
segmentation and gloss, which the text layer reads a column at a time. Each example sets the
Gitksan over its segmentation, the gloss in small capitals (read as lower case) and the translation
with the consultant's initials or the source at the right. Table 1 and the Japanese table (26) are
cited forms glossed with their cells; the trees (23) to (25), (27) to (29), (32) and (33) are a note to
each printed line as the glyph rows read it.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Gitksan"
AUTHORS = ["Clarissa Forbes"]
paper = gen.Paper("Forbes_2019_ICSNL", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Barbara Sennott", "Gitksan consultant (BS), the author's teacher"),
         ("Vince Gogag", "Gitksan consultant (VG), the author's teacher"),
         ("Hector Hill", "Gitksan consultant (HH), the author's teacher"),
         ("Louise Wilson", "Gitksan consultant (LW), the author's teacher"),
         ("Heidi Harley", "thanked; Harley (2008, 2017), Folli and Harley (2007, 2008)"),
         ("Susana Bejar", "thanked"), ("Michael Schwan", "of the UBC Gitksan Research Lab"),
         ("Dunham", "Joel Dunham, the Online Linguistic Database (2014)"),
         ("Rigsby", "Bruce Rigsby, Gitxsan grammar (1986), a later view of Gitksan syntax (1989)"),
         ("Tarpent", "Marie-Lucie Tarpent, a grammar of the Nisgha language (1987)"),
         ("Hunt", "Katharine Hunt, clause structure, agreement and case in Gitksan (1993)"),
         ("Pylkkänen", "Liina Pylkkänen, introducing arguments (2002)"),
         ("Halle", "Morris Halle; Halle and Marantz (1993)"), ("Marantz", "Alec Marantz; Halle and Marantz (1993)"),
         ("Hale", "Kenneth Hale; Hale and Keyser (1993a, 1993b)"),
         ("Keyser", "Samuel Jay Keyser; Hale and Keyser (1993a, 1993b)"),
         ("Larson", "Richard Larson, the double object construction (1988)"),
         ("Burzio", "Luigi Burzio, Italian syntax (1986)"),
         ("Kratzer", "Angelika Kratzer, severing the external argument (1996)"),
         ("Coon", "Jessica Coon, little-v agreement in Ch’ol (2017)"),
         ("Woolford", "Ellen Woolford (1997, 2006)"), ("Folli", "Raffaella Folli; Folli and Harley (2007, 2008)"),
         ("Key", "Gregory Key, the Turkish causative (2013)"),
         ("Peterson", "Tyler Peterson (2006, 2007, 2012, 2019)"),
         ("Belvin", "Robert S. Belvin, the causation hierarchy in Nisgha (1997)"),
         ("Jelinek", "Eloise Jelinek, the ergativity hypothesis in Nisgha (1986)"),
         ("McIntyre", "Andrew McIntyre, particle verbs (2007, 2015)"),
         ("Milway", "Daniel Milway, particle verbs (2013)"), ("Forbes", "Clarissa Forbes, the author (2018)")]
LANGUAGES = [(LANGUAGE, "Interior Tsimshianic, spoken in the British Columbia northern interior"),
             ("Tsimshianic", "the family"), ("Interior Tsimshianic", "the branch, Gitksan and Nisg̲a’a"),
             ("Nisg̲a’a", "Interior Tsimshianic, mutually intelligible with Gitksan, to the west"),
             ("Japanese", "lexical causatives with distinct allomorphs (Harley 2008)"),
             ("Turkish", "an iterated causative (Key 2013)"), ("English", "the lexical causative")]


# The Gitksan the paper sets in italics is defined in plain letters, si-, gun, yukw; its other italics
# are English (Jane ate salmon), the terms it defines (monotonicity), the heads of its trees (vCAUS),
# and titles. The italic runs that are Gitksan, each read in its sentence on the page.
GITKSAN = {"/jilksi", "Gun", "an", "bax̲", "dalk̲", "daw", "di", "didalk̲", "dimootxw", "diyee", "gun", "gun wil",
           "gup", "g̲a", "he", "his", "his gup", "i", "ii", "in", "jiks", "jilks", "jilksd", "kw’as", "kw’asin",
           "kw’ast", "mahl", "mas", "mootxw", "naks", "naksxw", "s", "si", "sil", "sil(g̲a) he", "siwilaak’in",
           "siwilaax", "siwilaayin", "si’mas", "T", "t", "ts’eet’iksihl", "ts’iip", "t’aa", "wil", "wilaax",
           "wog̲", "wok̲", "xw", "x̲", "yee", "yukw", "wii t’isim ha’miiyaa ’nii’y aloos"}
paper.form_language = lambda run: L if run in GITKSAN or all(one in GITKSAN for one in re.split(r"[-~∼]", run)) \
    else "Japanese" if run == "s)ase" else None


def display(stop, gloss):
    """A display to the line stop matches, a note to each printed line."""
    return lambda start, where: paper.display(start, stop, gloss=gloss, per_line=True)


TREE = "a line of the tree as the glyph rows read it"


def cells(number):
    return [one for one in re.split(r"\s{3,}", paper.spaced[number].strip()) if one]


def table_1(start, where):
    """Table 1: the caption and the column heads, then each transitivizer a cited form, and its
    intransitive and transitive forms each a cited form glossed with its English, the English a
    note after each."""
    page = paper.page(start)
    paper.add("Table 1", A, "note", paper.text(start), "page %d, the table's caption" % page)
    paper.add("Table 1", A, "note", paper.text(start + 1), "page %d, the column heads" % page)
    number = start + 2
    while len(cells(number)) == 5:
        marker, intransitive, plain, transitive, meaning = cells(number)
        if marker == "unmarked":
            paper.add("Table 1", A, "note", marker, "page %d, Table 1, the row of verbs no transitivizer marks" % page)
            how = "unmarked"
        else:
            paper.add("Table 1", L, "cited form", marker, "page %d, Table 1, the transitivizer" % page)
            how = "with " + marker
        paper.add("Table 1", L, "cited form", intransitive, "page %d, Table 1, intransitive, %s" % (page, plain))
        paper.add("Table 1", A, "note", plain, "page %d, Table 1, the English of intransitive %s" % (page, intransitive))
        paper.add("Table 1", L, "cited form", transitive, "page %d, Table 1, transitive %s, %s" % (page, how, meaning))
        paper.add("Table 1", A, "note", meaning, "page %d, Table 1, the English of transitive %s" % (page, transitive))
        number += 1
    return number


def japanese(start, where):
    """(26): Harley's Japanese roots, each row's root, intransitive and transitive a cited form of
    Japanese glossed with the row's English, and the source under the table a citation."""
    page = paper.page(start)
    paper.add("(26) line 1", A, "note", gen.EXAMPLE.match(paper.text(start)).group(2), "page %d, the column heads" % page)
    number, count = start + 1, 1
    while len(cells(number)) == 4:
        root, intransitive, transitive, meaning = cells(number)
        count += 1
        for kind, form in (("root", root), ("intransitive", intransitive), ("transitive", transitive)):
            paper.add("(26) line %d" % count, "Japanese", "cited form", form, "page %d, the %s of %s" % (page, kind, meaning))
        paper.add("(26) line %d" % count, A, "note", meaning, "page %d, the gloss of the row of %s" % (page, root))
        number += 1
    paper.add("(26) line %d" % (count + 1), A, "citation", paper.text(number), "page %d, under the table" % page)
    return number + 1


blocks = {paper.find(r"^Table 1: Transitivizing"): table_1, paper.find(r"^\(26\) "): japanese,
          paper.find(r"^\(23\) "): display(r"^A split VP", "the trees (23) and (24) side by side, " + TREE),
          paper.find(r"^\(25\) "): display(r"^The v projection", TREE),
          paper.find(r"^\(27\) "): display(r"^In so-called", "the trees (27a) and (27b) side by side, " + TREE),
          paper.find(r"^\(28\) "): display(r"^In a split-vP", TREE),
          paper.find(r"^\(29\) "): display(r"^One crucial", TREE),
          paper.find(r"^\(32\) "): display(r"^The use of the causative", "the trees (32) and (33) side by side, " + TREE)}
# Section 2.2 heads each transitivizer's subsection with the morpheme, 2.2.1 si- and 2.2.2 -xw, a
# title in lower case or opening on a hyphen that the heading reader does not take.
footnote_lines = {one for parts, _ in paper.page_footnotes().values() for one in parts} | set(paper.volume_header())
headings = paper.headings(1, paper.find(r"^References$") - 1, skip=footnote_lines)
headings.update({number: paper.text(number).split()[0] for number in range(1, paper.last + 1)
                 if re.match(r"^2\.2\.[1-8] ", paper.text(number))})
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, spaced={"13": "(34b)."}, headings=headings)
# An affix's italic letters, in and t, are also English words, and the reader finds them where the
# English stands; the page sets them hyphened, -in on page 2 and -t and -di- on page 6 (checked on
# renders). The an of page 18 is the -an of wog̲-an, cited whole.
AFFIXES = {("in", "page 2"): "-in", ("t", "page 6"): "-t", ("di", "page 6"): "-di-"}
paper.rows = [row for row in paper.rows if not (row[2] == "cited form" and row[3] == "an")]
for row in paper.rows:
    if row[2] == "cited form" and (row[3], row[4].split(",")[0]) in AFFIXES:
        row[3] = AFFIXES[(row[3], row[4].split(",")[0])]
# Footnote 7's (ii) is Tarpent's Nisg̲a'a: its translation carries a cf. with the italic kw'ast, and
# the language, italic, stands at the right before the source.
where = next(number for number, row in enumerate(paper.rows) if row[0] == "footnote 7 (ii) line 4")
for row in paper.rows[where - 3:where]:
    row[1] = "Nisg̲a’a"
paper.rows[where][3] = "‘Now the glass is broken.’"
paper.rows[where + 1:where + 1] = [
    ["footnote 7 (ii) line 4", A, "note", "(cf. kw’ast ‘broken’)", "page 7, after the translation"],
    ["footnote 7 (ii) line 4", "Nisg̲a’a", "cited form", "kw’ast", "page 7, broken, the intransitive in isolation"],
    ["footnote 7 (ii) line 4", A, "citation", "Nisg̲a’a;", "page 7, the language at the right of the translation"]]
# Footnote 8's (ia) wraps its last word under the tiers: lax̲mo'on. over lax-mo'on over on-salt.
where = next(number for number, row in enumerate(paper.rows) if row[0] == "footnote 8 (ia) line 4")
for row, kind in zip(paper.rows[where:where + 3], ("transcription", "segmentation", "gloss")):
    row[2] = kind
paper.write()
