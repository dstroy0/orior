"""The ops of 25-Mattina-Reichards-ear-08: Nancy Mattina on Gladys Amanda Reichard (1893–1955), the
author of the first scientific grammar of a Salishan language, Coeur d'Alene (1938), sixty years
after her death: her critics, her ear, her fieldwork and the work she left unfinished.

The paper is prose with no examples; the text layer spaces inside words, and the page text closes
them up from the glyph positions (page_text.py). Witherspoon's words on page 6 are set as a block
quotation, one note.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
STEM = "25-Mattina-Reichards-ear-08"
LANGUAGE = "Coeur d’Alene"
paper = gen.Paper(STEM, authors="Nancy Mattina", language=LANGUAGE)

NAMES = [("Gladys Reichard", "Gladys Amanda Reichard, 1893–1955, author of the grammar of Coeur d’Alene (1938), the paper's subject"),
         ("Franz Boas", "Reichard's teacher at Columbia, who set her to Coeur d’Alene in 1927"),
         ("Lilian", "Reichard's sister, at her side in Flagstaff"),
         ("Carl Voegelin", "past president of the Linguistic Society of America, a pallbearer at Reichard's funeral"),
         ("Flo Voegelin", "Florence Voegelin, founder of Anthropological Linguistics; saw Reichard's six "
                          "posthumous articles on comparative Salishan into print"),
         ("Melville Jacobs", "reviewed Reichard's grammar of Coeur d’Alene (1940)"),
         ("Larry Thompson", "guest editor of IJAL 46 (1980), the issue dedicated to Reichard"),
         ("Herbert Landar", "an annotated list of Reichard's linguistic publications (1980)"),
         ("Gary Witherspoon", "opened the IJAL memorial issue (1980); quoted on page 6"),
         ("Ivy Doak", "called Reichard's work on Coeur d’Alene outstanding (1997)"),
         ("M. Dale Kinkade", "quoted by Falk (1999) on early Salishan linguistics"),
         ("Ray Brinkman", "on staff at the Coeur d’Alene language program"),
         ("Lawrence Nicodemus", "introduced to linguistics by Reichard, her student at Columbia in 1935 and 1936"),
         ("Harry Hoijer", "reviewed Reichard's Navaho Grammar in IJAL (1953)"),
         ("George Trager", "reviewed Reichard's Navaho Grammar in American Anthropologist (1953)"),
         ("Mary Haas", "Karl Teeter's teacher"),
         ("Karl Teeter", "wrote his dissertation on Wiyot"),
         ("Edward Sapir", "the source of the animosity of Reichard's critics, as Falk (1999) traces it"),
         ("A.L. Kroeber", "with Sapir, expected Reichard's grammar of Wiyot to support the California "
                          "Algonquian hypothesis"),
         ("Lynnika Butler", "linguist for the Wiyot Tribe's language revitalization program"),
         ("Giulio Panconcelli-Calzia", "Rousselot's student, taught the phonetics course Reichard audited in Hamburg"),
         ("Pascal George", "x-rayed pronouncing ten Coeur d’Alene sounds in 1927"),
         ("Julia Antelope", "Nicodemus' mother, wrote to Reichard in Coeur d’Alene"),
         ("Noam Chomsky", "his doubt that language exists (1984, p. 26)")]
LANGUAGES = [(LANGUAGE, "Interior Salishan, the language of Reichard's grammar (1938)"),
             ("Navajo", "Reichard's Navaho Grammar and her work of three decades"),
             ("Wiyot", "the language of Reichard's dissertation (1925)"),
             ("Yurok", "joined with Wiyot in Sapir's California Algonquian hypothesis")]


def quotation(start, where):
    """Witherspoon's words set as a block quotation, one note, to the paragraph that opens it."""
    end = paper.find(r"^Witherspoon’s distillation", start)
    body = paper.joined(range(start, end))
    paper.add(where, A, "note", body, "page %d, a block quotation of Witherspoon (1980, p. 1)" % paper.page(start))
    paper.mentions(where, body, NAMES, "name")
    paper.mentions(where, body, LANGUAGES, "language")
    return end


paper.standard(["Nancy Mattina"], NAMES, LANGUAGES,
               blocks={paper.find(r"^Although some of her writings"): quotation})

# The author's address stands at the foot of page 1 with no mark, and the paragraph over the page
# break reads it as its own; it is a note of its own.
CONTACT = "Contact info: nmattina@prescott.edu"
note = next(row for row in paper.rows if row[0] == "§1" and " %s " % CONTACT in row[3])
note[3] = note[3].replace(" %s " % CONTACT, " ")
at = paper.rows.index(next(row for row in paper.rows if row[0] == "§1"))
paper.rows.insert(at, ["front", A, "note", CONTACT, "page 1, the note at the foot of the page with no mark"])
# The page prints the letter to Boas with a stop after Reichard, Reichard. G.A., and the entry
# before it runs on into it.
entry = next(row for row in paper.rows if row[2] == "reference" and " Reichard. G.A. (1935" in row[3])
first, second = entry[3].split(" Reichard. G.A. (1935")
entry[3] = first
paper.rows.insert(paper.rows.index(entry) + 1, ["references", A, "reference", "Reichard. G.A. (1935" + second, entry[4]])

paper.write()
