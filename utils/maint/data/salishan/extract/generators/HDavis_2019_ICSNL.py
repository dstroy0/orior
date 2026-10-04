"""The ops of HDavis_2019_ICSNL: Henry Davis on proper names in St'át'imcets. Bare proper nouns act as
common nouns, predicates of type ⟨e,t⟩ taking the common determiners, pluralized, modified and
quantified; proper nouns nominalized with s- act as directly referring expressions, taking the proprial
determiner kw= and only the associative plural wi=. Proper names are lexically ambiguous between the
two types, and s- is Partee's Ident, shifting a name of type e into a predicate that keeps its
reference.

The St’át’imcets examples are segmented on their first line (opening = "segmentation"), glossed under
it and translated. The English examples (2) to (6) and (57) and the formulas (1), (7), (64), (65) and
(67) to (70) are displays, a formula row to each printed line, their subscripts set inline. The
claims (i) and (ii) of the introduction and the properties (i) to (iv) of section 2.1 are notes.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
LANGUAGE = "St’át’imcets"
AUTHORS = ["Henry Davis"]
paper = gen.Paper("HDavis_2019_ICSNL", authors=AUTHORS[0], language=LANGUAGE)
paper.opening = "segmentation"
NAMES = [("Carl Alexander", "Qway7án’ak, a St’át’imcets speaker; Alexander (2016), Sptakwlh múta7 Sqwéqwqel’"),
         ("Lisa Matthewson", "thanked; Matthewson (2005), Iwan Kwikws"),
         ("Ora Matushansky", "thanked; Matushansky (2008), the linguistic complexity of proper names"),
         ("Malte Zimmermann", "thanked"),
         ("Francis “Bill” Edwards", "from Ts’k’wáylacw (Pavilion); Edwards et al. (2017)"),
         ("Frege", "the descriptivist tradition"), ("Russell", "the descriptivist tradition"),
         ("Cumming", "Sam Cumming, Names (2016)"), ("Burge", "Tyler Burge, reference and proper names (1973)"),
         ("Geurts", "Bart Geurts, the description theory of names (1997)"),
         ("Fara", "Delia Graff Fara, names are predicates (2015)"),
         ("Sloat", "Clarence Sloat, proper nouns in English (1969)"), ("J. S. Mill", "Millianism"),
         ("Kaplan", "David Kaplan, demonstratives (1989)"), ("Kripke", "Saul Kripke, Naming and Necessity (1980)"),
         ("LaPorte", "Joseph LaPorte, rigid designators (2018), Laporte in the references"),
         ("Muñoz", "Patrick Muñoz, the proprial article (to appear)"),
         ("Montler", "Timothy Montler, traditional personal names in Klallam (2012)"),
         ("van Eijk", "Jan van Eijk, The Lillooet Language (1997), and his orthography"),
         ("Lyon", "John Lyon; Lyon and Davis (2018), The Outlaws"),
         ("Mitchell", "Sam Mitchell, St’át’imcets stories (to appear); Edwards et al. (2017)"),
         ("Thoma", "Sonja Thoma, independent pronouns (2007)"),
         ("Chierchia", "Gennaro Chierchia, reference to kinds (1998)"),
         ("Partee", "Barbara Partee, type-shifting principles (1986)"),
         ("Davis", "Henry Davis, the author (2011, 2018a, 2018b)"),
         ("Bach", "Kent Bach, the predicate view of proper names (2015)"),
         ("Kroeber", "P. Kroeber, the Salish language family (1999)")]
LANGUAGES = [(LANGUAGE, "Northern Interior Salish (Lillooet), ISO-363 lil"),
             ("Lillooet", "St’át’imcets"), ("Salish", "the family"), ("Northern Interior Salish", "the branch"),
             ("English", "proper nouns with and without determiners"),
             ("Tsimshianic", "two sets of determiners, one for proper nouns"),
             ("Catalan", "a proprial article"), ("Icelandic", "a proprial article"),
             ("Tagalog", "a proprial article"), ("Maori", "a proprial article"),
             ("Klallam", "traditional names (Montler 2012)")]


def display(count, kind="note", gloss="set as an example"):
    """A display of count printed lines, a row of kind to each."""
    return lambda start, where: paper.display(start, count, kind=kind, gloss=gloss, per_line=True)


FORMULA = "a formula, its subscripts set inline"
blocks = {paper.find(r"^\(%s\) " % label): display(count, "formula", FORMULA)
          for label, count in (("1", 1), ("7", 1), ("64", 1), ("65", 2), ("67", 1), ("68", 1), ("69", 2), ("70", 1))}
blocks.update({paper.find(r"^\(%s\) " % label): display(count, gloss="an English example")
               for label, count in (("2", 1), ("3", 1), ("4", 1), ("5", 1), ("6", 2), ("57", 1))})
# The claims (i) and (ii) of the introduction and the properties (i) to (iv) of section 2.1.
blocks.update({paper.find(r"^\(i\) When not"): display(r"^\(ii\) However", gloss="a claim of the paper"),
               paper.find(r"^\(ii\) However"): display(r"^I consider two", gloss="a claim of the paper"),
               **{paper.find(r"^\(%s\) (?:Proper|In many)" % label): display(1, gloss="a property proper nouns "
                                                                                     "share with common nouns")
                  for label in ("i", "ii", "iii", "iv")}})
# Page 14 sets mark 11 on the auxiliary wa7 after its stop, wa7.11 Normally.
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks, glued={"11": "wa7."})
# Burge's entry opens Burge. Tyler. with a stop for the comma, and the reader runs it on from Bach's.
for index, row in enumerate(paper.rows):
    if row[2] == "reference" and " Burge. Tyler. 1973." in row[3]:
        first, second = row[3].split(" Burge. Tyler. 1973.")
        row[3] = first
        paper.rows.insert(index + 1, ["references", A, "reference", "Burge. Tyler. 1973." + second, row[4]])
        break
# The displays (i) of footnotes 14 and 18 are a formula and the prose around it, which the example
# reader takes for tiers: footnote 14's formula over its note on n, and footnote 18's formula between
# its condition and the sentence that closes the note.
KINDS = {("footnote 14 (i) line 1", "segmentation"): (A, "formula", "a formula, its subscripts set inline"),
         ("footnote 14 (i) line 2", "gloss"): (A, "note", "under the formula"),
         ("footnote 18 (i) line 1", "segmentation"): (A, "note", "over the formula"),
         ("footnote 18 (i) line 2", "gloss"): (A, "formula", "a formula, its subscripts set inline"),
         ("footnote 18 (i) line 3", "segmentation"): (A, "note", "closing the note")}
# Page 1 sets nominalizing prefix s- in italics whole, for emphasis; s- is its own cited form.
paper.rows = [row for row in paper.rows if not (row[2] == "cited form" and row[3] == "nominalizing prefix s-")]
for row in paper.rows:
    if (row[0], row[2]) in KINDS:
        row[1], row[2], why = KINDS[(row[0], row[2])]
        row[4] = row[4].split(",")[0] + ", " + why
paper.write()
