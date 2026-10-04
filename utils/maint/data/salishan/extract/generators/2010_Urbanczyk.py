"""The ops of 2010_Urbanczyk: Suzanne Urbanczyk's abstract, Evidence from Halkomelem for a Grounded
Morphology, one page arguing for a word-based, relational model of morphology over morpheme-based and
realizational ones.

The page holds the title, the byline, the university and one paragraph, with no abstract heading,
keywords or sections for gen.front to find; the rows are added here and the paragraph walked by flow.
The italic grounded and Grounded Morphology are English emphasis, the model's name, and are not cited
forms. The paragraph prints Matthews 197x with its year unfilled.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
paper = gen.Paper("2010_Urbanczyk", authors="Suzanne Urbanczyk", language="Halkomelem")
NAMES = [("Anderson", "Anderson (1992), a stem- or lexeme-based approach"),
         ("Matthews", "Matthews (197x), a stem- or lexeme-based approach, the year printed unfilled"),
         ("Stump", "Stump (2001), a stem- or lexeme-based approach"),
         ("Halle", "Halle and Marantz (1992) and Embick and Halle (2005), root-based approaches"),
         ("Marantz", "Halle and Marantz (1992), a root-based approach"),
         ("Embick", "Embick and Halle (2005), a root-based approach"),
         ("Wolf", "Wolf (2008), an OT-based approach"),
         ("Blevins", "Blevins (2006), abstractive against constructive approaches to word formation")]
LANGUAGES = [("Halkomelem", "Central Salish, the language the abstract draws its evidence from")]
paper.add("front", A, "title", paper.text(2), "page 1")
paper.add("front", A, "name", "Suzanne Urbanczyk", "author")
paper.add("front", A, "note", paper.text(4), "page 1, under Suzanne Urbanczyk")
paper.mentioned.add(("name", "Suzanne Urbanczyk"))
paper.add("front", A, "language", LANGUAGES[0][0], LANGUAGES[0][1])
paper.mentioned.add(("language", LANGUAGES[0][0]))
paper.flow(5, 23, "front", names=NAMES, languages=LANGUAGES)
paper.rows = [row for row in paper.rows if row[2] != "cited form"]
paper.write()
