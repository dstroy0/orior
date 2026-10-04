"""The ops of 05_ICSNL55_Davis_Griffin_Huijsmans_Mellesmoen_final: Henry Davis, Laura Griffin,
Marianne Huijsmans and Gloria Mellesmoen on extracting possessors in ʔayʔaǰuθəm, snaʔ and naʔ, and
the possessive predicates of Sechelt, Pentlatch, Squamish, Halkomelem and Lillooet.

The ʔayʔaǰuθəm examples set four lines, the community orthography over a segmentation, its gloss
and the translation; the other languages' examples drop the orthography line (footnote 7). Each
example opens on the tier its layout gives (opening = "auto"), and takes its language from the name
at the right of its translation, (Squamish: Kuipers 1967:177). Tables 1 and 2 are written a row a
printed line.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
LANGUAGE = "ʔayʔaǰuθəm"
AUTHORS = ["Henry Davis", "Laura Griffin", "Marianne Huijsmans", "Gloria Mellesmoen"]
paper = gen.Paper("05_ICSNL55_Davis_Griffin_Huijsmans_Mellesmoen_final", authors=", ".join(AUTHORS),
                  language=LANGUAGE)
paper.opening = "auto"

NAMES = [("Elsie Paul", "ʔayʔaǰuθəm consultant"), ("Betty Wilson", "ʔayʔaǰuθəm consultant"),
         ("Freddie Louie", "ʔayʔaǰuθəm consultant"), ("Joanne Francis", "ʔayʔaǰuθəm consultant"),
         ("Karen Galligos", "the late ʔayʔaǰuθəm consultant"),
         ("Carl Alexander", "St’át’imcets consultant"),
         ("Daniel Reisinger", "suggested the comparison with Sechelt"),
         ("Watanabe", "Honoré Watanabe, the grammar of ʔayʔaǰuθəm (2003)")]
LANGUAGES = [(LANGUAGE, "Comox-Sliammon, the northernmost Central Salish language, ISO 639-3 coo"),
             ("Comox-Sliammon", "the paper's other name for ʔayʔaǰuθəm"),
             ("Squamish", "Central Salish"), ("Halkomelem", "Central Salish"),
             ("Downriver Halkomelem", "Suttles 2004"), ("Island Halkomelem", "Gerdts 1988, Gerdts and Hukari 2008"),
             ("Sechelt", "Central Salish, Beaumont 2011"), ("Pentlatch", "Central Salish, Kinkade 1980"),
             ("Lillooet", "Northern Interior Salish, St’át’imcets"), ("St’át’imcets", "Lillooet"),
             ("Shuswap", "Northern Interior Salish"), ("Thompson", "Northern Interior Salish"),
             ("Russian", "where ‘x is at y’ means ‘y has x’")]
TAGS = {"(" + name: name for name in ("Squamish", "Downriver Halkomelem", "Island Halkomelem", "Sechelt",
                                      "Pentlatch", "Lillooet", LANGUAGE)}


def table(start, where):
    """A table a row a printed line, its caption first, to the blank line under it."""
    here = paper.text(start).split(":")[0]
    paper.add(here, A, "note", paper.text(start), "page %d, the table's caption" % paper.page(start))
    number, count = start + 1, 0
    while paper.text(number):
        count += 1
        paper.add("%s line %d" % (here, count), A, "note", paper.text(number),
                  "page %d, the table a printed line" % paper.page(number))
        number += 1
    return number


blocks = {paper.find(r"^Table 1: "): table, paper.find(r"^Table 2: "): table}
paper.standard(AUTHORS, NAMES, LANGUAGES, front_languages=2, blocks=blocks)
paper.languages_by_tag(TAGS)
# Footnote 19's (vii) is Lillooet, set in the text that introduces it; footnote 3's (i) and (ii)
# are Sechelt, from Beaumont's dictionary entry the note cites.
for row in paper.rows:
    if row[0].startswith("footnote 19 (vii)") and row[1] == gen.L:
        row[1] = "Lillooet"
    if row[0].startswith(("footnote 3 (i)", "footnote 3 (ii)")) and row[1] == gen.L:
        row[1] = "Sechelt"
# The cited forms of Section 5 and its notes 15, 19 and 20 are of the language each subsection
# treats. The sentence citing a form names another language for Lillooet cúwaʔ in 5.2, Pentlatch
# c̓uwa and Halkomelem c-weʔ in 5.5, and Lillooet waʔ in 5.6; (s)naʔ in 5.1 is ʔayʔaǰuθəm's own.
# Footnote 20 gives wa(ʔ) to Squamish, Pentlatch and Lillooet, a row each. The italics run naʔ, səná
# together across their comma. Page 12 prints Pentlatch tθuwá and tθ- in italics with a raised θ,
# which breaks the italic run: each is cited before the form after it, cuwá and c-. Page 19 raises
# the ʷ of sk̓ʷičiy and the θ and ʷ of st̕θuk̓ʷ, ʔayʔaǰuθəm words cited after snəq.
SECTIONS = {"§5.1": "Sechelt", "§5.2": "Pentlatch", "footnote 15": "Pentlatch", "§5.3": "Squamish",
            "§5.4": "Halkomelem", "§5.5": "Lillooet", "footnote 19": "Lillooet"}
CITED = {("§5.1", "(s)naʔ"): gen.L, ("§5.2", "cúwaʔ"): "Lillooet", ("§5.5", "c̓uwa"): "Pentlatch",
         ("§5.5", "c-weʔ"): "Halkomelem", ("§5.6", "waʔ"): "Lillooet"}
SPLIT = {("§5.1", "naʔ, səná"): [(gen.L, "naʔ"), ("Sechelt", "səná")],
         ("§5.2", "cuwá"): [("Pentlatch", "tθuwá"), ("Pentlatch", "cuwá")],
         ("§5.2", "c-"): [("Pentlatch", "tθ-"), ("Pentlatch", "c-")],
         ("footnote 20", "wa(ʔ)"): [("Squamish", "wa(ʔ)"), ("Pentlatch", "wa(ʔ)"), ("Lillooet", "wa(ʔ)")]}
AFTER = {("§5.6", "snəq"): [(gen.L, "sk̓ʷičiy", "page 19, in italics, ‘bothersome’"),
                            (gen.L, "st̕θuk̓ʷ", "page 19, in italics, ‘day’")]}
rows = []
for row in paper.rows:
    if row[1] == gen.L and row[2] == "cited form" and (row[0], row[3]) in SPLIT:
        rows += [[row[0], who, "cited form", form, row[4]] for who, form in SPLIT[row[0], row[3]]]
        continue
    if row[1] == gen.L and row[2] == "cited form":
        row[1] = CITED.get((row[0], row[3]), SECTIONS.get(row[0], row[1]))
    rows.append(row)
    if row[2] == "cited form" and (row[0], row[3]) in AFTER:
        rows += [[row[0], who, "cited form", form, gloss] for who, form, gloss in AFTER[row[0], row[3]]]
assert len(rows) == len(paper.rows) + 7, "the split rows are not all found"
paper.rows[:] = rows
paper.write()
