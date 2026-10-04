"""The ops of Davis-final: Henry Davis on the count-mass distinction in St'át'imcets (and beyond):
the core diagnostics, number marking on determiners and plural reduplication on nouns in
St'át'imcets, the Upriver Halkomelem evidence against Wiltschko's claim that the distinction is
absent, and a preliminary typology.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
paper = gen.Paper("Davis-final", authors="Henry Davis", language="St’át’imcets")

NAMES = [("Carl Alexander", "St’át’imcets speaker, supplied the unattributed examples"),
         ("Laura Thevarge", "St’át’imcets speaker, supplied the unattributed examples"),
         ("Strang Burton", "thanked"), ("Peter Jacobs", "thanked"), ("Hotze Rullmann", "thanked"),
         ("Martina Wiltschko", "thanked; the Upriver Halkomelem analysis"), ("Lisa Matthewson", "thanked"),
         ("Wiltschko", "Martina Wiltschko (2005, 2008, 2012)"), ("Chierchia", "Chierchia (1998, 2010)"),
         ("Wilhelm", "Wilhelm (2008), Dëne Suɬine")]
LANGUAGES = [("St’át’imcets", "Northern Interior Salish (Lillooet)"),
             ("Upriver Halkomelem", "Central Salish"), ("Halkomelem", "Central Salish"),
             ("English", "compared for the core diagnostics"), ("Mandarin", "classifiers and massifiers"),
             ("Dëne Suɬine", "Athabaskan, number neutral")]



def paradigm(start, where):
    """A list of nouns under SINGULAR and PLURAL heads, a. ‘child’ sk’úk’wmi7t sk’wemk’úk’wmi7t:
    the heads a note, each item's gloss a translation and its two forms cited forms."""
    number = gen.EXAMPLE.match(paper.text(start)).group(1)
    language = "Upriver Halkomelem" if number == "41" else gen.L
    paper.add("(%s) line 1" % number, A, "note", paper.text(start).split(None, 1)[1],
              "page %d, the column heads" % paper.page(start))
    line = start + 1
    while gen.SUB.match(paper.text(line)):
        cells = [one.strip() for one in paper.spaced[line].split("   ") if one.strip()]
        letter, said = cells[0].split(None, 1)
        where = "(%s%s)" % (number, letter[0])
        paper.add(where, A, "translation", said, "page %d" % paper.page(line))
        paper.add(where, language, "cited form", cells[1], "page %d, the singular" % paper.page(line))
        paper.add(where, language, "cited form", cells[2], "page %d, the plural" % paper.page(line))
        line += 1
    return line


blocks = {paper.find(r"^\(%s\)\s+SINGULAR" % number): paradigm for number in ("17", "20", "41")}
# The English sentences of the core diagnostics, (1) to (6), a row to each lettered item.
for number, count in (("1", 2), ("2", 2), ("3", 2), ("4", 2), ("5", 2), ("6", 4)):
    blocks[paper.find(r"^\(%s\)\s+a\." % number)] = (lambda count: lambda first, where: paper.display(
        first, count, who="English", gloss="the English sentence"))(count)
# The determiner table of (8), the number-marking table of (47) and the typology of (53) are set a
# printed line to a row.
DISPLAYS = {"8": (r"^Singular count nouns obligatorily", True), "47": (r"^The two systems are", True),
            "53": (r"^Of course, \(53\)", True),
            # The claims of (7) and (52) and the formulas of (48) to (51), whose logical signs the
            # text layer lost, are English set as an example.
            "7": (1, True), "48": (2, True), "49": (2, True), "50": (5, True), "51": (3, True), "52": (1, True)}
# Marks the page glues to a form, whose 7 is the glottal stop, each named by that form.
GLUED = {"5": "lhel=ki=qú7=a", "8": "smelhmúlhats"}
paper.standard(["Henry Davis"], NAMES, LANGUAGES, blocks=blocks, displays=DISPLAYS, glued=GLUED)
# The glued mark placed its note and comes off the form.
for row in paper.rows:
    for mark, form in GLUED.items():
        if re.search(r"(?:^|\s)%s%s(?=\s|$)" % (re.escape(form), mark), row[3]):
            row[3] = re.sub(r"(?<=%s)%s(?=\s|$)" % (re.escape(form), mark), "", row[3])
            row[4] += ", carries footnote " + mark
UH = "Upriver Halkomelem"
fixed = []
for index, row in enumerate(paper.rows):
    # Wiltschko's examples in 4.1 and 4.2, and those of note 16, are Upriver Halkomelem, and so is
    # the adjective i'axwíl the text of 4.3 cites.
    if row[1] == gen.L and (re.match(r"^\((42|43|44|46)[a-z]?\)", row[0]) or row[0].startswith("footnote 16 (")
                            or row[3] == "i’axwíl"):
        row[1] = UH
    # The paper sets no segmentation tier: a third line of a language is its sentence wrapped.
    if row[2] == "segmentation":
        if row[3] == "(Strang Burton, p.c. 2014)":
            row[1], row[2], row[4] = A, "citation", row[4] + ", under the gloss"
        else:
            row[2] = "transcription"
    # (44b) gives two readings, (i) and (ii), and its source after the second.
    if row[3] == "(i) ‘I saw a piece of wood.":
        fixed.append(["(44b) line 3", A, "translation", "‘I saw a piece of wood.", row[4] + ", reading (i)"])
        continue
    if row[3].startswith("(ii) ‘I saw a little bit of wood.’ (Wiltschko 2012:154) "):
        following = paper.rows[index + 1]
        fixed.append(["(44b) line 4", A, "translation", "‘I saw a little bit of wood.’", row[4] + ", reading (ii)"])
        fixed.append(["(44b) line 5", A, "citation", "(Wiltschko 2012:154)", row[4] + ", the source at the right"])
        following[3] = row[3].split("(Wiltschko 2012:154) ", 1)[1] + " " + following[3]
        continue
    fixed.append(row)
paper.rows[:] = fixed
paper.write()
