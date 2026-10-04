"""The ops of 11_ICSNL55_Huijsmans_Reisinger_Matthewson_final: Marianne Huijsmans, D. K. E.
Reisinger and Lisa Matthewson on the determiners of ʔayʔaǰuθəm as evidentials: tə and ɬə current
direct evidence, šə and ɬ previous direct evidence, kʷ evidence-neutral, with ɬə and ɬ feminine
singular, and a Speasian analysis by relations between situations.

The examples are the interlinear layout gen.Paper.example reads: a context, the orthographic line,
a phonemic segmentation, a gloss and a translation, the alternatives in braces and a tier that
wraps read by the gloss under it (footnote 4). (2) and (3) are St’át’imcets, from Matthewson 2012,
and take that language from their citation. (25) and (28) are formulas, set as displays. Each
table is a note to its caption and one to each printed line; each figure caption a note.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
LANGUAGE = "ʔayʔaǰuθəm"
AUTHORS = ["Marianne Huijsmans", "D. K. E. Reisinger", "Lisa Matthewson"]
paper = gen.Paper("11_ICSNL55_Huijsmans_Reisinger_Matthewson_final", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Elsie Paul", "ʔayʔaǰuθəm consultant"), ("Freddie Louie", "ʔayʔaǰuθəm consultant"),
         ("Phyllis Dominic", "ʔayʔaǰuθəm consultant"), ("Betty Wilson", "ʔayʔaǰuθəm consultant"),
         ("Marion Harry", "the late ʔayʔaǰuθəm consultant"), ("Henry Davis", "held the SSHRC Insight grant"),
         ("Davis", "John H. Davis, the determiners in 1973 and 1974, and this volume"),
         ("Watanabe", "Honoré Watanabe, A Morphological Description of Sliammon (2003)"),
         ("Harris", "Herbert Harris, A grammatical sketch of Comox (1981)"), ("Boas", "Franz Boas's notes on Comox"),
         ("Speas", "Margaret Speas, evidentials as functional heads (2010)"),
         ("Kalsang", "Kalsang, Garfield, Speas and de Villiers, Tibetan evidentials (2013)"),
         ("Matthewson", "Lisa Matthewson, St’át’imcets evidentials and determiners"),
         ("Murray", "Sarah Murray, Evidentials (in press)"), ("Jurafsky", "Daniel Jurafsky, the diminutive (1996)"),
         ("Suttles", "Wayne Suttles, Musqueam Reference Grammar (2004)"), ("Montler", "Timothy Montler, Saanich and Klallam"),
         ("Gillon", "Carrie Gillon, The Semantics of Determiners (2006)"), ("Gerdts", "Donna B. Gerdts and Thomas E. Hukari (2004)"),
         ("Sylvia", "in Matthewson's St’át’imcets examples (2), (3)"), ("Daniel", "Daniel Reisinger, in (11) and (19)"),
         ("Marianne", "Marianne Huijsmans, in (16)"), ("Betty", "Betty Wilson, in (16)"), ("Gloria", "in (22) to (24)"),
         ("Kratzer", "Angelika Kratzer, situations (2019)"), ("Elbourne", "Paul Elbourne, definite descriptions (2013)"),
         ("Renans", "Agata Renans, Ga determiners (2016)"), ("Grice", "H. Paul Grice (1975)"), ("Heim", "Irene Heim (1991)"),
         ("Bochnak", "M. Ryan Bochnak (2016)")]
LANGUAGES = [(LANGUAGE, "Comox-Sliammon, Central Salish, ISO 639-3 coo"),
             ("Comox-Sliammon", "ʔayʔaǰuθəm"), ("Central Salish", "the branch"), ("Salish", "the family"),
             ("St’át’imcets", "Lillooet, Northern Interior Salish; (2) and (3)"), ("Lillooet", "St’át’imcets"),
             ("Comox", "Harris 1981, the Island dialect"), ("Halkomelem", "Central Salish"),
             ("Island Halkomelem", "Central Salish, the feminine determiner of diminutives"),
             ("Upriver Halkomelem", "Central Salish"), ("Musqueam", "Central Salish, Table 6"),
             ("Lummi", "Straits Salish, Table 7"), ("Sḵwx̱wú7mesh", "Squamish, Table 8"),
             ("Secwepemctsin", "Shuswap, Interior Salish"), ("Nɬeʔkepmxcín", "Thompson, Interior Salish"),
             ("Saanich", "Straits Salish"), ("Sechelt", "Central Salish"), ("Klallam", "Straits Salish"),
             ("Hebrew", "diminutives as feminine"), ("Hindi", "diminutives as feminine"),
             ("Berber", "diminutives as feminine"), ("Tibetan", "Kalsang et al. 2013"), ("English", "the translations")]


def table(start, where):
    """A table: its caption a note and each printed line under it a note. A table's lines are
    short; it ends at a line of the full measure, Formally, we propose ... under Table 5, or at a
    blank line with no short line of a cell after it (Table 6 sets its Oblique cell ƛ̓ apart)."""
    label = re.match(r"^Table (\d+):", paper.text(start)).group(1)
    here = "Table %s" % label
    paper.add(here, A, "note", paper.text(start), "page %d, the table's caption" % paper.page(start))
    number, count = start + 1, 0

    def next_text(at):
        while at <= paper.last and not paper.text(at).strip():
            at += 1
        return paper.text(at) if at <= paper.last else ""

    while number <= paper.last:
        text = paper.text(number)
        if not text.strip():
            after = next_text(number)
            if count and (len(after.strip()) >= 40 or after.startswith("Table ") or gen.EXAMPLE.match(after)):
                break
            number += 1
            continue
        if len(text.strip()) >= 70 or text.startswith("Table ") or gen.HEADING.match(text):
            break
        count += 1
        paper.add("%s line %d" % (here, count), A, "note", text,
                  "page %d, the table's line %d as printed" % (paper.page(number), count))
        number += 1
    return number


def figure(start, where):
    """A figure's caption, a note; the figure itself is a drawing the text layer holds nothing of."""
    label = re.match(r"^Figure (\d+):", paper.text(start)).group(1)
    paper.add("Figure %s" % label, A, "note", paper.text(start), "page %d, the figure's caption" % paper.page(start))
    return start + 1


blocks = {number: table for number in range(1, paper.last + 1) if re.match(r"^Table \d+:", paper.text(number))}
blocks.update({number: figure for number in range(1, paper.last + 1) if re.match(r"^Figure \d+:", paper.text(number))})
paper.standard(AUTHORS, NAMES, LANGUAGES, front_languages=1, blocks=blocks, displays={"25": 2, "28": 8})
print("# untagged:", paper.languages_by_tag({"(Matthewson 2012": "St’át’imcets"}), file=sys.stderr)
paper.write()
